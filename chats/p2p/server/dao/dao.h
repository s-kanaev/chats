#ifndef _P2P_MU_DAO_H_
# define _P2P_MU_DAO_H_

# include "protocol.h"
# include "list.h"
# include <stddef.h>

struct dao;
typedef struct dao dao_t;

typedef struct dao_client {
    long long int id;
    char nickname[P2P_NICKNAME_LENGTH];
    char host[P2P_HOST_LENGTH];
    char port[P2P_PORT_LENGTH];
} dao_client_t;

/** initialize DAO */
dao_t *dao_init(const char *db_path);
/** deinitialize DAO */
void dao_deinit(dao_t *dao);

/** Fetch a list of clients from DB.
 * \return list of \c dao_client_t
 */
list_t *dao_list_clients(dao_t *dao);
/** Add client to DB
 * \param dao_client client to add
 * \return ID. \c 0 if failed
 */
long long int dao_add_client(dao_t *dao, const dao_client_t *dao_client);
void dao_remove_client_by_nickname(dao_t *dao, const char *nickname);
void dao_remove_client_by_addr(dao_t *dao, const char *host, const char *port);
void dao_remove_client_by_id(dao_t *dao, long long int id);

#endif /* _P2P_MU_DAO_H_ */
