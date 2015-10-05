#ifndef _THREAD_POOL_
#define _THREAD_POOL_

# include "lib.h"

# include <stdbool.h>
# include <pthread.h>
/* Thread pool facilities */
typedef void (*job_function_t)(void *);

typedef struct job_t {
    list_entry_t le;
    job_function_t job;
    void *ctx;
};

typedef struct thread_pool {
    job_t *queue_head;
    job_t *queue_tail;                                      ///< append jobs here
    size_t queue_size;
    mutex job_mutex;                                        ///< queue access mtx
    semaphore job_semaphore;                                ///< thread run semaphore
    bool run;                                               ///< should threads run any more
    pthread_t *thread_id;                                   ///< run thread IDs
} thread_pool_t;

thread_pool_t *thread_pool_init(size_t thread_count);
void thread_pool_stop(thread_pool_t *tp);

#endif /* _THREAD_POOL_ */
