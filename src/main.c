#include "arkin_core.h"
#include "arkin_log.h"

#include "internal.h"

//
// Steps:
// Preprocess
// Split into modules
// Translate to spirv
// Do reflection
// Translate to other shader languages
//
// Generate a header file (CLI only)
//

//
// Search paths:
// Current directory
// Relative to the current file
//

// #include <shaderc/shaderc.h>
// #include <spirv_cross_c.h>
//
// ArStr compile_to_spv(ArArena *arena, const char *name, ArStr src) {
//     shaderc_compiler_t compiler = shaderc_compiler_initialize();
//
//     shaderc_compile_options_t options = shaderc_compile_options_initialize();
//     shaderc_compile_options_set_forced_version_profile(options, 450, shaderc_profile_core);
//     // shaderc_compile_options_set_forced_version_profile(options, 320, shaderc_profile_es);
//
//     shaderc_compilation_result_t result = shaderc_compile_into_spv(
//             compiler,
//             (const char *) src.data,
//             src.len,
//             shaderc_vertex_shader,
//             name,
//             "main",
//             options
//         );
//
//     U32 errs = shaderc_result_get_num_errors(result);
//     if (errs > 0) {
//         const char *err = shaderc_result_get_error_message(result);
//         ar_error("%s", err);
//         return (ArStr) {0};
//     }
//
//     ArStr spv = ar_str(
//             (const U8 *) shaderc_result_get_bytes(result),
//             shaderc_result_get_length(result)
//         );
//     ArStr spv_result = ar_str_push_copy(arena, spv);
//
//     shaderc_compile_options_release(options);
//     shaderc_result_release(result);
//     shaderc_compiler_release(compiler);
//
//     return spv_result;
// }
//
// void error_cb(void *userdata, const char *error) {
//     (void) userdata;
//     ar_error("%s", error);
// }
//
// ArStr compile_to_glsl(ArArena *arena, ArStr spv) {
//     spvc_context ctx;
//     spvc_context_create(&ctx);
//
//     spvc_context_set_error_callback(ctx, error_cb, NULL);
//
//     spvc_parsed_ir ir;
//     spvc_context_parse_spirv(ctx, (const SpvId *) spv.data, spv.len / sizeof(SpvId), &ir);
//
//     spvc_compiler compiler;
//     spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
//
//     // Reflection
//     // spvc_resources resources;
//     // spvc_compiler_create_shader_resources(compiler, &resources);
//     // U32 count = 0;
//     // const spvc_reflected_resource *list = NULL;
//     // spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, (size_t *) &count);
//     //
//     // // https://www.reddit.com/r/vulkan/comments/8dkkub/spirvcross_how_can_i_get_ubo_structure_member/
//     // for (U32 i = 0; i < count; i++) {
//     //     spvc_reflected_resource resource = list[i];
//     //     ar_debug("%u", resource.type_id);
//     //     ar_debug("%u", resource.base_type_id);
//     //     ar_debug("%s", resource.name);
//     //
//     //     spvc_type type = spvc_compiler_get_type_handle(compiler, resource.type_id);
//     //     spvc_basetype basetype = spvc_type_get_basetype(type);
//     //     U32 member_count = spvc_type_get_num_member_types(type);
//     //     for (U32 j = 0; j < member_count; j++) {
//     //         spvc_type_id member_type_id = spvc_type_get_member_type(type, j);
//     //         spvc_type member_type = spvc_compiler_get_type_handle(compiler, member_type_id);
//     //         spvc_basetype member_basetype = spvc_type_get_basetype(member_type);
//     //         const char *name = spvc_compiler_get_member_name(compiler, resource.base_type_id, j);
//     //         ar_debug("%s", name);
//     //     }
//     //     ar_debug("%u", basetype);
//     // }
//
//     const char *result;
//     spvc_compiler_compile(compiler, &result);
//     ArStr glsl_result = ar_str_cstr(result);
//     glsl_result = ar_str_push_copy(arena, glsl_result);
//
//     spvc_context_destroy(ctx);
//
//     return glsl_result;
// }

static void _info(ArStr str, const char *file, U32 line) {
    U32 last = 0;
    U32 curr = 0;
    while (curr < str.len) {
        if (str.data[curr] == '\n') {
            if (curr == last) {
                ar_info(" ");
                curr++;
                last = curr;
                continue;
            }

            ArStr substr = ar_str_sub(str, last, curr - 1);
            _ar_log(AR_LOG_LEVEL_INFO, file, line, "%.*s", (I32) substr.len, substr.data);
            last = curr + 1;
        }

        curr++;
    }
    ArStr substr = ar_str_chop_start(str, last);
    _ar_log(AR_LOG_LEVEL_INFO, file, line, "%.*s", (I32) substr.len, substr.data);
}
#define info(str) _info(str, __FILE__, __LINE__);

I32 main(I32 argc, char **argv) {
    arkin_init(&(ArkinCoreDesc) {
            .error.callback = ar_log_error_callback
        });
    ArArena *arena = ar_arena_create_default();

    test_dirname();

    if (argc < 2) {
        ar_error("No input file provided.");
    }
    ArStr filepath = ar_str_cstr(argv[1]);
    ArStr file = read_file(arena, filepath);

    ArStrList path_list = {0};
    ArStr file_dir = dirname(ar_str_cstr(argv[1]));
    ar_str_list_push(arena, &path_list, file_dir);
    ar_str_list_push(arena, &path_list, ar_str_lit("."));

    ParsedShader shader = parse_shader(arena, file, path_list);
    info(shader.program.name);
    info(shader.program.vertex_source);
    info(shader.program.fragment_source);

    // ArStr spv = compile_to_spv(arena, "vert", vert_soruce);
    // ArStr glsl = compile_to_glsl(arena, spv);
    // info(glsl);

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
