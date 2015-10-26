#include "io-service.h"
#include "thread-pool.h"
#include "timer.h"
#include "common.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define THREAD_COUNT 2

typedef struct {
    pthread_mutex_t mtx;
    char chr;
    tmr_t *timer;
    thread_pool_t *tp;
    io_service_t *iosvc;
} context;

static void do_input(void *ctx_) {
    context *ctx = ctx_;
    char c;
    read(STDIN_FILENO, &c, 1);
    pthread_mutex_lock(&ctx->mtx);
    ctx->chr = c;
    io_service_stop(ctx->iosvc, false);
    pthread_mutex_unlock(&ctx->mtx);
}

static void timer_cb_(void *ctx_) {
    context *ctx = ctx_;
    pthread_mutex_lock(&ctx->mtx);
    if (ctx->chr == 0) {
        fprintf(stdout, ".");
        fflush(stdout);
    }
    pthread_mutex_unlock(&ctx->mtx);
}

static void timer_cb(void *ctx_) {
    context *ctx = ctx_;
    thread_pool_post_job(ctx->tp, timer_cb_, ctx_);
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
    ctx.timer = timer_init(ctx.iosvc);

    pthread_mutex_init(&ctx.mtx, NULL);

    timer_set_periodic(ctx.timer, 1, 0, timer_cb, &ctx);

    io_service_post_job(ctx.iosvc, STDIN_FILENO, IO_SVC_OP_READ, true, input, &ctx);
    //thread_pool_post_job(ctx.tp, progress, (void *)&ctx);

    io_service_run(ctx.iosvc);

    pthread_mutex_lock(&ctx.mtx);
    fprintf(stdout, "You've entered: %c, %02x\n", ctx.chr, (int)ctx.chr);

    timer_deinit(ctx.timer);
    io_service_deinit(ctx.iosvc);
    thread_pool_stop(ctx.tp, true);
    pthread_mutex_unlock(&ctx.mtx);

    pthread_mutex_destroy(&ctx.mtx);

    return 0;
}