#include "parser.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../lexer/token.h"
#include "../diag.h"

static token_t consume(tokenizer_t *tokenizer, token_type_t type) {
    token_t token = tokenizer_advance(tokenizer);
    if(token.type == type) return token;
    diag_error(token.diag_loc, "expected %s got %s", token_type_name(type), token_type_name(token.type));
}

static bool try_expect(tokenizer_t *tokenizer, token_type_t type) {
    if(tokenizer_peek(tokenizer).type != type) return false;
    tokenizer_advance(tokenizer);
    return true;
}

static void expect(tokenizer_t *tokenizer, token_type_t type) {
    if(try_expect(tokenizer, type)) return;
    token_t token = tokenizer_peek(tokenizer);
    diag_error(token.diag_loc, "expected %s got %s", token_type_name(type), token_type_name(token.type));
}

static const char *make_text_from_token(tokenizer_t *tokenizer, token_t token) {
    char *text = malloc(token.length + 1);
    memcpy(text, tokenizer->source->data + token.offset, token.length);
    text[token.length] = '\0';
    return text;
}

static void free_text(tokenizer_t *tokenizer, const char *text) {
    free((char *) text);
}

static ir_type_t *type_from_text(const char *text) {
    if(strcmp(text, "void") == 0) return ir_type_get_void();
    if(strcmp(text, "bool") == 0) return ir_type_get_bool();
    if(strcmp(text, "uint") == 0) return ir_type_get_uint();
    if(strcmp(text, "u8") == 0) return ir_type_get_u8();
    if(strcmp(text, "u16") == 0) return ir_type_get_u16();
    if(strcmp(text, "u32") == 0) return ir_type_get_u32();
    if(strcmp(text, "u64") == 0) return ir_type_get_u64();
    if(strcmp(text, "int") == 0) return ir_type_get_int();
    if(strcmp(text, "i8") == 0) return ir_type_get_i8();
    if(strcmp(text, "i16") == 0) return ir_type_get_i16();
    if(strcmp(text, "i32") == 0) return ir_type_get_i32();
    if(strcmp(text, "i64") == 0) return ir_type_get_i64();
    return NULL;
}

static ir_type_t *parse_type(tokenizer_t *tokenizer) {
    token_t token_type = consume(tokenizer, TOKEN_TYPE_TYPE);
    const char *text = make_text_from_token(tokenizer, token_type);
    ir_type_t *type = type_from_text(text);
    if(type == NULL) diag_error(token_type.diag_loc, "invalid type %s", text);
    free_text(tokenizer, text);
    while(try_expect(tokenizer, TOKEN_TYPE_STAR)) type = ir_type_make_pointer(type);
    return type;
}

static const char *string_escape(diag_loc_t diag_loc, const char *src, size_t src_length) {
    char *dest = malloc(src_length + 1);
    int dest_index = 0;
    bool escaped = false;
    for(size_t i = 0; i < src_length; i++) {
        char c = src[i];
        if(escaped) {
            switch(c) {
                case 'n': dest[dest_index++] = '\n'; break;
                case 't': dest[dest_index++] = '\t'; break;
                case '\\': dest[dest_index++] = '\\'; break;
                default:
                    diag_warn(diag_loc, "unknown escape sequence `\\%c`", c);
                    dest[dest_index++] = c;
                    break;
            }
            escaped = false;
            continue;
        }
        if(c == '\\') {
            escaped = true;
            continue;
        }
        dest[dest_index++] = c;
    }
    dest[dest_index] = '\0';
    return dest;
}

static ir_node_t *helper_binary_operation(tokenizer_t *tokenizer, ir_node_t *(*func)(tokenizer_t *), size_t count, ...) {
    ir_node_t *left = func(tokenizer);
    va_list list;
    va_start(list, count);
    while(token_match_list(tokenizer_peek(tokenizer), count, list)) {
        token_t token_operation = tokenizer_advance(tokenizer);
        ir_binary_operation_t operation;
        switch(token_operation.type) {
            case TOKEN_TYPE_PLUS: operation = IR_BINARY_OPERATION_ADDITION; break;
            case TOKEN_TYPE_MINUS: operation = IR_BINARY_OPERATION_SUBTRACTION; break;
            case TOKEN_TYPE_STAR: operation = IR_BINARY_OPERATION_MULTIPLICATION; break;
            case TOKEN_TYPE_SLASH: operation = IR_BINARY_OPERATION_DIVISION; break;
            case TOKEN_TYPE_PERCENTAGE: operation = IR_BINARY_OPERATION_MODULO; break;
            case TOKEN_TYPE_GREATER: operation = IR_BINARY_OPERATION_GREATER; break;
            case TOKEN_TYPE_GREATER_EQUAL: operation = IR_BINARY_OPERATION_GREATER_EQUAL; break;
            case TOKEN_TYPE_LESS: operation = IR_BINARY_OPERATION_LESS; break;
            case TOKEN_TYPE_LESS_EQUAL: operation = IR_BINARY_OPERATION_LESS_EQUAL; break;
            case TOKEN_TYPE_EQUAL_EQUAL: operation = IR_BINARY_OPERATION_EQUAL; break;
            case TOKEN_TYPE_NOT_EQUAL: operation = IR_BINARY_OPERATION_NOT_EQUAL; break;
            default: diag_error(token_operation.diag_loc, "expected a binary operator");
        }
        left = ir_node_make_expr_binary(operation, left, func(tokenizer), token_operation.diag_loc);
        va_end(list);
        va_start(list, count);
    }
    va_end(list);
    return left;
}

static ir_node_t *parse_expression(tokenizer_t *tokenizer);
static ir_node_t *parse_statement(tokenizer_t *tokenizer);

static ir_node_t *parse_literal_numeric(tokenizer_t *tokenizer) {
    int base = 0;
    token_t token_numeric = tokenizer_advance(tokenizer);
    switch(token_numeric.type) {
        case TOKEN_TYPE_NUMBER_DEC: base = 10; break;
        case TOKEN_TYPE_NUMBER_HEX: base = 16; break;
        case TOKEN_TYPE_NUMBER_BIN: base = 2; break;
        case TOKEN_TYPE_NUMBER_OCT: base = 8; break;
        default: diag_error(token_numeric.diag_loc, "expected a numeric literal");
    }
    const char *text = make_text_from_token(tokenizer, token_numeric);
    errno = 0;
    uintmax_t value = strtoull(text, NULL, base);
    if(errno == ERANGE) diag_error(token_numeric.diag_loc, "integer constant too large");
    free_text(tokenizer, text);
    return ir_node_make_expr_literal_numeric(value, token_numeric.diag_loc);
}

static ir_node_t *parse_literal_string(tokenizer_t *tokenizer) {
    token_t token_string = consume(tokenizer, TOKEN_TYPE_STRING);
    const char *text = make_text_from_token(tokenizer, token_string);
    const char *value = string_escape(token_string.diag_loc, &text[1], strlen(text) - 2);
    free_text(tokenizer, text);
    return ir_node_make_expr_literal_string(value, token_string.diag_loc);
}

static ir_node_t *parse_literal_char(tokenizer_t *tokenizer) {
    token_t token_char = consume(tokenizer, TOKEN_TYPE_CHAR);
    const char *text = make_text_from_token(tokenizer, token_char);
    char value = text[1];
    free_text(tokenizer, text);
    return ir_node_make_expr_literal_char(value, token_char.diag_loc);
}

static ir_node_t *parse_literal_bool(tokenizer_t *tokenizer) {
    token_t token_bool = consume(tokenizer, TOKEN_TYPE_BOOL);
    const char *text = make_text_from_token(tokenizer, token_bool);
    bool value = strcmp(text, "true") == 0;
    free_text(tokenizer, text);
    return ir_node_make_expr_literal_bool(value, token_bool.diag_loc);
}

static ir_node_t *parse_literal(tokenizer_t *tokenizer) {
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_STRING: return parse_literal_string(tokenizer);
        case TOKEN_TYPE_CHAR: return parse_literal_char(tokenizer);
        case TOKEN_TYPE_BOOL: return parse_literal_bool(tokenizer);
        default: return parse_literal_numeric(tokenizer);
    }
}

static ir_node_t *parse_var_or_call(tokenizer_t *tokenizer) {
    token_t token_identifier = consume(tokenizer, TOKEN_TYPE_IDENTIFIER);
    const char *name = make_text_from_token(tokenizer, token_identifier);
    if(try_expect(tokenizer, TOKEN_TYPE_PARENTHESES_LEFT)) {
        size_t argument_count = 0;
        ir_node_t **arguments = NULL;
        if(!try_expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT)) {
            do {
                arguments = realloc(arguments, sizeof(ir_node_t *) * ++argument_count);
                arguments[argument_count - 1] = parse_expression(tokenizer);
            } while(try_expect(tokenizer, TOKEN_TYPE_COMMA));
            expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT);
        }
        return ir_node_make_expr_call(name, argument_count, arguments, token_identifier.diag_loc);
    }
    return ir_node_make_expr_var(name, token_identifier.diag_loc);
}

static ir_node_t *parse_group(tokenizer_t *tokenizer) {
    expect(tokenizer, TOKEN_TYPE_PARENTHESES_LEFT);
    ir_node_t *inner = parse_expression(tokenizer);
    expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT);
    return inner;
}

static ir_node_t *parse_primary(tokenizer_t *tokenizer) {
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_IDENTIFIER: return parse_var_or_call(tokenizer);
        case TOKEN_TYPE_PARENTHESES_LEFT: return parse_group(tokenizer);
        default: return parse_literal(tokenizer);
    }
}

static ir_node_t *parse_unary(tokenizer_t *tokenizer) {
    if(!token_match(tokenizer_peek(tokenizer), 4, TOKEN_TYPE_MINUS, TOKEN_TYPE_NOT, TOKEN_TYPE_STAR, TOKEN_TYPE_AMPERSAND)) return parse_primary(tokenizer);
    token_t token_operator = tokenizer_advance(tokenizer);
    ir_unary_operation_t operation;
    switch(token_operator.type) {
        case TOKEN_TYPE_STAR: operation = IR_UNARY_OPERATION_DEREF; break;
        case TOKEN_TYPE_MINUS: operation = IR_UNARY_OPERATION_NEGATIVE; break;
        case TOKEN_TYPE_NOT: operation = IR_UNARY_OPERATION_NOT; break;
        case TOKEN_TYPE_AMPERSAND: operation = IR_UNARY_OPERATION_REF; break;
        default: diag_error(token_operator.diag_loc, "expected a unary operator");
    }
    return ir_node_make_expr_unary(operation, parse_unary(tokenizer), token_operator.diag_loc);
}

static ir_node_t *parse_factor(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_unary, 3, TOKEN_TYPE_STAR, TOKEN_TYPE_SLASH, TOKEN_TYPE_PERCENTAGE);
}

static ir_node_t *parse_term(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_factor, 2, TOKEN_TYPE_PLUS, TOKEN_TYPE_MINUS);
}

static ir_node_t *parse_comparison(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_term, 4, TOKEN_TYPE_GREATER, TOKEN_TYPE_GREATER_EQUAL, TOKEN_TYPE_LESS, TOKEN_TYPE_LESS_EQUAL);
}

static ir_node_t *parse_equality(tokenizer_t *tokenizer) {
    return helper_binary_operation(tokenizer, parse_comparison, 2, TOKEN_TYPE_EQUAL_EQUAL, TOKEN_TYPE_NOT_EQUAL);
}

static ir_node_t *parse_assignment(tokenizer_t *tokenizer) {
    ir_node_t *left = parse_equality(tokenizer);
    ir_binary_operation_t operation;
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_EQUAL: operation = IR_BINARY_OPERATION_ASSIGN; break;
        case TOKEN_TYPE_PLUS_EQUAL: operation = IR_BINARY_OPERATION_ADDITION; break;
        case TOKEN_TYPE_MINUS_EQUAL: operation = IR_BINARY_OPERATION_SUBTRACTION; break;
        case TOKEN_TYPE_STAR_EQUAL: operation = IR_BINARY_OPERATION_MULTIPLICATION; break;
        case TOKEN_TYPE_SLASH_EQUAL: operation = IR_BINARY_OPERATION_DIVISION; break;
        case TOKEN_TYPE_PERCENTAGE_EQUAL: operation = IR_BINARY_OPERATION_MODULO; break;
        default: return left;
    }
    token_t token_operation = tokenizer_advance(tokenizer);
    ir_node_t *right = parse_assignment(tokenizer);
    if(operation != IR_BINARY_OPERATION_ASSIGN) right = ir_node_make_expr_binary(operation, left, right, token_operation.diag_loc);
    return ir_node_make_expr_binary(IR_BINARY_OPERATION_ASSIGN, left, right, token_operation.diag_loc);
}

static ir_node_t *parse_expression(tokenizer_t *tokenizer) {
    return parse_assignment(tokenizer);
}

static ir_node_t *parse_decl(tokenizer_t *tokenizer) {
    ir_type_t *type = parse_type(tokenizer);
    token_t token_identifier = consume(tokenizer, TOKEN_TYPE_IDENTIFIER);
    const char *name = make_text_from_token(tokenizer, token_identifier);
    ir_node_t *initial = NULL;
    if(try_expect(tokenizer, TOKEN_TYPE_EQUAL)) initial = parse_expression(tokenizer);
    return ir_node_make_stmt_decl(type, name, initial, token_identifier.diag_loc);
}

static ir_node_t *parse_return(tokenizer_t *tokenizer) {
    token_t token_return = consume(tokenizer, TOKEN_TYPE_KEYWORD_RETURN);
    ir_node_t *node_expression = NULL;
    if(tokenizer_peek(tokenizer).type != TOKEN_TYPE_SEMI_COLON) node_expression = parse_expression(tokenizer);
    return ir_node_make_stmt_return(node_expression, token_return.diag_loc);
}

static ir_node_t *parse_simple_statement(tokenizer_t *tokenizer) {
    ir_node_t *node;
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_KEYWORD_RETURN: node = parse_return(tokenizer); break;
        case TOKEN_TYPE_TYPE: node = parse_decl(tokenizer); break;
        default: node = parse_expression(tokenizer); break;
    }
    expect(tokenizer, TOKEN_TYPE_SEMI_COLON);
    return node;
}

static ir_node_t *parse_block(tokenizer_t *tokenizer) {
    token_t token_left_brace = consume(tokenizer, TOKEN_TYPE_BRACE_LEFT);
    size_t statement_count = 0;
    ir_node_t **statements = NULL;
    while(!tokenizer_is_eof(tokenizer) && tokenizer_peek(tokenizer).type != TOKEN_TYPE_BRACE_RIGHT) {
        statements = realloc(statements, sizeof(ir_node_t *) * ++statement_count);
        statements[statement_count - 1] = parse_statement(tokenizer);
    }
    expect(tokenizer, TOKEN_TYPE_BRACE_RIGHT);
    return ir_node_make_stmt_block(statement_count, statements, token_left_brace.diag_loc);
}

static ir_node_t *parse_if(tokenizer_t *tokenizer) {
    token_t token_if = consume(tokenizer, TOKEN_TYPE_KEYWORD_IF);
    expect(tokenizer, TOKEN_TYPE_PARENTHESES_LEFT);
    ir_node_t *condition = parse_expression(tokenizer);
    expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT);
    ir_node_t *body = parse_statement(tokenizer);
    ir_node_t *else_body = try_expect(tokenizer, TOKEN_TYPE_KEYWORD_ELSE) ? parse_statement(tokenizer) : NULL;
    return ir_node_make_stmt_if(condition, body, else_body, token_if.diag_loc);
}

static ir_node_t *parse_while(tokenizer_t *tokenizer) {
    ir_node_t *condition = NULL;
    token_t token_while = consume(tokenizer, TOKEN_TYPE_KEYWORD_WHILE);
    if(try_expect(tokenizer, TOKEN_TYPE_PARENTHESES_LEFT)) {
        condition = parse_expression(tokenizer);
        expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT);
    }
    return ir_node_make_stmt_while(condition, parse_statement(tokenizer), token_while.diag_loc);
}

static ir_node_t *parse_statement(tokenizer_t *tokenizer) {
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_BRACE_LEFT: return parse_block(tokenizer);
        case TOKEN_TYPE_KEYWORD_IF: return parse_if(tokenizer);
        case TOKEN_TYPE_KEYWORD_WHILE: return parse_while(tokenizer);
        default: return parse_simple_statement(tokenizer);
    }
}

static ir_function_decl_t parse_function_declaration(tokenizer_t *tokenizer, diag_loc_t *diag_loc) {
    ir_type_t *return_type = parse_type(tokenizer);
    token_t token_identifier = consume(tokenizer, TOKEN_TYPE_IDENTIFIER);
    const char *name = make_text_from_token(tokenizer, token_identifier);
    bool varargs = false;
    size_t argument_count = 0;
    ir_function_decl_argument_t *arguments = NULL;
    expect(tokenizer, TOKEN_TYPE_PARENTHESES_LEFT);
    if(!try_expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT)) {
        do {
            if(try_expect(tokenizer, TOKEN_TYPE_TRIPLE_PERIOD)) {
                varargs = true;
                break;
            }
            diag_loc_t diag_loc = tokenizer_peek(tokenizer).diag_loc;
            ir_type_t *argument_type = parse_type(tokenizer);
            const char *argument_name = make_text_from_token(tokenizer, consume(tokenizer, TOKEN_TYPE_IDENTIFIER));
            arguments = realloc(arguments, sizeof(ir_function_decl_argument_t) * ++argument_count);
            arguments[argument_count - 1] = (ir_function_decl_argument_t) {
                .type = argument_type,
                .name = argument_name,
                .diag_loc = diag_loc
            };
        } while(try_expect(tokenizer, TOKEN_TYPE_COMMA));
        expect(tokenizer, TOKEN_TYPE_PARENTHESES_RIGHT);
    }
    *diag_loc = token_identifier.diag_loc;
    return (ir_function_decl_t) {
        .name = name,
        .return_type = return_type,
        .varargs = varargs,
        .argument_count = argument_count,
        .arguments = arguments
    };
}

static ir_node_t *parse_function(tokenizer_t *tokenizer) {
    diag_loc_t diag_loc;
    ir_function_decl_t function_decl = parse_function_declaration(tokenizer, &diag_loc);
    return ir_node_make_global_function(function_decl, parse_statement(tokenizer), diag_loc);
}

static ir_node_t *parse_extern(tokenizer_t *tokenizer) {
    expect(tokenizer, TOKEN_TYPE_KEYWORD_EXTERN);
    diag_loc_t diag_loc;
    ir_function_decl_t function_decl = parse_function_declaration(tokenizer, &diag_loc);
    expect(tokenizer, TOKEN_TYPE_SEMI_COLON);
    return ir_node_make_global_extern(function_decl, diag_loc);
}

static ir_node_t *parse_global(tokenizer_t *tokenizer) {
    switch(tokenizer_peek(tokenizer).type) {
        case TOKEN_TYPE_KEYWORD_EXTERN: return parse_extern(tokenizer);
        default: return parse_function(tokenizer);
    }
}

static ir_node_t *parse_program(tokenizer_t *tokenizer) {
    diag_loc_t diag_loc = tokenizer_peek(tokenizer).diag_loc;
    size_t global_count = 0;
    ir_node_t **globals = NULL;
    while(!tokenizer_is_eof(tokenizer)) {
        globals = realloc(globals, sizeof(ir_node_t *) * ++global_count);
        globals[global_count - 1] = parse_global(tokenizer);
    }
    return ir_node_make_program(global_count, globals, diag_loc);
}

ir_node_t *parser_parse(tokenizer_t *tokenizer) {
    return parse_program(tokenizer);
}