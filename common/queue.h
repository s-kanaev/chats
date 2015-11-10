#ifndef _CHATS_COMMON_LIB_QUEUE_H_
# define _CHATS_COMMON_LIB_QUEUE_H_

# include "list.h"
# include <stddef.h>

typedef list_t queue_t;

queue_t *queue_init(size_t element_size);
void *queue_push(queue_t *q);
void *queue_front(queue_t *q);
void queue_pop(queue_t *q);
size_t queue_size(queue_t *q);
void queue_deinit(queue_t *q);

#endif /* _CHATS_COMMON_LIB_QUEUE_H_ */
