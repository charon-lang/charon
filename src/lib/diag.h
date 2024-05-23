#pragma once

#include "lib/source.h"

#include <stddef.h>
#include <stdarg.h>

[[noreturn]] void diag_error(source_location_t srcloc, char *fmt, ...);
void diag_warn(source_location_t srcloc, char *fmt, ...);