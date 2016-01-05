#include "hash-map.h"
#include "memory.h"

void hash_map_init(hash_map_t *hm, hasher_t hasher) {
    if (!hm) return;
    if (!hasher) return;
    hm->hasher = hasher;
    hm->tree.root = NULL;
    avl_tree_init(&hm->tree);
}

hash_map_t *hash_map_allocate(hasher_t hasher) {
    hash_map_t *hm = allocate(sizeof(hash_map_t));
    hash_map_init(hm, hasher);
    return hm;
}

void hash_map_deinit(hash_map_t *hm, bool deallocate_data) {
    if (!hm) return;
    avl_tree_deinit(&hm->tree, deallocate_data);
    hm->hasher = NULL;
}

void hash_map_deallocate(hash_map_t *hm, bool deallocate_data) {
    if (!hm) return;
    hash_map_deinit(hm, deallocate_data);
    deallocate(hm);
}

avl_tree_node_t *hash_map_insert_by_hash(hash_map_t *hm,
                                         long long int hash, void *data) {
    avl_tree_node_t *ret = NULL;
    if (!hm) return NULL;

    ret = avl_tree_add(&hm->tree, hash, data);
    return ret;
}

avl_tree_node_t *hash_map_insert_by_key(hash_map_t *hm,
                                        void *key_data, size_t key_len,
                                        void *data) {
    long long hash;
    if (!hm) return NULL;

    hash = hm->hasher(key_data, key_len);
    return hash_map_insert_by_hash(hm, hash, data);
}

avl_tree_node_t *hash_map_get_by_hash(hash_map_t *hm, long long int hash) {
    if (!hm) return NULL;

    return avl_tree_get(&hm->tree, hash);
}

avl_tree_node_t *hash_map_get_by_key(hash_map_t *hm,
                                     void *key_data, size_t key_len) {
    if (!hm) return NULL;

    return avl_tree_get(
        &hm->tree,
        hm->hasher(key_data, key_len)
    );
}

void *hash_map_remove_by_hash(hash_map_t *hm, long long int hash) {
    if (!hm) return NULL;

    return avl_tree_remove(&hm->tree, hash);
}

void *hash_map_remove_by_key(hash_map_t *hm, void *key_data, size_t key_len) {
    if (!hm) return NULL;

    return avl_tree_remove(
        &hm->tree,
        hm->hasher(key_data, key_len)
    );
}

size_t hash_map_count(const hash_map_t *hm) {
    return hm ? avl_tree_count(&hm->tree) : 0;
}
