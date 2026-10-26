#include "queries.h"

#include "charon/lexer.h"
#include "core/db.h"
#include "lexer_pipeline.h"
#include "parser/parser.h"
#include "query/input.h"
#include "query/query.h"
#include "syntax/element.h"

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

static void free_diagnostics(diag_item_t *diagnostics) {
    while(diagnostics != nullptr) {
        diag_item_t *next = diagnostics->next;
        free(diagnostics->data);
        if(diagnostics->path != nullptr) path_destroy(diagnostics->path);
        free(diagnostics);
        diagnostics = next;
    }
}

static void drop_parse_result(void *value) {
    query_parse_result_t *result = value;
    free_diagnostics(result->diagnostics);
    free(result);
}

static void drop_diag_array(void *value) {
    diag_array_t *array = value;
    free(array->items);
    free(array);
}

static void drop_decl_array(void *value) {
    decl_array_t *array = value;
    free(array->items);
    free(array);
}

static void drop_element_array(void *value) {
    element_array_t *array = value;
    free(array->tokens);
    free(array);
}

static void *exec_file_parse(db_t *db, const query_input_t *input) {
    const file_t *file = input->ptr;
    const text_t *source = db_file_source(db, file);

    charon_lexer_t *lexer = charon_lexer_new(source->data, source->size);
    lexer_pipeline_t *pipeline = lexer_pipeline_make(db, lexer);
    parser_t *parser = parser_make(db, pipeline, file->handle);

    parser_output_t output = parser_parse_root(parser);

    parser_destroy(parser);
    lexer_pipeline_destroy(pipeline);
    charon_lexer_destroy(lexer);

    query_parse_result_t *result = malloc(sizeof(query_parse_result_t));
    result->root = output.root;
    result->diagnostics = output.diagnostics;
    return result;
}

static void *exec_file_diagnostics(db_t *db, const query_input_t *input) {
    query_parse_result_t *parse = (query_parse_result_t *) query_execute(db, QUERY_FILE_PARSE, input);

    size_t count = 0;
    for(diag_item_t *item = parse->diagnostics; item != nullptr; item = item->next) count++;

    diag_array_t *array = malloc(sizeof(diag_array_t));
    array->count = count;
    array->items = count == 0 ? nullptr : malloc(sizeof(diag_item_t *) * count);

    size_t index = 0;
    for(diag_item_t *item = parse->diagnostics; item != nullptr; item = item->next) array->items[index++] = item;

    return array;
}

static size_t count_declarations(const element_inner_t *node) {
    if(node->type != ELEMENT_TYPE_NODE) return 0;

    size_t count = 0;
    for(size_t i = 0; i < node->node.child_count; i++) {
        if(node->node.children[i]->type == ELEMENT_TYPE_NODE) count++;
    }
    return count;
}

static void *exec_file_items(db_t *db, const query_input_t *input) {
    query_parse_result_t *parse = (query_parse_result_t *) query_execute(db, QUERY_FILE_PARSE, input);

    const element_inner_t *root = parse->root;
    assert(root->type == ELEMENT_TYPE_NODE);

    size_t count = count_declarations(root);
    decl_array_t *array = malloc(sizeof(decl_array_t));
    array->count = count;
    array->items = count == 0 ? nullptr : malloc(sizeof(const element_inner_t *) * count);

    size_t index = 0;
    for(size_t i = 0; i < root->node.child_count; i++) {
        if(root->node.children[i]->type == ELEMENT_TYPE_NODE) array->items[index++] = root->node.children[i];
    }

    return array;
}

static void count_tokens(const element_inner_t *element, size_t *count) {
    switch(element->type) {
        case ELEMENT_TYPE_TRIVIA: return;
        case ELEMENT_TYPE_TOKEN:  (*count)++; return;
        case ELEMENT_TYPE_NODE:
            for(size_t i = 0; i < element->node.child_count; i++) count_tokens(element->node.children[i], count);
            return;
    }
}

static void collect_tokens(const element_inner_t *element, const element_inner_t **tokens, size_t *index) {
    switch(element->type) {
        case ELEMENT_TYPE_TRIVIA: return;
        case ELEMENT_TYPE_TOKEN:  tokens[(*index)++] = element; return;
        case ELEMENT_TYPE_NODE:
            for(size_t i = 0; i < element->node.child_count; i++) collect_tokens(element->node.children[i], tokens, index);
            return;
    }
}

static void *exec_file_tokens(db_t *db, const query_input_t *input) {
    query_parse_result_t *parse = (query_parse_result_t *) query_execute(db, QUERY_FILE_PARSE, input);

    size_t count = 0;
    count_tokens(parse->root, &count);

    element_array_t *array = malloc(sizeof(element_array_t));
    array->count = count;
    array->tokens = count == 0 ? nullptr : malloc(sizeof(const element_inner_t *) * count);

    size_t index = 0;
    collect_tokens(parse->root, array->tokens, &index);

    return array;
}

static const query_desc_t DESCRIPTORS[QUERY_KIND_COUNT] = {
    [QUERY_FILE_TEXT] = { .name = "FILE_TEXT",        .is_input = true,  .exec = nullptr,               .drop = nullptr            },
    [QUERY_FILE_PARSE] = { .name = "FILE_PARSE",       .is_input = false, .exec = exec_file_parse,       .drop = drop_parse_result  },
    [QUERY_FILE_DIAGNOSTICS] = { .name = "FILE_DIAGNOSTICS", .is_input = false, .exec = exec_file_diagnostics, .drop = drop_diag_array    },
    [QUERY_FILE_ITEMS] = { .name = "FILE_ITEMS",       .is_input = false, .exec = exec_file_items,       .drop = drop_decl_array    },
    [QUERY_FILE_TOKENS] = { .name = "FILE_TOKENS",      .is_input = false, .exec = exec_file_tokens,      .drop = drop_element_array },
};

const query_desc_t *query_descriptor(query_kind_t kind) {
    assert(kind < QUERY_KIND_COUNT);
    return &DESCRIPTORS[kind];
}
