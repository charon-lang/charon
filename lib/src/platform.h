#pragma once
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef __APPLE__
void *reallocarray(void *ptr, size_t nmemb, size_t size);
#endif
