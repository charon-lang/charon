#include "log.h"

#include "charon/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define BASH_COLOR(CODE) "\e[" CODE "m"
#define BASH_COLOR_RESET BASH_COLOR("0")
#define BASH_COLOR_RED BASH_COLOR("91")
#define BASH_COLOR_YELLOW BASH_COLOR("93")
#define BASH_COLOR_CYAN BASH_COLOR("36")

static charon_log_callback_t g_log_callback = NULL;

static void default_logger(charon_log_level_t level, const char *message, va_list args) {
    switch(level) {
        case CHARON_LOG_LEVEL_DEBUG: printf(BASH_COLOR_CYAN "DEBUG "); break;
        case CHARON_LOG_LEVEL_WARN:  printf(BASH_COLOR_YELLOW "WARN "); break;
        case CHARON_LOG_LEVEL_FATAL: printf(BASH_COLOR_RED "FATAL "); break;
    }
    printf(BASH_COLOR_RESET);
    vprintf(message, args);
    printf("\n");
}

void charon_set_logger(charon_log_callback_t fn) {
    g_log_callback = fn;
}

[[noreturn]] void log_fatal(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    (g_log_callback == NULL ? default_logger : g_log_callback)(CHARON_LOG_LEVEL_FATAL, fmt, list);
    va_end(list);
    exit(EXIT_FAILURE);
}

void log_warning(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    (g_log_callback == NULL ? default_logger : g_log_callback)(CHARON_LOG_LEVEL_WARN, fmt, list);
    va_end(list);
}

void log_debug(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    (g_log_callback == NULL ? default_logger : g_log_callback)(CHARON_LOG_LEVEL_DEBUG, fmt, list);
    va_end(list);
}
