#pragma once

struct triangle_info {
	float pos[3][4];
	float normal[3][4];
};

struct bvh_node {
	float bounds_min[3];
	unsigned int child_index;
	float bounds_max[3];
	unsigned int triangle_count;
};

struct mesh_info {
	float bounds_min[3];
	unsigned int triangle_index;
	float bounds_max[3];
	unsigned int bvh_index;
};

struct material_info {
	float color_roughness[4];
	float color_metalic[4];
	float emission_color_strength[4];
};

struct model_info {
	unsigned int mesh_index, pad0, pad1, pad2;
	float local_to_world_matrix[4][4];
	float world_to_local_matrix[4][4];
	material_info material;
};