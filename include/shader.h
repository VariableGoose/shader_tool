#ifndef ARKIN_SHADER_H
#define ARKIN_SHADER_H

#include "arkin_core.h"

typedef struct Program Program;
struct Program {
    const Module vertex;
    const Module fragment;
};

typedef struct ParsedShader ParsedShader;
struct ParsedShader {
    ArHashMap *modules;
    ArHashMap *ctypes;
    Program program;
};

extern ParsedShader parse_shader(ArStr source, ArStrList search_paths);

#endif
