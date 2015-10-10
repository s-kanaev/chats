#include "io-service.h"
#include "lib.h"

#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <sys/eventfd.h>
#include <sys/epoll.h>

typedef struct job {
    iosvc_job_function_t job;
    void *ctx;
} job_t;

typedef struct lookup_table_element {
    list_entry_t le;
    int fd;
    struct epoll_event event;
    job_t job[IO_SVC_OP_COUNT];
} lookup_table_element_t;

struct io_service {
    /* are we still running flag */
    bool allow_new;
    bool running;
    /* used for notification purposes */
    int event_fd;
    /* job list by fd lookup table  */
    lookup_table_element_t *lookup_table;
    size_t lookup_table_size;

    int epoll_fd;
    struct epoll_event event_fd_event;

    pthread_mutex_t object_mutex;
};

static const int OP_FLAGS[IO_SVC_OP_COUNT] = {
    [IO_SVC_OP_READ] = EPOLLIN,
    [IO_SVC_OP_WRITE] = EPOLLOUT
};

static
void notify_svc(int fd) {
    eventfd_write(fd, 1);
}

static
eventfd_t read_svc(int fd) {
    eventfd_t v;
    eventfd_read(fd, &v);
    return v;
}

io_service_t *io_service_init() {
    io_service_t *iosvc = allocate(sizeof(io_service_t));
    int r;

    memset(iosvc, 0, sizeof(io_service_t));

    r = pthread_mutex_init(&iosvc->object_mutex, NULL);

    if (r) {
        errno = r;
        return NULL;
    }

    iosvc->event_fd = eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);

    if (iosvc->event_fd < 0) {
        pthread_mutex_destroy(&iosvc->object_mutex);
        deallocate(iosvc);
        return NULL;
    }

    iosvc->lookup_table_size = 0;
    iosvc->lookup_table = NULL;
    iosvc->allow_new = iosvc->running = true;

    iosvc->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (iosvc->epoll_fd < 0) {
        close(iosvc->event_fd);
        pthread_mutex_destroy(&iosvc->object_mutex);
        deallocate(iosvc);
        return NULL;
    }

    memset(&iosvc->event_fd_event, 0, sizeof(iosvc->event_fd_event));

    iosvc->event_fd_event.events = EPOLLIN;
    iosvc->event_fd_event.data.fd = iosvc->event_fd;

    if (epoll_ctl(iosvc->epoll_fd, EPOLL_CTL_ADD, iosvc->event_fd, &iosvc->event_fd_event)) {
        io_service_deinit(iosvc);
        return NULL;
    }

    return iosvc;
}

void io_service_stop(io_service_t *iosvc, bool wait_pending) {
    pthread_mutex_lock(&iosvc->object_mutex);
    iosvc->allow_new = false;
    iosvc->running = wait_pending;
    notify_svc(iosvc->event_fd);
    pthread_mutex_unlock(&iosvc->object_mutex);
}

void io_service_deinit(io_service_t *iosvc) {
    pthread_mutex_destroy(&iosvc->object_mutex);
    close(iosvc->event_fd);
    close(iosvc->epoll_fd);

    purge_list((list_entry_t *)iosvc->lookup_table, NULL);

    deallocate(iosvc);
}

void io_service_post_job(io_service_t *iosvc,
                         int fd, io_svc_op_t op,
                         iosvc_job_function_t job,
                         void *ctx) {
    pthread_mutex_lock(&iosvc->object_mutex);

    if (iosvc->allow_new) {
        job_t *new_job;

        lookup_table_element_t *lte;
        for (lte = iosvc->lookup_table;
             lte;
             lte = (lookup_table_element_t *)lte->le.next)
            if (lte->fd == fd) break;

        if (!lte || lte->job[op].job == NULL) {
            if (!lte) {
                lte = list_add((list_entry_t *)iosvc->lookup_table,
                               sizeof(lookup_table_element_t));
                ++ iosvc->lookup_table_size;
            }

            if (lte) {
                lte->event.events |= OP_FLAGS[op];
                lte->job[op].job = job;
                lte->job[op].ctx = ctx;

                iosvc->lookup_table = lte;

                notify_svc(iosvc->event_fd);
            }
        }
    }

    pthread_mutex_unlock(&iosvc->object_mutex);
}

void io_service_run(io_service_t *iosvc) {
    pthread_mutex_t *mutex = &iosvc->object_mutex;
    bool *running = &iosvc->running;
    struct epoll_event event;
    int epoll_fd = iosvc->epoll_fd;
    int event_fd = iosvc->event_fd;
    int r, fd;
    ssize_t idx;
    io_svc_op_t op;
    lookup_table_element_t *lte;
    iosvc_job_function_t job;
    void *ctx;

    pthread_mutex_lock(mutex);
    while (*running) {
        /* refresh epoll fd list */
        for (lte = iosvc->lookup_table; lte;) {
            if (lte->event.events == 0) {
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, lte->fd, NULL);
                lte = remove_from_list((list_entry_t *)lte);

                if (-- iosvc->lookup_table_size) {
                    iosvc->lookup_table_size = 0;
                    break;
                }

                continue;
            }

            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, lte->fd, &lte->event)) {
                if (errno == ENOENT)
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, lte->fd, &lte->event);
            }

            lte = (lookup_table_element_t *)lte->le.next;
        }

        pthread_mutex_unlock(mutex);
        r = epoll_wait(epoll_fd, &event, 1, -1);
        pthread_mutex_lock(mutex);

        if (r < 0) continue;

        fd = event.data.fd;

        if (fd == event_fd) {
            read_svc(fd);
            continue;
        } /* if (fd == event_fd) */

        for (op = 0; op < IO_SVC_OP_COUNT; ++op) {
            if (!(event.events & OP_FLAGS[op])) continue;

            for (lte = iosvc->lookup_table; lte; lte = (lookup_table_element_t *)lte->le.next)
                if (lte->fd == fd) {
                    if (lte->job[op].job != NULL) break;
                    else {
                        lte = NULL;
                        break;
                    }
                }

            if (lte) {
                job = lte->job[op].job;
                ctx = lte->job[op].ctx;

                lte->job[op].job = NULL;
#if 0

                for (idx = 0; idx < IO_SVC_OP_COUNT; ++idx)
                    if (lte->job[idx].job != NULL)
                        events |= OP_FLAGS[idx];

                if (events == 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_from_list((list_entry_t *)lte);

                    if (-- iosvc->lookup_table_size) iosvc->lookup_table = NULL;
                }
                else {
                    struct epoll_event event;
                    memset(&event, 0, sizeof(event));
                    event.events = events;
                    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
                }

#endif
                pthread_mutex_unlock(mutex);
                (*job)(fd, op, ctx);
                pthread_mutex_lock(mutex);
            } /* if (lte) */
        }
    }

    pthread_mutex_unlock(mutex);
}
