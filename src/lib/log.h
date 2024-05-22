#pragma once

#include <stdarg.h>

[[noreturn]] void log_fatal(const char *fmt, ...);
void log_warning(const char *fmt, ...);