#ifndef _COMMON_LIB_STACK_H_
# define _COMMON_LIB_STACK_H_

# include "list.h"
# include <stddef.h>

typedef list_t stack_t;

stack_t *stack_init(size_t element_size);
void *stack_push(stack_t *s);
void *stack_top(stack_t *s);
void stack_pop(stack_t *s);
size_t stack_size(stack_t *s);
void stack_deinit(stack_t *s);

#endif /* _COMMON_LIB_STACK_H_ */