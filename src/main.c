#include "arkin_core.h"
#include "arkin_log.h"

#include "internal.h"
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

void _info(ArStr str, const char *file, U32 line) {
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

void write_reflected_type(FILE *fp, const char *prefix, ReflectedType type, U32 level) {
    if (type.data_type == REFLECTED_DATA_TYPE_STRUCT && level == 0) {
        fprintf(fp, "typedef struct %s_%.*s %s_%.*s;\n",
            prefix, (I32) type.name.len, type.name.data,
            prefix, (I32) type.name.len, type.name.data);
        fprintf(fp, "struct %s_%.*s {\n", prefix, (I32) type.name.len, type.name.data);

        for (U32 i = 0; i < type.member_count; i++) {
            write_reflected_type(fp, prefix, type.members[i], level + 1);
        }

        fprintf(fp, "};\n");
        fprintf(fp, "\n");

        return;
    }

    char spaces[512] = {0};
    for (U32 i = 0; i < level*4; i++) {
        spaces[i] = ' ';
    }

    if (type.data_type == REFLECTED_DATA_TYPE_STRUCT) {
        fprintf(fp, "%sstruct {\n", spaces);
        for (U32 i = 0; i < type.member_count; i++) {
            write_reflected_type(fp, prefix, type.members[i], level + 1);
        }
        fprintf(fp, "%s} %.*s", spaces, (I32) type.name.len, type.name.data);
    } else {
        const U32 type_arr_lens[REFLECTED_DATA_TYPE_COUNT][2] = {
            {0, 0},
            {0, 0},
            {0, 0},
            {0, 0},

            {0, 0},
            {0, 0},
            {0, 0},
            {0, 0},

            {2, 0},
            {2, 0},
            {2, 0},
            {2, 0},

            {3, 0},
            {3, 0},
            {3, 0},
            {3, 0},

            {4, 0},
            {4, 0},
            {4, 0},
            {4, 0},

            {2, 2},
            {2, 2},

            {3, 3},
            {3, 3},

            {4, 4},
            {4, 4},
        };

        const char *type_defs[REFLECTED_DATA_TYPE_COUNT] = {
            "#error \"unknown datatype\"",
            "#error \"void\"",
            "#error \"struct\"",
            "#error \"sampler\"",

            "int",
            "unsigned int",
            "float",
            "double",

            "int",
            "unsigned int",
            "float",
            "double",

            "int",
            "unsigned int",
            "float",
            "double",

            "int",
            "unsigned int",
            "float",
            "double",

            "float",
            "double",

            "float",
            "double",

            "float",
            "double",
        };
        fprintf(fp, "%s%s %.*s", spaces, type_defs[type.data_type], (I32) type.name.len, type.name.data);

        for (U32 i = 0; i < 2; i++) {
            U32 arr_len = type_arr_lens[type.data_type][i];
            if (arr_len > 0) {
                fprintf(fp, "[%u]", arr_len);
            } else {
                break;
            }
        }
    }

    for (U32 i = 0; i < type.array_dimensions; i++) {
        fprintf(fp, "[%u]", type.array_dimension_lengths[i]);
    }
    fprintf(fp, ";\n");
}

void write_reflected_types(FILE *fp, const char *prefix, ReflectedStage stage) {
    for (U32 i = 0; i < REFLECTION_INDEX_COUNT; i++) {
        for (U32 j = 0; j < stage.count[i]; j++) {
            write_reflected_type(fp, prefix, stage.types[i][j], 0);
        }
    }
}

void write_header(CompiledShader shader, const char *filepath) {
    FILE *fp = fopen(filepath, "wb");

    fprintf(fp, "#ifndef %.*s_HEADER\n", (I32) shader.name.len, shader.name.data);
    fprintf(fp, "#define %.*s_HEADER\n", (I32) shader.name.len, shader.name.data);

    fprintf(fp, "\n");
    fprintf(fp, "// Vertex\n");

    // Create push constants and uniform buffer types.
    char prefix[512] = {0};
    snprintf(prefix, 512, "%.*s_VS", (I32) shader.name.len, shader.name.data);
    write_reflected_types(fp, prefix, shader.vertex.reflection);

    // Create SPV source variable.
    U32 len = fprintf(fp, "const char* %.*s_VS_SOURCE = \"", (I32) shader.name.len, shader.name.data);
    for (U64 i = 0; i < shader.vertex.spv.len; i++) {
        fprintf(fp, "\\x%.2x", shader.vertex.spv.data[i]);
        if ((i + 1) % 20 == 0) {
            fprintf(fp, " \\\n");
            for (U32 j = 0; j < len; j++) {
                fprintf(fp, " ");
            }
        }
    }
    fprintf(fp, "\";\n");

    fprintf(fp, "\n");
    fprintf(fp, "// Fragment\n");

    // Create push constants and uniform buffer types.
    snprintf(prefix, 512, "%.*s_FS", (I32) shader.name.len, shader.name.data);
    write_reflected_types(fp, prefix, shader.fragment.reflection);

    // Create SPV source variable.
    len = fprintf(fp, "const char* %.*s_FS_SOURCE = \"", (I32) shader.name.len, shader.name.data);
    for (U64 i = 0; i < shader.fragment.spv.len; i++) {
        fprintf(fp, "\\x%.2x", shader.fragment.spv.data[i]);
        if ((i + 1) % 20 == 0) {
            fprintf(fp, " \\\n");
            for (U32 j = 0; j < len; j++) {
                fprintf(fp, " ");
            }
        }
    }
    fprintf(fp, "\";\n");

    fprintf(fp, "\n");
    fprintf(fp, "#endif\n");

    fclose(fp);
}

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

    ParsedShader parsed = parse_shader(arena, file, path_list);
    CompiledShader compiled = compile_shader(arena, parsed.program.name, parsed.program.vertex_source, parsed.program.fragment_source);

    write_header(compiled, "header.h");

    // for (U32 i = 0; i < REFLECTION_INDEX_COUNT; i++) {
    //     for (Usize j = 0; j < compiled.vertex.reflection.count[i]; j++) {
    //         print_reflected_type(compiled.vertex.reflection.types[i][j], 0);
    //     }
    // }
    //
    // for (U32 i = 0; i < REFLECTION_INDEX_COUNT; i++) {
    //     for (Usize j = 0; j < compiled.fragment.reflection.count[i]; j++) {
    //         print_reflected_type(compiled.fragment.reflection.types[i][j], 0);
    //     }
    // }

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
