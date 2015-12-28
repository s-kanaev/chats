#include "dao-cache.h"
#include "dao.h"
#include "hash-map.h"
#include "hash-functions.h"
#include "list.h"
#include "memory.h"

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

struct dao_cache_element {
    long long int id, hash_nickname, hash_addr;
    avl_tree_node_t *node_by_id;
    avl_tree_node_t *node_by_nickname;
    avl_tree_node_t *node_by_addr;
    dao_client_t *client;
};

struct dao_cache {
    bool initialized;
    pthread_mutex_t mtx;
    dao_t *dao;
    list_t *dao_list;
    list_t *dce_list;
    hash_map_t hm_by_id;
    hash_map_t hm_by_nickname;
    hash_map_t hm_by_addr;
};

static struct dao_cache DAO_CACHE = {
    .initialized = false
};

static
long long int nohash(const void *data, size_t l) {
    assert(0);
    return (long long int)data;
}

bool dao_cache_init(const char *db_path) {
    dao_t *dao = NULL;
    list_t *dao_list = NULL;
    list_t *dce_list = NULL;
    dao_client_t *cl;

    if (!db_path) return false;

    dao = dao_init(db_path);
    if (!dao) return false;
    dao_list = dao_list_clients(dao);

    if (!dao_list) {
        dao_deinit(dao);
        return false;
    }

    dce_list = list_init(sizeof(dc_el_t));
    if (!dce_list) {
        dao_deinit(dao);
        return false;
    }

    DAO_CACHE.dao = dao;
    DAO_CACHE.dao_list = dao_list;
    DAO_CACHE.dce_list = dce_list;
    pthread_mutex_init(&DAO_CACHE.mtx, NULL);

    hash_map_init(&DAO_CACHE.hm_by_id, nohash);
    hash_map_init(&DAO_CACHE.hm_by_nickname, pearson_hash);
    hash_map_init(&DAO_CACHE.hm_by_addr, pearson_hash);

    for (cl = list_first_element(dao_list); cl; cl = list_next_element(dao_list, cl)) {
        dc_el_t *dc_el = list_append(dce_list);
        long long int hash;

        dc_el->id = cl->id;
        dc_el->client = cl;
        dc_el->node_by_id = hash_map_insert_by_hash(&DAO_CACHE.hm_by_id, cl->id, dc_el);

        hash = pearson_hash(cl->nickname, strlen(cl->nickname));
        dc_el->hash_nickname = hash;
        dc_el->node_by_nickname = hash_map_insert_by_hash(&DAO_CACHE.hm_by_nickname, hash, dc_el);

        hash = pearson_hash(cl->host, strlen(cl->host));
        hash = pearson_hash_update(hash, ":", 1);
        hash = pearson_hash_update(hash, cl->port, strlen(cl->port));
        dc_el->hash_addr = hash;
        dc_el->node_by_addr = hash_map_insert_by_hash(&DAO_CACHE.hm_by_addr, hash, dc_el);
    }

    DAO_CACHE.initialized = true;

    return true;
}

void dao_cache_deinit(void) {
    assert(DAO_CACHE.initialized);

    pthread_mutex_lock(&DAO_CACHE.mtx);

    hash_map_deinit(&DAO_CACHE.hm_by_id, false);
    hash_map_deinit(&DAO_CACHE.hm_by_nickname, false);
    hash_map_deinit(&DAO_CACHE.hm_by_addr, false);

    list_deinit(DAO_CACHE.dce_list);
    list_deinit(DAO_CACHE.dao_list);
    dao_deinit(DAO_CACHE.dao);

    DAO_CACHE.dce_list = DAO_CACHE.dao_list = NULL;
    DAO_CACHE.dao = NULL;

    DAO_CACHE.initialized = false;

    pthread_mutex_unlock(&DAO_CACHE.mtx);
    pthread_mutex_destroy(&DAO_CACHE.mtx);
}

dc_el_t *dao_cache_get_client_by_addr(const char *host, const char *port) {
    dc_el_t *dce;
    avl_tree_node_t *node;
    long long hash;

    assert(DAO_CACHE.initialized);

    pthread_mutex_lock(&DAO_CACHE.mtx);

    hash = pearson_hash(host, strlen(host));
    hash = pearson_hash_update(hash, ":", 1);
    hash = pearson_hash_update(hash, port, strlen(port));
    node = hash_map_get_by_hash(&DAO_CACHE.hm_by_addr, hash);
    dce = node ? node->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return dce;
}

dc_el_t *dao_cache_get_client_by_id(long long int id) {
    dc_el_t *dce;
    avl_tree_node_t *node;
    assert(DAO_CACHE.initialized);

    pthread_mutex_lock(&DAO_CACHE.mtx);

    node = hash_map_get_by_hash(&DAO_CACHE.hm_by_id, id);
    dce = node ? node->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return dce;
}

dc_el_t *dao_cache_get_client_by_nickname(const char *nickname) {
    dc_el_t *dce;
    avl_tree_node_t *node;
    long long hash;

    assert(DAO_CACHE.initialized);

    pthread_mutex_lock(&DAO_CACHE.mtx);

    hash = pearson_hash(nickname, strlen(nickname));
    node = hash_map_get_by_hash(&DAO_CACHE.hm_by_nickname, hash);
    dce = node ? node->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return dce;
}

dc_el_t *dao_cache_next_client_by_id(dc_el_t *dce) {
    dc_el_t *next;
    avl_tree_node_t *nnode;

    assert(DAO_CACHE.initialized);
    if (!dce) return NULL;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    nnode = avl_tree_next(dce->node_by_id);
    next = nnode ? nnode->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return next;
}

dc_el_t *dao_cache_next_client_by_nickname(dc_el_t *dce) {
    dc_el_t *next;
    avl_tree_node_t *nnode;

    assert(DAO_CACHE.initialized);
    if (!dce) return NULL;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    nnode = avl_tree_next(dce->node_by_nickname);
    next = nnode ? nnode->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return next;
}

dc_el_t *dao_cache_next_client_by_addr(dc_el_t *dce) {
    dc_el_t *next;
    avl_tree_node_t *nnode;

    assert(DAO_CACHE.initialized);
    if (!dce) return NULL;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    nnode = avl_tree_next(dce->node_by_addr);
    next = nnode ? nnode->data : NULL;

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return next;
}

dc_el_t *dao_cache_add_client(const char *nickname, const char *host, const char *port) {
    dc_el_t *dc_el;
    dao_client_t *cl;
    long long int hash;
    long long int id;

    assert(DAO_CACHE.initialized);

    if (!nickname || !host || !port) return NULL;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    cl = list_append(DAO_CACHE.dao_list);
    memset(cl->nickname, 0, sizeof(cl->nickname));
    memset(cl->host, 0, sizeof(cl->host));
    memset(cl->port, 0, sizeof(cl->port));
    strncpy(cl->nickname, nickname, sizeof(cl->nickname)-1);
    strncpy(cl->host, host, sizeof(cl->host)-1);
    strncpy(cl->port, port, sizeof(cl->port)-1);

    id = dao_add_client(DAO_CACHE.dao, cl);

    if (!id) {
        list_remove_element(DAO_CACHE.dao_list, cl);
        pthread_mutex_unlock(&DAO_CACHE.mtx);
        return NULL;
    }

    cl->id = id;

    dc_el = list_append(DAO_CACHE.dce_list);

    dc_el->id = id;
    dc_el->client = cl;
    dc_el->node_by_id = hash_map_insert_by_hash(&DAO_CACHE.hm_by_id, cl->id, dc_el);

    hash = pearson_hash(cl->nickname, strlen(cl->nickname));
    dc_el->hash_nickname = hash;
    dc_el->node_by_nickname = hash_map_insert_by_hash(&DAO_CACHE.hm_by_nickname, hash, dc_el);

    hash = pearson_hash(cl->host, strlen(cl->host));
    hash = pearson_hash_update(hash, ":", 1);
    hash = pearson_hash_update(hash, cl->port, strlen(cl->port));
    dc_el->hash_addr = hash;
    dc_el->node_by_addr = hash_map_insert_by_hash(&DAO_CACHE.hm_by_addr, hash, dc_el);

    pthread_mutex_unlock(&DAO_CACHE.mtx);

    return dc_el;
}

void dao_cache_remove_client(dc_el_t *dce) {
    assert(DAO_CACHE.initialized);

    if (!dce) return;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    dao_remove_client_by_id(DAO_CACHE.dao, dce->id);

    hash_map_remove_by_hash(&DAO_CACHE.hm_by_id, dce->id);
    hash_map_remove_by_hash(&DAO_CACHE.hm_by_nickname, dce->hash_nickname);
    hash_map_remove_by_hash(&DAO_CACHE.hm_by_addr, dce->hash_addr);

    list_remove_element(DAO_CACHE.dao_list, dce->client);
    list_remove_element(DAO_CACHE.dce_list, dce);

    pthread_mutex_unlock(&DAO_CACHE.mtx);
}

void dao_cache_remove_cient_by_id(long long int id) {
    dc_el_t *dce;

    assert(DAO_CACHE.initialized);

    pthread_mutex_lock(&DAO_CACHE.mtx);

    dce = hash_map_remove_by_hash(&DAO_CACHE.hm_by_id, id);
    dao_remove_client_by_id(DAO_CACHE.dao, dce->id);

    hash_map_remove_by_hash(&DAO_CACHE.hm_by_nickname, dce->hash_nickname);
    hash_map_remove_by_hash(&DAO_CACHE.hm_by_addr, dce->hash_addr);

    list_remove_element(DAO_CACHE.dao_list, dce->client);
    list_remove_element(DAO_CACHE.dce_list, dce);

    pthread_mutex_unlock(&DAO_CACHE.mtx);
}

void dao_cache_remove_client_by_nickname(const char *nickname) {
    dc_el_t *dce;
    long long int hash;

    assert(DAO_CACHE.initialized);
    if (!nickname) return;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    hash = pearson_hash(nickname, strlen(nickname));
    dce = hash_map_remove_by_hash(&DAO_CACHE.hm_by_nickname, hash);

    dao_remove_client_by_id(DAO_CACHE.dao, dce->id);

    hash_map_remove_by_hash(&DAO_CACHE.hm_by_id, dce->id);
    hash_map_remove_by_hash(&DAO_CACHE.hm_by_addr, dce->hash_addr);

    list_remove_element(DAO_CACHE.dao_list, dce->client);
    list_remove_element(DAO_CACHE.dce_list, dce);

    pthread_mutex_unlock(&DAO_CACHE.mtx);
}

void dao_cache_remove_client_by_addr(const char *host, const char *port) {
    dc_el_t *dce;
    long long int hash;

    assert(DAO_CACHE.initialized);
    if (!host || !port) return;

    pthread_mutex_lock(&DAO_CACHE.mtx);

    hash = pearson_hash(host, strlen(host));
    hash = pearson_hash_update(hash, ":", 1);
    hash = pearson_hash_update(hash, port, strlen(port));
    dce = hash_map_remove_by_hash(&DAO_CACHE.hm_by_addr, hash);

    dao_remove_client_by_id(DAO_CACHE.dao, dce->id);

    hash_map_remove_by_hash(&DAO_CACHE.hm_by_id, dce->id);
    hash_map_remove_by_hash(&DAO_CACHE.hm_by_nickname, dce->hash_nickname);

    list_remove_element(DAO_CACHE.dao_list, dce->client);
    list_remove_element(DAO_CACHE.dce_list, dce);

    pthread_mutex_unlock(&DAO_CACHE.mtx);
}

long long int dao_cache_id(dc_el_t *dce) {
    return dce ? dce->client->id : 0;
}

const char *dao_cache_nickname(dc_el_t *dce) {
    return dce ? dce->client->nickname : NULL;
}

const char *dao_cache_host(dc_el_t *dce) {
    return dce ? dce->client->host : NULL;
}

const char *dao_cache_port(dc_el_t *dce) {
    return dce ? dce->client->port : NULL;
}
