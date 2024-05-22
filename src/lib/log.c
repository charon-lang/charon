#include "log.h"

#include <stdio.h>
#include <stdlib.h>

#define BASH_COLOR(CODE) "\e[" CODE "m"
#define BASH_COLOR_RESET BASH_COLOR("0")
#define BASH_COLOR_RED BASH_COLOR("91")
#define BASH_COLOR_YELLOW BASH_COLOR("93")

[[noreturn]] void log_fatal(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    printf(BASH_COLOR_RED "FATAL " BASH_COLOR_RESET);
    vprintf(fmt, list);
    printf("\n");
    va_end(list);
    exit(EXIT_FAILURE);
}

void log_warning(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    printf(BASH_COLOR_YELLOW "WARN " BASH_COLOR_RESET);
    vprintf(fmt, list);
    printf("\n");
    va_end(list);
}