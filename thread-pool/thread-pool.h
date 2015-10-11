#ifndef _THREAD_POOL_
#define _THREAD_POOL_

# include "lib.h"

# include <stdbool.h>

/* Thread pool facilities */
typedef void (*tp_job_function_t)(void *);

struct thread_pool;
typedef struct thread_pool thread_pool_t;

thread_pool_t *thread_pool_init(size_t thread_count);
void thread_pool_stop(thread_pool_t *tp, bool wait_for_stop);
void thread_pool_post_job(thread_pool_t *tp, tp_job_function_t job, void *ctx);

#endif /* _THREAD_POOL_ */
