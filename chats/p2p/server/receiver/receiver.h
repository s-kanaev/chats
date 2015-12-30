#ifndef _P2P_MU_RECEIVER_H_
# define _P2P_MU_RECEIVER_H_

# include "thread-pool.h"
# include "io-service.h"

/** Singleton */

/* API */
void receiver_init(thread_pool_t *tp,
                   io_service_t *iosvc,
                   const char *addr, const char *port,
                   int connection_backlog);
void receiver_deinit(void);
void receiver_run(void);

/* for use by watchdog */
void receiver_disconnect_client(void *descr);

#endif /* _P2P_MU_RECEIVER_H_ */