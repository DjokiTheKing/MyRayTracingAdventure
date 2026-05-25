#include "Model.h"

Model::Model() {
	mesh_index = -1;

	color = glm::vec3(0.f, 0.f, 0.f); 
	roughness = 1.f;
	
	metalic_color = glm::vec3(1.f, 1.f, 1.f); 
	metalic = 0.f;

	emission_color = glm::vec3(0.f, 0.f, 0.f);
	emission_strength = 0.f;

	local_world_transformation_mat = glm::mat4(1.f);
	world_local_transformation_mat = glm::mat4(1.f);
}
