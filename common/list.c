#include "list.h"
#include "memory.h"

#include <stddef.h>

/* this struct prepends every user data instance */
struct list_element {
    struct list_element *prev;
    struct list_element *next;
    list_t *host;
};

struct list {
    size_t count;
    size_t element_size;

    struct list_element *first;
    struct list_element *last;
};

static struct list_element *list_allocate(list_t *l) {
    struct list_element *le = allocate(
        l->element_size + sizeof(struct list_element)
    );

    le->host = l;
}

static struct list_element *get_list_element(void *d) {
    return (d - sizeof(struct list_element));
}

list_t *list_init(size_t element_size) {
    list_t *l = allocate(sizeof(list_t));

    if (!l) return NULL;

    l->element_size = element_size;
    l->count = 0;
    l->first = l->last = NULL;

    return l;
}

void *list_deinit(list_t *l) {
    struct list_element *el, *el2;

    if (!l) return;

    for (el = l->first; el; el = el2) {
        el2 = el->next;
        deallocate(el);
    }

    deallocate(l);
}

size_t list_size(list_t *l) {
    if (!l) return;
    return l->count;
}

void *list_append(list_t *l) {
    struct list_element *le;
    void *p;

    if (!l) return NULL;

    le = list_allocate(l);
    if (!le) return NULL;

    p = (void *)(le + 1);

    le->next = NULL;
    ++l->count;
    if (l->count == 1) {
        le->prev = NULL;
        l->first = l->last = le;
        return p;
    }

    le->prev = l->last;
    l->last->next = le;
    l->last = le;
    return p;
}

void *list_prepend(list_t *l) {
    struct list_element *le;
    void *p;

    if (!l) return NULL;

    le = list_allocate(l);
    if (!le) return NULL;

    p = (void *)(le + 1);

    le->prev = NULL;
    ++l->count;
    if (l->count == 1) {
        le->next = NULL;
        l->first = l->last = le;
        return p;
    }

    le->next = l->first;
    l->first->prev = le;
    l->first = le;
    return p;
}

void *list_add_after(list_t *l, void *el) {
    struct list_element *le_el, *le;
    void *p;

    if (!l) return NULL;

    if (el == NULL) return list_append(l);

    le_el = get_list_element(el);
    if (le_el->host != l) return NULL;

    le = list_allocate(l);
    if (!le) return NULL;

    p = (void *)(le + 1);

    le->next = le_el->next;
    le->prev = le_el;
    le_el->next = le;

    if (le->next) le->next->prev = le;
    if (el == l->last) l->last = el;

    ++l->count;

    return p;
}

void *list_add_before(list_t *l, void *el) {
    struct list_element *le_el, *le;
    void *p;

    if (!l) return NULL;

    if (el == NULL) return list_append(l);

    le_el = get_list_element(el);
    if (le_el->host != l) return NULL;

    le = list_allocate(l);
    if (!le) return NULL;

    p = (void *)(le + 1);

    le->prev = le_el->prev;
    le->next = le_el;
    le_el->prev = le;

    if (le->prev) le->prev->next = le;
    if (el == l->first) l->first = el;

    ++l->count;

    return p;
}

void *list_remove_next(list_t *l, void *el) {
    struct list_element *le_el, *next, *prev;
    void *p = NULL;

    if (!l) return NULL;

    le_el = get_list_element(el);
    if (le_el->host != l) return NULL;

    next = le_el->next;
    prev = le_el->prev;

    if (next) {
        p = (void *)(next + 1);
        next->prev = prev;
    }
    if (prev) prev->next = next;

    deallocate(le_el);
    if (!(--l->count)) l->first = l->last = NULL;
    else if (le_el == l->last) l->last = prev;
    else if (le_el == l->first) l->first = next;

    return p;
}

void *list_remove_prev(list_t *l, void *el) {
    struct list_element *le_el, *next, *prev;
    void *p = NULL;

    if (!l) return NULL;

    le_el = get_list_element(el);
    if (le_el->host != l) return NULL;

    next = le_el->next;
    prev = le_el->prev;

    if (next) next->prev = prev;
    if (prev) {
        p = (void *)(prev + 1);
        prev->next = next;
    }

    deallocate(le_el);
    if (!(--l->count)) l->first = l->last = NULL;
    else if (le_el == l->last) l->last = prev;
    else if (le_el == l->first) l->first = next;

    return p;
}

void list_remove_element(list_t *l, void *el) {
    struct list_element *le_el, *next, *prev;

    if (!l) return;

    le_el = get_list_element(el);
    if (le_el->host != l) return;

    next = le_el->next;
    prev = le_el->prev;

    if (next) next->prev = prev;
    if (prev) prev->next = next;

    deallocate(le_el);
    if (!(--l->count)) l->first = l->last = NULL;
    else if (le_el == l->last) l->last = prev;
    else if (le_el == l->first) l->first = next;
}

void *list_first_element(list_t *l) {
    return (void *)(l->first ? l->first + 1 : NULL);
}

void *list_last_element(list_t *l) {
    return (void *)(l->last ? l->last + 1 : NULL);
}

void *list_next_element(list_t *l, void *el) {
    struct list_element *le_el, *e;

    if (!el) return NULL;
    le_el = get_list_element(el);
    e = le_el->next;

    return (void *)(e ? e + 1 : NULL);
}

void *list_prev_element(list_t *l, void *el) {
    struct list_element *le_el, *e;

    if (!el) return NULL;
    le_el = get_list_element(el);
    e = le_el->prev;

    return (void *)(e ? e + 1 : NULL);
}
