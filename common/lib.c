#include "lib.h"

#include <stdlib.h>

void *allocate(size_t size) {
    return malloc(size);
}

void deallocate(void *d) {
    return free(d);
}

void *list_init(size_t size) {
    list_entry_t *head = allocate(size);
    head->next = head->prev = NULL;

    return (void *)head;
}

void *list_add(list_entry_t *le, size_t size) {
    if (!le) return list_init(le, size);

    list_entry_t *new_le = allocate(size);
    new_le->next = le->next;
    new_le->prev = le;
    le->next = new_le;

    if (new_le->next) new_le->next->prev = new_le;

    return (void *)new_le;
}

void *remove_from_list(list_entry_t *le) {
    if (!le) return NULL;

    list_entry_t *prev = le->prev;
    list_entry_t *next = le->next;

    if (prev) prev->next = next;
    if (next) next->prev = prev;

    deallocate(le);

    return next;
}

void purge_list(list_entry_t *head, void (*function) (void *)) {
    list_entry_t *le;

    if (!head) return;

    while (head) {
        le = head->next;

        if (function) function(head);
        deallocate(head);

        head = le;
    }
}
