#include dir/include.glsl

#vert vs
#include_module funcs

layout (location = 0) in vec2 v_pos;
layout (location = 1) in vec2 v_uv;

out vec2 f_uv;

void main() {
    f_uv = double(v_uv);
    gl_Position = vec4(v_pos);
}
#end

#frag fs
#include_module funcs

layout (location = 0) out vec4 frag_color;

in vec2 f_uv;

void main() {
    f_uv = half(f_uv);
    frag_color = vec4(f_uv, 0.0, 1.0);
}
#end

#program test_shader vs fs
