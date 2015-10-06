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

struct thread_descr;
typedef struct thread_descr thread_descr_t;

typedef struct thread_pool {
    job_t *queue_head;
    job_t *queue_tail;                                      ///< append jobs here
    size_t queue_size;
    pthread_mutex_t job_mutex;                              ///< queue access mtx
    pthread_cond_t job_semaphore;                           ///< thread run semaphore
    bool run;                                               ///< should threads run any more
    size_t thread_count;
    thread_descr_t *thread_descr;
} thread_pool_t;

thread_pool_t *thread_pool_init(size_t thread_count);
void thread_pool_stop(thread_pool_t *tp);
void thread_pool_post_job(thread_pool_t *tp, job_function_t job, void *ctx);

#endif /* _THREAD_POOL_ */
