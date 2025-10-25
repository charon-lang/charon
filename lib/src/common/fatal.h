#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

[[noreturn]] static inline void fatal(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    vfprintf(stderr, fmt, list);
    va_end(list);
    exit(EXIT_FAILURE);
}
