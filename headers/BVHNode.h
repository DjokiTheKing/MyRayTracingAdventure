#pragma once

#include <vector>
#include <limits>

#include <glm/glm.hpp>

#include "Structs.h"
#include "Triangle.h"

class BVHNode {
public:
	BVHNode()
		:
		bounds_min(std::numeric_limits<float>::max()),
		bounds_max(std::numeric_limits<float>::lowest())
	{
		child_index = 0;
		triangle_count = 0;
	};
	glm::vec3 bounds_min, bounds_max;
	unsigned int child_index;
	unsigned int triangle_count;

	void grow_to_include(Triangle tri) {
		bounds_min = glm::min(bounds_min, tri.bounds_min);
		bounds_max = glm::max(bounds_max, tri.bounds_max);
	}
	glm::vec3 size() {
		return bounds_max - bounds_min;
	}
};