#include "list.h"
#include "memory.h"

void *list_init(size_t size) {
    list_entry_t *head = allocate(size);
    head->next = head->prev = NULL;

    return (void *)head;
}

void *list_add_element(list_entry_t *le, size_t size) {
    if (!le) return list_init(size);

    list_entry_t *new_le = allocate(size);
    new_le->next = le->next;
    new_le->prev = le;
    le->next = new_le;

    if (new_le->next) new_le->next->prev = new_le;

    return (void *)new_le;
}

void *list_remove_element(list_entry_t *le) {
    if (!le) return NULL;

    list_entry_t *prev = le->prev;
    list_entry_t *next = le->next;

    if (prev) prev->next = next;
    if (next) next->prev = prev;

    deallocate(le);

    return next;
}

void list_purge(list_entry_t *head, void (*function) (void *)) {
    list_entry_t *le;

    if (!head) return;

    while (head) {
        le = head->next;

        if (function) function(head);
        deallocate(head);

        head = le;
    }
}
