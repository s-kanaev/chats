#ifndef _CHATS_COMMON_HASH_MAP_H_
# define _CHATS_COMMON_HASH_MAP_H_

# include <stddef.h>
# include <stdbool.h>

# include "avl-tree.h"

typedef long long (*hasher_t)(const void *data, size_t len);

struct hash_map {
    hasher_t hasher;
    avl_tree_t tree;
};

typedef struct hash_map hash_map_t;

void hash_map_init(hash_map_t *hm, hasher_t hasher);
void hash_map_deinit(hash_map_t *hm, bool deallocate_data);
hash_map_t *hash_map_allocate(hasher_t hasher);
void hash_map_deallocate(hash_map_t *hm, bool deallocate_data);

avl_tree_node_t *hash_map_get_by_key(hash_map_t *hm, void *key_data, size_t key_len);
avl_tree_node_t *hash_map_get_by_hash(hash_map_t *hm, long long hash);
avl_tree_node_t *hash_map_insert_by_key(hash_map_t *hm,
                                        void *key_data, size_t key_len,
                                        void *data);
avl_tree_node_t *hash_map_insert_by_hash(hash_map_t *hm,
                                         long long hash,
                                         void *data);
void *hash_map_remove_by_key(hash_map_t *hm,
                             void *key_data, size_t key_len);
void *hash_map_remove_by_hash(hash_map_t *hm,
                              long long hash);

#endif /* _CHATS_COMMON_HASH_MAP_H_ */
