#include "memory.h"
#include <stdlib.h>
#include <stdbool.h>

typedef bool buffer_realloc_t(buffer_t **b, size_t new_size);

struct buffer {
    buffer_policy_t pol;
    size_t real_size;
    size_t user_size;
    /* the data is just after describing struct */
};

buffer_realloc_t buffer_realloc_no_shrink;
buffer_realloc_t buffer_realloc_shrink;

static const buffer_realloc_t *buffer_realocator[buffer_policy_count] = {
    [buffer_policy_shrink] = buffer_realloc_shrink,
    [buffer_policy_no_shrink] = buffer_realloc_no_shrink
};

void *allocate(size_t size) {
    return malloc(size);
}

void deallocate(void *d) {
    return free(d);
}

void *reallocate(void *d, size_t new_size) {
    return realloc(d, new_size);
}

buffer_t *buffer_init(size_t initial_size, buffer_policy_t pol) {
    buffer_t *b = allocate(initial_size + sizeof(buffer_t));
    if (!b) return NULL;

    b->pol = pol;
    b->real_size = b->user_size = initial_size;
}

void *buffer_data(buffer_t *b) {
    return (void *)(b + 1);
}

size_t buffer_size(buffer_t *b) {
    return b->user_size;
}

size_t buffer_size_real(buffer_t *b) {
    return b->real_size;
}

bool buffer_resize(buffer_t **b, size_t new_size) {
    return buffer_realocator[(*b)->pol](b, new_size);
}

void buffer_deinit(buffer_t *b) {
    deallocate(b);
}

bool buffer_realloc_no_shrink(buffer_t **b, size_t new_size) {
    buffer_t *new_b;
    if (new_size < (*b)->user_size) return true;

    new_b = reallocate((*b), new_size + sizeof(buffer_t));
    if (!new_b) return false;

    *b = new_b;
    return true;
}

bool buffer_realloc_shrink(buffer_t **b, size_t new_size) {
    buffer_t *new_b = reallocate((*b), new_size + sizeof(buffer_t));

    if (!new_b) return false;

    *b = new_b;
    return true;
}
