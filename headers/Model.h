#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny_gltf.h"
#include "stb_image.h"
#include "Structs.h"

class Model {
public:
	Model();
	int mesh_index;

	glm::vec3 color; float roughness;
	glm::vec3 metalic_color; float metalic;
	glm::vec3 emission_color; float emission_strength;
	glm::mat4 local_world_transformation_mat, world_local_transformation_mat;
	glm::vec3 pos;
};