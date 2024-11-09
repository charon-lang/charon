#include "parser.h"

#include "lib/diag.h"
#include "parser/util.h"

#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>

static char *add_char(char *str, size_t str_size, char ch) {
    str = realloc(str, str_size);
    str[str_size - 1] = ch;
    return str;
}

static char *helper_string_escape(const char *src, size_t src_length, source_location_t source_location) {
    char *dest = NULL;
    int dest_index = 0;

    bool escaped = false;
    char escape_sequence[4] = {};
    int escape_sequence_length = 0;

    for(size_t i = 0; i < src_length; i++) {
        char c = src[i];

        if(!escaped) {
            if(c == '\\') {
                escaped = true;
            } else {
                dest = add_char(dest, ++dest_index, c);
            }
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
            if(errno == ERANGE || value > UINT8_MAX) diag_error(source_location, "numeric constant of escape sequence too large");
            dest = add_char(dest, ++dest_index, (char) value);

            escaped = false;
            escape_sequence_length = 0;
            memset(escape_sequence, '\0', sizeof(escape_sequence));
            continue;
        }

        switch(c) {
            case '\'': dest = add_char(dest, ++dest_index, '\''); break;
            case '"': dest = add_char(dest, ++dest_index, '\"'); break;
            case '?': dest = add_char(dest, ++dest_index, '\?'); break;
            case '\\': dest = add_char(dest, ++dest_index, '\\'); break;
            case 'a': dest = add_char(dest, ++dest_index, '\a'); break;
            case 'b': dest = add_char(dest, ++dest_index, '\b'); break;
            case 'f': dest = add_char(dest, ++dest_index, '\f'); break;
            case 'n': dest = add_char(dest, ++dest_index, '\n'); break;
            case 'r': dest = add_char(dest, ++dest_index, '\r'); break;
            case 't': dest = add_char(dest, ++dest_index, '\t'); break;
            case 'v': dest = add_char(dest, ++dest_index, '\v'); break;
            default:
                diag_warn(source_location, "unknown escape sequence `\\%c`", c);
                dest = add_char(dest, ++dest_index, c);
                break;
        }

        escaped = false;
    }

    dest = add_char(dest, ++dest_index, '\0');
    return dest;
}

static bool helper_tokens_match(token_t token, size_t count, va_list list) {
    for(size_t i = 0; i < count; i++) {
        token_kind_t type = va_arg(list, token_kind_t);
        if(type == token.kind) return true;
    }
    return false;
}

static hlir_node_t *helper_binary_operation(tokenizer_t *tokenizer, hlir_node_t *(*func)(tokenizer_t *), size_t count, ...) {
    hlir_node_t *left = func(tokenizer);
    va_list list;
    va_start(list, count);
    while(helper_tokens_match(tokenizer_peek(tokenizer), count, list)) {
        token_t token_operation = tokenizer_advance(tokenizer);
        hlir_node_binary_operation_t operation;
        switch(token_operation.kind) {
            case TOKEN_KIND_PLUS: operation = HLIR_NODE_BINARY_OPERATION_ADDITION; break;
            case TOKEN_KIND_MINUS: operation = HLIR_NODE_BINARY_OPERATION_SUBTRACTION; break;
            case TOKEN_KIND_STAR: operation = HLIR_NODE_BINARY_OPERATION_MULTIPLICATION; break;
            case TOKEN_KIND_SLASH: operation = HLIR_NODE_BINARY_OPERATION_DIVISION; break;
            case TOKEN_KIND_PERCENTAGE: operation = HLIR_NODE_BINARY_OPERATION_MODULO; break;
            case TOKEN_KIND_GREATER: operation = HLIR_NODE_BINARY_OPERATION_GREATER; break;
            case TOKEN_KIND_GREATER_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_GREATER_EQUAL; break;
            case TOKEN_KIND_LESS: operation = HLIR_NODE_BINARY_OPERATION_LESS; break;
            case TOKEN_KIND_LESS_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_LESS_EQUAL; break;
            case TOKEN_KIND_EQUAL_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_EQUAL; break;
            case TOKEN_KIND_NOT_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_NOT_EQUAL; break;
            default: diag_error(util_loc(tokenizer, token_operation), "expected a binary operator");
        }
        left = hlir_node_make_expr_binary(operation, left, func(tokenizer), util_loc(tokenizer, token_operation));
        va_end(list);
        va_start(list, count);
    }
    va_end(list);
    return left;
}

static hlir_node_t *parse_literal_numeric(tokenizer_t *tokenizer) {
    token_t token_numeric = tokenizer_advance(tokenizer);
    return hlir_node_make_expr_literal_numeric(util_number_make_from_token(tokenizer, token_numeric), util_loc(tokenizer, token_numeric));
}

static hlir_node_t *parse_literal_string(tokenizer_t *tokenizer) {
    token_t token_string = util_consume(tokenizer, TOKEN_KIND_CONST_STRING);
    char *text = util_text_make_from_token(tokenizer, token_string);
    char *value = helper_string_escape(&text[1], strlen(text) - 2, util_loc(tokenizer, token_string));
    free(text);
    return hlir_node_make_expr_literal_string(value, util_loc(tokenizer, token_string));
}

static hlir_node_t *parse_literal_string_raw(tokenizer_t *tokenizer) {
    token_t token_string = util_consume(tokenizer, TOKEN_KIND_CONST_STRING_RAW);
    char *text = util_text_make_from_token(tokenizer, token_string);
    char *value = strndup(text + 2, strlen(text) - 4);
    free(text);
    return hlir_node_make_expr_literal_string(value, util_loc(tokenizer, token_string));
}

static hlir_node_t *parse_literal_char(tokenizer_t *tokenizer) {
    token_t token_char = util_consume(tokenizer, TOKEN_KIND_CONST_CHAR);
    char *text = util_text_make_from_token(tokenizer, token_char);
    char *value = helper_string_escape(&text[1], strlen(text) - 2, util_loc(tokenizer, token_char));
    if(strlen(value) == 0) diag_error(util_loc(tokenizer, token_char), "zero characters in character literal");
    if(strlen(value) > 1) diag_error(util_loc(tokenizer, token_char), "multiple characters in character literal");
    char value_char = value[0];
    free(value);
    free(text);
    return hlir_node_make_expr_literal_char(value_char, util_loc(tokenizer, token_char));
}

static hlir_node_t *parse_literal_bool(tokenizer_t *tokenizer) {
    token_t token_bool = util_consume(tokenizer, TOKEN_KIND_CONST_BOOL);
    return hlir_node_make_expr_literal_bool(util_token_cmp(tokenizer, token_bool, "true") == 0, util_loc(tokenizer, token_bool));
}

static hlir_node_t *parse_identifier(tokenizer_t *tokenizer) {
    token_t token_name = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
    const char *name = util_text_make_from_token(tokenizer, token_name);

    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_DOUBLE_COLON:
            util_consume(tokenizer, TOKEN_KIND_DOUBLE_COLON);

            hlir_node_t *value = parse_identifier(tokenizer);
            return hlir_node_make_expr_selector(name, value, util_loc(tokenizer, token_name));
        case TOKEN_KIND_PARENTHESES_LEFT:
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);

            hlir_node_list_t arguments = HLIR_NODE_LIST_INIT;
            if(tokenizer_peek(tokenizer).kind != TOKEN_KIND_PARENTHESES_RIGHT) {
                do {
                    hlir_node_list_append(&arguments, parser_expr(tokenizer));
                } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
            }
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
            return hlir_node_make_expr_call(name, arguments, util_loc(tokenizer, token_name));
        default: return hlir_node_make_expr_variable(name, util_loc(tokenizer, token_name));
    }
    __builtin_unreachable();
}

static hlir_node_t *parse_primary(tokenizer_t *tokenizer) {
    token_t token = tokenizer_peek(tokenizer);
    switch(token.kind) {
        case TOKEN_KIND_PARENTHESES_LEFT:
            token_t token_left_paren = util_consume(tokenizer, TOKEN_KIND_PARENTHESES_LEFT);
            hlir_node_t *value = parser_expr(tokenizer);
            if(util_try_consume(tokenizer, TOKEN_KIND_COMMA)) {
                hlir_node_list_t values = HLIR_NODE_LIST_INIT;
                hlir_node_list_append(&values, value);
                do {
                    hlir_node_list_append(&values, parser_expr(tokenizer));
                } while(util_try_consume(tokenizer, TOKEN_KIND_COMMA));
                util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
                return hlir_node_make_expr_tuple(values, util_loc(tokenizer, token_left_paren));
            }
            util_consume(tokenizer, TOKEN_KIND_PARENTHESES_RIGHT);
            return value;
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
        default: diag_error(util_loc(tokenizer, token), "expected primary, got %s", token_kind_stringify(token.kind));
    }
    __builtin_unreachable();
}

static hlir_node_t *parse_unary_post(tokenizer_t *tokenizer) {
    hlir_node_t *value = parse_primary(tokenizer);
    source_location_t source_location = util_loc(tokenizer, tokenizer_peek(tokenizer));
    while(true) {
        if(util_try_consume(tokenizer, TOKEN_KIND_KEYWORD_AS)) {
            hlir_type_t *type = util_parse_type(tokenizer);
            value = hlir_node_make_expr_cast(value, type, source_location);
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_BRACKET_LEFT)) {
            hlir_node_t *index = parser_expr(tokenizer);
            util_consume(tokenizer, TOKEN_KIND_BRACKET_RIGHT);
            value = hlir_node_make_expr_subscript_index(value, index, source_location);
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_PERIOD)) {
            if(tokenizer_peek(tokenizer).kind == TOKEN_KIND_CONST_NUMBER_DEC) {
                token_t token_index = util_consume(tokenizer, TOKEN_KIND_CONST_NUMBER_DEC);
                char *text = util_text_make_from_token(tokenizer, token_index);
                uintmax_t index = strtoull(text, NULL, 10);
                if(errno == ERANGE) diag_error(util_loc(tokenizer, token_index), "index too large");
                free(text);
                value = hlir_node_make_expr_subscript_index_const(value, index, source_location);
            } else {
                token_t token_member = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
                value = hlir_node_make_expr_subscript_member(value, util_text_make_from_token(tokenizer, token_member), source_location);
            }
            continue;
        }
        if(util_try_consume(tokenizer, TOKEN_KIND_ARROW)) {
            token_t token_member = util_consume(tokenizer, TOKEN_KIND_IDENTIFIER);
            value = hlir_node_make_expr_subscript_member(hlir_node_make_expr_unary(HLIR_NODE_UNARY_OPERATION_DEREF, value, source_location), util_text_make_from_token(tokenizer, token_member), source_location);
            continue;
        }
        return value;
    }
}

static hlir_node_t *parse_unary_pre(tokenizer_t *tokenizer) {
    hlir_node_unary_operation_t operation;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_STAR: operation = HLIR_NODE_UNARY_OPERATION_DEREF; break;
        case TOKEN_KIND_MINUS: operation = HLIR_NODE_UNARY_OPERATION_NEGATIVE; break;
        case TOKEN_KIND_NOT: operation = HLIR_NODE_UNARY_OPERATION_NOT; break;
        case TOKEN_KIND_AMPERSAND: operation = HLIR_NODE_UNARY_OPERATION_REF; break;
        default: return parse_unary_post(tokenizer);
    }
    token_t token_operator = tokenizer_advance(tokenizer);
    return hlir_node_make_expr_unary(operation, parse_unary_pre(tokenizer), util_loc(tokenizer, token_operator));
}

static hlir_node_t *parse_factor(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_unary_pre, 3, TOKEN_KIND_STAR, TOKEN_KIND_SLASH, TOKEN_KIND_PERCENTAGE);
}

static hlir_node_t *parse_term(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_factor, 2, TOKEN_KIND_PLUS, TOKEN_KIND_MINUS);
}

static hlir_node_t *parse_comparison(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_term, 4, TOKEN_KIND_GREATER, TOKEN_KIND_GREATER_EQUAL, TOKEN_KIND_LESS, TOKEN_KIND_LESS_EQUAL);
}

static hlir_node_t *parse_equality(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_comparison, 2, TOKEN_KIND_EQUAL_EQUAL, TOKEN_KIND_NOT_EQUAL);
}

static hlir_node_t *parse_assignment(tokenizer_t *tokenizer) {
    hlir_node_t *left = parse_equality(tokenizer);
    hlir_node_binary_operation_t operation;
    switch(tokenizer_peek(tokenizer).kind) {
        case TOKEN_KIND_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_ASSIGN; break;
        case TOKEN_KIND_PLUS_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_ADDITION; break;
        case TOKEN_KIND_MINUS_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_SUBTRACTION; break;
        case TOKEN_KIND_STAR_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_MULTIPLICATION; break;
        case TOKEN_KIND_SLASH_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_DIVISION; break;
        case TOKEN_KIND_PERCENTAGE_EQUAL: operation = HLIR_NODE_BINARY_OPERATION_MODULO; break;
        default: return left;
    }
    token_t token_operation = tokenizer_advance(tokenizer);
    hlir_node_t *right = parse_assignment(tokenizer);
    if(operation != HLIR_NODE_BINARY_OPERATION_ASSIGN) right = hlir_node_make_expr_binary(operation, left, right, util_loc(tokenizer, token_operation));
    return hlir_node_make_expr_binary(HLIR_NODE_BINARY_OPERATION_ASSIGN, left, right, util_loc(tokenizer, token_operation));
}

hlir_node_t *parser_expr(tokenizer_t *tokenizer) {
    return parse_assignment(tokenizer);
}