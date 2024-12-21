#include "parser.h"

#include "lib/diag.h"
#include "lib/alloc.h"
#include "parser/util.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
    char *data;
    size_t data_length;
} string_t;

static void string_append_char(string_t *str, char ch) {
    str->data = alloc_resize(str->data, ++str->data_length);
    str->data[str->data_length - 1] = ch;
}

static string_t helper_string_escape(source_location_t source_location, const char *src, size_t src_length) {
    string_t dest = { .data = NULL, .data_length = 0 };

    bool escaped = false;
    char escape_sequence[4] = {};
    int escape_sequence_length = 0;

    for(size_t i = 0; i < src_length; i++) {
        char c = src[i];

        if(!escaped) {
            if(c == '\\') {
                escaped = true;
                continue;
            }
            string_append_char(&dest, c);
            continue;
        }

        if(c >= '0' && c <= '9') {
            escape_sequence[escape_sequence_length++] = c;
            if(escape_sequence_length < 3 && i != src_length - 1) continue;
            goto write_escape;
        }

        if(escape_sequence_length > 0) {
            i--;
            write_escape:
            uintmax_t value = strtoull(escape_sequence, NULL, 10);
            if(errno == ERANGE || value > UINT8_MAX) diag_error(source_location, LANG_E_TOO_LARGE_ESCAPE_SEQUENCE);
            string_append_char(&dest, (char) value);

            escaped = false;
            escape_sequence_length = 0;
            memset(escape_sequence, '\0', sizeof(escape_sequence));
            continue;
        }

        switch(c) {
            case '\'': string_append_char(&dest, '\''); break;
            case '"': string_append_char(&dest, '\"'); break;
            case '?': string_append_char(&dest, '\?'); break;
            case '\\': string_append_char(&dest, '\\'); break;
            case 'a': string_append_char(&dest, '\a'); break;
            case 'b': string_append_char(&dest, '\b'); break;
            case 'f': string_append_char(&dest, '\f'); break;
            case 'n': string_append_char(&dest, '\n'); break;
            case 'r': string_append_char(&dest, '\r'); break;
            case 't': string_append_char(&dest, '\t'); break;
            case 'v': string_append_char(&dest, '\v'); break;
            default:
                diag_warn(source_location, LANG_E_UNKNOWN_ESCAPE_SEQUENCE, c);
                string_append_char(&dest, c);
                break;
        }

        escaped = false;
    }

    string_append_char(&dest, '\0');
    return dest;
}

static bool helper_tokens_match(token_t token, size_t count, va_list list) {
    for(size_t i = 0; i < count; i++) {
        token_kind_t type = va_arg(list, token_kind_t);
        if(type == token.kind) return true;
    }
    return false;
}

static ast_node_t *helper_binary_operation(tokenizer_t *tokenizer, ast_node_t *(*func)(tokenizer_t *), size_t count, ...) {
    ast_node_t *left = func(tokenizer);
    va_list list;
    va_start(list, count);
    while(helper_tokens_match(tokenizer_peek(tokenizer), count, list)) {
        token_t token_operation = tokenizer_advance(tokenizer);
        ast_node_binary_operation_t operation;
        switch(token_operation.kind) {
            case TOKEN_KIND_PLUS: operation = AST_NODE_BINARY_OPERATION_ADDITION; break;
            case TOKEN_KIND_MINUS: operation = AST_NODE_BINARY_OPERATION_SUBTRACTION; break;
            case TOKEN_KIND_STAR: operation = AST_NODE_BINARY_OPERATION_MULTIPLICATION; break;
            case TOKEN_KIND_SLASH: operation = AST_NODE_BINARY_OPERATION_DIVISION; break;
            case TOKEN_KIND_PERCENTAGE: operation = AST_NODE_BINARY_OPERATION_MODULO; break;
            case TOKEN_KIND_CARET_RIGHT: operation = AST_NODE_BINARY_OPERATION_GREATER; break;
            case TOKEN_KIND_GREATER_EQUAL: operation = AST_NODE_BINARY_OPERATION_GREATER_EQUAL; break;
            case TOKEN_KIND_CARET_LEFT: operation = AST_NODE_BINARY_OPERATION_LESS; break;
            case TOKEN_KIND_LESS_EQUAL: operation = AST_NODE_BINARY_OPERATION_LESS_EQUAL; break;
            case TOKEN_KIND_EQUAL_EQUAL: operation = AST_NODE_BINARY_OPERATION_EQUAL; break;
            case TOKEN_KIND_NOT_EQUAL: operation = AST_NODE_BINARY_OPERATION_NOT_EQUAL; break;
            case TOKEN_KIND_LOGICAL_AND: operation = AST_NODE_BINARY_OPERATION_LOGICAL_AND; break;
            case TOKEN_KIND_LOGICAL_OR: operation = AST_NODE_BINARY_OPERATION_LOGICAL_OR; break;
            case TOKEN_KIND_SHIFT_LEFT: operation = AST_NODE_BINARY_OPERATION_SHIFT_LEFT; break;
            case TOKEN_KIND_SHIFT_RIGHT: operation = AST_NODE_BINARY_OPERATION_SHIFT_RIGHT; break;
            case TOKEN_KIND_AMPERSAND: operation = AST_NODE_BINARY_OPERATION_AND; break;
            case TOKEN_KIND_PIPE: operation = AST_NODE_BINARY_OPERATION_OR; break;
            case TOKEN_KIND_CARET: operation = AST_NODE_BINARY_OPERATION_XOR; break;
            default: diag_error(util_loc(tokenizer, token_operation), LANG_E_EXPECTED_BINARY_OP, token_kind_stringify(token_operation.kind));
        }
        left = ast_node_make_expr_binary(operation, left, func(tokenizer), util_loc(tokenizer, token_operation));
        va_end(list);
        va_start(list, count);
    }
    va_end(list);
    return left;
}

static ast_node_t *parse_literal_numeric(tokenizer_t *tokenizer) {
    token_t token_numeric = tokenizer_advance(tokenizer);
    return ast_node_make_expr_literal_numeric(util_number_make_from_token(tokenizer, token_numeric), util_loc(tokenizer, token_numeric));
}

static ast_node_t *parse_literal_string(tokenizer_t *tokenizer) {
    token_t token_string = util_consume(tokenizer, TOKEN_KIND_CONST_STRING);
    char *text = util_text_make_from_token_inset(tokenizer, token_string, 1);
    string_t value = helper_string_escape(util_loc(tokenizer, token_string), text, strlen(text));
    alloc_free(text);
    return ast_node_make_expr_literal_string(value.data, util_loc(tokenizer, token_string));
}

static ast_node_t *parse_literal_string_raw(tokenizer_t *tokenizer) {
    token_t token_string = util_consume(tokenizer, TOKEN_KIND_CONST_STRING_RAW);
    char *value = util_text_make_from_token_inset(tokenizer, token_string, 2);
    return ast_node_make_expr_literal_string(value, util_loc(tokenizer, token_string));
}

static ast_node_t *parse_literal_char(tokenizer_t *tokenizer) {
    token_t token_char = util_consume(tokenizer, TOKEN_KIND_CONST_CHAR);
    char *text = util_text_make_from_token_inset(tokenizer, token_char, 1);
    string_t value = helper_string_escape(util_loc(tokenizer, token_char), text, strlen(text));
    if(value.data_length == 0) diag_error(util_loc(tokenizer, token_char), LANG_E_EMPTY_CHAR_LITERAL);
    if(value.data_length - 1 > 1) diag_error(util_loc(tokenizer, token_char), LANG_E_TOO_LARGE_CHAR_LITERAL);
    char value_char = value.data[0];
    alloc_free(value.data);
    alloc_free(text);
    return ast_node_make_expr_literal_char(value_char, util_loc(tokenizer, token_char));
}

static ast_node_t *parse_literal_bool(tokenizer_t *tokenizer) {
    token_t token_bool = util_consume(tokenizer, TOKEN_KIND_CONST_BOOL);
    return ast_node_make_expr_literal_bool(util_token_cmp(tokenizer, token_bool, "true") == 0, util_loc(tokenizer, token_bool));
}

static ast_node_t *parse_identifier(tokenizer_t *tokenizer) {
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    const char *name = util_text_make_from_token(tokenizer, token_name);

    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_DOUBLE_COLON:
            util_consume(tokenizer, TOKEN_KIND_DOUBLE_COLON);

            ast_node_t *value = parse_identifier(tokenizer);
            return ast_node_make_expr_selector(name, value, util_loc(tokenizer, token_name));
        default: return ast_node_make_expr_variable(name, util_loc(tokenizer, token_name));
    }
    __builtin_unreachable();
}

static ast_node_t *parse_primary(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_PARENTHESES_LEFT: {
            token_t token_left_paren = util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
            ast_node_t *value = parser_expr(tokenizer);
            if(util_try_consume(tokenizer, TOKEN_KIND_COMMA)) {
                ast_node_list_t values = AST_NODE_LIST_INIT;
                ast_node_list_append(&values, value);
                do {
                    ast_node_list_append(&values, parser_expr(tokenizer));
                } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
                util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
                return ast_node_make_expr_tuple(values, util_loc(tokenizer, token_left_paren));
            }
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
            return value;
        }
        case TOKEN_KIND_IDENTIFIER: return parse_identifier(tokenizer);
        case TOKEN_KIND_CONST_STRING: return parse_literal_string(tokenizer);
        case TOKEN_KIND_CONST_STRING_RAW: return parse_literal_string_raw(tokenizer);
        case TOKEN_KIND_CONST_CHAR: return parse_literal_char(tokenizer);
        case TOKEN_KIND_CONST_BOOL: return parse_literal_bool(tokenizer);
        case TOKEN_KIND_CONST_NUMBER_DEC:
        case TOKEN_KIND_CONST_NUMBER_HEX:
        case TOKEN_KIND_CONST_NUMBER_BIN:
        case TOKEN_KIND_CONST_NUMBER_OCT:
            return parse_literal_numeric(tokenizer);
        case TOKEN_KIND_KEYWORD_SIZEOF: {
            source_location_t source_location = util_loc(tokenizer, util_consume(tokenizer, TOKEN_KIND_KEYWORD_SIZEOF));
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
            ast_type_t *type = util_parse_type(tokenizer);
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
            return ast_node_make_expr_sizeof(type, source_location);
        }
        default: diag_error(util_loc(tokenizer, token), LANG_E_EXPECTED_PRIMARY, token_kind_stringify(token.kind));
    }
    __builtin_unreachable();
}

static ast_node_t *parse_unary_post(tokenizer_t *tokenizer) {
    ast_node_t *value = parse_primary(tokenizer);
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));
    while(true) {
        if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_AS)) {
            ast_type_t *type = util_parse_type(tokenizer);
            value = ast_node_make_expr_cast(value, type, source_location);
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_BRACKET_LEFT)) {
            ast_node_t *index = parser_expr(tokenizer);
            util_consume(tokenizer, TOKEN_KIND_BRACKET_RIGHT);
            value = ast_node_make_expr_subscript_index(value, index, source_location);
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT)) {
            ast_node_list_t arguments = AST_NODE_LIST_INIT;
            if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_PARENTHESES_RIGHT) {
                do {
                    ast_node_list_append(&arguments, parser_expr(tokenizer));
                } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
            }
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
            value = ast_node_make_expr_call(value, arguments, source_location);
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_PERIOD)) {
            if(tokenizer_peek(tokenizer).kind == TOKEN_KIND_CONST_NUMBER_DEC) {
                token_t token_index = util_consume(tokenizer, TOKEN_KIND_CONST_NUMBER_DEC);
                uintmax_t index = util_number_make_from_token(tokenizer, token_index);
                value = ast_node_make_expr_subscript_index_const(value, index, source_location);
            } else {
                token_t token_member = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
                value = ast_node_make_expr_subscript_member(value, util_text_make_from_token(tokenizer, token_member), source_location);
            }
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_ARROW)) {
            token_t token_member = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            value = ast_node_make_expr_subscript_member(ast_node_make_expr_unary(AST_NODE_UNARY_OPERATION_DEREF, value, source_location), util_text_make_from_token(tokenizer, token_member), source_location);
            continue;
        }
        return value;
    }
}

static ast_node_t *parse_unary_pre(tokenizer_t *tokenizer) {
    ast_node_unary_operation_t operation;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_STAR: operation = AST_NODE_UNARY_OPERATION_DEREF; break;
        case TOKEN_KIND_MINUS: operation = AST_NODE_UNARY_OPERATION_NEGATIVE; break;
        case TOKEN_KIND_NOT: operation = AST_NODE_UNARY_OPERATION_NOT; break;
        case TOKEN_KIND_AMPERSAND: operation = AST_NODE_UNARY_OPERATION_REF; break;
        default: return parse_unary_post(tokenizer);
    }
    token_t token_operator = tokenizer_advance(tokenizer);
    return ast_node_make_expr_unary(operation, parse_unary_pre(tokenizer), util_loc(tokenizer, token_operator));
}

static ast_node_t *parse_factor(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_unary_pre, 3, TOKEN_KIND_STAR, TOKEN_KIND_SLASH, TOKEN_KIND_PERCENTAGE);
}

static ast_node_t *parse_term(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_factor, 2, TOKEN_KIND_PLUS, TOKEN_KIND_MINUS);
}

static ast_node_t *parse_shift(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_term, 2, TOKEN_KIND_SHIFT_LEFT, TOKEN_KIND_SHIFT_RIGHT);
}

static ast_node_t *parse_comparison(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_shift, 4, TOKEN_KIND_CARET_RIGHT, TOKEN_KIND_GREATER_EQUAL, TOKEN_KIND_CARET_LEFT, TOKEN_KIND_LESS_EQUAL);
}

static ast_node_t *parse_equality(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_comparison, 2, TOKEN_KIND_EQUAL_EQUAL, TOKEN_KIND_NOT_EQUAL);
}

static ast_node_t *parse_bitwise_and(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_equality, 1, TOKEN_KIND_AMPERSAND);
}

static ast_node_t *parse_bitwise_xor(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_bitwise_and, 1, TOKEN_KIND_CARET);
}

static ast_node_t *parse_bitwise_or(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_bitwise_xor, 1, TOKEN_KIND_PIPE);
}

static ast_node_t *parse_logical_and(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_bitwise_or, 1, TOKEN_KIND_LOGICAL_AND);
}

static ast_node_t *parse_logical_or(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_logical_and, 1, TOKEN_KIND_LOGICAL_OR);
}

static ast_node_t *parse_assignment(tokenizer_t *tokenizer) {
    ast_node_t *left = parse_logical_or(tokenizer);
    ast_node_binary_operation_t operation;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_EQUAL: operation = AST_NODE_BINARY_OPERATION_ASSIGN; break;
        case TOKEN_KIND_PLUS_EQUAL: operation = AST_NODE_BINARY_OPERATION_ADDITION; break;
        case TOKEN_KIND_MINUS_EQUAL: operation = AST_NODE_BINARY_OPERATION_SUBTRACTION; break;
        case TOKEN_KIND_STAR_EQUAL: operation = AST_NODE_BINARY_OPERATION_MULTIPLICATION; break;
        case TOKEN_KIND_SLASH_EQUAL: operation = AST_NODE_BINARY_OPERATION_DIVISION; break;
        case TOKEN_KIND_PERCENTAGE_EQUAL: operation = AST_NODE_BINARY_OPERATION_MODULO; break;
        default: return left;
    }
    token_t token_operation = tokenizer_advance(tokenizer);
    ast_node_t *right = parse_assignment(tokenizer);
    if(operation != AST_NODE_BINARY_OPERATION_ASSIGN) right = ast_node_make_expr_binary(operation, left, right, util_loc(tokenizer, token_operation));
    return ast_node_make_expr_binary(AST_NODE_BINARY_OPERATION_ASSIGN, left, right, util_loc(tokenizer, token_operation));
}

ast_node_t *parser_expr(tokenizer_t *tokenizer) {
    return parse_assignment(tokenizer);
}