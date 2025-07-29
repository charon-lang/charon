#include "context.h"

#include "lib/diag.h"
#include "lib/vector.h"

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>

__thread context_t *g_global_context = NULL;

void context_global_initialize() {
    assert(g_global_context == NULL);
    g_global_context = malloc(sizeof(context_t));
    g_global_context->diag_items = vector_diag_item_init();
    g_global_context->recovery.boundary = NULL;
    g_global_context->recovery.associated_diag = NULL;
}

void context_global_finish() {
    assert(g_global_context != NULL);
    assert(g_global_context->recovery.boundary == NULL);

    VECTOR_FOREACH(&g_global_context->diag_items, i, diag_item) free(diag_item);
    free(g_global_context->diag_items.values);

    free(g_global_context);
    g_global_context = NULL;
}

context_recovery_boundary_t *context_recover_boundary_push() {
    assert(g_global_context != NULL);
    context_recovery_boundary_t *boundary = malloc(sizeof(context_recovery_boundary_t));
    boundary->parent = g_global_context->recovery.boundary;
    g_global_context->recovery.boundary = boundary;
    return boundary;
}

void context_recover_boundary_pop() {
    assert(g_global_context != NULL);
    context_recovery_boundary_t *boundary = g_global_context->recovery.boundary;
    assert(boundary != NULL);
    g_global_context->recovery.boundary = boundary->parent;
    free(boundary);
}

[[noreturn]] void context_recover_to_boundary() {
    assert(g_global_context != NULL);
    assert(g_global_context->recovery.boundary != NULL);
    longjmp(g_global_context->recovery.boundary->jmpbuf, 1);
}
