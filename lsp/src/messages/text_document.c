#include "charon/diag.h"
#include "charon/node.h"
#include "charon/path.h"
#include "charon/utf8.h"
#include "charon/util.h"
#include "document.h"
#include "io.h"
#include "linedb.h"
#include "lsp.h"
#include "stdlib.h"

#include <assert.h>
#include <charon/element.h>
#include <charon/lexer.h>
#include <charon/memory.h>
#include <charon/parser.h>
#include <json.h>
#include <json_object.h>
#include <stddef.h>
#include <string.h>

[[maybe_unused]] static void print_tree(charon_memory_allocator_t *allocator, charon_element_t *element, int depth) {
    switch(charon_element_type(element->inner)) {
        case CHARON_ELEMENT_TYPE_TRIVIA: assert(false);
        case CHARON_ELEMENT_TYPE_NODE:   {
            charon_node_kind_t node_kind = charon_element_node_kind(element->inner);
            lsp_log("%*s%s", depth * 2, "", charon_node_kind_tostring(node_kind));

            size_t child_count = charon_element_node_child_count(element->inner);
            for(size_t i = 0; i < child_count; i++) {
                charon_element_t *child = charon_element_wrap_node_child(allocator, element, i);
                print_tree(allocator, child, depth + 1);
            }
            break;
        }
        case CHARON_ELEMENT_TYPE_TOKEN:
            charon_token_kind_t token_kind = charon_element_token_kind(element->inner);
            lsp_log("%*s%s `%s`", depth * 2, "", charon_token_kind_tostring(token_kind), charon_element_token_text(element->inner));
            break;
    }
}

static size_t document_position_to_offset(document_t *document, size_t line, size_t column) {
    size_t offset;
    bool ok = linedb_line_to_offset(&document->linedb, line, &offset);
    assert(ok);

    for(size_t i = 0; i < column; i++) {
        assert(offset < document->text_size);
        char chr = document->text[offset];
        assert(chr != '\n' && chr != '\0');
        size_t chr_width = charon_utf8_lead_width(chr);
        offset += chr_width;
        if(chr_width == 4) column--;
    }
    return offset;
}

static void document_offset_to_position(document_t *document, size_t offset, size_t *out_line, size_t *out_column) {
    size_t line_offset = offset;
    bool ok = linedb_offset_to_line(&document->linedb, &line_offset, out_line);
    assert(ok);

    size_t column = 0;
    for(size_t i = line_offset; i < offset;) {
        char chr = document->text[offset];
        assert(chr != '\n' && chr != '\0');
        size_t chr_width = charon_utf8_lead_width(chr);
        i += chr_width;
        column += (chr_width == 4) ? 2 : 1;
    }
    *out_column = column;
}

static charon_element_t *find_element(charon_memory_allocator_t *allocator, charon_element_t *current, size_t offset) {
    if(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_NODE) {
        for(size_t i = 0; i < charon_element_node_child_count(current->inner); i++) {
            charon_element_t *child = charon_element_wrap_node_child(allocator, current, i);
            if(offset >= child->offset && offset < child->offset + charon_element_length(child->inner)) return find_element(allocator, child, offset);
        }
    }

    assert(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_TOKEN);
    return current;
}

// Find the child that contains a given range.
// - range_start is inclusive
// - range_end is exclusive
static charon_element_t *find_range(charon_memory_allocator_t *allocator, charon_element_t *element, size_t range_start, size_t range_end) {
    assert(charon_element_type(element->inner) == CHARON_ELEMENT_TYPE_NODE);

    size_t low = 0;
    size_t high = charon_element_node_child_count(element->inner) - 1;
    while(low <= high) {
        int mid = low + (high - low) / 2;

        charon_element_t *child = charon_element_wrap_node_child(allocator, element, mid);
        size_t child_start = child->offset;
        size_t child_end = child_start + charon_element_length(child->inner);

        if(range_start >= child_start && range_end <= child_end) return child;

        if(range_start >= child_start) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return nullptr;
}

static void edit_text(char **text, size_t *text_length, size_t range_start, size_t range_end, const char *text_changed, size_t text_changed_length) {
    assert(range_start <= range_end && range_end <= *text_length);

    size_t range_length = range_end - range_start;

    size_t new_length = *text_length - range_length + text_changed_length;
    char *new = malloc(new_length);

    memcpy(new, *text, range_start);
    memcpy(&new[range_start], text_changed, text_changed_length);
    memcpy(&new[range_start + text_changed_length], &(*text)[range_end], *text_length - range_end);

    free(*text);

    *text = new;
    *text_length = new_length;
}

static void publish_diagnostics(document_t *document) {
    struct json_object *diagnostics = json_object_new_array();

    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    charon_element_t *root_element = charon_element_wrap_root(allocator, document->root_element);
    for(charon_diag_item_t *diag = document->diagnostics; diag != nullptr; diag = diag->next) {
        charon_element_t *current_element = root_element;
        for(size_t i = 0; i < diag->path->length; i++) {
            size_t index = diag->path->steps[i];
            assert(charon_element_type(current_element->inner) == CHARON_ELEMENT_TYPE_NODE);
            assert(charon_element_node_child_count(current_element->inner) > index);
            current_element = charon_element_wrap_node_child(allocator, current_element, diag->path->steps[i]);
        }

        size_t length = charon_element_length(current_element->inner);
        size_t offset = current_element->offset;

        size_t start_line, start_column, end_line, end_column;
        document_offset_to_position(document, offset, &start_line, &start_column);
        document_offset_to_position(document, offset + length, &end_line, &end_column);

        struct json_object *start_pos = json_object_new_object();
        json_object_object_add(start_pos, "line", json_object_new_uint64(start_line));
        json_object_object_add(start_pos, "character", json_object_new_uint64(start_column));

        struct json_object *end_pos = json_object_new_object();
        json_object_object_add(end_pos, "line", json_object_new_uint64(end_line));
        json_object_object_add(end_pos, "character", json_object_new_uint64(end_column));

        struct json_object *range = json_object_new_object();
        json_object_object_add(range, "start", start_pos);
        json_object_object_add(range, "end", end_pos);

        struct json_object *diag_obj = json_object_new_object();
        json_object_object_add(diag_obj, "range", range);
        char *message = charon_diag_fmt(diag->kind, diag->data);
        json_object_object_add(diag_obj, "message", json_object_new_string(message));
        free(message);

        json_object_array_add(diagnostics, diag_obj);
    }
    charon_memory_allocator_free(allocator);

    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "uri", json_object_new_string(document->uri));
    json_object_object_add(p, "diagnostics", diagnostics);

    io_write_message_notification(stdout, "textDocument/publishDiagnostics", p);
}

static void handle_open(struct json_object *message) {
    struct json_object *params = json_object_object_get(message, "params");
    struct json_object *text_doc = json_object_object_get(params, "textDocument");
    struct json_object *uri = json_object_object_get(text_doc, "uri");

    const char *data = json_object_get_string(json_object_object_get(text_doc, "text"));
    size_t data_length = strlen(data);

    document_t *document = document_get(json_object_get_string(uri));
    document->text = strdup(data);
    document->text_size = data_length;

    linedb_clear(&document->linedb);
    linedb_build(&document->linedb, data, data_length);

    charon_utf8_text_t *text = charon_utf8_from(data, data_length);
    charon_lexer_t *lexer = charon_lexer_make(document->cache, text);
    charon_parser_t *parser = charon_parser_make(document->cache, lexer);

    charon_parser_output_t parser_output = charon_parser_parse_root(parser);
    document->diagnostics = parser_output.diagnostics;
    document->root_element = parser_output.root;

    charon_parser_destroy(parser);
    charon_lexer_destroy(lexer);

    free(text);

    publish_diagnostics(document);
}

static void handle_change(struct json_object *message) {
    struct json_object *params = json_object_object_get(message, "params");
    struct json_object *text_doc = json_object_object_get(params, "textDocument");
    struct json_object *uri = json_object_object_get(text_doc, "uri");

    struct json_object *changes = json_object_object_get(params, "contentChanges");

    document_t *document = document_get(json_object_get_string(uri));

    for(size_t i = 0; i < json_object_array_length(changes); i++) {
        struct json_object *change = json_object_array_get_idx(changes, i);
        struct json_object *range = json_object_object_get(change, "range");
        assert(range != nullptr);

        struct json_object *start_position = json_object_object_get(range, "start");
        uint64_t start_line = json_object_get_uint64(json_object_object_get(start_position, "line"));
        uint64_t start_column = json_object_get_uint64(json_object_object_get(start_position, "character"));

        struct json_object *end_position = json_object_object_get(range, "end");
        uint64_t end_line = json_object_get_uint64(json_object_object_get(end_position, "line"));
        uint64_t end_column = json_object_get_uint64(json_object_object_get(end_position, "character"));

        struct json_object *new_text_obj = json_object_object_get(change, "text");

        const char *new_text = json_object_get_string(new_text_obj);
        size_t new_text_size = strlen(new_text);

        lsp_log("===> [%lu:%lu - %lu:%lu] changed to (%lu)%s", start_line, start_column, end_line, end_column, new_text_size, new_text);

        // Compute range
        size_t range_start = document_position_to_offset(document, start_line, start_column);
        size_t range_end = document_position_to_offset(document, end_line, end_column);

        // Determine the narrowest reparse
        enum {
            REPARSE_LEVEL_FULL,
            REPARSE_LEVEL_BLOCK
        } reparse_level = REPARSE_LEVEL_BLOCK;

        for(size_t i = 0; i < new_text_size; i++) {
            switch(new_text[i]) {
                case '{':
                case '}': reparse_level = REPARSE_LEVEL_FULL; break;
            }
        }

        for(size_t i = range_start; i < range_end; i++) {
            switch(document->text[i]) {
                case '{':
                case '}': reparse_level = REPARSE_LEVEL_FULL; break;
            }
        }

        // Find the LCA
        charon_memory_allocator_t *allocator = charon_memory_allocator_make();
        charon_element_t *root = charon_element_wrap_root(allocator, document->root_element);
        assert(charon_element_length(root->inner) == document->text_size);

        charon_element_t *lca = root;
        if(reparse_level == REPARSE_LEVEL_BLOCK) {
            charon_element_t *current = root;
            while(current != nullptr && charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_NODE) {
                charon_node_kind_t kind = charon_element_node_kind(current->inner);
                if(kind == CHARON_NODE_KIND_STMT_BLOCK) lca = current;
                current = find_range(allocator, current, range_start, range_end);
            }
        }

        // Expand reparse if edit falls into trailing trivia
        {
            charon_element_t *current = lca;
            while(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_NODE) {
                size_t child_count = charon_element_node_child_count(current->inner);
                assert(child_count > 0);
                current = charon_element_wrap_node_child(allocator, current, child_count - 1);
            }
            assert(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_TOKEN);

            size_t trailing_length = charon_element_token_trailing_trivia_length(current->inner);
            size_t token_length = charon_element_length(current->inner);
            if(range_end >= current->offset + (token_length - trailing_length)) lca = root;
        }

        // Expand reparse if edit falls into leading trivia
        {
            charon_element_t *current = lca;
            while(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_NODE) {
                size_t child_count = charon_element_node_child_count(current->inner);
                assert(child_count > 0);
                current = charon_element_wrap_node_child(allocator, current, 0);
            }
            assert(charon_element_type(current->inner) == CHARON_ELEMENT_TYPE_TOKEN);

            size_t leading_length = charon_element_token_leading_trivia_length(current->inner);
            if(range_start <= current->offset + leading_length) lca = root;
        }

        lsp_log("= BEFORE ============================================================");
        lsp_log("%.*s", (int) charon_element_length(lca->inner), &document->text[lca->offset]);
        lsp_log("=====================================================================");

        /* Cull diagnostics within LCA */
        charon_path_t *lca_path = nullptr;
        {
            size_t index_count = 0;
            size_t *indices = nullptr;

            charon_element_t *current = lca;
            while(current->parent != nullptr) {
                indices = reallocarray(indices, ++index_count, sizeof(size_t));
                indices[index_count - 1] = current->self_index;
                current = current->parent;
            }

            lca_path = charon_path_make(index_count);
            for(size_t i = 0; i < index_count; i++) lca_path->steps[i] = indices[index_count - 1 - i];
            free(indices);

            charon_diag_item_t *new_diagnostics = nullptr;
            charon_diag_item_t *next_diag = document->diagnostics;
            while(next_diag != nullptr) {
                charon_diag_item_t *diag = next_diag;
                next_diag = diag->next;

                if(diag->path->length < lca_path->length) goto push;
                for(size_t i = 0; i < lca_path->length; i++) {
                    if(lca_path->steps[i] != diag->path->steps[i]) goto push;
                }

                charon_path_destroy(diag->path);
                free(diag);
                continue;
            push:
                diag->next = new_diagnostics;
                new_diagnostics = diag;
            }

            document->diagnostics = new_diagnostics;
        }
        assert(lca_path != nullptr);

        /* Update the document text */
        edit_text(&document->text, &document->text_size, range_start, range_end, new_text, new_text_size);
        linedb_clear(&document->linedb);
        linedb_build(&document->linedb, document->text, document->text_size);

        /* Reparse */
        size_t reparse_length = charon_element_length(lca->inner) - (range_end - range_start) + new_text_size;

        lsp_log("Range [%lu - %lu]", range_start, range_end);
        lsp_log("New Text Size: %lu", new_text_size);
        lsp_log("%lu / %lu", charon_element_length(lca->inner), reparse_length);
        lsp_log("= AFTER =============================================================");
        lsp_log("%.*s", (int) reparse_length, &document->text[lca->offset]);
        lsp_log("=====================================================================");

        charon_utf8_text_t *text = charon_utf8_from(&document->text[lca->offset], reparse_length);
        charon_lexer_t *lexer = charon_lexer_make(document->cache, text);

        charon_parser_t *parser = charon_parser_make(document->cache, lexer);
        charon_parser_output_t (*reparse_fn)(charon_parser_t *parser) = nullptr;
        switch(charon_element_node_kind(lca->inner)) {
            case CHARON_NODE_KIND_ROOT:       reparse_fn = charon_parser_parse_root; break;
            case CHARON_NODE_KIND_STMT_BLOCK: reparse_fn = charon_parser_parse_stmt_block; break;
            default:                          assert(false);
        }
        assert(reparse_fn != nullptr);

        charon_parser_output_t parser_output = reparse_fn(parser);

        charon_diag_item_t *next_diag = parser_output.diagnostics;
        while(next_diag != nullptr) {
            charon_diag_item_t *diag = next_diag;
            next_diag = diag->next;

            charon_path_t *new_path = charon_path_make(lca_path->length + diag->path->length);
            memcpy(new_path->steps, lca_path->steps, lca_path->length * sizeof(lca_path->steps[0]));
            memcpy(&new_path->steps[lca_path->length], diag->path->steps, diag->path->length * sizeof(diag->path->steps[0]));

            charon_path_destroy(diag->path);
            diag->path = new_path;

            diag->next = document->diagnostics;
            document->diagnostics = diag;
        }

        charon_path_destroy(lca_path);

        charon_parser_destroy(parser);
        charon_lexer_destroy(lexer);

        free(text);

        document->root_element = charon_util_element_swap(document->cache, lca, parser_output.root);

        charon_memory_allocator_free(allocator);

        publish_diagnostics(document);

        lsp_log("===> change computed");
    }
}

void handle_hover(struct json_object *message) {
    struct json_object *id = NULL;
    json_object_object_get_ex(message, "id", &id);

    struct json_object *params = json_object_object_get(message, "params");
    struct json_object *text_doc = json_object_object_get(params, "textDocument");
    struct json_object *uri = json_object_object_get(text_doc, "uri");

    struct json_object *position = json_object_object_get(params, "position");
    uint64_t line = json_object_get_uint64(json_object_object_get(position, "line"));
    uint64_t column = json_object_get_uint64(json_object_object_get(position, "character"));

    document_t *document = document_get(json_object_get_string(uri));
    if(document->root_element == nullptr) return;

    charon_memory_allocator_t *allocator = charon_memory_allocator_make();
    charon_element_t *root_element = charon_element_wrap_root(allocator, document->root_element);

    size_t offset = document_position_to_offset(document, line, column);
    charon_element_t *elem = find_element(allocator, root_element, offset);

    struct json_object *contents = json_object_new_object();
    json_object_object_add(contents, "kind", json_object_new_string("plaintext"));
    if(elem == nullptr) {
        json_object_object_add(contents, "value", json_object_new_string("Line index search failed."));
    } else {
        if(charon_element_type(elem->inner) == CHARON_ELEMENT_TYPE_TOKEN) {
            json_object_object_add(contents, "value", json_object_new_string(charon_token_kind_tostring(charon_element_token_kind(elem->inner))));
        } else {
            json_object_object_add(contents, "value", json_object_new_string(charon_node_kind_tostring(charon_element_node_kind(elem->inner))));
        }
    }

    charon_memory_allocator_free(allocator);

    struct json_object *result = json_object_new_object();
    json_object_object_add(result, "contents", contents);

    io_write_message_response_result(stdout, id, result);
}

LSP_REGISTER_MESSAGE_HANDLER("textDocument/didOpen", handle_open);
LSP_REGISTER_MESSAGE_HANDLER("textDocument/didChange", handle_change);
LSP_REGISTER_MESSAGE_HANDLER("textDocument/hover", handle_hover);
