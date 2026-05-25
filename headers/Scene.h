#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <cassert>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny_gltf.h"
#include "stb_image.h"
#include "Structs.h"

#include "Model.h"
#include "Mesh.h"

class Scene {
public:
	Scene() = default;
	void load_scene(std::string scene_path);
	void add_model(Model &model);

	std::vector<Model> models;
	std::vector<Mesh> meshes;
};