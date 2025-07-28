#pragma once

[[noreturn]] void log_fatal(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_debug(const char *fmt, ...);
