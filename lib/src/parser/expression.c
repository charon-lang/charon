#include "charon/element.h"
#include "charon/node.h"
#include "charon/token.h"
#include "parse.h"
#include "parser/collector.h"
#include "parser/parser.h"

#include <stdarg.h>

static bool helper_tokens_match_list(charon_token_kind_t kind, size_t count, va_list list) {
    for(size_t i = 0; i < count; i++) {
        charon_token_kind_t valid_kind = va_arg(list, charon_token_kind_t);
        if(kind == valid_kind) return true;
    }
    return false;
}

static bool helper_tokens_match(charon_token_kind_t kind, size_t count, ...) {
    va_list list;
    va_start(list, count);
    bool res = helper_tokens_match_list(kind, count, list);
    va_end(list);
    return res;
}

static charon_element_inner_t *helper_binary_operation(charon_parser_t *parser, charon_element_inner_t *(*func)(charon_parser_t *), size_t count, ...) {
    charon_element_inner_t *left = func(parser);

    va_list list;
    va_start(list, count);
    while(helper_tokens_match_list(parser_peek(parser), count, list)) {
        collector_t collector = COLLECTOR_INIT;

        collector_push(&collector, left);
        parser_consume_any(parser, &collector);
        collector_push(&collector, func(parser));

        left = parser_build(parser, CHARON_NODE_KIND_EXPR_BINARY, &collector);

        va_end(list);
        va_start(list, count);
    }
    va_end(list);
    return left;
}

static charon_element_inner_t *parse_numeric_literal(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX: parser_consume_any(parser, &collector); break;
        default:                                   return parser_build(parser, CHARON_NODE_KIND_ERROR, &collector);
    }
    return parser_build(parser, CHARON_NODE_KIND_EXPR_LITERAL_NUMERIC, &collector);
}

static charon_element_inner_t *parse_string_literal(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_LITERAL_STRING:
        case CHARON_TOKEN_KIND_LITERAL_STRING_RAW: parser_consume_any(parser, &collector); break;
        default:                                   return parser_build(parser, CHARON_NODE_KIND_ERROR, &collector);
    }
    return parser_build(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRING, &collector);
}

static charon_element_inner_t *parse_char_literal(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_LITERAL_CHAR);
    return parser_build(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRING, &collector);
}

static charon_element_inner_t *parse_bool_literal(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    parser_consume(parser, &collector, CHARON_TOKEN_KIND_LITERAL_BOOL);
    return parser_build(parser, CHARON_NODE_KIND_EXPR_LITERAL_BOOL, &collector);
}

static charon_element_inner_t *parse_identifier(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;

    parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_DOUBLE_COLON)) {
        collector_push(&collector, parse_identifier(parser));
        return parser_build(parser, CHARON_NODE_KIND_EXPR_SELECTOR, &collector);
    }

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COLON)) {
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_LEFT);
        do {
            collector_push(&collector, parse_type(parser));
        } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT);
        return parser_build(parser, CHARON_NODE_KIND_EXPR_VARIABLE, &collector);
    }

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_LEFT)) {
        if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT)) {
            do {
                parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
                parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_EQUAL);
                collector_push(&collector, parse_expr(parser));
            } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACE_RIGHT);
        }
        return parser_build(parser, CHARON_NODE_KIND_EXPR_LITERAL_STRUCT, &collector);
    }

    return parser_build(parser, CHARON_NODE_KIND_EXPR_VARIABLE, &collector);
}

static charon_element_inner_t *parse_primary(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
        collector_push(&collector, parse_expr(parser));
        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA)) {
            do {
                collector_push(&collector, parse_expr(parser));
            } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
            return parser_build(parser, CHARON_NODE_KIND_EXPR_TUPLE, &collector);
        }
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_build(parser, CHARON_NODE_KIND_EXPR_PARENTHESES, &collector);
    }

    if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_SIZEOF)) {
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT);
        collector_push(&collector, parse_type(parser));
        parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
        return parser_build(parser, CHARON_NODE_KIND_EXPR_SIZEOF, &collector);
    }

    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_IDENTIFIER:         return parse_identifier(parser);
        case CHARON_TOKEN_KIND_LITERAL_STRING:
        case CHARON_TOKEN_KIND_LITERAL_STRING_RAW: return parse_string_literal(parser);
        case CHARON_TOKEN_KIND_LITERAL_CHAR:       return parse_char_literal(parser);
        case CHARON_TOKEN_KIND_LITERAL_BOOL:       return parse_bool_literal(parser);
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
        case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN: return parse_numeric_literal(parser);
        default:                                   parser_consume_any(parser, &collector); return parser_build(parser, CHARON_NODE_KIND_ERROR, &collector);
    }
}

static charon_element_inner_t *parse_unary_post(charon_parser_t *parser) {
    charon_element_inner_t *value = parse_primary(parser);
    while(true) {
        collector_t collector = COLLECTOR_INIT;
        collector_push(&collector, value);

        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_KEYWORD_AS)) {
            collector_push(&collector, parse_type(parser));
            value = parser_build(parser, CHARON_NODE_KIND_EXPR_CAST, &collector);
            continue;
        }

        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACKET_LEFT)) {
            collector_push(&collector, parse_expr(parser));
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_BRACKET_RIGHT);
            value = parser_build(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT, &collector);
            continue;
        }

        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_LEFT)) {
            if(!parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT)) {
                do {
                    collector_push(&collector, parse_expr(parser));
                } while(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_COMMA));
                parser_consume(parser, &collector, CHARON_TOKEN_KIND_PNCT_PARENTHESES_RIGHT);
            }
            value = parser_build(parser, CHARON_NODE_KIND_EXPR_CALL, &collector);
            continue;
        }

        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_PERIOD)) {
            switch(parser_peek(parser)) {
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_DEC:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_HEX:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_OCT:
                case CHARON_TOKEN_KIND_LITERAL_NUMBER_BIN: parse_numeric_literal(parser); break;
                default:                                   parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER); break;
            }
            value = parser_build(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT, &collector);
            continue;
        }

        if(parser_consume_try(parser, &collector, CHARON_TOKEN_KIND_PNCT_ARROW)) {
            parser_consume(parser, &collector, CHARON_TOKEN_KIND_IDENTIFIER);
            value = parser_build(parser, CHARON_NODE_KIND_EXPR_SUBSCRIPT_DEREF, &collector);
            continue;
        }

        collector_clear(&collector);
        return value;
    }
}

static charon_element_inner_t *parse_unary_pre(charon_parser_t *parser) {
    collector_t collector = COLLECTOR_INIT;
    switch(parser_peek(parser)) {
        case CHARON_TOKEN_KIND_PNCT_STAR:
        case CHARON_TOKEN_KIND_PNCT_MINUS:
        case CHARON_TOKEN_KIND_PNCT_NOT:
        case CHARON_TOKEN_KIND_PNCT_AMPERSAND:
            parser_consume_any(parser, &collector);
            collector_push(&collector, parse_unary_pre(parser));
            break;
        default: return parse_unary_post(parser);
    }
    return parser_build(parser, CHARON_NODE_KIND_EXPR_UNARY, &collector);
}

static charon_element_inner_t *parse_factor(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_unary_pre, 3, CHARON_TOKEN_KIND_PNCT_STAR, CHARON_TOKEN_KIND_PNCT_SLASH, CHARON_TOKEN_KIND_PNCT_PERCENTAGE);
}

static charon_element_inner_t *parse_term(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_factor, 2, CHARON_TOKEN_KIND_PNCT_PLUS, CHARON_TOKEN_KIND_PNCT_MINUS);
}

static charon_element_inner_t *parse_shift(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_term, 2, CHARON_TOKEN_KIND_PNCT_SHIFT_LEFT, CHARON_TOKEN_KIND_PNCT_SHIFT_RIGHT);
}

static charon_element_inner_t *parse_comparison(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_shift, 4, CHARON_TOKEN_KIND_PNCT_CARET_RIGHT, CHARON_TOKEN_KIND_PNCT_GREATER_EQUAL, CHARON_TOKEN_KIND_PNCT_CARET_LEFT, CHARON_TOKEN_KIND_PNCT_LESS_EQUAL);
}

static charon_element_inner_t *parse_equality(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_comparison, 2, CHARON_TOKEN_KIND_PNCT_EQUAL_EQUAL, CHARON_TOKEN_KIND_PNCT_NOT_EQUAL);
}

static charon_element_inner_t *parse_bitwise_and(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_equality, 1, CHARON_TOKEN_KIND_PNCT_AMPERSAND);
}

static charon_element_inner_t *parse_bitwise_xor(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_bitwise_and, 1, CHARON_TOKEN_KIND_PNCT_CARET);
}

static charon_element_inner_t *parse_bitwise_or(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_bitwise_xor, 1, CHARON_TOKEN_KIND_PNCT_PIPE);
}

static charon_element_inner_t *parse_logical_and(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_bitwise_or, 1, CHARON_TOKEN_KIND_PNCT_LOGICAL_AND);
}

static charon_element_inner_t *parse_logical_or(charon_parser_t *parser) {
    return helper_binary_operation(parser, parse_logical_and, 1, CHARON_TOKEN_KIND_PNCT_LOGICAL_OR);
}

static charon_element_inner_t *parse_assignment(charon_parser_t *parser) {
    charon_element_inner_t *left = parse_logical_or(parser);

    if(!helper_tokens_match(
           parser_peek(parser),
           6,
           CHARON_TOKEN_KIND_PNCT_EQUAL,
           CHARON_TOKEN_KIND_PNCT_PLUS_EQUAL,
           CHARON_TOKEN_KIND_PNCT_MINUS_EQUAL,
           CHARON_TOKEN_KIND_PNCT_STAR_EQUAL,
           CHARON_TOKEN_KIND_PNCT_SLASH_EQUAL,
           CHARON_TOKEN_KIND_PNCT_PERCENTAGE_EQUAL
       ))
    {
        return left;
    }

    collector_t collector = COLLECTOR_INIT;
    collector_push(&collector, left);
    parser_consume_any(parser, &collector);
    collector_push(&collector, parse_assignment(parser));
    return parser_build(parser, CHARON_NODE_KIND_EXPR_BINARY, &collector);
}

charon_element_inner_t *parse_expr(charon_parser_t *parser) {
    return parse_assignment(parser);
}
