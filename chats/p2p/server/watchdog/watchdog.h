#ifndef _P2P_MU_WATCHDOG_H_
# define _P2P_MU_WATCHDOG_H_

# include "thread-pool.h"
# include "io-service.h"

/** Singleton */

/* API */
void watchdog_init(thread_pool_t *tp,
                   io_service_t *iosvc);
void watchdog_deinit(void);
void watchdog_run(void);

/* for use by receivers */
void register_client(const char *nickname, size_t nickname_len,
                     const char *host, size_t host_len,
                     const char *port, size_t port_len);
void deregister_client(const char *nickname, size_t nickname_len);
void continue_registration(const char *nickname, size_t nickname_len);

#endif /* _P2P_MU_WATCHDOG_H_ */