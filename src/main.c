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

static void log(ArStr str) {
    U32 last = 0;
    U32 curr = 0;
    while (curr < str.len) {
        if (str.data[curr] == '\n') {
            if (curr == last) {
                ar_info("");
                curr++;
                last = curr;
                continue;
            }

            ArStr substr = ar_str_sub(str, last, curr - 1);
            ar_info("%.*s", (I32) substr.len, substr.data);
            last = curr + 1;
        }

        curr++;
    }
    ArStr substr = ar_str_sub(str, last, str.len);
    ar_info("%.*s", (I32) substr.len, substr.data);
}

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

    file = preprocess(arena, file, path_list);

    // log(file);

    ar_arena_destroy(&arena);
    arkin_terminate();
    return 0;
}
