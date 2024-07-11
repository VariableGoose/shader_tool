#include "arkin_core.h"
#include "arkin_log.h"

#include <shaderc/shaderc.h>

const ArStr vert_source = ar_str_lit(" \
#version 330 core\n\
void main() {\n\
}\n\
");

I32 main(void) {
    shaderc_compiler_t compiler = shaderc_compiler_initialize();

    shaderc_compilation_result_t result = shaderc_compile_into_spv(
            compiler,
            (const char *) vert_source.data,
            vert_source.len,
            shaderc_vertex_shader,
            "test shader",
            "main",
            NULL
        );

    U32 warns = shaderc_result_get_num_warnings(result);
    U32 errs = shaderc_result_get_num_errors(result);

    ar_debug("Warnings: %u", warns);
    ar_debug("Errors: %u", errs);

    ArStr spv = ar_str(
            (const U8 *) shaderc_result_get_bytes(result),
            shaderc_result_get_length(result)
        );

    for (U32 i = 0; i < spv.len; i++) {
        printf("%02x ", spv.data[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n");
        }
    }
    if (spv.len % 8 != 0) {
        printf("\n");
    }

    shaderc_result_release(result);

    shaderc_compiler_release(compiler);
    return 0;
}
