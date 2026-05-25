#version 460 core

// OUTPUT IMAGE BUFFER
layout(rgba32f, binding = 0) uniform image2D current_frame;
layout(rgba32f, binding = 1) uniform image2D accum_frame;

// LOCAL INVOCATION LAYOUT
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform uint accum_frame_num; 

void main() {
    ivec2 output_size = imageSize(current_frame);
    ivec2 pixel_position = ivec2(gl_GlobalInvocationID.xy);

    if(pixel_position.x >= output_size.x || pixel_position.y >= output_size.y) return;

    vec3 current_pixel_info = imageLoad(current_frame, pixel_position).rgb;
    if(accum_frame_num > 1) current_pixel_info += imageLoad(accum_frame, pixel_position).rgb;
    imageStore(accum_frame, pixel_position, vec4(current_pixel_info, 1.f));

    imageStore(current_frame, pixel_position, vec4(current_pixel_info/float(accum_frame_num), 1.f));
}