#include "util.h"

#include "charon/diag.h"
#include "constants.h"
#include "lexer/token.h"
#include "lib/alloc.h"
#include "lib/context.h"
#include "lib/diag.h"
#include "lib/source.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

source_location_t util_loc(tokenizer_t *tokenizer, token_t token) {
    return (source_location_t) { .source = tokenizer->source, .offset = token.offset, .length = token.size };
}

bool util_try_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    if(tokenizer_peek(tokenizer).kind == kind) {
        tokenizer_advance(tokenizer);
        return true;
    }
    return false;
}

token_t util_consume(tokenizer_t *tokenizer, token_kind_t kind) {
    token_t token = tokenizer_advance(tokenizer);
    if(token.kind == kind) return token;
    g_global_context->recovery.associated_diag = diag(
        util_loc(tokenizer, token),
        (diag_t) {
            .type = DIAG_TYPE__EXPECTED,
            .data.expected = { .expected = kind, .actual = token.kind }
    }
    );
    context_recover_to_boundary();
}

char *util_text_make_from_token_inset(tokenizer_t *tokenizer, token_t token, size_t inset) {
    char *text = alloc(token.size + 1 - inset * 2);
    memcpy(text, tokenizer->source->data_buffer + token.offset + inset, token.size - inset * 2);
    text[token.size - inset * 2] = '\0';
    return text;
}

char *util_text_make_from_token(tokenizer_t *tokenizer, token_t token) {
    return util_text_make_from_token_inset(tokenizer, token, 0);
}

int util_token_cmp(tokenizer_t *tokenizer, token_t token, const char *string) {
    return strncmp(tokenizer->source->data_buffer + token.offset, string, token.size);
}

uintmax_t util_number_make_from_token(tokenizer_t *tokenizer, token_t token) {
    int base;
    switch(token.kind) {
        case TOKEN_KIND_CONST_NUMBER_DEC: base = 10; break;
        case TOKEN_KIND_CONST_NUMBER_HEX: base = 16; break;
        case TOKEN_KIND_CONST_NUMBER_BIN: base = 2; break;
        case TOKEN_KIND_CONST_NUMBER_OCT: base = 8; break;
        default:                          {
            g_global_context->recovery.associated_diag = diag(util_loc(tokenizer, token), (diag_t) { .type = DIAG_TYPE__EXPECTED_NUMERIC_LITERAL });
            context_recover_to_boundary();
        }
    }
    char *text = util_text_make_from_token(tokenizer, token);
    errno = 0;
    char *stripped = text;
    if(base != 10) stripped += 2;
    uintmax_t value = strtoull(stripped, NULL, base);
    if(errno == ERANGE) {
        g_global_context->recovery.associated_diag = diag(util_loc(tokenizer, token), (diag_t) { .type = DIAG_TYPE__TOO_LARGE_NUMERIC_CONSTANT });
        context_recover_to_boundary();
    }
    alloc_free(text);
    return value;
}

ast_type_t *util_parse_type(tokenizer_t *tokenizer) {
    ast_attribute_list_t attributes = util_parse_ast_attributes(tokenizer);

    if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_STRUCT)) {
        util_consume(tokenizer, TOKEN_KIND_BRACE_LEFT);

        size_t member_count = 0;
        ast_type_structure_member_t *members = NULL;
        if(!util_try_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT)) {
            do {
                token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
                util_consume(tokenizer, TOKEN_KIND_COLON);
                ast_type_t *type = util_parse_type(tokenizer);

                members = alloc_array(members, ++member_count, sizeof(ast_type_structure_member_t));
                members[member_count - 1] = (ast_type_structure_member_t) { .type = type, .name = util_text_make_from_token(tokenizer, token_identifier) };
            } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));

            util_consume(tokenizer, TOKEN_KIND_BRACE_RIGHT);
        }
        return ast_type_structure_make(member_count, members, attributes);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
        size_t count = 0;
        ast_type_t **types = NULL;
        do {
            types = alloc_array(types, ++count, sizeof(ast_type_t *));
            types[count - 1] = util_parse_type(tokenizer);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
        util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
        return ast_type_tuple_make(count, types, attributes);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_BRACKET_LEFT)) {
        uintmax_t size = util_number_make_from_token(tokenizer, tokenizer_advance(tokenizer));
        util_consume(tokenizer, TOKEN_KIND_BRACKET_RIGHT);
        return ast_type_array_make(util_parse_type(tokenizer), size, attributes);
    }
    if(util_try_consume(tokenizer, TOKEN_KIND_STAR)) return ast_type_pointer_make(util_parse_type(tokenizer), attributes);
    if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_FUNCTION)) return ast_type_function_reference_make(util_parse_function_type(tokenizer, NULL), attributes);

    token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    size_t module_count = 0;
    const char **modules = NULL;
    while(util_try_consume(tokenizer, TOKEN_KIND_DOUBLE_COLON)) {
        modules = alloc_array(modules, ++module_count, sizeof(const char *));
        modules[module_count - 1] = util_text_make_from_token(tokenizer, token_identifier);
        token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    }

    if(util_try_consume(tokenizer, TOKEN_KIND_CARET_LEFT)) {
        size_t type_count = 0;
        ast_type_t **types = NULL;
        do {
            types = alloc_array(types, ++type_count, sizeof(ast_type_t *));
            types[type_count - 1] = util_parse_type(tokenizer);
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));

        util_consume(tokenizer, TOKEN_KIND_CARET_RIGHT);
        return ast_type_reference_make(util_text_make_from_token(tokenizer, token_identifier), module_count, modules, type_count, types, attributes);
    }

    if(util_token_cmp(tokenizer, token_identifier, "bool") == 0) return ast_type_integer_make(1, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "char") == 0) return ast_type_integer_make(CONSTANTS_CHAR_SIZE, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "paddr") == 0) return ast_type_integer_make(CONSTANTS_ADDRESS_SIZE, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "vaddr") == 0) {
    type_vaddr:
        ast_attribute_add(&attributes, "allow_coerce_pointer", NULL, 0, util_loc(tokenizer, token_identifier));
        return ast_type_integer_make(CONSTANTS_ADDRESS_SIZE, false, attributes);
    }
    if(util_token_cmp(tokenizer, token_identifier, "ptr") == 0) goto type_vaddr;
    if(util_token_cmp(tokenizer, token_identifier, "size") == 0) return ast_type_integer_make(CONSTANTS_SIZE_SIZE, false, attributes);

    if(util_token_cmp(tokenizer, token_identifier, "uint") == 0) return ast_type_integer_make(CONSTANTS_INT_SIZE, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u8") == 0) return ast_type_integer_make(8, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u16") == 0) return ast_type_integer_make(16, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u32") == 0) return ast_type_integer_make(32, false, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "u64") == 0) return ast_type_integer_make(64, false, attributes);

    if(util_token_cmp(tokenizer, token_identifier, "int") == 0) return ast_type_integer_make(CONSTANTS_INT_SIZE, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i8") == 0) return ast_type_integer_make(8, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i16") == 0) return ast_type_integer_make(16, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i32") == 0) return ast_type_integer_make(32, true, attributes);
    if(util_token_cmp(tokenizer, token_identifier, "i64") == 0) return ast_type_integer_make(64, true, attributes);

    return ast_type_reference_make(util_text_make_from_token(tokenizer, token_identifier), module_count, modules, 0, NULL, attributes);
}

ast_type_function_t *util_parse_function_type(tokenizer_t *tokenizer, const char ***argument_names) {
    bool varargs = false;
    ast_type_t **arguments = NULL;
    size_t argument_count = 0;
    ast_type_t *return_type = ast_type_void_make(AST_ATTRIBUTE_LIST_INIT);

    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
    if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_PARENTHESES_RIGHT) {
        do {
            if(util_try_consume(tokenizer, TOKEN_KIND_TRIPLE_PERIOD)) {
                varargs = true;
                break;
            }

            token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            util_consume(tokenizer, TOKEN_KIND_COLON);

            arguments = alloc_array(arguments, ++argument_count, sizeof(ast_type_t *));
            arguments[argument_count - 1] = util_parse_type(tokenizer);
            if(argument_names != NULL) {
                *argument_names = alloc_array(*argument_names, argument_count, sizeof(char *));
                (*argument_names)[argument_count - 1] = util_text_make_from_token(tokenizer, token_identifier);
            }
        } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
    }
    util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);

    if(util_try_consume(tokenizer, TOKEN_KIND_COLON)) return_type = util_parse_type(tokenizer);

    return ast_type_function_make(argument_count, arguments, varargs, return_type);
}

ast_attribute_list_t util_parse_ast_attributes(tokenizer_t *tokenizer) {
    ast_attribute_list_t list = AST_ATTRIBUTE_LIST_INIT;

    while(util_try_consume(tokenizer, TOKEN_KIND_AT)) {
        token_t token_identifier = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);

        size_t argument_count = 0;
        ast_attribute_argument_t *arguments = NULL;
        if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
            do {
                arguments = alloc_array(arguments, ++argument_count, sizeof(ast_attribute_argument_type_t));
                token_t token = tokenizer_peek(tokenizer);
                switch(token.kind) {
                    case TOKEN_KIND_CONST_STRING: {
                        token_t string_token = util_consume(tokenizer, TOKEN_KIND_CONST_STRING);
                        char *value = util_text_make_from_token_inset(tokenizer, string_token, 1);
                        arguments[argument_count - 1] = (ast_attribute_argument_t) { .type = AST_ATTRIBUTE_ARGUMENT_TYPE_STRING, .value.string = value };
                        break;
                    }
                    case TOKEN_KIND_CONST_NUMBER_DEC:
                    case TOKEN_KIND_CONST_NUMBER_BIN:
                    case TOKEN_KIND_CONST_NUMBER_HEX:
                    case TOKEN_KIND_CONST_NUMBER_OCT: {
                        token_t number_token = tokenizer_advance(tokenizer);
                        uintmax_t value = util_number_make_from_token(tokenizer, number_token);
                        arguments[argument_count - 1] = (ast_attribute_argument_t) { .type = AST_ATTRIBUTE_ARGUMENT_TYPE_NUMBER, .value.number = value };
                        break;
                    }
                    default: {
                        diag(util_loc(tokenizer, token), (diag_t) { .type = DIAG_TYPE__EXPECTED_ATTRIBUTE_ARGUMENT });
                        continue;
                    }
                }
            } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
        }
        ast_attribute_add(&list, util_text_make_from_token(tokenizer, token_identifier), arguments, argument_count, util_loc(tokenizer, token_identifier));
    }

    return list;
}
