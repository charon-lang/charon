#pragma once

#include "lib/diag.h"

#include <setjmp.h>

typedef struct context_recovery_boundary {
    jmp_buf jmpbuf;
    struct context_recovery_boundary *parent;
} context_recovery_boundary_t;

typedef struct {
    vector_diag_item_t diag_items;

    struct {
        diag_t *associated_diag;
        context_recovery_boundary_t *boundary;
    } recovery;
} context_t;

extern __thread context_t *g_global_context;

void context_global_initialize();
void context_global_finish();

context_recovery_boundary_t *context_recover_boundary_push();
void context_recover_boundary_pop();
[[noreturn]] void context_recover_to_boundary();
