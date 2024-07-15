#pragma once

#include "arkin_core.h"

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

extern ArStr preprocess(ArArena *arena, ArStr source, ArStrList paths);

extern void test_dirname(void);
