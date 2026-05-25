#version 460 core

// OUTPUT IMAGE BUFFER
layout(rgba32f, binding = 0) uniform image2D current_frame;
layout(rgba32f, binding = 1) uniform image2D output_frame;
layout(rgba32f, binding = 2) uniform image2D normal_depth_frame;

// LOCAL INVOCATION LAYOUT
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform int step_size, num_frames;

const float kernel_weights[5][5] = {
    {1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0},
    {1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0},
    {3.0/128.0, 3.0/32.0, 9.0/64.0,  3.0/32.0, 3.0/128.0},
    {1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0},
    {1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0}
};

void main() {
    ivec2 pixel_position = ivec2(gl_GlobalInvocationID.xy);
    ivec2 output_size = imageSize(current_frame);
    if(pixel_position.x >= output_size.x || pixel_position.y >= output_size.y) return;

    vec4 current_pixel = imageLoad(current_frame, pixel_position);
    vec4 current_normal_depth = imageLoad(normal_depth_frame, pixel_position);

    vec3 accumulating_result = vec3(0.0);
    float total_weight = 0.0;

    float zc = current_normal_depth.w;
    float z_right, z_left, z_down, z_up;

    // X-axis neighbors with bounds checks
    if(pixel_position.x >= (output_size.x - 1)) z_right = zc;
    else z_right = imageLoad(normal_depth_frame, pixel_position + ivec2(1, 0)).w;

    if(pixel_position.x <= 0) z_left = zc;
    else z_left = imageLoad(normal_depth_frame, pixel_position + ivec2(-1, 0)).w;

    // Y-axis neighbors with bounds checks
    if(pixel_position.y >= (output_size.y - 1)) z_down = zc;
    else z_down = imageLoad(normal_depth_frame, pixel_position + ivec2(0, 1)).w;

    if(pixel_position.y <= 0) z_up = zc;
    else z_up = imageLoad(normal_depth_frame, pixel_position + ivec2(0, -1)).w;

    // Take the minimum difference along each axis
    float dzdx = min(abs(z_right - zc), abs(z_left - zc));
    float dzdy = min(abs(z_down - zc), abs(z_up - zc));
    
    float zGradient = dzdx + dzdy;
        float Denominator = max(abs(zGradient), 1e-6f) * step_size;

        for(int i = -2; i <= 2; ++i){
            for(int j = -2; j <= 2; ++j) {
                ivec2 cur_pos = pixel_position + ivec2(i,j)*step_size;
                if(cur_pos.x < 0 || cur_pos.x >= output_size.x || cur_pos.y < 0 || cur_pos.y >= output_size.y) continue;

                ivec2 kernel_weight_index = ivec2(i,j) + ivec2(2,2);
                float kernel_weight = kernel_weights[kernel_weight_index.x][kernel_weight_index.y];

                vec4 target_normal_depth = imageLoad(normal_depth_frame, cur_pos);

                const float weightNormal = pow(max(dot(current_normal_depth.xyz, target_normal_depth.xyz), 0), 6);

                const float Numerator = abs((current_normal_depth.w - target_normal_depth.w));
                const float weightDepth = exp(-(Numerator / Denominator));

                const float final_weight = weightDepth * weightNormal * kernel_weight;
                total_weight += final_weight;

                accumulating_result += (imageLoad(current_frame, cur_pos).xyz) * final_weight;
            }
        }
        accumulating_result /= max(total_weight, 1e-6);
        imageStore(output_frame, pixel_position, vec4(accumulating_result, 1.f));
}