#include "charon/diag.h"
#include "charon/node.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/parser.h"

#include <stdarg.h>
#include <string.h>

static void helper_binary_operation(charon_parser_t *parser, void (*func)(charon_parser_t *), size_t count, ...) {
    parser_event_t *checkpoint = parser_checkpoint(parser);
    func(parser);

    va_list list;
    va_start(list, count);
    while(parser_consume_try_many_list(parser, count, list)) {
        func(parser);

        parser_open_element_at(parser, checkpoint);
        parser_close_element(parser, CHARON_NODE_KIND_EXPR_BINARY);

        va_end(list);
        va_start(list, count);
    }
    va_end(list);
}

static void parse_numeric_literal(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume_many(parser, 4, CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN, CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC, CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT, CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX);

    parser_close_element(parser, CHARON_NODE_KIND_EXPR_LITERAL_NUMERIC);
}

static void parse_string_literal(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume_many(parser, 2, CHARON_TOKEN_KIND_LITERAL_STRING, CHARON_TOKEN_KIND_LITERAL_STRING_RAW);

    parser_close_element(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRING);
}

static void parse_char_literal(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_LITERAL_CHAR);

    parser_close_element(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRING);
}

static void parse_bool_literal(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_LITERAL_BOOL);

    parser_close_element(parser, CHARON_NODE_KIND_EXPR_LITERAL_BOOL);
}

static void parse_identifier(charon_parser_t *parser) {
    parser_open_element(parser);

    parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON)) {
        parse_identifier(parser);
        return parser_close_element(parser, CHARON_NODE_KIND_EXPR_SELECTOR);
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COLON)) {
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_CARET_LEFT);
        do {
            parse_type(parser);
        } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
        return parser_close_element(parser, CHARON_NODE_KIND_EXPR_VARIABLE);
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT)) {
        if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
            do {
                parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);
                parser_consume(parser, CHARON_TOKEN_KIND_PNCT_EQUAL);
                parse_expr(parser);
            } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);
        }
        return parser_close_element(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRUCT);
    }

    parser_close_element(parser, CHARON_NODE_KIND_EXPR_VARIABLE);
}

static void parse_primary(charon_parser_t *parser) {
    if(parser_peek(parser) == CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT) {
        parser_open_element(parser);
        parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
        parse_expr(parser);
        if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA)) {
            do {
                parse_expr(parser);
            } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
            return parser_close_element(parser, CHARON_NODE_KIND_EXPR_TUPLE);
        }
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_close_element(parser, CHARON_NODE_KIND_EXPR_PARENTHESES);
    }

    if(parser_peek(parser) == CHARON_TOKEN_KIND_KEYWORD_SIZEOF) {
        parser_open_element(parser);
        parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_SIZEOF);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
        parse_type(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_close_element(parser, CHARON_NODE_KIND_EXPR_SIZEOF);
    }

    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_IDENTIFIER:         parse_identifier(parser); break;
        case CHARON_TOKEN_KIND_LITERAL_STRING:
        case CHARON_TOKEN_KIND_LITERAL_STRING_RAW: parse_string_literal(parser); break;
        case CHARON_TOKEN_KIND_LITERAL_CHAR:       parse_char_literal(parser); break;
        case CHARON_TOKEN_KIND_LITERAL_BOOL:       parse_bool_literal(parser); break;
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN: parse_numeric_literal(parser); break;
        default:                                   {
            charon_token_kind_t kinds[9] = {
                CHARON_TOKEN_KIND_IDENTIFIER,         CHARON_TOKEN_KIND_LITERAL_STRING,     CHARON_TOKEN_KIND_LITERAL_STRING_RAW, CHARON_TOKEN_KIND_LITERAL_CHAR,       CHARON_TOKEN_KIND_LITERAL_BOOL,
                CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC, CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX, CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT, CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN,
            };

            charon_diag_data_t *diag_data = malloc(sizeof(charon_diag_data_t) + sizeof(kinds));
            diag_data->unexpected_token.found = parser_peek(parser);
            diag_data->unexpected_token.expected_count = sizeof(kinds) / sizeof(kinds[0]);
            memcpy(&diag_data->unexpected_token.expected, kinds, sizeof(kinds));

            parser_error(parser, CHARON_DIAG_UNEXPECTED_TOKEN, diag_data);
            break;
        }
    }
}

static void parse_unary_post(charon_parser_t *parser) {
    parser_event_t *checkpoint = parser_checkpoint(parser);

    parse_primary(parser);

repeat:
    if(parser_consume_try(parser, CHARON_TOKEN_KIND_KEYWORD_AS)) {
        parser_open_element_at(parser, checkpoint);

        parse_type(parser);

        parser_close_element(parser, CHARON_NODE_KIND_EXPR_CAST);
        goto repeat;
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT)) {
        parser_open_element_at(parser, checkpoint);

        parse_expr(parser);
        parser_consume(parser, CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT);

        parser_close_element(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT);
        goto repeat;
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        parser_open_element_at(parser, checkpoint);

        if(!parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT)) {
            do {
                parse_expr(parser);
            } while(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        }

        parser_close_element(parser, CHARON_NODE_KIND_EXPR_CALL);
        goto repeat;
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_PERIOD)) {
        parser_open_element_at(parser, checkpoint);

        switch(parser_peek(parser)) {
            case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:
            case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX:
            case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
            case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN: parse_numeric_literal(parser); break;
            default:                                   parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER); break;
        }

        parser_close_element(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT);
        goto repeat;
    }

    if(parser_consume_try(parser, CHARON_TOKEN_KIND_PNCT_ARROW)) {
        parser_open_element_at(parser, checkpoint);

        parser_consume(parser, CHARON_TOKEN_KIND_IDENTIFIER);

        parser_close_element(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT_DEREF);
        goto repeat;
    }
}

static void parse_unary_pre(charon_parser_t *parser) {
    parser_event_t *checkpoint = parser_checkpoint(parser);

    if(parser_consume_try_many(parser, 4, CHARON_TOKEN_KIND_PNCT_STAR, CHARON_TOKEN_KIND_PNCT_MINUS, CHARON_TOKEN_KIND_PNCT_NOT, CHARON_TOKEN_KIND_PNCT_AMPERSAND)) {
        parser_open_element_at(parser, checkpoint);
        parse_unary_pre(parser);
        parser_close_element(parser, CHARON_NODE_KIND_EXPR_UNARY);
    } else {
        parse_unary_post(parser);
    }
}

static void parse_factor(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_unary_pre, 3, CHARON_TOKEN_KIND_PNCT_STAR, CHARON_TOKEN_KIND_PNCT_SLASH, CHARON_TOKEN_KIND_PNCT_PERCENTAGE);
}

static void parse_term(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_factor, 2, CHARON_TOKEN_KIND_PNCT_PLUS, CHARON_TOKEN_KIND_PNCT_MINUS);
}

static void parse_shift(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_term, 2, CHARON_TOKEN_KIND_PNCT_SHIFT_LEFT, CHARON_TOKEN_KIND_PNCT_SHIFT_RIGHT);
}

static void parse_comparison(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_shift, 4, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT, CHARON_TOKEN_KIND_PNCT_GREATER_EQUAL, CHARON_TOKEN_KIND_PNCT_CARET_LEFT, CHARON_TOKEN_KIND_PNCT_LESS_EQUAL);
}

static void parse_equality(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_comparison, 2, CHARON_TOKEN_KIND_PNCT_EQUAL_EQUAL, CHARON_TOKEN_KIND_PNCT_NOT_EQUAL);
}

static void parse_bitwise_and(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_equality, 1, CHARON_TOKEN_KIND_PNCT_AMPERSAND);
}

static void parse_bitwise_xor(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_bitwise_and, 1, CHARON_TOKEN_KIND_PNCT_CARET);
}

static void parse_bitwise_or(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_bitwise_xor, 1, CHARON_TOKEN_KIND_PNCT_PIPE);
}

static void parse_logical_and(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_bitwise_or, 1, CHARON_TOKEN_KIND_PNCT_LOGICAL_AND);
}

static void parse_logical_or(charon_parser_t *parser) {
    helper_binary_operation(parser, parse_logical_and, 1, CHARON_TOKEN_KIND_PNCT_LOGICAL_OR);
}

static void parse_assignment(charon_parser_t *parser) {
    parser_event_t *checkpoint = parser_checkpoint(parser);

    parse_logical_or(parser);
    if(parser_consume_try_many(
           parser,
           6,
           CHARON_TOKEN_KIND_PNCT_EQUAL,
           CHARON_TOKEN_KIND_PNCT_PLUS_EQUAL,
           CHARON_TOKEN_KIND_PNCT_MINUS_EQUAL,
           CHARON_TOKEN_KIND_PNCT_STAR_EQUAL,
           CHARON_TOKEN_KIND_PNCT_SLASH_EQUAL,
           CHARON_TOKEN_KIND_PNCT_PERCENTAGE_EQUAL
       ))
    {
        parser_open_element_at(parser, checkpoint);
        parse_assignment(parser);
        parser_close_element(parser, CHARON_NODE_KIND_EXPR_BINARY);
    }
}

void parse_expr(charon_parser_t *parser) {
    parse_assignment(parser);
}
