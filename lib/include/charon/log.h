#pragma once

#include "stdarg.h"

typedef enum charon_log_level charon_log_level_t;
typedef void (*charon_log_callback_t)(charon_log_level_t level, const char *message, va_list args);

enum charon_log_level {
    CHARON_LOG_LEVEL_DEBUG,
    CHARON_LOG_LEVEL_WARN,
    CHARON_LOG_LEVEL_FATAL,
};

void charon_set_logger(charon_log_callback_t fn);
