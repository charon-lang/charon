#pragma once

#include "lib/source.h"

[[noreturn]] void diag_error(source_location_t source_location, char *fmt, ...);
void diag_warn(source_location_t source_location, char *fmt, ...);