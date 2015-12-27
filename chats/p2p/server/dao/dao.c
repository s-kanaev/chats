#include "dao.h"
#include "memory.h"

#define _GNU_SOURCE
#include <sqlite3.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFFER(a) char a[1024]

static const char *ADD_CLIENT_REQUEST_TPL =
"INSERT INTO clients "
"(nickname, host, port) "
"VALUES ('%15s', '%256s', '%5s')";

static const char *REMOVE_CLIENT_BY_NICKNAME_REQUEST_TPL =
"DELETE FROM clients WHERE nickname = '%15s'";

static const char *REMOVE_CLIENT_BY_ADDR_REQUEST_TPL =
"DELETE FROM clients WHERE host = '%256s' AND port = '%5s'";

static const char *REMOVE_CLIENT_BY_ID_REQUEST_TPL =
"DELETE FROM clients WHERE id = %lld";

static const char *LIST_CLIENTS_REQUEST =
"SELECT id, nickname, host, port FROM clients";

struct dao {
    pthread_mutex_t mtx;
    sqlite3 *db;
};

dao_t *dao_init(const char *db_path) {
    sqlite3 *db = NULL;
    int rc;
    dao_t *dao = allocate(sizeof(dao_t));

    if (!dao) return NULL;

    pthread_mutex_init(&dao->mtx, NULL);

    rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, "unix");
    if (rc != SQLITE_OK) {
        deallocate(dao);
        return NULL;
    }

    return dao;
}

void dao_deinit(dao_t *dao) {
    if (!dao) return;

    pthread_mutex_lock(&dao->mtx);

    sqlite3_close(dao->db);
    dao->db = NULL;

    pthread_mutex_unlock(&dao->mtx);
    pthread_mutex_destroy(&dao->mtx);
}

long long int dao_add_client(dao_t *dao, const dao_client_t *dao_client) {
    BUFFER(buffer);
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;
    int sql_len = 0;
    long long int id;

    if (!dao) return 0;

    pthread_mutex_lock(&dao->mtx);

    db = dao->db;
    if (!db) {
        pthread_mutex_unlock(&dao->mtx);
        return 0;
    }

    sql_len = sprintf(buffer, ADD_CLIENT_REQUEST_TPL,
                      dao_client->nickname, dao_client->host, dao_client->port);
    if (0 > sql_len) {
        pthread_mutex_unlock(&dao->mtx);
        return 0;
    }

    if (SQLITE_OK != sqlite3_exec(db, buffer, NULL, NULL, &err_msg)) {
        free(err_msg);
        pthread_mutex_unlock(&dao->mtx);
        return 0;
    }

    id = sqlite3_last_insert_rowid(db);
    pthread_mutex_unlock(&dao->mtx);

    return id;
}

void dao_remove_client_by_id(dao_t *dao, long long int id) {
    BUFFER(buffer);
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;
    int sql_len = 0;

    if (!dao) return;

    pthread_mutex_lock(&dao->mtx);

    db = dao->db;
    if (!db) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    sql_len = sprintf(buffer, REMOVE_CLIENT_BY_ID_REQUEST_TPL, id);
    if (0 > sql_len) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    if (SQLITE_OK != sqlite3_exec(db, buffer, NULL, NULL, &err_msg)) {
        free(err_msg);
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    pthread_mutex_unlock(&dao->mtx);
}

void dao_remove_client_by_nickname(dao_t *dao, const char *nickname) {
    BUFFER(buffer);
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;
    int sql_len = 0;

    if (!dao) return;

    pthread_mutex_lock(&dao->mtx);

    db = dao->db;
    if (!db) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    sql_len = sprintf(buffer, REMOVE_CLIENT_BY_NICKNAME_REQUEST_TPL,
                      nickname);
    if (0 > sql_len) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    if (SQLITE_OK != sqlite3_exec(db, buffer, NULL, NULL, &err_msg)) {
        free(err_msg);
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    pthread_mutex_unlock(&dao->mtx);
}

void dao_remove_client_by_addr(dao_t *dao, const char *host, const char *port) {
    BUFFER(buffer);
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;
    int sql_len = 0;

    if (!dao) return;

    pthread_mutex_lock(&dao->mtx);

    db = dao->db;
    if (!db) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    sql_len = sprintf(buffer, REMOVE_CLIENT_BY_ADDR_REQUEST_TPL,
                      host, port);
    if (0 > sql_len) {
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    if (SQLITE_OK != sqlite3_exec(db, buffer, NULL, NULL, &err_msg)) {
        free(err_msg);
        pthread_mutex_unlock(&dao->mtx);
        return;
    }

    pthread_mutex_unlock(&dao->mtx);
}

list_t *dao_list_clients(dao_t *dao) {
    BUFFER(buffer);
    sqlite3 *db;
    char *err_msg = NULL;
    int rc;
    sqlite3_stmt *stmt;
    list_t *list = NULL;
    dao_client_t *dc = NULL;

    if (!dao) return NULL;

    pthread_mutex_lock(&dao->mtx);

    db = dao->db;
    if (!db) {
        pthread_mutex_unlock(&dao->mtx);
        return NULL;
    }

    rc = sqlite3_prepare_v2(db,
                            LIST_CLIENTS_REQUEST, strlen(LIST_CLIENTS_REQUEST) + 1,
                            &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&dao->mtx);
        return NULL;
    }

    list = list_init(sizeof(dao_client_t));

    do {
        rc = sqlite3_step(stmt);

        if (rc == SQLITE_DONE) break;
        if (rc != SQLITE_OK) break;

        /* fetch whole row column by column */
        dc = list_append(list);
        dc->id = sqlite3_column_int64(stmt, 0);
        strncpy(
            dc->nickname,
            (const char *)sqlite3_column_text(stmt, 1),
            sizeof(dc->nickname)
        );
        dc->nickname[sizeof(dc->nickname) - 1] = '\0';

        strncpy(
            dc->host,
            (const char *)sqlite3_column_text(stmt, 2),
            sizeof(dc->host)
        );
        dc->host[sizeof(dc->host) - 1] = '\0';

        strncpy(
            dc->port,
            (const char *)sqlite3_column_text(stmt, 3),
            sizeof(dc->port)
        );
        dc->port[sizeof(dc->port) - 1] = '\0';
    } while (true);

    if (!(rc == SQLITE_DONE || rc == SQLITE_OK)) {
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&dao->mtx);
        list_deinit(list);
        return NULL;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&dao->mtx);
    return list;
}
#if 0
/* TODO TODO TODO */
size_t dao_list_clients(dao_t *dao, struct dao_client **clients_) {
    int nrows, ncols;
    sqlite3 *db = dao;
    char *err_msg = NULL;
    char **table = NULL;
    int rc, row, idx;
    struct dao_client *clients;

    assert(db);
    rc = sqlite3_get_table(db, LIST_CLIENTS_REQUEST, &table,
                           &nrows, &ncols, &err_msg);

    if (SQLITE_OK != rc) {
        free(err_msg);
        return 0;
    }

    if (nrows == 0) {
        sqlite3_free_table(table);
        return 0;
    }

    clients = allocate(nrows * sizeof(struct dao_client));
    if (!clients) {
        sqlite3_free_table(table);
        return 0;
    }

    for (row = 0, idx = ncols; row < nrows; ++row, idx += ncols) {
        memcpy(clients[row].nickname, table[idx + 1], sizeof(clients[row].nickname));
        memcpy(clients[row].host, table[idx + 3], sizeof(clients[row].host));
        memcpy(clients[row].port, table[idx + 4], sizeof(clients[row].port));
    }

    sqlite3_free_table(table);

    *clients_ = clients;
    return nrows;
}
#endif
