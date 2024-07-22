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
