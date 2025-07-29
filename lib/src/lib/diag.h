#pragma once

#include "charon/diag.h"
#include "lib/lang.h"
#include "lib/source.h"
#include "lib/vector.h"

#include <stdlib.h>

typedef enum charon_diag_type diag_type_t;
typedef struct charon_diag diag_t;

typedef struct {
    diag_t diagnostic;
    source_location_t location;
} diag_item_t;

VECTOR_DEFINE_EXT(diag_item, diag_item_t *, reallocarray);

[[noreturn]] void diag_error(source_location_t source_location, lang_t fmt, ...);
void diag_warn(source_location_t source_location, lang_t fmt, ...);

diag_t *diag(source_location_t source_location, diag_t diag);
