#include "arkin_core.h"
#include "arkin_log.h"
#include "internal.h"

typedef struct Parser Parser;
struct Parser {
    Parser *next;

    ArStr source;
    U32 i;
};

typedef enum {
    MODULE_NONE,
    MODULE_MODULE,
    MODULE_VERT,
    MODULE_FRAG,
} ModuleType;

typedef struct Preprocessor Preprocessor;
struct Preprocessor {
    ArArena *arena;
    Parser *parser_stack;
    ModuleType current_module;
    ArStrList module_parts;
    ArHashMap *module_map;
    ArStr module_name;
};

U8 parser_peek(Parser parser) {
    return parser.source.data[parser.i];
}

U8 parser_peek_next(Parser parser) {
    return parser.source.data[parser.i + 1];
}

const ArStr GLSL_KEYWORDS[] = {
    ar_str_lit("define"),
    ar_str_lit("undef"),
    ar_str_lit("if"),
    ar_str_lit("ifdef"),
    ar_str_lit("ifndef"),
    ar_str_lit("else"),
    ar_str_lit("elif"),
    ar_str_lit("endif"),
    ar_str_lit("error"),
    ar_str_lit("pragma"),
    ar_str_lit("extension"),
    ar_str_lit("version"),
    ar_str_lit("line"),
};

typedef enum {
    TOKEN_END,
    TOKEN_MODULE,
    TOKEN_VERT,
    TOKEN_FRAG,
    TOKEN_PROGRAM,
    TOKEN_INCLUDE,
    TOKEN_INCLUDE_MODULE,
    TOKEN_CTYPEDEF,

    TOKEN_ERROR,
    TOKEN_GLSL,
} TokenType;

const ArStr KEYWORDS[] = {
    ar_str_lit("end"),
    ar_str_lit("module"),
    ar_str_lit("vert"),
    ar_str_lit("frag"),
    ar_str_lit("program"),
    ar_str_lit("include"),
    ar_str_lit("include_module"),
    ar_str_lit("ctypedef"),
};

const U32 KEYWORD_ARG_COUNT[] = {
    0,
    1,
    1,
    1,
    3,
    1,
    1,
    2,
};

typedef struct Token Token;
struct Token {
    TokenType type;
    ArStr error;
    ArStr args[4];
};

ArStr extract_statement(Parser *parser) {
    U32 start = parser->i;
    while (parser_peek(*parser) != '\n') {
        parser->i++;
    }
    U32 end = parser->i - 1;
    return ar_str_sub(parser->source, start, end);
}

ArStrList split_statement(ArArena *arena, ArStr statement) {
    ArStrList list = {0};

    U32 i = 0;
    while (i < statement.len) {
        while (ar_char_is_whitespace(statement.data[i])) {
            i++;
        }

        U32 start = i;
        while (!ar_char_is_whitespace(statement.data[i])) {
            i++;
        }
        U32 end = i - 1;
        ArStr word = ar_str_sub(statement, start, end);
        ar_str_list_push(arena, &list, word);
    }

    return list;
}

TokenType match_token_type(ArStr keyword) {
    for (U32 i = 0; i < ar_arrlen(KEYWORDS); i++) {
        if (ar_str_match(keyword, KEYWORDS[i], AR_STR_MATCH_FLAG_EXACT)) {
            return i;
        }
    }

    for (U32 i = 0; i < ar_arrlen(GLSL_KEYWORDS); i++) {
        if (ar_str_match(keyword, GLSL_KEYWORDS[i], AR_STR_MATCH_FLAG_EXACT)) {
            return TOKEN_GLSL;
        }
    }

    return TOKEN_ERROR;
}

Token tokenize_statement_list(ArArena *err_arena, ArStrList statement_list) {
    Token token = {0};

    ArStrListNode *curr = statement_list.first;
    ArStr keyword = curr->str;
    token.type = match_token_type(keyword);

    if (token.type == TOKEN_GLSL) {
        return token;
    } else if (token.type == TOKEN_ERROR) {
        token.error = ar_str_pushf(err_arena, "%.*s: Invalid token.", (I32) keyword.len, keyword.data);
        return token;
    }

    U32 arg_count = 0;
    for (ArStrListNode *c = curr->next; c != NULL; c = c->next) {
        arg_count++;
    }

    if (arg_count != KEYWORD_ARG_COUNT[token.type]) {
        token.error = ar_str_pushf(err_arena, "%.*s: Expected %u argument(s), got %u.", (I32) keyword.len, keyword.data, KEYWORD_ARG_COUNT[token.type], arg_count);
        token.type = TOKEN_ERROR;
        return token;
    }

    arg_count = 0;
    for (curr = curr->next; curr != NULL; curr = curr->next) {
        token.args[arg_count] = curr->str;
        arg_count++;
    }

    return token;
}

ArStr _preprocess(Preprocessor *pp, ArStr source, ArStrList paths);

void expand_token(Preprocessor *pp, Token token, ArStrList paths) {
    switch (token.type) {
        case TOKEN_END:
            if (pp->current_module == MODULE_NONE) {
                ar_error("Extranious end statment.");
                break;
            }

            ArStr complete_module = ar_str_list_join(pp->arena, pp->module_parts);
            B8 unique = ar_hash_map_insert(pp->module_map, pp->module_name, complete_module);
            if (!unique) {
                ar_error("%.*s: Module has already been defined.", (I32) pp->module_name.len, pp->module_name.data);
            }

            pp->current_module = MODULE_NONE;
            pp->module_name = (ArStr) {0};
            pp->module_parts = AR_STR_LIST_INIT;

            break;
        case TOKEN_MODULE:
            if (pp->current_module != MODULE_NONE) {
                ar_error("%.*s: New module started before ending the last module.", (I32) token.args[0].len, token.args[0].data);
                break;
            }

            pp->module_name = token.args[0];
            pp->current_module = MODULE_MODULE;
            break;
        case TOKEN_VERT:
            if (pp->current_module != MODULE_NONE) {
                ar_error("%.*s: New vertex module started before ending the last module.", (I32) token.args[0].len, token.args[0].data);
                break;
            }

            pp->module_name = token.args[0];
            pp->current_module = MODULE_VERT;
            break;
        case TOKEN_FRAG:
            if (pp->current_module != MODULE_NONE) {
                ar_error("%.*s: New fragment module started before ending the last module.", (I32) token.args[0].len, token.args[0].data);
                break;
            }

            pp->module_name = token.args[0];
            pp->current_module = MODULE_FRAG;
            break;
        case TOKEN_PROGRAM:
            break;
        case TOKEN_INCLUDE:
            if (paths.first == NULL) {
                ar_error("Cannot include files without providing search paths.");
                break;
            }

            ArTemp scratch = ar_scratch_get(NULL, 0);
            FILE *fp = NULL;
            ArStr path = {0};
            for (ArStrListNode *curr = paths.first; curr != NULL; curr = curr->next) {
                ArStr include_path = ar_str_pushf(scratch.arena, "%.*s/%.*s", (I32) curr->str.len, curr->str.data, (I32) token.args[0].len, token.args[0].data);
                const char *cstr_include_path = ar_str_to_cstr(scratch.arena, include_path);
                fp = fopen(cstr_include_path, "rb");
                if (fp != NULL) {
                    path = include_path;
                    break;
                }
            }

            if (fp == NULL) {
                ar_error("Couldn't find file %.*s, in the provided paths.", (I32) token.args[0].len, token.args[0].data);
            } else {
                fclose(fp);
            }

            ArStr imported_file = read_file(scratch.arena, path);
            ArStr path_dir = dirname(path);
            ar_str_list_push_front(scratch.arena, &paths, path_dir);
            ar_str_list_pop(&paths);
            _preprocess(pp, imported_file, paths);

            ar_scratch_release(&scratch);

            break;
        case TOKEN_INCLUDE_MODULE: {
            ArStr module_value = ar_hash_map_get(pp->module_map, token.args[0], ArStr);
            if (module_value.data == NULL) {
                ar_error("%.*s: Module couldn't be found.", (I32) token.args[0].len, token.args[0].data);
                break;
            }
            ar_str_list_push(pp->arena, &pp->module_parts, module_value);
        } break;
        case TOKEN_CTYPEDEF:
            break;

        case TOKEN_ERROR:
            ar_error("%.*s", (I32) token.error.len, token.error.data);
            break;
        case TOKEN_GLSL:
            break;
    }
}

ArStr _preprocess(Preprocessor *pp, ArStr source, ArStrList paths) {
    Parser parser = {
        .source = source,
    };

    while (parser.i < parser.source.len) {
        if (parser_peek(parser) == '/' && parser_peek_next(parser) == '/') {
            while (parser_peek(parser) != '\n') {
                parser.i++;
            }
            continue;
        }

        if (parser_peek(parser) == '#') {
            parser.i++;
            ArStr statement = extract_statement(&parser);
            ArTemp scratch = ar_scratch_get(NULL, 0);
            ArStrList statement_list = split_statement(scratch.arena, statement);
            Token token = tokenize_statement_list(scratch.arena, statement_list);
            expand_token(pp, token, paths);
            ar_scratch_release(&scratch);
        }
        parser.i++;
    }

    return source;
}

static U64 hash_str(const void *key, U64 len) {
    (void) len;
    const ArStr *_key = key;
    return ar_fvn1a_hash(_key->data, _key->len);
}

static B8 str_eq(const void *a, const void *b, U64 len) {
    (void) len;
    const ArStr *_a = a;
    const ArStr *_b = b;
    return ar_str_match(*_a, *_b, AR_STR_MATCH_FLAG_EXACT);
}

ArStr preprocess(ArArena *arena, ArStr source, ArStrList paths) {
    ArTemp scratch = ar_scratch_get(&arena, 1);

    ArHashMapDesc module_map_desc = {
        .arena = arena,
        .capacity = 32,

        .hash_func = hash_str,
        .eq_func = str_eq,

        .key_size = sizeof(ArStr),
        .value_size = sizeof(ArStr),
        .null_value = &(ArStr) {0},
    };

    Preprocessor pp = {
        .arena = scratch.arena,
        .module_map = ar_hash_map_init(module_map_desc),
    };

    _preprocess(&pp, source, paths);

    for (ArHashMapIter *iter = ar_hash_map_iter_init(scratch.arena, pp.module_map);
        ar_hash_map_iter_valid(iter);
        ar_hash_map_iter_next(iter)) {
        ArStr *key = ar_hash_map_iter_get_key_ptr(iter);
        ArStr *value = ar_hash_map_iter_get_value_ptr(iter);

        ar_info("%.*s: %.*s", (I32) key->len, key->data, (I32) value->len, value->data);
    }

    ar_scratch_release(&scratch);
}
