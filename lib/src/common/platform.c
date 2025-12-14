#include "include/charon/platform.h"

#ifdef __APPLE__
void *reallocarray(void *ptr, size_t nmemb, size_t size) {
    size_t total;
    if(__builtin_mul_overflow(nmemb, size, &total)) {
        errno = ENOMEM;
        return NULL;
    }

    return realloc(ptr, total);
}
#endif
