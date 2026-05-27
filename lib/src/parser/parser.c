#include "parser.h"

#include "charon/syntax/token.h"
#include "core/store.h"
#include "lexer_pipeline.h"
#include "parse.h"
#include "syntax/element.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    PARSER_EVENT_TYPE_OPEN,
    PARSER_EVENT_TYPE_CLOSE,
    PARSER_EVENT_TYPE_TOKEN,
    PARSER_EVENT_TYPE_ERROR
} parser_event_type_t;

typedef struct parser_event {
    parser_event_type_t event_type;
    union {
        struct {
            store_handle_t token;
        } token;
        struct {
            charon_node_kind_t kind;
        } close;
        struct {
            diag_t diag;
            diag_data_t *diag_data;
        } error;
    };

    list_node_t list_node;
} parser_event_t;

typedef struct build_node {
    struct build_node *parent;

    size_t self_index;

    size_t element_count;
    store_handle_t *elements;
} build_node_t;

static void build_node_push(build_node_t *node, store_handle_t element) {
    node->elements = reallocarray(node->elements, ++node->element_count, sizeof(store_handle_t));
    node->elements[node->element_count - 1] = element;
}

static void raw_consume(parser_t *parser) {
    parser_event_t *event = malloc(sizeof(parser_event_t));
    event->event_type = PARSER_EVENT_TYPE_TOKEN;
    event->token.token = lexer_pipeline_advance(parser->lexer_pipeline);
    list_push(&parser->events, &event->list_node);
}

parser_t *parser_make(db_t *db, lexer_pipeline_t *lexer_pipeline) {
    parser_t *parser = malloc(sizeof(parser_t));
    parser->db = db;
    parser->lexer_pipeline = lexer_pipeline;

    for(size_t i = 0; i < CHARON_TOKEN_KIND_COUNT; i++) parser->syncset.token_kinds[i] = false;
    parser->events = LIST_INIT;
    return parser;
}

void parser_destroy(parser_t *parser) {
    list_node_t *lnode;
    while((lnode = list_pop(&parser->events)) != nullptr) { free(LIST_CONTAINER_OF(lnode, parser_event_t, list_node)); }

    free(parser);
}

parser_output_t parser_parse_stmt(parser_t *parser) {
    parse_stmt(parser);
    return parser_build(parser);
}

parser_output_t parser_parse_stmt_block(parser_t *parser) {
    parse_stmt_block(parser);
    return parser_build(parser);
}

parser_output_t parser_parse_root(parser_t *parser) {
    parse_root(parser);
    return parser_build(parser);
}

bool parser_is_eof(parser_t *parser) {
    return lexer_pipeline_is_eof(parser->lexer_pipeline);
}

charon_token_kind_t parser_peek(parser_t *parser) {
    return lexer_pipeline_peek(parser->lexer_pipeline);
}

void parser_consume(parser_t *parser, charon_token_kind_t kind) {
    parser_consume_many(parser, 1, kind);
}

void parser_consume_many(parser_t *parser, size_t count, ...) {
    va_list list;
    va_start(list, count);
    bool result = parser_consume_try_many_list(parser, count, list);
    va_end(list);
    if(result) return;

    diag_data_t *diag_data = malloc(sizeof(diag_data_t) + count * sizeof(charon_token_kind_t));
    diag_data->unexpected_token.found = parser_peek(parser);
    diag_data->unexpected_token.expected_count = count;

    va_start(list, count);
    for(size_t i = 0; i < count; i++) diag_data->unexpected_token.expected[i] = va_arg(list, charon_token_kind_t);
    va_end(list);

    parser_error(parser, CHARON_DIAG_UNEXPECTED_TOKEN, diag_data);
}

bool parser_consume_try(parser_t *parser, charon_token_kind_t kind) {
    return parser_consume_try_many(parser, 1, kind);
}

bool parser_consume_try_many(parser_t *parser, size_t count, ...) {
    va_list list;
    va_start(list, count);
    bool result = parser_consume_try_many_list(parser, count, list);
    va_end(list);
    return result;
}

bool parser_consume_try_many_list(parser_t *parser, size_t count, va_list list) {
    charon_token_kind_t kind = parser_peek(parser);
    for(size_t i = 0; i < count; i++) {
        charon_token_kind_t valid_kind = va_arg(list, charon_token_kind_t);
        if(kind != valid_kind) continue;

        raw_consume(parser);
        return true;
    }
    return false;
}

parser_event_t *parser_checkpoint(parser_t *parser) {
    return LIST_CONTAINER_OF(parser->events.head, parser_event_t, list_node);
}

void parser_open_element_at(parser_t *parser, parser_event_t *checkpoint) {
    parser_event_t *event = malloc(sizeof(parser_event_t));
    event->event_type = PARSER_EVENT_TYPE_OPEN;
    list_node_prepend(&parser->events, &checkpoint->list_node, &event->list_node);
}

void parser_open_element(parser_t *parser) {
    parser_event_t *event = malloc(sizeof(parser_event_t));
    event->event_type = PARSER_EVENT_TYPE_OPEN;
    list_push(&parser->events, &event->list_node);
}

void parser_close_element(parser_t *parser, charon_node_kind_t kind) {
    parser_event_t *event = malloc(sizeof(parser_event_t));
    event->event_type = PARSER_EVENT_TYPE_CLOSE;
    event->close.kind = kind;
    list_push(&parser->events, &event->list_node);
}

void parser_error(parser_t *parser, diag_t diag, diag_data_t *diag_data) {
    parser_open_element(parser);
    if(!parser->syncset.token_kinds[parser_peek(parser)]) raw_consume(parser);

    parser_event_t *event = malloc(sizeof(parser_event_t));
    event->event_type = PARSER_EVENT_TYPE_ERROR;
    event->error.diag = diag;
    event->error.diag_data = diag_data;
    list_push(&parser->events, &event->list_node);
}

parser_output_t parser_build(parser_t *parser) {
    size_t depth = 0;
    build_node_t *open_node = nullptr;

    diag_item_t *diagnostics = nullptr;

    list_node_t *lnode;
    while((lnode = list_pop_back(&parser->events)) != nullptr) {
        parser_event_t *event = LIST_CONTAINER_OF(lnode, parser_event_t, list_node);

        charon_node_kind_t build_kind;
        switch(event->event_type) {
            case PARSER_EVENT_TYPE_OPEN: {
                build_node_t *new_node = malloc(sizeof(build_node_t));
                new_node->parent = open_node;
                new_node->self_index = open_node == nullptr ? 0 : open_node->element_count;
                new_node->elements = nullptr;
                new_node->element_count = 0;
                open_node = new_node;
                depth++;
                break;
            }
            case PARSER_EVENT_TYPE_TOKEN: {
                build_node_push(open_node, event->token.token);
                break;
            }
            case PARSER_EVENT_TYPE_CLOSE: {
                build_kind = event->close.kind;
                goto build_node;
            }
            case PARSER_EVENT_TYPE_ERROR: {
                path_t *path = path_make(depth - 1);
                build_node_t *current_node = open_node;

                size_t index = depth - 1;
                while(current_node->parent != nullptr) {
                    assert(index > 0);
                    path->indices[--index] = current_node->self_index;
                    current_node = current_node->parent;
                }

                diag_item_t *diag_item = malloc(sizeof(diag_item_t));
                diag_item->kind = event->error.diag;
                diag_item->data = event->error.diag_data;
                diag_item->path = path;
                diag_item->next = diagnostics;

                diagnostics = diag_item;

                build_kind = CHARON_NODE_KIND_ERROR;
                goto build_node;
            }

            build_node: {
                build_node_t *current = open_node;
                assert(current != nullptr);

                build_node_t *parent = open_node->parent;
                open_node = parent;

                store_handle_t element = element_inner_make_node(parser->db, build_kind, current->element_count, current->elements);
                free(current->elements);
                free(current);

                if(parent == nullptr) return (parser_output_t) { .root_element = element, .diagnostics = diagnostics };

                build_node_push(parent, element);
                depth--;
                break;
            }
        }

        free(event);
    }
    assert(false);
}
