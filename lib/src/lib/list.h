#pragma once

#include <stddef.h>
#include <stdint.h>

#define LIST_INIT ((list_t) { .count = 0, .head = nullptr, .tail = nullptr })

/// Iterate over a list.
/// @param LIST `list_t` to iterate over
/// @param NODE `list_node_t *` iterator name
#define LIST_ITERATE(LIST, NODE) for(list_node_t * (NODE) = (LIST)->head; (NODE) != nullptr; (NODE) = (NODE)->next)

/// Get the container of a child struct.
/// @param PTR Pointer to the child struct
/// @param TYPE Type of the container
/// @param MEMBER Name of the child member in the container
#define LIST_CONTAINER_OF(PTR, TYPE, MEMBER)                                                                                           \
    ({                                                                                                                                 \
        static_assert(__builtin_types_compatible_p(typeof(((TYPE *) 0)->MEMBER), typeof(*PTR)), "member type does not match pointer"); \
        (TYPE *) (((uintptr_t) (PTR)) - __builtin_offsetof(TYPE, MEMBER));                                                             \
    })

typedef struct list_node list_node_t;
typedef struct list list_t;

struct list {
    size_t count;
    list_node_t *head, *tail;
};

struct list_node {
    list_node_t *next, *prev;
};

/// Push a node to the front of the list.
void list_push_front(list_t *list, list_node_t *node);

/// Push a node to the back of the list.
void list_push_back(list_t *list, list_node_t *node);

/// Alias of `list_push_font`.
void list_push(list_t *list, list_node_t *node);

/// Pop the head of the list.
list_node_t *list_pop_front(list_t *list);

/// Pop the tail of the list.
list_node_t *list_pop_back(list_t *list);

/// Alias of `list_pop_front`.
list_node_t *list_pop(list_t *list);

/// Insert a node after another node in a list.
/// @param pos The "position", insert the new node after this node
/// @param node The new node
void list_node_append(list_t *list, list_node_t *pos, list_node_t *node);

/// Insert a node before another node in a list.
/// @param pos The "position", insert the new node before this node
/// @param node The new node
void list_node_prepend(list_t *list, list_node_t *pos, list_node_t *node);

/// Remove a node off of a list.
void list_node_delete(list_t *list, list_node_t *node);
