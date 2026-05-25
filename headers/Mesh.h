#pragma once

#include <vector>
#include <limits>
#include <algorithm>

#include <glm/glm.hpp>

#include "Structs.h"
#include "Triangle.h"
#include "BVHNode.h"

class Mesh {
public:
	Mesh() 
		:
		bounds_min(std::numeric_limits<float>::max()),
		bounds_max(std::numeric_limits<float>::lowest()),
		triangles(),
		bvh(),
		bvh_depth(0)
	{
	};

	std::vector<Triangle> triangles;
	std::vector<BVHNode> bvh;
	glm::vec3 bounds_min, bounds_max;
	unsigned int bvh_depth;

	void build_bvh(const int max_depth);
private:
	struct split_info {
		int axis;
		float pos, cost;
	};

	static float node_cost(glm::vec3 size, int num_triangles);
	float evaluate_split(unsigned int node, int split_axis, float split_pos);
	split_info choose_split(unsigned int node);
	void split(unsigned int parent, const int max_depth, int depth=0);
};