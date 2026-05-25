#include "Scene.h"

static glm::vec3 ReadVec3FromAccessor(const tinygltf::Model& model, const tinygltf::Accessor& acc) {
    const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];

    size_t numComponents = 3; // VEC3
    size_t compSize = 4;      // float
    size_t stride = bv.byteStride ? bv.byteStride : (numComponents * compSize);

    const unsigned char* dataPtr = buf.data.data() + bv.byteOffset + acc.byteOffset;
    float x = *reinterpret_cast<const float*>(dataPtr + 0);
    float y = *reinterpret_cast<const float*>(dataPtr + 4);
    float z = *reinterpret_cast<const float*>(dataPtr + 8);
    return glm::vec3(x, y, z);
}

static glm::vec3 ReadVec3AtIndex(const tinygltf::Model& model, const tinygltf::Accessor& acc, size_t idx) {
    const tinygltf::BufferView& bv = model.bufferViews[acc.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];

    size_t numComponents = 3; // VEC3
    size_t compSize = 4;      // float
    size_t stride = bv.byteStride ? bv.byteStride : (numComponents * compSize);

    const unsigned char* dataPtr = buf.data.data() + bv.byteOffset + acc.byteOffset + idx * stride;
    float x = *reinterpret_cast<const float*>(dataPtr + 0);
    float y = *reinterpret_cast<const float*>(dataPtr + 4);
    float z = *reinterpret_cast<const float*>(dataPtr + 8);
    return glm::vec3(x, y, z);
}

// Read an index value (supports UBYTE, USHORT, UINT)
static uint32_t ReadIndexAt(const tinygltf::Model& model, const tinygltf::Accessor& idxAcc, size_t idx) {
    const tinygltf::BufferView& bv = model.bufferViews[idxAcc.bufferView];
    const tinygltf::Buffer& buf = model.buffers[bv.buffer];
    const unsigned char* ptr = buf.data.data() + bv.byteOffset + idxAcc.byteOffset;

    switch (idxAcc.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        return static_cast<uint32_t>(p[idx]);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
        return static_cast<uint32_t>(p[idx]);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        const uint32_t* p = reinterpret_cast<const uint32_t*>(ptr);
        return p[idx];
    }
    default:
        assert(false && "Unsupported index component type");
        return 0;
    }
}

void Scene::load_scene(std::string model_path)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, model_path);
    if (!warn.empty()) fprintf(stderr, "gltf warn: %s\n", warn.c_str());
    if (!err.empty())  fprintf(stderr, "gltf err: %s\n", err.c_str());
    if (!ret) return;

    // map: (gltf mesh index, primitive index) -> scene mesh index
    std::map<std::pair<std::string, int>, unsigned int> meshMap;

    for (size_t nodeIndex = 0; nodeIndex < gltfModel.nodes.size(); ++nodeIndex) {
        const tinygltf::Node& node = gltfModel.nodes[nodeIndex];
        if (node.mesh < 0) continue;

        // --- Read TRS ---
        glm::vec3 T(0.0f);
        glm::vec3 S(1.0f);
        glm::mat4 Rmat(1.0f);

        if (node.translation.size() == 3)
            T = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        if (node.scale.size() == 3)
            S = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
        if (node.rotation.size() == 4) {
            glm::quat q(
                (float)node.rotation[3],
                (float)node.rotation[0],
                (float)node.rotation[1],
                (float)node.rotation[2]
            );
            Rmat = glm::mat4_cast(q);
        }

        glm::mat4 M_local_to_world =
            glm::translate(glm::mat4(1.0f), T) *
            Rmat *
            glm::scale(glm::mat4(1.0f), S);

        glm::mat4 M_world_to_local = glm::inverse(M_local_to_world);

        const tinygltf::Mesh& mesh = gltfModel.meshes[node.mesh];

        for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex) {
            const auto& prim = mesh.primitives[primIndex];

            Model model{};
            model.local_world_transformation_mat = M_local_to_world;
            model.world_local_transformation_mat = M_world_to_local;
            model.pos = T;

            // ---------- material ----------
            if (prim.material >= 0 && prim.material < (int)gltfModel.materials.size()) {
                const auto& mat = gltfModel.materials[prim.material];
                const auto& pbr = mat.pbrMetallicRoughness;

                model.color = pbr.baseColorFactor.size() == 4
                    ? glm::vec3(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2])
                    : glm::vec3(0.75f);

                model.metalic = (float)pbr.metallicFactor;
                model.roughness = (float)pbr.roughnessFactor;

                if (mat.emissiveFactor.size() == 3) {
                    model.emission_color = glm::vec3(
                        mat.emissiveFactor[0],
                        mat.emissiveFactor[1],
                        mat.emissiveFactor[2]
                    );
                    if (mat.extensions.count("KHR_materials_emissive_strength"))
                        model.emission_strength =
                        (float)mat.extensions.at("KHR_materials_emissive_strength")
                        .Get("emissiveStrength").GetNumberAsDouble();
                    else
                        model.emission_strength = glm::length(model.emission_color);
                }
            }
            else {
                model.color = glm::vec3(0.75f, 0.45f, 0.2f);
                model.metalic = 0.0f;
                model.roughness = 1.0f;
                model.emission_color = glm::vec3(0.0f);
                model.emission_strength = 0.0f;
            }

            // ---------- mesh ----------
            auto key = std::make_pair(mesh.name, (int)primIndex);
            auto it = meshMap.find(key);

            if (it == meshMap.end()) {
                // build new mesh
                unsigned int newMeshIndex = (unsigned int)meshes.size();
                meshMap[key] = newMeshIndex;

                Mesh local_mesh;
                glm::vec3 bounds_min(std::numeric_limits<float>::max());
                glm::vec3 bounds_max(std::numeric_limits<float>::lowest());

                const auto& posAcc = gltfModel.accessors[prim.attributes.at("POSITION")];
                bool hasNormals = prim.attributes.count("NORMAL") > 0;
                const tinygltf::Accessor* normAcc =
                    hasNormals ? &gltfModel.accessors[prim.attributes.at("NORMAL")] : nullptr;

                bool hasIndices = prim.indices >= 0;
                const tinygltf::Accessor* idxAcc =
                    hasIndices ? &gltfModel.accessors[prim.indices] : nullptr;

                size_t triCount = hasIndices ? idxAcc->count / 3 : posAcc.count / 3;

                for (size_t t = 0; t < triCount; ++t) {
                    Triangle tri;
                    glm::vec3 tri_bounds_min(std::numeric_limits<float>::max());
                    glm::vec3 tri_bounds_max(std::numeric_limits<float>::lowest());
                    for (int v = 0; v < 3; ++v) {
                        uint32_t vi = hasIndices
                            ? ReadIndexAt(gltfModel, *idxAcc, t * 3 + v)
                            : (uint32_t)(t * 3 + v);

                        glm::vec3 p = ReadVec3AtIndex(gltfModel, posAcc, vi);
                        tri.pos[v][0] = p.x;
                        tri.pos[v][1] = p.y;
                        tri.pos[v][2] = p.z;

                        if (hasNormals) {
                            glm::vec3 n = ReadVec3AtIndex(gltfModel, *normAcc, vi);
                            tri.normal[v][0] = n.x;
                            tri.normal[v][1] = n.y;
                            tri.normal[v][2] = n.z;
                        }

                        bounds_min = glm::min(bounds_min, p);
                        bounds_max = glm::max(bounds_max, p);

                        tri_bounds_min = glm::min(tri_bounds_min, p);
                        tri_bounds_max = glm::max(tri_bounds_max, p);
                    }

                    tri.center = (tri.pos[0] + tri.pos[1] + tri.pos[2]) / 3.f;
                    tri.bounds_min = tri_bounds_min;
                    tri.bounds_max = tri_bounds_max;
                    local_mesh.triangles.push_back(tri);
                }

                local_mesh.bounds_min = bounds_min;
                local_mesh.bounds_max = bounds_max;

                meshes.push_back(std::move(local_mesh));

                model.mesh_index = newMeshIndex;
                
            }
            else {
                model.mesh_index = it->second;
            }

            models.push_back(model);
        }
    }
}

void Scene::add_model(Model& model)
{
	models.push_back(model);
}
