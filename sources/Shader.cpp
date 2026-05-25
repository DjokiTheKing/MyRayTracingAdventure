#include "Shader.h"

Shader::Shader(const std::string& vertex_path, const std::string& fragment_path)
{
	int status;
	char info[512];

	std::ifstream vertex_file(vertex_path);

	if(!vertex_file.is_open()){
		std::cout << "Failed to read shader file: " << vertex_path << std::endl;
		PID = -1;
		return;
	}
	std::stringstream vertex_stream; vertex_stream << vertex_file.rdbuf();
	std::string vertex_code = vertex_stream.str();
	const char* c_vertex_code = vertex_code.c_str();

	unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &c_vertex_code, nullptr);
	glCompileShader(vertex_shader);

	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(vertex_shader, 512, nullptr, info);
		std::cout << "Vertex shader compilation failed: " << info << std::endl;
		PID = -1;
		return;
	}

	std::ifstream fragment_file(fragment_path);

	if (!fragment_file.is_open()) {
		std::cout << "Failed to read shader file: " << fragment_path << std::endl;
		PID = -1;
		return;
	}

	std::stringstream fragment_stream; fragment_stream << fragment_file.rdbuf();
	std::string fragment_code = fragment_stream.str();
	const char* c_fragment_code = fragment_code.c_str();

	unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &c_fragment_code, nullptr);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
	if (!status) {
		glGetShaderInfoLog(fragment_shader, 512, nullptr, info);
		std::cout << "Fragment shader compilation failed: " << info << std::endl;
		PID = -1;
		return;
	}

	PID = glCreateProgram();
	glAttachShader(PID, vertex_shader);
	glAttachShader(PID, fragment_shader);
	glLinkProgram(PID);

	glGetProgramiv(PID, GL_LINK_STATUS, &status);
	if (!status) {
		glGetProgramInfoLog(PID, 512, nullptr, info);
		std::cout << "Program linking failed: " << info << std::endl;
		glDeleteProgram(PID);
		PID = -1;
		return;
	}

	// Cleanup
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
}

Shader::~Shader()
{
	glDeleteProgram(PID);
}

void Shader::use()
{
	glUseProgram(PID);
}

void Shader::setBool(const std::string& name, bool value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1i(location, (int)value);
}

void Shader::setFloat(const std::string& name, float value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1f(location, value);
}

void Shader::setInt(const std::string& name, int value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform1i(location, value);
}

void Shader::setFloat2(const std::string& name, float value0, float value1) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform2f(location, value0, value1);
}

void Shader::setFloat2(const std::string& name, const glm::vec2& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform2fv(location, 1, glm::value_ptr(value));
}

void Shader::setFloat3(const std::string& name, const glm::vec3& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform3fv(location, 1, glm::value_ptr(value));
}

void Shader::setFloat4(const std::string& name, const glm::vec4& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniform4fv(location, 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const
{
	int location = glGetUniformLocation(PID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}
