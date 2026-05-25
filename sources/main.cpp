#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <thread>
#include <mutex>
#include <queue>

#include <glad/glad.h> 
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny_gltf.h"
#include "stb_image.h"
#include "Shader.h"
#include "ComputeShader.h"
#include "Structs.h"

#include "Scene.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window, float delta_time);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
unsigned int generate_output_texture(unsigned int width, unsigned int height);
void regenerate_buffers();

glm::vec3 camera_position = glm::vec3(0.0f, 8.0f, -80.0f);
glm::vec3 camera_front = glm::vec3(0.0f, 0.0f, 1.0f);
glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
float pitch = 0.f, yaw = 0.f;

float camera_speed = 5.f;
float resolution_scale = 1.0f;
glm::mat4 projection(0.f);

unsigned int samples_per_pixel = 1;
unsigned int max_bounces = 4;

unsigned int SCREEN_WIDTH = 1920;
unsigned int SCREEN_HEIGHT = 1080;
unsigned int RENDER_WIDTH = static_cast<unsigned int>(float(SCREEN_WIDTH) / resolution_scale);
unsigned int RENDER_HEIGHT = static_cast<unsigned int>(float(SCREEN_HEIGHT) / resolution_scale);
unsigned int compute_texture, accum_texture, normal_depth_texture, output_texture;
unsigned int bvh_stack_buffer, scene_bvh_max_depth=0;
unsigned int accum_frame_num = 1;

float lastX = SCREEN_WIDTH / 2, lastY = SCREEN_HEIGHT / 2;
bool first_mouse = true, camera_pos_changed = false;

struct scene_gpu_ready {
	std::vector<triangle_info> triangles;
	std::vector<bvh_node> bvh_nodes;
	std::vector<mesh_info> meshes;
	std::vector<model_info> models;
};

std::vector<Model> models_static;
std::vector<Model> models_sorted;

const int max_bvh_depth = 32;

std::queue<Mesh*> mesh_queue;
std::mutex mesh_queue_mutex;

scene_gpu_ready gpu_ready_scene;

void print_bvh(Mesh& mesh, std::string name, int index, int depth=0) {
	if (depth == 4) return;

	std::cout << name << " node, depth " << depth << std::endl;
	std::cout << "min node: (" << mesh.bvh[index].bounds_min.x << ", " << mesh.bvh[index].bounds_min.y << ", " << mesh.bvh[index].bounds_min.z << "), max node: (" << mesh.bvh[index].bounds_max.x << ", " << mesh.bvh[index].bounds_max.y << ", " << mesh.bvh[index].bounds_max.z << ")" << std::endl;
	if (mesh.bvh[index].triangle_count > 0) {
		std::cout << "Triangle count: " << mesh.bvh[index].triangle_count << std::endl << std::endl;
	}
	else {
		std::cout << std::endl;
		print_bvh(mesh, name + " > left_child", mesh.bvh[index].child_index, depth+1);
		print_bvh(mesh, name + " > right_child", mesh.bvh[index].child_index+1, depth + 1);
	}
}

bool model_pred_func(const Model& first, const Model& second) {
	return glm::length(first.pos - camera_position) < glm::length(second.pos - camera_position);
}

void makeGpuReady(Scene scene) {
	int counter = 0;
	for (int i = 0; i < scene.meshes.size(); ++i) {
		mesh_queue.push(&scene.meshes[i]);
	}
	std::vector<std::thread> bvh_threads;
	for (int i = 0; i < 8; ++i) {
		bvh_threads.push_back(std::thread([]() {
			while (true) {
				mesh_queue_mutex.lock();
				if (mesh_queue.empty()) {
					mesh_queue_mutex.unlock();
					break;
				}
				Mesh* working_mesh = mesh_queue.front();
				mesh_queue.pop();
				mesh_queue_mutex.unlock();
				working_mesh->build_bvh(max_bvh_depth);
			}
			}));
	}
	for (auto& i : bvh_threads) i.join();

	for (auto& i : scene.meshes) {
		std::cout << "-------------------------------" << std::endl;
		std::cout << "Mesh " << counter << std::endl;
		//print_bvh(i, "root", 0);

		std::cout << "Bounds min: " << i.bounds_min[0] << ", " << i.bounds_min[1] << ", " << i.bounds_min[2] << ", " << std::endl;
		std::cout << "Bounds max: " << i.bounds_max[0] << ", " << i.bounds_max[1] << ", " << i.bounds_max[2] << ", " << std::endl;
		std::cout << "Triangle count: " << i.triangles.size() << ", bvh size: " << i.bvh.size() << std::endl;
		
		counter++;

		mesh_info local_mesh = { 0 };

		local_mesh.bounds_min[0] = i.bounds_min[0];
		local_mesh.bounds_min[1] = i.bounds_min[1];
		local_mesh.bounds_min[2] = i.bounds_min[2];
		local_mesh.bounds_max[0] = i.bounds_max[0];
		local_mesh.bounds_max[1] = i.bounds_max[1];
		local_mesh.bounds_max[2] = i.bounds_max[2];
		
		local_mesh.triangle_index = gpu_ready_scene.triangles.size();
		local_mesh.bvh_index = gpu_ready_scene.bvh_nodes.size();

		for (auto& j : i.triangles) {
			triangle_info tri = { 0 };
			for(int o = 0; o < 3; o++) {
				for (int p = 0; p < 3; p++) {
					tri.pos[o][p] = j.pos[o][p];
					tri.normal[o][p] = j.normal[o][p];
				}
			}
			gpu_ready_scene.triangles.push_back(tri);
		}
		for (auto& j : i.bvh) {
			bvh_node node = { 0 };
			for (int o = 0; o < 3; o++) {
				node.bounds_min[o] = j.bounds_min[o];
				node.bounds_max[o] = j.bounds_max[o];
				node.child_index = j.child_index;
				node.triangle_count = j.triangle_count;
			}
			gpu_ready_scene.bvh_nodes.push_back(node);
		}

		if (i.bvh_depth > scene_bvh_max_depth) scene_bvh_max_depth = i.bvh_depth;

		std::cout << "Triangle total: " << gpu_ready_scene.triangles.size() << ", bvh total: " << gpu_ready_scene.bvh_nodes.size() << std::endl;

		gpu_ready_scene.meshes.push_back(local_mesh);
	}
	counter = 0;

	models_static = std::move(scene.models);
	models_sorted.clear();
	models_sorted = models_static;
	std::sort(models_sorted.begin(), models_sorted.end(), model_pred_func);

	for (auto& i : models_sorted) {
		model_info local_model = { 0 };

		if (i.mesh_index >= 0) local_model.mesh_index = i.mesh_index;
		else continue;

		local_model.material.color_roughness[0] = i.color[0];
		local_model.material.color_roughness[1] = i.color[1];
		local_model.material.color_roughness[2] = i.color[2];
		local_model.material.color_roughness[3] = i.roughness;

		local_model.material.color_metalic[0] = i.metalic_color[0];
		local_model.material.color_metalic[1] = i.metalic_color[1];
		local_model.material.color_metalic[2] = i.metalic_color[2];
		local_model.material.color_metalic[3] = i.metalic;

		local_model.material.emission_color_strength[0] = i.emission_color[0];
		local_model.material.emission_color_strength[1] = i.emission_color[1];
		local_model.material.emission_color_strength[2] = i.emission_color[2];
		local_model.material.emission_color_strength[3] = i.emission_strength;

		std::cout << "Model " << counter << std::endl;
		counter++;
		std::cout << "-------------------------------" << std::endl;

		std::cout << "Emission color: (" << i.emission_color[0] << ", " << i.emission_color[1] << ", " << i.emission_color[0] << ")" << std::endl;
		std::cout << "Emission strength: " << i.emission_strength << std::endl;
		std::cout << "-------------------------------" << std::endl << std::endl;

		memcpy(local_model.local_to_world_matrix, glm::value_ptr(i.local_world_transformation_mat), 4 * 4 * sizeof(float));
		memcpy(local_model.world_to_local_matrix, glm::value_ptr(i.world_local_transformation_mat), 4 * 4 * sizeof(float));

		gpu_ready_scene.models.push_back(local_model);
	}
}

void update_models() {
	models_sorted.clear();
	models_sorted = models_static;
	std::sort(models_sorted.begin(), models_sorted.end(), model_pred_func);
	gpu_ready_scene.models.clear();

	for (auto& i : models_sorted) {
		model_info local_model = { 0 };

		if (i.mesh_index >= 0) local_model.mesh_index = i.mesh_index;
		else continue;

		local_model.material.color_roughness[0] = i.color[0];
		local_model.material.color_roughness[1] = i.color[1];
		local_model.material.color_roughness[2] = i.color[2];
		local_model.material.color_roughness[3] = i.roughness;

		local_model.material.color_metalic[0] = i.metalic_color[0];
		local_model.material.color_metalic[1] = i.metalic_color[1];
		local_model.material.color_metalic[2] = i.metalic_color[2];
		local_model.material.color_metalic[3] = i.metalic;

		local_model.material.emission_color_strength[0] = i.emission_color[0];
		local_model.material.emission_color_strength[1] = i.emission_color[1];
		local_model.material.emission_color_strength[2] = i.emission_color[2];
		local_model.material.emission_color_strength[3] = i.emission_strength;

		memcpy(local_model.local_to_world_matrix, glm::value_ptr(i.local_world_transformation_mat), 4 * 4 * sizeof(float));
		memcpy(local_model.world_to_local_matrix, glm::value_ptr(i.world_local_transformation_mat), 4 * 4 * sizeof(float));

		gpu_ready_scene.models.push_back(local_model);
	}
}

int main() {
	// GLFW INIT
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwSwapInterval(0);

	GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "RayTracing_06", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		glfwTerminate();
		return -1;
	}

	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	// GLFW CALLBACKS

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetCursorPosCallback(window, mouse_callback);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glClearColor(0.f, 0.f, 0.f, 1.0f);

	// VARIABLES INIT

	std::vector<triangle_info> triangles;
	std::vector<mesh_info> meshes;

	Scene scene;
	scene.load_scene("resources/Test_Scene.gltf");
	makeGpuReady(scene);

	std::cout << "Number of models: " << gpu_ready_scene.models.size() << std::endl;
	std::cout << "Number of meshes: " << gpu_ready_scene.meshes.size() << std::endl;
	std::cout << "Number of triangles: " << gpu_ready_scene.triangles.size() << std::endl;
	std::cout << "Number of bvh_nodes: " << gpu_ready_scene.bvh_nodes.size() << std::endl;
	std::cout << "Max bvh depth: " << scene_bvh_max_depth << std::endl;

	std::random_device rd;
	std::mt19937 rng(rd());

	projection = glm::perspective(glm::radians(60.0f), 1920.f / 1080.0f, 0.1f, 100.0f);

	float vertices[] = {
		 1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,   0.0f, 1.0f
	};
	unsigned int indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	double t0 = glfwGetTime(), t1, delta_time, time = 0;
	
	// SHADERS INIT

	Shader main_shader("shaders/vertex_shader.glsl", "shaders/fragment_shader.glsl");
	ComputeShader compute_shader("shaders/ray_tracing_compute_shader.glsl");
	ComputeShader accum_shader("shaders/accumulating_compute_shader.glsl");
	ComputeShader denoise_shader("shaders/denoising_compute_shader.glsl");

	// BUFFERS INIT

	unsigned int VAO;
	glGenVertexArrays(1, &VAO);

	unsigned int VBO;
	glGenBuffers(1, &VBO);

	unsigned int EBO;
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	compute_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	accum_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	normal_depth_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	output_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);

	unsigned int  triangle_buffer, bvh_node_buffer, mesh_buffer, model_buffer;

	glGenBuffers(1, &triangle_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, triangle_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_ready_scene.triangles.size()*sizeof(triangle_info), gpu_ready_scene.triangles.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangle_buffer);
	
	glGenBuffers(1, &bvh_node_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvh_node_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_ready_scene.bvh_nodes.size() * sizeof(bvh_node), gpu_ready_scene.bvh_nodes.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bvh_node_buffer);

	glGenBuffers(1, &mesh_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mesh_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_ready_scene.meshes.size() * sizeof(mesh_info), gpu_ready_scene.meshes.data(), GL_STATIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mesh_buffer);

	glGenBuffers(1, &model_buffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, model_buffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_ready_scene.models.size() * sizeof(model_info), gpu_ready_scene.models.data(), GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, model_buffer);

	while (!glfwWindowShouldClose(window))
	{
		t1 = glfwGetTime();
		delta_time = t1 - t0;
		t0 = t1;
		time += delta_time;

		glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 camera_right = glm::normalize(glm::cross(up, camera_front));
		camera_up = glm::cross(camera_front, camera_right);
		processInput(window, float(delta_time));

		glm::mat4 view = glm::mat4(1.0f);
		view = glm::lookAt(camera_position, camera_position + camera_front, camera_up);

		glm::mat4 inv_view = glm::inverse(view);
		glm::mat4 inv_projection = glm::inverse(projection);

		if (camera_pos_changed) {
			update_models();
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, model_buffer);
			glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_ready_scene.models.size() * sizeof(model_info), gpu_ready_scene.models.data(), GL_DYNAMIC_DRAW);
			camera_pos_changed = false;
		}

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangle_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bvh_node_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mesh_buffer);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, model_buffer);

		compute_shader.use();
		compute_shader.setFloat("time", time);
		compute_shader.setUnsignedInt("max_bounces", max_bounces);
		compute_shader.setUnsignedInt("samples_per_pixel", samples_per_pixel);
		compute_shader.setUnsignedInt("rand_seed_in", rng());
		compute_shader.setUnsignedInt("scene_bvh_max_depth", scene_bvh_max_depth);
		compute_shader.setFloat3("camera_pos", camera_position);
		compute_shader.setMat4("inv_view", inv_view);
		compute_shader.setMat4("inv_projection", inv_projection);

		glBindImageTexture(0, compute_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, normal_depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		glDispatchCompute(ceil(float(RENDER_WIDTH)/16.f), ceil(float(RENDER_HEIGHT)/ 16.f), 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		accum_shader.use();
		accum_shader.setUnsignedInt("accum_frame_num", accum_frame_num);

		glBindImageTexture(0, compute_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindImageTexture(1, accum_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		glDispatchCompute(ceil(float(RENDER_WIDTH) / 16.f), ceil(float(RENDER_HEIGHT) / 16.f), 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		if (accum_frame_num < 1024) {
			denoise_shader.use();
			denoise_shader.setInt("step_size", 1);

			glBindImageTexture(0, compute_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
			glBindImageTexture(1, output_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
			glBindImageTexture(2, normal_depth_texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

			glDispatchCompute(ceil(float(RENDER_WIDTH) / 16.f), ceil(float(RENDER_HEIGHT) / 16.f), 1);
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			main_shader.use();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, output_texture);
			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}
		else {
			main_shader.use();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, compute_texture);
			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		}

		accum_frame_num++;

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();
	return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{	
	if (width > 0 && height > 0) {
		glViewport(0, 0, width, height);
		projection = glm::perspective(glm::radians(60.0f), float(width) / float(height), 0.1f, 100.0f);

		SCREEN_WIDTH = width;
		SCREEN_HEIGHT = height;
		RENDER_WIDTH = static_cast<unsigned int>(float(SCREEN_WIDTH) / resolution_scale);
		RENDER_HEIGHT = static_cast<unsigned int>(float(SCREEN_HEIGHT) / resolution_scale);

		regenerate_buffers();

		std::cout << "Current resolution: \n" << "RENDER_WIDTH: " << RENDER_WIDTH << "\nRENDER_HEIGHT: " << RENDER_HEIGHT << std::endl;
	}
}

void processInput(GLFWwindow* window, float delta_time)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		camera_position += camera_front * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
		camera_position -= camera_front * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
		camera_position += glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
		camera_position -= glm::normalize(glm::cross(camera_front, camera_up)) * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
		camera_position += camera_up * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS){
		camera_speed = 25.f;
	}
	else {
		camera_speed = 5.f;
	}
	if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
		camera_position -= camera_up * camera_speed * delta_time;
		accum_frame_num = 1;
		camera_pos_changed = true;
	}
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	accum_frame_num = 1;
	if (first_mouse)
	{
		lastX = xpos;
		lastY = ypos;
		first_mouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;

	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw += xoffset;
	pitch += yoffset;

	if (pitch > 89.0f)
		pitch = 89.0f;
	if (pitch < -89.0f)
		pitch = -89.0f;

	glm::vec3 direction;
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	camera_front = glm::normalize(direction);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	accum_frame_num = 1;
	if (yoffset > 0) {
		if (resolution_scale > 8.f) {
			resolution_scale -= 1.f;
		}else if (resolution_scale > 0.751f) {
			resolution_scale -= resolution_scale > 1.5f ? 0.1f : 0.05f;
		}
	}
	else {
		if (resolution_scale >= 8.f) {
			resolution_scale += 1.f;
		}else if (resolution_scale < 1.499f) {
			resolution_scale += 0.05f;
		}
		else {
			resolution_scale += 0.1f;
		}
	}

	RENDER_WIDTH = static_cast<unsigned int>(float(SCREEN_WIDTH) / resolution_scale);
	RENDER_HEIGHT = static_cast<unsigned int>(float(SCREEN_HEIGHT) / resolution_scale);
	regenerate_buffers();
	std::cout << "Current resolution: \n" << "RENDER_WIDTH: " << RENDER_WIDTH << "\nRENDER_HEIGHT: " << RENDER_HEIGHT << "\nRESOLUTION SCALE: " << resolution_scale << std::endl;
}

unsigned int generate_output_texture(unsigned int width, unsigned int height) {
	unsigned int compute_texture_local;
	glGenTextures(1, &compute_texture_local);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, compute_texture_local);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
	return compute_texture_local;
}

void regenerate_buffers()
{
	glDeleteTextures(1, &compute_texture);
	glDeleteTextures(1, &accum_texture);
	glDeleteTextures(1, &output_texture);
	glDeleteTextures(1, &normal_depth_texture);
	
	compute_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	accum_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	output_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
	normal_depth_texture = generate_output_texture(RENDER_WIDTH, RENDER_HEIGHT);
}