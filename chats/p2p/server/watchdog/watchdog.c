#include "watchdog.h"

#include "thread-pool.h"
#include "io-service.h"
#include "timer.h"
#include "avl-tree.h"
#include "hash-map.h"
#include "hash-functions.h"

#include <stdbool.h>
#include <pthread.h>
#include <assert.h>

struct watchdog {
    bool initialized;
    bool running;
    tmr_t *absolute_timer;

    hash_map_t time_by_nickname;
    /* map absolute time (disconnection time) to time_by_nickname element */
    avl_tree_t timer_tree;

    pthread_mutex_t mtx;
} WATCHDOG = {
    .initialized = false,
    .running = false
};

void watchdog_init(thread_pool_t *tp, io_service_t *iosvc) {
    assert(!WATCHDOG.initialized);
}

