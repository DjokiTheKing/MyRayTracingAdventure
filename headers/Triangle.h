#pragma once

#include <vector>
#include <limits>

#include <glm/glm.hpp>

#include "Structs.h"

class Triangle {
public:
	Triangle() 
		:
		bounds_min(std::numeric_limits<float>::max()),
		bounds_max(std::numeric_limits<float>::min()),
		center(0)
	{
		for (auto& i : pos) i = glm::vec4(0.0);
		for (auto& i : normal) i = glm::vec4(0.0);
	};
	glm::vec4 pos[3];
	glm::vec4 normal[3];
	glm::vec3 bounds_min, bounds_max, center;
};