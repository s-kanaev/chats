#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "io-service.h"
#include "thread-pool.h"
#include "common.h"

#define THREAD_COUNT 2
#define JOB_COUNT 100

typedef struct {
    pthread_mutex_t mtx;
    char chr;
    thread_pool_t *tp;
    io_service_t *iosvc;
} context;

static void progress(void *ctx_) {
    context *ctx = ctx_;
    usleep(1000000);
    pthread_mutex_lock(&ctx->mtx);
    if (ctx->chr == 0) {
        fprintf(stdout, ".");
        fflush(stdout);
        thread_pool_post_job(ctx->tp, progress, ctx_);
    }
    pthread_mutex_unlock(&ctx->mtx);
}

static void do_input(void *ctx_) {
    context *ctx = ctx_;
    pthread_mutex_lock(&ctx->mtx);
    fscanf(stdin, "%c", &ctx->chr);
    io_service_stop(ctx->iosvc, false);
    pthread_mutex_unlock(&ctx->mtx);
}

static void input(int fd, io_svc_op_t op, void *ctx_) {
    context *ctx = ctx_;
    thread_pool_post_job(ctx->tp, do_input, ctx_);
}

int main(void) {
    context ctx;

    memset(&ctx, 0, sizeof(ctx));

    ctx.tp = thread_pool_init(THREAD_COUNT);
    ctx.iosvc = io_service_init();
    pthread_mutex_init(&ctx.mtx, NULL);

    io_service_post_job(ctx.iosvc, STDIN_FILENO, IO_SVC_OP_READ, input, &ctx);
    thread_pool_post_job(ctx.tp, progress, (void *)&ctx);

    io_service_run(ctx.iosvc);

    fprintf(stdout, "You've entered: %c\n", ctx.chr);

    pthread_mutex_destroy(&ctx.mtx);
    io_service_deinit(ctx.iosvc);
    thread_pool_stop(ctx.tp, true);

    return 0;
}