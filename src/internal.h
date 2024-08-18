#pragma once

#include "arkin_core.h"

typedef struct ParsedShader ParsedShader;
struct ParsedShader {
    struct {
        ArStr name;
        ArStr vertex_source;
        ArStr fragment_source;
    } program;
    ArHashMap *ctypes;
};


extern ParsedShader parse_shader(ArArena *arena, ArStr source, ArStrList paths);

typedef enum {
    SHADER_TYPE_VERTEX,
    SHADER_TYPE_FRAGMENT,
} ShaderType;

extern ArStr compile_to_spv(ArArena *arena, ArStr glsl, ShaderType type);

// NOTE: Booleans reflect into unsigned integers.
// bool -> uint
// bvec2 -> uvec2
// bvec3 -> uvec3
// bvec4 -> uvec4
typedef enum {
    REFLECTED_DATA_TYPE_UNKNOWN,

    REFLECTED_DATA_TYPE_VOID,
    REFLECTED_DATA_TYPE_STRUCT,
    REFLECTED_DATA_TYPE_SAMPLER,

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

typedef enum {
    REFLECTION_INDEX_UNIFORM_BUFFER,
    REFLECTION_INDEX_SAMPLER,
    REFLECTION_INDEX_PUSH_CONSTANT,

    REFLECTION_INDEX_COUNT,
} ReflectionIndex;

typedef struct ReflectedShader ReflectedShader;
struct ReflectedShader {
    ReflectedType *types[REFLECTION_INDEX_COUNT];
    Usize count[REFLECTION_INDEX_COUNT];
};

extern ReflectedShader reflect_spv(ArArena *arena, ArStr spv);

//
// Utils
//
extern char *ar_str_to_cstr(ArArena *arena, ArStr str);
extern ArStr read_file(ArArena *arena, ArStr path);

// Strips the last part off of a path.
// /home/user/file.txt  ->      /home/user
// /home/user           ->      /home
// /home/user/          ->      /home
// /home/user/.         ->      /home/user
// /home/user///.       ->      /home/user
// /home/user///        ->      /home
// foobar.txt           ->      .
// ./foobar.txt         ->      .
extern ArStr dirname(ArStr filepath);
extern void test_dirname(void);
