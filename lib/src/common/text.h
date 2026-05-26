#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t size;
    uint8_t data[];
} text_t;

bool text_equal(const text_t *a, const text_t *b);
uint64_t text_hash(const text_t *text);
