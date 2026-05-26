#include "element.h"

#include "common/hasher.h"
#include "core/db.h"
#include "core/interner.h"

#include <assert.h>
#include <stdlib.h>

static const element_inner_t *token_trivia(const element_inner_t *inner_element, size_t index) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    assert(index < inner_element->token.leading_trivia_count + inner_element->token.trailing_trivia_count);
    return inner_element->token.trivia[index];
}

// static charon_element_t *token_trivia_wrap(charon_element_t *element, size_t index) {
//     charon_element_t *trivia = charon_memory_allocate(allocator, sizeof(charon_element_t));
//     trivia->inner = token_trivia(element->inner, index);
//     trivia->parent = element;
//     trivia->offset = element->offset;
//     trivia->self_index = index;
//     if(index >= element->inner->token.leading_trivia_count) trivia->offset += element->inner->length - (element->inner->token.leading_trivia_length + element->inner->token.trailing_trivia_length);
//     for(size_t i = 0; i < index; i++) trivia->offset += element->inner->token.trivia[i]->length;

//     return trivia;
// }

uint64_t element_inner_hash(const element_inner_t *inner_element) {
    hasher_t hasher = hasher_new();

    hasher_hash(&hasher, inner_element->type);

    switch(inner_element->type) {
        case ELEMENT_TYPE_TRIVIA: {
            hasher_hash(&hasher, inner_element->trivia.kind);

            if(inner_element->trivia.text != nullptr) hasher_hash(&hasher, text_hash(inner_element->trivia.text));
            break;
        }
        case ELEMENT_TYPE_TOKEN: {
            hasher_hash(&hasher, inner_element->token.kind);

            if(inner_element->token.text != nullptr) hasher_hash(&hasher, text_hash(inner_element->token.text));

            size_t trivia_count = inner_element->token.leading_trivia_count + inner_element->token.trailing_trivia_count;
            hasher_hash(&hasher, trivia_count);
            for(size_t i = 0; i < trivia_count; ++i) {
                hasher_hash(&hasher, inner_element->token.trivia[i]->hash);
            }
            break;
        }
        case ELEMENT_TYPE_NODE:
            hasher_hash(&hasher, inner_element->node.kind);

            hasher_hash(&hasher, inner_element->node.child_count);
            for(size_t i = 0; i < inner_element->node.child_count; ++i) {
                hasher_hash(&hasher, inner_element->node.children[i]->hash);
            }
            break;
    }

    return hasher_finalize(hasher);
}

bool element_inner_equal(const element_inner_t *a, const element_inner_t *b) {
    if(a->length != b->length) return false; // Optimization, not necessary

    if(a->type != b->type) return false;

    switch(a->type) {
        case ELEMENT_TYPE_TRIVIA: {
            if(a->trivia.kind != b->trivia.kind) return false;
            if(a->trivia.text != b->trivia.text) return false;
            break;
        }
        case ELEMENT_TYPE_TOKEN: {
            if(a->token.kind != b->token.kind) return false;
            if(a->token.text != b->token.text) return false;

            if(a->token.leading_trivia_count != b->token.leading_trivia_count) return false;
            if(a->token.trailing_trivia_count != b->token.trailing_trivia_count) return false;
            if(a->token.leading_trivia_length != b->token.leading_trivia_length) return false; // Optimization, not necessary
            if(a->token.trailing_trivia_length != b->token.trailing_trivia_length) return false; // Optimization, not necessary
            for(size_t i = 0; i < a->token.leading_trivia_count + a->token.trailing_trivia_count; i++) {
                if(a->token.trivia[i] != b->token.trivia[i]) return false;
            }
            break;
        }
        case ELEMENT_TYPE_NODE: {
            if(a->node.kind != b->node.kind) return false;

            if(a->node.child_count != b->node.child_count) return false;
            for(size_t i = 0; i < a->node.child_count; i++) {
                if(a->node.children[i] != b->node.children[i]) return false;
            }
            break;
        }
    }

    return true;
}

const element_inner_t *element_inner_make_trivia(db_t *db, charon_trivia_kind_t kind, text_t *text) {
    element_inner_t element = {
        .length = text == nullptr ? 0 : text->size,
        .type = ELEMENT_TYPE_TRIVIA,
        .trivia = { .kind = kind, .text = text },
    };

    // TODO: extremely iffy about how text is handled here
    // what lifetime does it have coming in, should we intern it here?

    return interner_intern(db->element_interner, &element);
}

const element_inner_t *element_inner_make_token(db_t *db, charon_token_kind_t kind, text_t *text, size_t leading_trivia_count, size_t trailing_trivia_count, const element_inner_t *trivia[]) {
    element_inner_t *element = malloc(sizeof(element_inner_t) + (leading_trivia_count + trailing_trivia_count) * sizeof(element_inner_t *));
    element->length = text == nullptr ? 0 : text->size;
    element->type = ELEMENT_TYPE_TOKEN;
    element->token.kind = kind;
    element->token.text = text;
    element->token.leading_trivia_count = leading_trivia_count;
    element->token.trailing_trivia_count = trailing_trivia_count;
    element->token.leading_trivia_length = 0;
    element->token.trailing_trivia_length = 0;
    for(size_t i = 0; i < leading_trivia_count + trailing_trivia_count; i++) {
        element->token.trivia[i] = trivia[i];
        element->length += trivia[i]->length;
        if(i < leading_trivia_count) {
            element->token.leading_trivia_length += trivia[i]->length;
        } else {
            element->token.trailing_trivia_length += trivia[i]->length;
        }
    }


    // TODO: extremely iffy about how text is handled here
    // what lifetime does it have coming in, should we intern it here?
    //
    // also should the trivia be interned here too instead of pointers

    const element_inner_t *interned_element = interner_intern(db->element_interner, &element);
    free(element);
    return interned_element;
}

const element_inner_t *element_inner_make_node(db_t *db, charon_node_kind_t kind, size_t child_count, const element_inner_t *children[]) {
    element_inner_t *element = malloc(sizeof(element_inner_t) + child_count * sizeof(element_inner_t *));
    element->length = 0;
    element->type = ELEMENT_TYPE_NODE;
    element->node.kind = kind;
    element->node.child_count = child_count;
    for(size_t i = 0; i < child_count; i++) {
        element->length += children[i]->length;
        element->node.children[i] = children[i];
    }

    // TODO: should children be interned here

    const element_inner_t *interned_element = interner_intern(db->element_interner, &element);
    free(element);
    return interned_element;
}

// charon_element_t *charon_element_wrap_root(charon_memory_allocator_t *allocator, const charon_element_inner_t *inner_root) {
//     charon_element_t *element = charon_memory_allocate(allocator, sizeof(charon_element_t));
//     element->inner = inner_root;
//     element->parent = NULL;
//     element->offset = 0;
//     element->self_index = 0;
//     return element;
// }

// charon_element_t *charon_element_wrap_node_child(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
//     charon_element_t *child = charon_memory_allocate(allocator, sizeof(charon_element_t));
//     child->inner = charon_element_node_child(element->inner, index);
//     child->parent = element;
//     child->offset = element->offset;
//     child->self_index = index;
//     for(size_t i = 0; i < index; i++) child->offset += element->inner->node.children[i]->length;

//     return child;
// }

// charon_element_t *charon_element_wrap_token_leading_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
//     assert(index < element->inner->token.leading_trivia_count);
//     return token_trivia_wrap(allocator, element, index);
// }

// charon_element_t *charon_element_wrap_token_trailing_trivia(charon_memory_allocator_t *allocator, charon_element_t *element, size_t index) {
//     assert(index < element->inner->token.trailing_trivia_count);
//     return token_trivia_wrap(allocator, element, element->inner->token.leading_trivia_count + index);
// }

element_type_t charon_element_type(const element_inner_t *inner_element) {
    return inner_element->type;
}

size_t charon_element_length(const element_inner_t *inner_element) {
    return inner_element->length;
}

const text_t *charon_element_trivia_text(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TRIVIA);
    return inner_element->trivia.text;
}

charon_trivia_kind_t charon_element_trivia_kind(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TRIVIA);
    return inner_element->trivia.kind;
}

const text_t *charon_element_token_text(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.text;
}

charon_token_kind_t charon_element_token_kind(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.kind;
}

size_t charon_element_token_leading_trivia_count(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.leading_trivia_count;
}

size_t charon_element_token_trailing_trivia_count(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.trailing_trivia_count;
}

size_t charon_element_token_leading_trivia_length(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.leading_trivia_length;
}

size_t charon_element_token_trailing_trivia_length(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_TOKEN);
    return inner_element->token.trailing_trivia_length;
}

const element_inner_t *charon_element_token_leading_trivia(const element_inner_t *inner_element, size_t index) {
    assert(index < inner_element->token.leading_trivia_count);
    return token_trivia(inner_element, index);
}

const element_inner_t *charon_element_token_trailing_trivia(const element_inner_t *inner_element, size_t index) {
    assert(index < inner_element->token.trailing_trivia_count);
    return token_trivia(inner_element, inner_element->token.leading_trivia_count + index);
}

charon_node_kind_t charon_element_node_kind(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_NODE);
    return inner_element->node.kind;
}

size_t charon_element_node_child_count(const element_inner_t *inner_element) {
    assert(inner_element->type == ELEMENT_TYPE_NODE);
    return inner_element->node.child_count;
}

const element_inner_t *charon_element_node_child(const element_inner_t *inner_element, size_t index) {
    assert(inner_element->type == ELEMENT_TYPE_NODE);
    assert(index < inner_element->node.child_count);
    return inner_element->node.children[index];
}
