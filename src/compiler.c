#include "internal.h"
#include "arkin_log.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>
#include <glslang/Public/resource_limits_c.h>

ArStr compile_to_spv(ArArena *arena, ArStr glsl, ShaderType type) {
    glslang_initialize_process();

    const char *code_cstr = ar_str_to_cstr(arena, glsl);

    glslang_stage_t stage;
    switch (type) {
        case SHADER_TYPE_VERTEX:
            stage = GLSLANG_STAGE_VERTEX;
            break;
        case SHADER_TYPE_FRAGMENT:
            stage = GLSLANG_STAGE_FRAGMENT;
            break;
    }

    glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_2,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_5,

        .code = code_cstr,
        .default_version = 450,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = glslang_default_resource(),
    };

    glslang_shader_t *shader = glslang_shader_create(&input);

    if (!glslang_shader_preprocess(shader, &input)) {
        ar_error("GLSLANG: Preprocessing failed.");
        ar_error("%s", glslang_shader_get_info_log(shader));
        ar_error("%s", glslang_shader_get_info_debug_log(shader));
        glslang_shader_delete(shader);
        return (ArStr) {0};
    }

    if (!glslang_shader_parse(shader, &input)) {
        ar_error("GLSLANG: Parsing failed.");
        ar_error("%s", glslang_shader_get_info_log(shader));
        ar_error("%s", glslang_shader_get_info_debug_log(shader));
        glslang_shader_delete(shader);
        return (ArStr) {0};
    }

    glslang_program_t *program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        ar_error("GLSLANG: Linking failed.");
        ar_error("%s", glslang_program_get_info_log(program));
        ar_error("%s", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return (ArStr) {0};
    }

    glslang_program_SPIRV_generate(program, stage);
    U64 len = glslang_program_SPIRV_get_size(program) * sizeof(U32);
    U8 *data = ar_arena_push_arr_no_zero(arena, U8, len);
    glslang_program_SPIRV_get(program, (U32 *) data);

    const char *spirv_messages = glslang_program_SPIRV_get_messages(program);
    if (spirv_messages != NULL) {
        ar_info("GLSLANG SPIR-V messages: %s", spirv_messages);
    }

    glslang_program_delete(program);
    glslang_shader_delete(shader);

    glslang_finalize_process();

    return ar_str(data, len);
}
