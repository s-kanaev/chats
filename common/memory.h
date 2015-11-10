#ifndef _CHATS_COMMON_MEMORY_H_
# define _CHATS_COMMON_MEMORY_H_

# include <stdbool.h>
# include <stddef.h>

struct buffer;
typedef struct buffer buffer_t;

typedef enum buffer_policy_enum {
    buffer_policy_no_shrink,
    buffer_policy_shrink,
    buffer_policy_count
} buffer_policy_t;

void *allocate(size_t size);
void deallocate(void *d);
void *reallocate(void *d, size_t new_size);

buffer_t *buffer_init(size_t initial_size, buffer_policy_t pol);
bool buffer_resize(buffer_t **b, size_t new_size);
void *buffer_data(buffer_t *b);
size_t buffer_size(buffer_t *b);
size_t buffer_size_real(buffer_t *b);
void buffer_deinit(buffer_t *b);

#endif /* _CHATS_COMMON_MEMORY_H_ */
