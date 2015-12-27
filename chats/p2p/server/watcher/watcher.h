#ifndef _P2P_MU_WATCHER_H_
# define _P2P_MU_WATCHER_H_

# include "common.h"
# include "io-service.h"

struct watcher;
typedef struct watcher watcher_t;

/** Watcher c-tor
 * \param host
 * \param port
 * \param timeout timeout in seconds to wait for PING message from clients
 * Will use \c host and \c port to watch for PING/QUIT messages from clients
 * and to send REF_ADD/REF_DEL messages to clients.
 */
watcher_t *watcher_init(const char *host, const char *port,
                        unsigned long timeout,
                        io_service_t *iosvc);
void watcher_add(watcher_t *watcher);
void watcher_remove(watcher_t *watcher);
void watcher_run(watcher_t *watcher);
void watcher_deinit(watcher_t *watcher);

#endif /* _P2P_MU_WATCHER_H_ */