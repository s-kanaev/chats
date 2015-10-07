#include "thread-pool.h"
#include "lib.h"

#include <stdbool.h>
#include <pthread.h>

struct job {
    list_entry_t le;
    job_function_t job;
    void *ctx;
};

struct thread_descr {
    pthread_attr_t attr;
    pthread_t id;
};

struct thread_pool {
    job_t *queue_head;
    job_t *queue_tail;                                      ///< append jobs here
    size_t queue_size;
    pthread_mutex_t job_mutex;                              ///< queue access mtx
    pthread_cond_t job_semaphore;                           ///< thread run semaphore
    bool run;                                               ///< should threads run any more
    size_t thread_count;
    thread_descr_t *thread_descr;
};

static void push_job(thread_pool_t *tp, job_function_t job, void *ctx) {
    list_entry_t *tail = &tp->queue_tail->le;
    job_t *new_job = list_add(tail, sizeof(job_t));

    if (NULL == new_job) return;

    new_job->job = job;
    new_job->ctx = ctx;
    tp->queue_tail = new_job;
}

static void get_and_pop_job(thread_pool_t *tp, job_function_t *job, void **ctx) {
    job_t *head = tp->queue_head;
    if (!head) {
        *job = NULL;
        *ctx = NULL;
        return;
    }

    *job = head->job;
    *ctx = head->ctx;

    head = remove_from_list((list_entry_t *)head);

    tp->queue_head = head;

    if (!head) tp->queue_tail = NULL;
}

static void *worker_tpl(void *_tp) {
    thread_pool_t *tp = (thread_pool_t *)_tp;
    pthread_mutex_t *job_mutex = &tp->job_mutex;
    pthread_cond_t *job_semaphore = &tp->job_semaphore;
    job_function_t job;
    void *ctx;

    while (true) {
        pthread_mutex_lock(job_mutex);
        if (!tp->run) break;
        pthread_cond_wait(job_semaphore, job_mutex);
        if (!tp->run) break;

        get_and_pop_job(tp, &job, &ctx);
        pthread_mutex_unlock(job_mutex);

        (*job)(ctx);
    }

    pthread_mutex_unlock(job_mutex);
    return NULL;
}

thread_pool_t *thread_pool_init(size_t thread_count) {
    size_t idx;
    thread_pool_t *tp = allocate(sizeof(thread_pool_t));
    thread_descr_t *td;

    pthread_mutex_init(&tp->job_mutex, NULL);
    pthread_cond_init(&tp->job_semaphore, NULL);

    tp->run = true;
    tp->thread_count = thread_count;
    td = allocate(thread_count * sizeof(thread_descr_t));
    tp->thread_descr = td;

    for (idx = 0; idx < thread_count; ++idx, ++td) {
        pthread_attr_init(&td->attr);
        pthread_attr_setdetachstate(&td->attr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&td->id, &td->attr, worker_tpl, tp);
    }

    return tp;
}

void thread_pool_stop(thread_pool_t *tp) {
    size_t idx, tc = tp->thread_count;
    pthread_mutex_lock(&tp->job_mutex);
    tp->run = false;
    pthread_cond_broadcast(&tp->job_semaphore);
    pthread_mutex_unlock(&tp->job_mutex);

    for (idx = 0; idx < tc; ++idx) {
        void *p;
        pthread_join(tp->thread_descr[idx].id, &p);
        pthread_attr_destroy(&tp->thread_descr[idx].attr);
    }

    pthread_mutex_destroy(&tp->job_mutex);
    pthread_cond_destroy(&tp->job_semaphore);
}

void thread_pool_post_job(thread_pool_t *tp, job_function_t job, void *ctx) {
    pthread_mutex_lock(&tp->job_mutex);
    push_job(tp, job, ctx);
    pthread_cond_broadcast(&tp->job_semaphore);
    pthread_mutex_unlock(&tp->job_mutex);
}
