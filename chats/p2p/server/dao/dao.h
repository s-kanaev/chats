#ifndef _P2P_MU_DAO_H_
# define _P2P_MU_DAO_H_

# include "list.h"
# include "protocol.h"

struct dao;
typedef struct dao dao_t;

typedef struct dao_client {
    char nickname[P2P_NICKNAME_LENGTH];
    char keyword[P2P_KEYWORD_LENGTH];
    char address[P2P_ADDR_LENGTH];
    char port[P2P_PORT_LENGTH];
} dao_client_t;

dao_t *dao_init(const char *db_path);
void dao_deinit(dao_t *dao);

list_t *dao_list_clients(dao_t *dao);
bool dao_add_client(dao_t *dao, dao_client_t dao_client);
void dao_remove_client(dao_t *dao, const char *nickname);

#endif /* _P2P_MU_DAO_H_ */