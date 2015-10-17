#include "timer.h"
#include "common.h"
#include "io-service.h"

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/timerfd.h>

struct tmr {
    int fd;
    bool armed;
    struct itimerspec spec;
    io_service_t *master;
    tmr_job_t job;
    void *ctx;
};

static void tmr_job_tpl(int fd, io_svc_op_t op, void *_ctx) {
    tmr_t *timer = _ctx;
    uint64_t stub;

    read(fd, &stub, sizeof(stub));

    timer->job(timer->ctx);

    io_service_post_job(timer->master,
                        fd, IO_SVC_OP_READ,
                        tmr_job_tpl, timer);
}

tmr_t* timer_init(io_service_t* iosvc) {
    tmr_t *timer = NULL;
    int fd;

    fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (fd < 0) return NULL;

    timer = allocate(sizeof(tmr_t));
    if (!timer) {
        close(fd);
        return NULL;
    }

    timer->fd = fd;
    timer->armed = false;
    timer->master = iosvc;

    io_service_post_job(iosvc, fd, IO_SVC_OP_READ, tmr_job_tpl, timer);

    return timer;
}

void timer_deinit(tmr_t* tmr) {
    timer_cancel(tmr);
    deallocate(tmr);
}

void timer_set_deadline(tmr_t *tmr,
                        time_t sec, long unsigned int nanosec,
                        tmr_job_t job, void *ctx) {
    tmr->spec.it_value.tv_sec = sec;
    tmr->spec.it_value.tv_nsec = nanosec;
    tmr->spec.it_interval.tv_sec = tmr->spec.it_interval.tv_nsec = 0;
    timerfd_settime(
        tmr->fd, 0,
        &tmr->spec,
        NULL
    );

    tmr->armed = true;
}

void timer_set_periodic(tmr_t *tmr,
                        time_t sec, long unsigned int nanosec,
                        tmr_job_t job, void *ctx) {
    tmr->spec.it_value.tv_sec = sec;
    tmr->spec.it_value.tv_nsec = nanosec;
    tmr->spec.it_interval.tv_sec = sec;
    tmr->spec.it_interval.tv_nsec = nanosec;
    timerfd_settime(
        tmr->fd, 0,
        &tmr->spec,
        NULL
    );

    tmr->armed = true;
}

void timer_set_absolute(tmr_t *tmr,
                        time_t sec, long unsigned int nanosec,
                        tmr_job_t job, void *ctx) {
    tmr->spec.it_value.tv_sec = sec;
    tmr->spec.it_value.tv_nsec = nanosec;
    tmr->spec.it_interval.tv_sec = tmr->spec.it_interval.tv_nsec = 0;
    timerfd_settime(
        tmr->fd, TFD_TIMER_ABSTIME,
        &tmr->spec,
        NULL
    );

    tmr->armed = true;
}

void timer_cancel(tmr_t *tmr) {
    io_service_remove_job(tmr->master,
                          tmr->fd, IO_SVC_OP_READ,
                          tmr_job_tpl, tmr);
}
