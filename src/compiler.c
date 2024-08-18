#include "arkin_core.h"
#include "internal.h"
#include "arkin_log.h"

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>
#include <glslang/Public/resource_limits_c.h>

typedef enum {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
} ShaderType;

static glslang_shader_t *create_shader(ArArena *arena, ArStr glsl, ShaderType type) {
    glslang_stage_t stage;
    switch (type) {
        case SHADER_TYPE_VERTEX:
            stage = GLSLANG_STAGE_VERTEX;
            break;
        case SHADER_TYPE_FRAGMENT:
            stage = GLSLANG_STAGE_FRAGMENT;
            break;
    }

    ArTemp scratch = ar_scratch_get(&arena, 1);
    const char *code_cstr = ar_str_to_cstr(arena, glsl);
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
        ar_scratch_release(&scratch);
        return NULL;
    }

    if (!glslang_shader_parse(shader, &input)) {
        ar_error("GLSLANG: Parsing failed.");
        ar_error("%s", glslang_shader_get_info_log(shader));
        ar_error("%s", glslang_shader_get_info_debug_log(shader));
        glslang_shader_delete(shader);
        ar_scratch_release(&scratch);
        return NULL;
    }

    ar_scratch_release(&scratch);
    return shader;
}

CompiledShader compile_shader(ArArena *arena, ArStr shader_name, ArStr vertex_source, ArStr fragment_source) {
    glslang_initialize_process();

    glslang_shader_t *vertex_shader = create_shader(arena, vertex_source, SHADER_TYPE_VERTEX);
    glslang_shader_t *fragment_shader = create_shader(arena, fragment_source, SHADER_TYPE_FRAGMENT);

    glslang_program_t *program = glslang_program_create();
    glslang_program_add_shader(program, vertex_shader);
    glslang_program_add_shader(program, fragment_shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        ar_error("GLSLANG: Linking failed.");
        ar_error("%s", glslang_program_get_info_log(program));
        ar_error("%s", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(vertex_shader);
        glslang_shader_delete(fragment_shader);
        return (CompiledShader) {0};
    }

    glslang_program_SPIRV_generate(program, GLSLANG_STAGE_VERTEX);
    U64 len = glslang_program_SPIRV_get_size(program) * sizeof(U32);
    U8 *data = ar_arena_push_arr_no_zero(arena, U8, len);
    glslang_program_SPIRV_get(program, (U32 *) data);
    const char *spirv_messages = glslang_program_SPIRV_get_messages(program);
    if (spirv_messages != NULL) {
        ar_info("GLSLANG SPIR-V messages: %s", spirv_messages);
    }
    ArStr vertex_spv = ar_str(data, len);

    glslang_program_SPIRV_generate(program, GLSLANG_STAGE_FRAGMENT);
    len = glslang_program_SPIRV_get_size(program) * sizeof(U32);
    data = ar_arena_push_arr_no_zero(arena, U8, len);
    glslang_program_SPIRV_get(program, (U32 *) data);
    spirv_messages = glslang_program_SPIRV_get_messages(program);
    if (spirv_messages != NULL) {
        ar_info("GLSLANG SPIR-V messages: %s", spirv_messages);
    }
    ArStr fragment_spv = ar_str(data, len);

    glslang_program_delete(program);
    glslang_shader_delete(vertex_shader);
    glslang_shader_delete(fragment_shader);

    glslang_finalize_process();

    return (CompiledShader) {
        .name = shader_name,
        .vertex = {
            .spv = vertex_spv,
            .reflection = reflect_spv(arena, vertex_spv),
        },
        .fragment = {
            .spv = fragment_spv,
            .reflection = reflect_spv(arena, fragment_spv),
        },
    };
}
