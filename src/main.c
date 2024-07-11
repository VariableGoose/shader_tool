#include "arkin_core.h"
#include "arkin_log.h"

#include <shaderc/shaderc.h>
#include <spirv_cross_c.h>

const ArStr vert_source = ar_str_lit("\
#version 450 core\n\
layout (location = 0) in vec2 v_pos;\n\
layout (location = 1) in vec2 v_uv;\n\
layout (location = 2) in vec4 v_color;\n\
layout (location = 0) out vec2 f_uv;\n\
layout (location = 1) out vec4 f_color;\n\
void main() {\n\
    f_uv = v_uv;\n\
    f_color = v_color;\n\
    gl_Position = vec4(v_pos, 0.0, 1.0);\n\
}\n\
");

ArStr compile_to_spv(ArArena *arena, const char *name, ArStr src) {
    shaderc_compiler_t compiler = shaderc_compiler_initialize();

    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);

    shaderc_compilation_result_t result = shaderc_compile_into_spv(
            compiler,
            (const char *) src.data,
            src.len,
            shaderc_vertex_shader,
            name,
            "main",
            options
        );

    U32 errs = shaderc_result_get_num_errors(result);
    if (errs > 0) {
        const char *err = shaderc_result_get_error_message(result);
        ar_error("%s", err);
        return (ArStr) {0};
    }

    ArStr spv = ar_str(
            (const U8 *) shaderc_result_get_bytes(result),
            shaderc_result_get_length(result)
        );
    ArStr spv_result = ar_str_push_copy(arena, spv);

    shaderc_compile_options_release(options);
    shaderc_result_release(result);
    shaderc_compiler_release(compiler);

    return spv_result;
}

void error_cb(void *userdata, const char *error) {
    (void) userdata;
    ar_error("%s", error);
}

ArStr compile_to_glsl(ArArena *arena, ArStr spv) {
    spvc_context ctx;
    spvc_context_create(&ctx);

    spvc_context_set_error_callback(ctx, error_cb, NULL);

    spvc_parsed_ir ir;
    spvc_context_parse_spirv(ctx, (const SpvId *) spv.data, spv.len / sizeof(SpvId), &ir);

    spvc_compiler compiler;
    spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

    const char *result;
    spvc_compiler_compile(compiler, &result);
    ArStr glsl_result = ar_str_cstr(result);
    glsl_result = ar_str_push_copy(arena, glsl_result);

    spvc_context_destroy(ctx);

    return glsl_result;
}

I32 main(void) {
    arkin_init(&(ArkinCoreDesc) {
            .error.callback = ar_log_error_callback
        });
    ArArena *arena = ar_arena_create_default();

    ArStr spv = compile_to_spv(arena, "vert_test", vert_source);
    ArStr glsl = compile_to_glsl(arena, spv);
    ar_debug("%.*s", (I32) glsl.len, glsl.data);

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
