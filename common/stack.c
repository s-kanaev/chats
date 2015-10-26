#include "stack.h"

#include "queue.h"
#include "list.h"
#include "memory.h"

#include <stddef.h>
#include <assert.h>

stack_t *stack_init(size_t element_size) {
    stack_t *q = list_init(element_size);

    return q;
}

void stack_deinit(stack_t *q) {
    if (q) list_deinit(q);
}

size_t stack_size(stack_t *q) {
    return q ? list_size(q) : 0;
}

void *stack_top(stack_t *q) {
    return q ? list_first_element(q) : NULL;
}

void stack_pop(stack_t *q) {
    if (q) list_remove_element(q, stack_top(q));
}

void *stack_push(stack_t *q) {
    return q ? list_prepend(q) : NULL;
}
