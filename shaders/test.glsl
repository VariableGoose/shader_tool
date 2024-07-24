#include dir/include.glsl

#vert vs
#include_module funcs

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    // int arr[16];
} ubo;

// https://www.khronos.org/opengl/wiki/Data_Type_(GLSL)
// layout (binding = 1) uniform AllTypes {
//     // Scalers
//     bool boolean;
//     int integer;
//     uint unsigned_integer;
//     float floating_point;
//     double double_floating_point;
//
//     // Vectors
//     bvec2 bool_vec1;
//     ivec2 int_vec2;
//     uvec3 uint_vec3;
//     vec4 float_vec4;
//     dvec4 double_vec4;
//
//     // Matrices
//     mat2 matrix2x2;
//     mat3 matrix3x3;
//     mat4 matrix4x4;
// } all_types_array[2][4][8];

layout (binding = 1) uniform sampler2D samp;
layout (binding = 2) uniform sampler2D other_samp;
layout (push_constant) uniform Consts {
    mat4 some_matrix;
} consts;

layout (location = 0) in vec3 v_pos;
layout (location = 1) in vec2 v_uv;

layout (location = 0) out vec2 f_uv;

void main() {
    f_uv = double_value(v_uv);
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(v_pos, 1.0);
}
#end

#frag fs
#include_module funcs

layout (location = 0) out vec4 frag_color;

layout (location = 0) in vec2 f_uv;

void main() {
    f_uv = half_value(f_uv);
    frag_color = vec4(f_uv, 0.0, 1.0);
}
#end

#program test_shader vs fs
