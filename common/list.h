#ifndef _COMMON_LIB_H_
#define _COMMON_LIB_H_

# include <stddef.h>

struct list;
typedef struct list list_t;

list_t *list_init(size_t element_size);
void* list_append(list_t *l);
void* list_prepend(list_t *l);
void* list_add_after(list_t *l, void *el);
void* list_add_before(list_t *l, void *el);
void* list_remove_next(list_t *l, void *el);
void* list_remove_prev(list_t *l, void *el);
void list_remove_element(list_t *l, void *el);
void* list_first_element(list_t *l);
void* list_last_element(list_t *l);
void* list_next_element(list_t *l, void *el);
void* list_prev_element(list_t *l, void *el);
size_t list_size(list_t *l);
void* list_deinit(list_t *l);

#endif /* _COMMON_LIB_H_ */
