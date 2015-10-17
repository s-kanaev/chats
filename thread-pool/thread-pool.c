#include "thread-pool.h"
#include "common.h"

#include <stdbool.h>
#include <pthread.h>

typedef struct job {
    tp_job_function_t job;
    void *ctx;
} job_t;

typedef struct thread_descr {
    pthread_attr_t attr;
    pthread_t id;
} thread_descr_t;

struct thread_pool {
    list_t *queue;
    pthread_mutex_t job_mutex;                              ///< queue access mtx
    pthread_cond_t job_semaphore;                           ///< thread run semaphore
    pthread_cond_t job_end_semaphore;
    bool run;                                               ///< should threads run any more
    bool allow_new_jobs;
    size_t thread_count;
    thread_descr_t *thread_descr;
};

static void push_job(thread_pool_t *tp, tp_job_function_t job, void *ctx) {
    job_t *job_el = list_append(tp->queue);

    if (!job_el) return;

    job_el->job = job;
    job_el->ctx = ctx;
}

static void get_and_pop_job(thread_pool_t *tp, tp_job_function_t *job, void **ctx) {
    job_t *job_el = list_first_element(tp->queue);
    if (!job_el) {
        *job = NULL;
        *ctx = NULL;
        return;
    }

    *job = job_el->job;
    *ctx = job_el->ctx;

    list_remove_element(tp->queue, job_el);
    if (!list_size(tp->queue)) pthread_cond_broadcast(&tp->job_end_semaphore);
}

static void *worker_tpl(void *_tp) {
    thread_pool_t *tp = (thread_pool_t *)_tp;
    pthread_mutex_t *job_mutex = &tp->job_mutex;
    pthread_cond_t *job_semaphore = &tp->job_semaphore;
    tp_job_function_t job;
    void *ctx;

    while (true) {
        pthread_mutex_lock(job_mutex);
        while (tp->run && (list_size(tp->queue) == 0))
            pthread_cond_wait(job_semaphore, job_mutex);

        if (!tp->run) break;

        get_and_pop_job(tp, &job, &ctx);
        pthread_mutex_unlock(job_mutex);

        if (job) (*job)(ctx);
    }

    pthread_mutex_unlock(job_mutex);
    return NULL;
}

thread_pool_t *thread_pool_init(size_t thread_count) {
    size_t idx;
    thread_pool_t *tp = allocate(sizeof(thread_pool_t));
    thread_descr_t *td;

    tp->queue = list_init(sizeof(job_t));

    pthread_mutex_init(&tp->job_mutex, NULL);
    pthread_cond_init(&tp->job_semaphore, NULL);
    pthread_cond_init(&tp->job_end_semaphore, NULL);

    tp->run = tp->allow_new_jobs = true;
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

void thread_pool_stop(thread_pool_t *tp, bool wait_for_stop) {
    size_t idx, tc = tp->thread_count;

    pthread_mutex_lock(&tp->job_mutex);
    tp->run = wait_for_stop;
    tp->allow_new_jobs = false;
    pthread_cond_broadcast(&tp->job_semaphore);

    if (wait_for_stop && (list_size(tp->queue) != 0))
        pthread_cond_wait(&tp->job_end_semaphore, &tp->job_mutex);

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
    pthread_cond_destroy(&tp->job_end_semaphore);

    deallocate(tp->thread_descr);
    list_deinit(tp->queue);
    deallocate(tp);
}

void thread_pool_post_job(thread_pool_t *tp, tp_job_function_t job, void *ctx) {
    pthread_mutex_lock(&tp->job_mutex);
    if (tp->allow_new_jobs) {
        push_job(tp, job, ctx);
        pthread_cond_broadcast(&tp->job_semaphore);
    }
    pthread_mutex_unlock(&tp->job_mutex);
}
