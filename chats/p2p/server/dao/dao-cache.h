#ifndef _P2P_MU_DAO_CACHE_H_
# define _P2P_MU_DAO_CACHE_H_

# include "dao.h"
# include <stdbool.h>

struct dao_cache_element;
typedef struct dao_cache_element dc_el_t;

bool dao_cache_init(const char *db_path);
void dao_cache_deinit(void);

dc_el_t *dao_cache_get_client_by_id(long long int id);
dc_el_t *dao_cache_get_client_by_nickname(const char *nickname);
dc_el_t *dao_cache_get_client_by_addr(const char *host,
                                      const char *port);

dc_el_t *dao_cache_next_client_by_id(dc_el_t *dce);
dc_el_t *dao_cache_next_client_by_nickname(dc_el_t *dce);
dc_el_t *dao_cache_next_client_by_addr(dc_el_t *dce);

dc_el_t *dao_cache_add_client(const char *nickname,
                              const char *host,
                              const char *port);

void dao_cache_remove_client(dc_el_t *dce);
void dao_cache_remove_cient_by_id(long long int id);
void dao_cache_remove_client_by_nickname(const char *nickname);
void dao_cache_remove_client_by_addr(const char *host,
                                     const char *port);

long long int dao_cache_id(dc_el_t *dce);
const char *dao_cache_nickname(dc_el_t *dce);
const char *dao_cache_host(dc_el_t *dce);
const char *dao_cache_port(dc_el_t *dce);

#endif /* _P2P_MU_DAO_CACHE_H_ */
