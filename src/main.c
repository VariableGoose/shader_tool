#include "arkin_core.h"
#include "arkin_log.h"

#include "internal.h"

//
// Steps:
// Preprocess
// Split into modules
// Translate to spirv
// Do reflection
//
// Generate a header file (CLI only)
//

//
// Search paths:
// Current directory
// Relative to the current file
//

#include <stdio.h>

const ArStr type_name[REFLECTED_DATA_TYPE_COUNT] = {
    ar_str_lit("ERR::Unkown"),

    ar_str_lit("void"),
    ar_str_lit("struct"),
    ar_str_lit("sampler"),

    ar_str_lit("int"),
    ar_str_lit("uint"),
    ar_str_lit("float"),
    ar_str_lit("double"),

    ar_str_lit("ivec2"),
    ar_str_lit("uvec2"),
    ar_str_lit("vec2"),
    ar_str_lit("dvec2"),

    ar_str_lit("ivec3"),
    ar_str_lit("uvec3"),
    ar_str_lit("vec3"),
    ar_str_lit("dvec3"),

    ar_str_lit("ivec4"),
    ar_str_lit("uvec4"),
    ar_str_lit("vec4"),
    ar_str_lit("dvec4"),

    ar_str_lit("mat2"),
    ar_str_lit("dmat2"),

    ar_str_lit("mat3"),
    ar_str_lit("dmat3"),

    ar_str_lit("mat4"),
    ar_str_lit("dmat4"),
};

void print_reflected_type(ReflectedType t, U32 level) {
    U8 spaces[1024] = {0};
    memset(spaces, ' ', level*4);
    if (t.data_type == REFLECTED_DATA_TYPE_STRUCT) {
        level++;
    }

    char arr[64] = {0};
    U32 off = 0;
    // Iterate backwards because the reflection gave the array dimensions in
    // reverse order.
    for (I32 i = t.array_dimensions - 1; i >= 0; i--) {
        off += snprintf(&arr[off], 64 - off, "[%u]", t.array_dimension_lengths[i]);
    }
    ar_info("%s%.*s: %.*s%s", spaces, (I32) t.name.len, t.name.data, (I32) type_name[t.data_type].len, type_name[t.data_type].data, arr);

    for (U32 i = 0; i < t.member_count; i++) {
        print_reflected_type(t.members[i], level);
    }
}

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
        ar_arena_destroy(&arena);
        arkin_terminate();
        return 1;
    }
    ArStr filepath = ar_str_cstr(argv[1]);
    ArStr file = read_file(arena, filepath);

    ArStrList path_list = {0};
    ArStr file_dir = dirname(ar_str_cstr(argv[1]));
    ar_str_list_push(arena, &path_list, file_dir);
    ar_str_list_push(arena, &path_list, ar_str_lit("."));

    ParsedShader shader = parse_shader(arena, file, path_list);
    // info(shader.program.name);
    // info(shader.program.vertex_source);
    // info(shader.program.fragment_source);

    ArStr spv = compile_to_spv(arena, shader.program.vertex_source);
    ReflectedShader reflection = reflect_spv(arena, spv);

    for (U32 i = 0; i < REFLECTION_INDEX_COUNT; i++) {
        for (Usize j = 0; j < reflection.count[i]; j++) {
            print_reflected_type(reflection.types[i][j], 0);
        }
    }

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
