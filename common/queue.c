#include "queue.h"
#include "list.h"
#include "memory.h"

#include <stddef.h>
#include <assert.h>

queue_t *queue_init(size_t element_size) {
    queue_t *q = list_init(element_size);

    return q;
}

void queue_deinit(queue_t *q) {
    if (q) list_deinit(q);
}

size_t queue_size(queue_t *q) {
    return q ? list_size(q) : 0;
}

void *queue_front(queue_t *q) {
    return q ? list_first_element(q) : NULL;
}

void queue_pop(queue_t *q) {
    if (q) list_remove_element(q, queue_front(q));
}

void *queue_push(queue_t *q) {
    return q ? list_append(q) : NULL;
}
