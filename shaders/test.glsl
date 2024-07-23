#include dir/include.glsl

#vert vs
#include_module funcs

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 projection;
    int arr[16];
    What what;
} ubo;

layout (binding = 1) uniform OtherUBO {
    float foo;
    mat4 mat;
} other[4][16];

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
