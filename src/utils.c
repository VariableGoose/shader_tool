#include "arkin_core.h"
#include "internal.h"

#include "arkin_log.h"

#include <assert.h>

char *ar_str_to_cstr(ArArena *arena, ArStr str) {
    char *cstr = ar_arena_push_no_zero(arena, str.len + 1);
    memcpy(cstr, str.data, str.len);
    cstr[str.len] = '\0';
    return cstr;
}

ArStr read_file(ArArena *arena, ArStr path) {
    ArTemp temp = ar_temp_begin(arena);
    const char *cstr_path = ar_str_to_cstr(temp.arena, path);
    FILE *fp = fopen(cstr_path, "rb");
    ar_temp_end(&temp);
    if (fp == NULL) {
        ar_error("Failed to open file %s.", cstr_path);
        return (ArStr) {0};
    }

    fseek(fp, 0, SEEK_END);
    U64 len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    U8 *buffer = ar_arena_push_no_zero(arena, len);
    fread(buffer, 1, len, fp);

    fclose(fp);

    return ar_str(buffer, len);
}

ArStr dirname(ArStr filepath) {
    U64 last_slash = ar_str_find_char(filepath, '/', AR_STR_MATCH_FLAG_LAST);

    // No slash present.
    if (last_slash == filepath.len) {
        return ar_str_lit(".");
    }

    B8 at_end = false;
    if (last_slash == filepath.len - 1) {
        at_end = true;
    }

    // Remove trailing slashes.
    while (filepath.data[last_slash] == '/') {
        last_slash--;
    }
    last_slash++;

    ArStr new_path = ar_str_chop_end(filepath, filepath.len - last_slash);

    if (at_end) {
        new_path = dirname(new_path);
    }

    return new_path;
}

void test_dirname(void) {
    ArTemp scratch = ar_scratch_get(NULL, 0);

    {
        ArStr path = ar_str_lit("/home/user/file.txt");
        ArStr expected = ar_str_lit("/home/user");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user");
        ArStr expected = ar_str_lit("/home");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user/");
        ArStr expected = ar_str_lit("/home");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user/");
        ArStr expected = ar_str_lit("/home");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user/.");
        ArStr expected = ar_str_lit("/home/user");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user///.");
        ArStr expected = ar_str_lit("/home/user");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("/home/user///");
        ArStr expected = ar_str_lit("/home");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("foobar.txt");
        ArStr expected = ar_str_lit(".");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    {
        ArStr path = ar_str_lit("./foobar.txt");
        ArStr expected = ar_str_lit(".");
        ArStr result = dirname(path);
        assert(ar_str_match(result, expected, AR_STR_MATCH_FLAG_EXACT));
    }

    ar_scratch_release(&scratch);
}
