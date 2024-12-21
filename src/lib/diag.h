#pragma once

#include "lib/source.h"
#include "lib/lang.h"

[[noreturn]] void diag_error(source_location_t source_location, lang_t fmt, ...);
void diag_warn(source_location_t source_location, lang_t fmt, ...);