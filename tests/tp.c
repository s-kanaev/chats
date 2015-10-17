#include <stdio.h>
#include <stdbool.h>

#include "thread-pool.h"
#include "common.h"

#define THREAD_COUNT 10
#define JOB_COUNT 100

static void r(void *ctx) {
    fprintf(stdout, "%s: %d\n", __func__, (int)ctx);
}

int main(void) {
    thread_pool_t *tp = thread_pool_init(THREAD_COUNT);
    size_t idx;

    for (idx = 0; idx < JOB_COUNT; ++idx) {
        fprintf(stderr, "Posting: %lu job\n", idx);
        thread_pool_post_job(tp, r, (void *)idx);
    }

    thread_pool_stop(tp, true);

    return 0;
}