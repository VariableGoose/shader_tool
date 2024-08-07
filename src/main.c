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

#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Include/glslang_c_shader_types.h>
#include <glslang/Public/resource_limits_c.h>
#include <spirv_cross_c.h>

ArStr compile_to_spv(ArArena *arena, ArStr src) {
    glslang_initialize_process();

    const char *code_cstr = ar_str_to_cstr(arena, src);

    glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = GLSLANG_STAGE_VERTEX,
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

    glslang_program_SPIRV_generate(program, GLSLANG_STAGE_VERTEX);
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

void error_cb(void *userdata, const char *error) {
    (void) userdata;
    ar_error("%s", error);
}

// NOTE: Booleans reflect into unsigned integers.
// bool -> uint
// bvec2 -> uvec2
// bvec3 -> uvec3
// bvec4 -> uvec4
typedef enum {
    REFLECTED_DATA_TYPE_UNKNOWN,

    REFLECTED_DATA_TYPE_VOID,
    REFLECTED_DATA_TYPE_STRUCT,

    // Scalers
    REFLECTED_DATA_TYPE_I32,
    REFLECTED_DATA_TYPE_U32,
    REFLECTED_DATA_TYPE_F32,
    REFLECTED_DATA_TYPE_F64,

    // Vectors
    REFLECTED_DATA_TYPE_IVEC2,
    REFLECTED_DATA_TYPE_UVEC2,
    REFLECTED_DATA_TYPE_VEC2,
    REFLECTED_DATA_TYPE_DVEC2,

    REFLECTED_DATA_TYPE_IVEC3,
    REFLECTED_DATA_TYPE_UVEC3,
    REFLECTED_DATA_TYPE_VEC3,
    REFLECTED_DATA_TYPE_DVEC3,

    REFLECTED_DATA_TYPE_IVEC4,
    REFLECTED_DATA_TYPE_UVEC4,
    REFLECTED_DATA_TYPE_VEC4,
    REFLECTED_DATA_TYPE_DVEC4,

    // Matrices
    REFLECTED_DATA_TYPE_MAT2,
    REFLECTED_DATA_TYPE_DMAT2,

    REFLECTED_DATA_TYPE_MAT3,
    REFLECTED_DATA_TYPE_DMAT3,

    REFLECTED_DATA_TYPE_MAT4,
    REFLECTED_DATA_TYPE_DMAT4,

    REFLECTED_DATA_TYPE_COUNT,
} ReflectedDataType;

const ArStr type_name[REFLECTED_DATA_TYPE_COUNT] = {
    ar_str_lit("ERR::Unkown"),

    ar_str_lit("void"),
    ar_str_lit("struct"),

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

typedef struct ReflectedType ReflectedType;
struct ReflectedType {
    ReflectedDataType data_type;
    ArStr name;

    // 0 if not an array.
    U32 array_dimensions;
    // Array of length 'array_dimensions'.
    U32 *array_dimension_lengths;

    U32 vec_size;
    U32 cols;

    U32 member_count;
    ReflectedType *members;
};

ReflectedDataType translate_type(spvc_basetype type, U32 vec_size, U32 cols) {
    switch (type) {
        case SPVC_BASETYPE_VOID:
            return REFLECTED_DATA_TYPE_VOID;

        // Booleans become uints for whatever reason.
        case SPVC_BASETYPE_BOOLEAN:
            break;

        case SPVC_BASETYPE_INT8:
            break;
        case SPVC_BASETYPE_UINT8:
            break;
        case SPVC_BASETYPE_INT16:
            break;
        case SPVC_BASETYPE_UINT16:
            break;

        case SPVC_BASETYPE_INT32:
            if (vec_size == 1 && cols == 1) {
                return REFLECTED_DATA_TYPE_I32;
            } else if (vec_size == 2 && cols == 1) {
                return REFLECTED_DATA_TYPE_IVEC2;
            } else if (vec_size == 3 && cols == 1) {
                return REFLECTED_DATA_TYPE_IVEC3;
            } else if (vec_size == 4 && cols == 1) {
                return REFLECTED_DATA_TYPE_IVEC4;
            }
            break;

        case SPVC_BASETYPE_UINT32:
            if (vec_size == 1 && cols == 1) {
                return REFLECTED_DATA_TYPE_U32;
            } else if (vec_size == 2 && cols == 1) {
                return REFLECTED_DATA_TYPE_UVEC2;
            } else if (vec_size == 3 && cols == 1) {
                return REFLECTED_DATA_TYPE_UVEC3;
            } else if (vec_size == 4 && cols == 1) {
                return REFLECTED_DATA_TYPE_UVEC4;
            }
            break;

        case SPVC_BASETYPE_INT64:
            break;
        case SPVC_BASETYPE_UINT64:
            break;
        case SPVC_BASETYPE_ATOMIC_COUNTER:
            break;
        case SPVC_BASETYPE_FP16:
            break;

        case SPVC_BASETYPE_FP32:
            if (vec_size == 1 && cols == 1) {
                return REFLECTED_DATA_TYPE_F32;
            } else if (vec_size == 2 && cols == 1) {
                return REFLECTED_DATA_TYPE_VEC2;
            } else if (vec_size == 3 && cols == 1) {
                return REFLECTED_DATA_TYPE_VEC3;
            } else if (vec_size == 4 && cols == 1) {
                return REFLECTED_DATA_TYPE_VEC4;
            } else if (vec_size == 2 && cols == 2) {
                return REFLECTED_DATA_TYPE_MAT2;
            } else if (vec_size == 3 && cols == 3) {
                return REFLECTED_DATA_TYPE_MAT3;
            } else if (vec_size == 4 && cols == 4) {
                return REFLECTED_DATA_TYPE_MAT4;
            }
            break;

        case SPVC_BASETYPE_FP64:
            if (vec_size == 1 && cols == 1) {
                return REFLECTED_DATA_TYPE_F64;
            } else if (vec_size == 2 && cols == 1) {
                return REFLECTED_DATA_TYPE_DVEC2;
            } else if (vec_size == 3 && cols == 1) {
                return REFLECTED_DATA_TYPE_DVEC3;
            } else if (vec_size == 4 && cols == 1) {
                return REFLECTED_DATA_TYPE_DVEC4;
            } else if (vec_size == 2 && cols == 2) {
                return REFLECTED_DATA_TYPE_DMAT2;
            } else if (vec_size == 3 && cols == 3) {
                return REFLECTED_DATA_TYPE_DMAT3;
            } else if (vec_size == 4 && cols == 4) {
                return REFLECTED_DATA_TYPE_DMAT4;
            }
            break;

        case SPVC_BASETYPE_STRUCT:
            return REFLECTED_DATA_TYPE_STRUCT;

        case SPVC_BASETYPE_IMAGE:
            break;
        case SPVC_BASETYPE_SAMPLED_IMAGE:
            break;
        case SPVC_BASETYPE_SAMPLER:
            break;
        case SPVC_BASETYPE_ACCELERATION_STRUCTURE:
            break;
        case SPVC_BASETYPE_UNKNOWN:
            break;
        case SPVC_BASETYPE_INT_MAX:
            break;
    }

    ar_error("Unkown: %d", type);
    return REFLECTED_DATA_TYPE_UNKNOWN;
}

ReflectedType reflect(ArArena *arena, spvc_compiler compiler, spvc_type type, ArStr name) {
    spvc_basetype basetype = spvc_type_get_basetype(type);

    U32 arr_dims = spvc_type_get_num_array_dimensions(type);
    U32 *arr_dim_lens = NULL;
    if (arr_dims > 0) {
        arr_dim_lens = ar_arena_push_arr_no_zero(arena, U32, arr_dims);
        for (U32 i = 0; i < arr_dims; i++) {
            arr_dim_lens[i] = spvc_type_get_array_dimension(type, i);
        }
    }

    // if vec_size == 1:
    //     type = scaler
    // else if vec_size > 1:
    //     if columns == 1:
    //         type = vector
    //     else if vec_size > 1:
    //         type = matrix

    U32 vec_size = spvc_type_get_vector_size(type);
    U32 cols = spvc_type_get_columns(type);

    ReflectedType reflected = {
        .data_type = (ReflectedDataType) translate_type(basetype, vec_size, cols),
        .name = ar_str_push_copy(arena, name),
        .array_dimensions = arr_dims,
        .array_dimension_lengths = arr_dim_lens,

        .vec_size = vec_size,
        .cols = cols,
    };

    switch (basetype) {
        case SPVC_BASETYPE_VOID:
            break;
        case SPVC_BASETYPE_BOOLEAN:
            break;
        case SPVC_BASETYPE_INT8:
            break;
        case SPVC_BASETYPE_UINT8:
            break;
        case SPVC_BASETYPE_INT16:
            break;
        case SPVC_BASETYPE_UINT16:
            break;
        case SPVC_BASETYPE_INT32:
            break;
        case SPVC_BASETYPE_UINT32:
            break;
        case SPVC_BASETYPE_INT64:
            break;
        case SPVC_BASETYPE_UINT64:
            break;
        case SPVC_BASETYPE_ATOMIC_COUNTER:
            break;
        case SPVC_BASETYPE_FP16:
            break;
        case SPVC_BASETYPE_FP32:
            break;
        case SPVC_BASETYPE_FP64:
            break;

        case SPVC_BASETYPE_STRUCT: {
            U32 member_count = spvc_type_get_num_member_types(type);
            reflected.member_count = member_count;
            reflected.members = ar_arena_push_arr(arena, ReflectedType, member_count);
            U32 id = spvc_type_get_base_type_id(type);
            for (U32 i = 0; i < member_count; i++) {
                const char *member_name = spvc_compiler_get_member_name(compiler, id, i);
                spvc_type_id member_type_id = spvc_type_get_member_type(type, i);
                spvc_type member_type = spvc_compiler_get_type_handle(compiler, member_type_id);
                reflected.members[i] = reflect(arena, compiler, member_type, ar_str_cstr(member_name));
            }
        } break;

        case SPVC_BASETYPE_IMAGE:
            break;
        case SPVC_BASETYPE_SAMPLED_IMAGE:
            break;
        case SPVC_BASETYPE_SAMPLER:
            break;
        case SPVC_BASETYPE_ACCELERATION_STRUCTURE:
            break;
        case SPVC_BASETYPE_UNKNOWN:
            break;
        case SPVC_BASETYPE_INT_MAX:
            break;
    }

    return reflected;
}

void print_reflected_type(ReflectedType t) {
    ar_info("%.*s: %.*s", (I32) t.name.len, t.name.data, (I32) type_name[t.data_type].len, type_name[t.data_type].data);
    ar_info("  Array dimensions: %u", t.array_dimensions);
    for (U32 i = 0; i < t.array_dimensions; i++) {
        ar_info("  Array dimension[%u] length: %u", i, t.array_dimension_lengths[i]);
    }
    ar_info("  Vector size: %u", t.vec_size);
    ar_info("  Columns: %u", t.cols);

    for (U32 i = 0; i < t.member_count; i++) {
        print_reflected_type(t.members[i]);
    }
}

ArStr compile_to_glsl(ArArena *arena, ArStr spv) {
    spvc_context ctx;
    spvc_context_create(&ctx);

    spvc_context_set_error_callback(ctx, error_cb, NULL);

    spvc_parsed_ir ir;
    spvc_context_parse_spirv(ctx, (const SpvId *) spv.data, spv.len / sizeof(SpvId), &ir);

    spvc_compiler compiler;
    spvc_context_create_compiler(ctx, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);
    // spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

    // Reflection
    spvc_resources resources;
    spvc_compiler_create_shader_resources(compiler, &resources);

    spvc_resource_type reflection_types[] = {
        SPVC_RESOURCE_TYPE_UNIFORM_BUFFER,
        SPVC_RESOURCE_TYPE_SAMPLED_IMAGE,
        SPVC_RESOURCE_TYPE_PUSH_CONSTANT,
        // SPVC_RESOURCE_TYPE_STAGE_INPUT,
    };

    for (U32 i = 0; i < ar_arrlen(reflection_types); i++) {
        Usize count = 0;
        const spvc_reflected_resource *list = NULL;
        spvc_resources_get_resource_list_for_type(resources, reflection_types[i], &list, &count);
        for (U32 j = 0; j < count; j++) {
            spvc_reflected_resource resource = list[j];
            spvc_type type = spvc_compiler_get_type_handle(compiler, resource.type_id);

            ReflectedType reflected_type = reflect(arena, compiler, type, ar_str_cstr(resource.name));
            print_reflected_type(reflected_type);
        }
    }

    const char *result;
    spvc_compiler_compile(compiler, &result);
    ArStr glsl_result = ar_str_cstr(result);
    glsl_result = ar_str_push_copy(arena, glsl_result);

    spvc_context_destroy(ctx);

    return glsl_result;
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
    ArStr glsl = compile_to_glsl(arena, spv);
    info(glsl);

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
