#version 460 core

#define PI 3.14159265359

// #define USE_AMBIENT_LIGHT
#define USE_DENOISING

// TYPE DEFINITIONS

struct triangle_info {
	vec4 pos_a, pos_b, pos_c;
    vec4 normal_a, normal_b, normal_c;
};

struct bvh_node {
	vec3 bounds_min;
	uint child_index;
	vec3 bounds_max;
	uint triangle_count;
};

struct mesh_info {
    vec3 bounds_min;
    uint triangle_index;
    vec3 bounds_max;
    uint bvh_index;
};

struct material_info {
    vec4 color_roughness;
    vec4 color_metalic;
    vec4 emission_color_strength;
};

struct model_info {
    uint mesh_index, pad0, pad1, pad2;
	mat4 local_to_world_matrix;
	mat4 world_to_local_matrix;
    material_info material;
};

struct hit_record {
    vec3 point;
    vec3 normal;
    float dst;
    bool back_face;
    material_info material;
};

struct ray_info {
    vec3 orig;
    vec3 dir;
};

float infinity = 1. / 0.;

uint rng_state;

// INPUT UNIFORMS
uniform float time;
uniform vec3 camera_pos;
uniform mat4 inv_projection;
uniform mat4 inv_view;
uniform uint samples_per_pixel;
uniform uint max_bounces;
uniform uint scene_bvh_max_depth;

uniform uint rand_seed_in; 

// OUTPUT IMAGE BUFFER
layout(rgba32f, binding = 0) uniform image2D output_image;
#ifdef USE_DENOISING
layout(rgba32f, binding = 1) uniform image2D normal_depth_output_image;

vec3 first_hit_normal;
float first_hit_depth;
#endif

// INPUT MODELS
layout(binding = 1, std430) readonly buffer TrianglesSSBO {
    triangle_info triangles[];
};

layout(binding = 2, std430) readonly buffer BVHNodesSSBO {
    bvh_node bvh_nodes[];
};

layout(binding = 3, std430) readonly buffer MeshesSSBO {
    mesh_info meshes[];
};

layout(binding = 4, std430) readonly buffer ModelsSSBO {
    model_info models[];
};

// LOCAL INVOCATION LAYOUT
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// RANDOM NUMBER GENERATOR
uint pcg_hash(uint input_seed)
{
    uint state = input_seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float random_float(){
    rng_state = pcg_hash(rng_state);
    return float(rng_state) * (1.0 / 4294967296.0);
}

float random_float(float target_min, float target_max){
    return target_min + (target_max-target_min)*random_float();
}

vec2 rand_vec2(vec2 target_min, vec2 target_max){
    return target_min + (target_max-target_min)*vec2(random_float(), random_float());
}

vec3 random_vec3() {
    return vec3(random_float(), random_float(), random_float());
}

vec3 random_vec3(float target_min, float target_max) {
    return vec3(random_float(target_min,target_max), random_float(target_min,target_max), random_float(target_min,target_max));
}

float random_value_normal_dist(){
    float theta = 2*PI*random_float();
    float rho = sqrt(-2*log(random_float()));
    return rho * cos(theta);
}

vec3 random_unit_vector() {
    return normalize(vec3(random_value_normal_dist(), random_value_normal_dist(), random_value_normal_dist()));
}

// RAY FUNCTIONS
vec3 ray_at(ray_info t_ray, float t){
    return t_ray.orig + t*t_ray.dir;
}

// HIT FUNCTIONS

bool hit_triangle(const triangle_info tri, const ray_info ray, const float ray_tmin, const float ray_tmax, out hit_record rec) {
    vec3 edge_ab = tri.pos_b.xyz - tri.pos_a.xyz;
    vec3 edge_ac = tri.pos_c.xyz - tri.pos_a.xyz;
    vec3 normal_vec = cross(edge_ab, edge_ac);
    vec3 ao = ray.orig - tri.pos_a.xyz;
    vec3 dao = cross(ao, ray.dir);

    float determinant = -dot(ray.dir, normal_vec);

    if (abs(determinant) < 1e-10) return false;
    float inv_det = 1.0 / determinant;
    float dst = dot(ao, normal_vec) * inv_det;

    if (dst < ray_tmin || dst >= ray_tmax) return false;

    float u = dot(edge_ac, dao) * inv_det;
    float v = -dot(edge_ab, dao) * inv_det;
    float w = 1.0 - u - v;

    if (u < 0.0 || v < 0.0 || w < 0.0) return false;

    rec.dst = dst;
    rec.point = ray.orig + dst * ray.dir;
    rec.normal = (tri.normal_a.xyz * w + tri.normal_b.xyz * u + tri.normal_c.xyz * v);
    rec.back_face = determinant < 0.0;
    return true;
}

bool hit_bounding_box(vec3 ray_origin, vec3 ray_inv_dir, vec3 box_min, vec3 box_max, out float dst)
{
    vec3 t_min = (box_min - ray_origin) * ray_inv_dir;
    vec3 t_max = (box_max - ray_origin) * ray_inv_dir;
    vec3 t1 = min(t_min, t_max);
    vec3 t2 = max(t_min, t_max);
    float t_near = max(max(t1.x, t1.y), t1.z);
    float t_far = min(min(t2.x, t2.y), t2.z);

    bool hit = t_far >= t_near && t_far > 0;
    dst = hit ? max(t_near, 0.0) : infinity;
    return hit;

};

bool hit(ray_info ray, float ray_tmin, float ray_tmax, out hit_record rec){
    
    bool hit_anything = false;
    rec.dst = ray_tmax;

    for(int i = 0; i < models.length(); ++i){
        bool hit_model = false;
        model_info model = models[i];

        ray_info local_ray;
        local_ray.orig = (model.world_to_local_matrix * vec4(ray.orig, 1.f)).xyz; 
        local_ray.dir =  (model.world_to_local_matrix * vec4(ray.dir, 0.f)).xyz;
        const vec3 ray_inv_dir = 1.0 / local_ray.dir;

        mesh_info mesh = meshes[model.mesh_index];

        float tmp_dst;

        if(!hit_bounding_box(local_ray.orig, ray_inv_dir, mesh.bounds_min, mesh.bounds_max, tmp_dst)) continue;
        if(tmp_dst > rec.dst) continue;

        hit_record temp_rec;
        uint triangle_index = mesh.triangle_index; 

        uint bvh_stack[32];
        uint stack_index = 0;
        bvh_stack[stack_index++] = mesh.bvh_index;

        while(stack_index > 0){
            bvh_node node = bvh_nodes[bvh_stack[--stack_index]];
                if(node.triangle_count > 0){
                    for(uint j = node.child_index; j < (node.child_index+node.triangle_count); ++j){
                        if(hit_triangle(triangles[triangle_index + j], local_ray, ray_tmin, rec.dst, temp_rec)){
                            hit_model = true;
                            rec = temp_rec;
                        }
                    }
                }else{
                    uint child_index_a = mesh.bvh_index+node.child_index;
                    uint child_index_b = mesh.bvh_index+node.child_index+1;

                    float dst_a, dst_b;
                    bool hit_a, hit_b;

                    hit_a = hit_bounding_box(local_ray.orig, ray_inv_dir, bvh_nodes[child_index_a].bounds_min, bvh_nodes[child_index_a].bounds_max, dst_a);
                    hit_b = hit_bounding_box(local_ray.orig, ray_inv_dir, bvh_nodes[child_index_b].bounds_min, bvh_nodes[child_index_b].bounds_max, dst_b);

                    if(!hit_a && !hit_b && (dst_a >= 0 || dst_b >= 0)) continue;

                    bool is_nearest_a = dst_a <= dst_b;

                    float dst_near = is_nearest_a ? dst_a : dst_b;
                    float dst_far = is_nearest_a ? dst_b : dst_a;

                    uint child_index_near = is_nearest_a ? child_index_a : child_index_b;
                    uint child_index_far = is_nearest_a ? child_index_b : child_index_a;

                    if (dst_far < rec.dst) bvh_stack[stack_index++] = child_index_far;
                    if (dst_near < rec.dst) bvh_stack[stack_index++] = child_index_near;
                }
        }
        if(hit_model){
            hit_anything = true;
            rec.material = model.material;
            rec.point = ray.orig + ray.dir * rec.dst;
            rec.normal = normalize((model.local_to_world_matrix*vec4(normalize(rec.normal) * (rec.back_face ? -1.f : 1.f),0.f))).xyz;
        }
    }
    return hit_anything;
}

vec4 ray_trace(ray_info ray, float ray_tmin, float ray_tmax){
    hit_record rec;
    bool hit_anything = false, finished = false;

    ray_info local_ray = ray;
    vec3 ray_colour = vec3(1., 1., 1.);
    vec3 incoming_light = vec3(0., 0., 0.);

    for(int i = 0; i <= max_bounces; ++i){
        if(hit(local_ray, ray_tmin, ray_tmax, rec)){
            hit_anything = true;
            #ifdef USE_DENOISING
            if(i == 0){
                first_hit_normal = rec.normal;
                first_hit_depth = rec.dst;
            }
            #endif
            // return vec4(rec.material.color_roughness.xyz, 1.);

            local_ray.orig = rec.point;
            const vec3 diffuse_dir = normalize(rec.normal+random_unit_vector());
            const vec3 specular_dir = reflect(local_ray.dir, rec.normal);

            const float is_specular = rec.material.color_metalic.w >= random_float() ? 1.0f : 0.0f;

            local_ray.dir = mix(diffuse_dir, specular_dir, (1.f-rec.material.color_roughness.w) * is_specular);

            ray_colour *= mix(rec.material.color_roughness.xyz, rec.material.color_metalic.xyz, is_specular);
            
            if(dot(ray_colour, ray_colour) < 0.0001) return vec4(incoming_light, 1.0);
            
            vec3 emmited_light = rec.material.emission_color_strength.xyz * rec.material.emission_color_strength.w;
            incoming_light += emmited_light * ray_colour;
        }else{
        #ifdef USE_DENOISING
        if(i == 0){
            first_hit_normal = ray.dir;
            first_hit_depth = 0.f;
        }
        #endif
        #ifdef USE_AMBIENT_LIGHT
            float a = 0.5*(local_ray.dir.y + 1.0);
            incoming_light += ((1.0-a)*vec3(0.7, 0.7, 0.7) + a*vec3(0.5, 0.6, 0.7))*ray_colour*0.25;
        #endif
            break;
        }
    }

    if(hit_anything){
        return vec4(incoming_light, 1.0);
    }
    #ifdef USE_AMBIENT_LIGHT
        float a = 0.5*(ray.dir.y + 1.0);
        return vec4((1.0-a)*vec3(0.7, 0.7, 0.7) + a*vec3(0.4, 0.55, 0.7), 1.0);
    #else
        return vec4(0., 0., 0., 1.);
    #endif
}

// SELF EXPLANATORY
void calculate_ray(out ray_info ray, ivec2 pixel_position, vec2 pixel_offset, ivec2 output_size){
    vec2 ndc = (vec2(pixel_position)+pixel_offset) / vec2(output_size);
    ndc = ndc * 2.0 - 1.0;

    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 view = inv_projection * clip;
    vec3 dir_view = normalize(view.xyz / view.w);
    vec3 dir_world = normalize(mat3(inv_view) * dir_view);

    ray.orig = camera_pos;
    ray.dir  = dir_world;
}

void main() {
    ivec2 output_size = imageSize(output_image);
    ivec2 pixel_position = ivec2(gl_GlobalInvocationID.xy);

    if(pixel_position.x >= output_size.x || pixel_position.y >= output_size.y) return;

    rng_state = pcg_hash(uint((pixel_position.x+232)*(pixel_position.y+111666))*rand_seed_in);

    ray_info ray;
    vec4 output_value = vec4(0.,0.,0.,1.);
    for(int i = 0; i < samples_per_pixel; ++i){
        // calculate_ray(ray, pixel_position, rand_vec2(vec2(-0.5, -0.5), vec2(0.5, 0.5)), output_size);
        calculate_ray(ray, pixel_position, vec2(0.0, 0.0), output_size);
        output_value += ray_trace(ray, 0.0001, infinity);
    }

    output_value /= float(samples_per_pixel);

    imageStore(output_image, pixel_position, output_value);

#ifdef USE_DENOISING
    imageStore(normal_depth_output_image, pixel_position, vec4(first_hit_normal, first_hit_depth));
#endif
}