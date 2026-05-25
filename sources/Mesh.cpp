#include "Mesh.h"

float Mesh::node_cost(glm::vec3 size, int num_triangles) {
	if (num_triangles == 0) return 0;
	return std::powf(1.f+(size.x * size.y + size.x * size.z + size.y * size.z),2.0f) * float(num_triangles);
}

float Mesh::evaluate_split(unsigned int node, int split_axis, float split_pos) {
	BVHNode bounds_a, bounds_b;
	int num_in_a = 0, num_in_b = 0;

	for (int i = bvh[node].child_index; i < bvh[node].child_index + bvh[node].triangle_count; ++i) {
		Triangle tri = triangles[i];
		if (tri.center[split_axis] < split_pos) {
			bounds_a.grow_to_include(tri);
			num_in_a++;
		}else{
			bounds_b.grow_to_include(tri);
			num_in_b++;
		}
	}
	return node_cost(bounds_a.size(), num_in_a) + node_cost(bounds_b.size(), num_in_b);
}

Mesh::split_info Mesh::choose_split(unsigned int node) {
	const unsigned int num_test_per_axis = 128;
	float best_cost = std::numeric_limits<float>::max();
	float best_pos = 0;
	int best_axis = 0;

	for (int axis = 0; axis < 3; axis++) {
		float bounds_start = bvh[node].bounds_min[axis];
		float bounds_end = bvh[node].bounds_max[axis];

		for (int i = 0; i < num_test_per_axis; ++i) {
			float split_t = float(i + 1) / (num_test_per_axis + 1.f);
			float pos = bounds_start + (bounds_end - bounds_start) * split_t;
			float cost = evaluate_split(node, axis, pos);

			if (cost < best_cost)
			{
				best_cost = cost;
				best_pos = pos;
				best_axis = axis;
			}
		}
	}
	split_info res;
	res.axis = best_axis;
	res.pos = best_pos;
	res.cost = best_cost;
	return res;
}

void Mesh::split(unsigned int parent, const int max_depth, int depth) {
	if (depth > bvh_depth) bvh_depth = depth;

	if (depth == max_depth || bvh[parent].triangle_count <= 1) {
		return;
	};

	split_info base_split = choose_split(parent);
	if (base_split.cost >= node_cost(bvh[parent].size(), bvh[parent].triangle_count)) {
		return;
	};

	unsigned int child_a, child_b;
	child_a = bvh.size();
	child_b = child_a + 1;
	bvh.emplace_back();
	bvh.emplace_back();
	bvh[child_a].child_index = bvh[parent].child_index;
	bvh[child_b].child_index = bvh[parent].child_index;

	for (int i = bvh[parent].child_index; i < (bvh[parent].child_index + bvh[parent].triangle_count); ++i) {
		bool is_side_a = triangles[i].center[base_split.axis] < base_split.pos;
		unsigned int child = is_side_a ? child_a : child_b;
		bvh[child].grow_to_include(triangles[i]);
		bvh[child].triangle_count++;

		if (is_side_a) {
			int swap = bvh[child].child_index + bvh[child].triangle_count - 1;
			std::swap(triangles[i], triangles[swap]);
			bvh[child_b].child_index++;
		}
	}

	bvh[parent].child_index = child_a;
	bvh[parent].triangle_count = 0;
	split(child_a, max_depth, depth + 1);
	split(child_b, max_depth, depth + 1);
}
#include <iostream>
void Mesh::build_bvh(const int max_depth)
{
	bvh.clear();
	bvh.emplace_back(BVHNode());

	for (auto& i : triangles) {
		bvh[0].grow_to_include(i);
	}

	bvh[0].child_index = 0;
	bvh[0].triangle_count = triangles.size();

	split(0, max_depth);
}
