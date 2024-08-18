#include "internal.h"

#include "arkin_log.h"

#include <spirv_cross_c.h>

static void error_cb(void *userdata, const char *error) {
    (void) userdata;
    ar_error("%s", error);
}

static ReflectedDataType translate_type(spvc_basetype type, U32 vec_size, U32 cols) {
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
            return REFLECTED_DATA_TYPE_SAMPLER;
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

static ReflectedType reflect(ArArena *arena, spvc_compiler compiler, spvc_type type, ArStr name) {
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


ReflectedStage reflect_spv(ArArena *arena, ArStr spv) {
    ReflectedStage shader = {0}; 

    spvc_context ctx;
    spvc_context_create(&ctx);

    spvc_context_set_error_callback(ctx, error_cb, NULL);

    spvc_parsed_ir ir;
    spvc_context_parse_spirv(ctx, (const SpvId *) spv.data, spv.len / sizeof(SpvId), &ir);

    spvc_compiler compiler;
    spvc_context_create_compiler(ctx, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler);

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
        const spvc_reflected_resource *list = NULL;
        spvc_resources_get_resource_list_for_type(resources, reflection_types[i], &list, &shader.count[i]);
        shader.types[i] = ar_arena_push_arr(arena, ReflectedType, shader.count[i]);
        for (U32 j = 0; j < shader.count[i]; j++) {
            spvc_reflected_resource resource = list[j];
            spvc_type type = spvc_compiler_get_type_handle(compiler, resource.type_id);

            shader.types[i][j] = reflect(arena, compiler, type, ar_str_cstr(resource.name));
        }
    }

    spvc_context_destroy(ctx);

    return shader;
}
