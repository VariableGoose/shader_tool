#include dir/include.glsl

#vert vs
#include_module funcs

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    int arr[16];
} ubo;

struct Structure {
    vec2 arr[16][4];
};

// https://www.khronos.org/opengl/wiki/Data_Type_(GLSL)
layout (binding = 1) uniform AllTypes {
    Structure structure;

    // Scalers
    bool boolean;
    int integer;
    uint unsigned_integer;
    float floating_point;
    double double_floating_point;

    // Vectors
    bvec2 bool_vec2;
    ivec2 int_vec2;
    uvec2 uint_vec2;
    vec2 float_vec2;
    dvec2 double_vec2;

    bvec3 bool_vec3;
    ivec3 int_vec3;
    uvec3 uint_vec3;
    vec3 float_vec3;
    dvec3 double_vec3;

    bvec4 bool_vec4;
    ivec4 int_vec4;
    uvec4 uint_vec4;
    vec4 float_vec4;
    dvec4 double_vec4;

    // Matrices
    mat2 float_mat2;
    dmat2 double_mat2;

    mat3 float_mat3;
    dmat3 double_mat3;

    mat4 float_mat4;
    dmat4 double_mat4;
} all_types_array[2];

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
