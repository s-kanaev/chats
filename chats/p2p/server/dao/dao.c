#include "dao.h"
#include "memory.h"

#define _GNU_SOURCE
#include <sqlite3.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *ADD_CLIENT_REQUEST_TPL =
"INSERT INTO clients "
"(nickname, keyword, host, port) "
"VALUES ('%s', '%s', '%s', '%s')";

static const char *REMOVE_CLIENT_REQUEST_TPL =
"DELETE FROM clients WHERE nickname = '%s'";

static const char *LIST_CLIENTS_REQUEST =
"SELECT id, nickname, keyword, host, port FROM clients";

dao_t *dao_init(const char *db_path) {
    sqlite3 *db = NULL;
    int rc;
    assert(sqlite3_threadsafe());

    rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, "unix");
    if (rc != SQLITE_OK) return NULL;

    return db;
}

void dao_deinit(dao_t *dao) {
    sqlite3 *db = dao;
    assert(db);
    sqlite3_close(db);
}

bool dao_add_client(dao_t *dao, dao_client_t dao_client) {
    sqlite3 *db = dao;
    char *sql = NULL;
    char *err_msg = NULL;

    assert(db);

    if(0 > asprintf(&sql, ADD_CLIENT_REQUEST_TPL,
                    dao_client.nickname,
                    dao_client.keyword,
                    dao_client.address,
                    dao_client.port)) return false;
    if (!sql) return false;

    if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, &err_msg)) {
        free(err_msg);
        free(sql);
        return false;
    }

    free(sql);
    return true;
}

void dao_remove_client(dao_t *dao, const char *nickname) {
    sqlite3 *db = dao;
    char *sql = NULL;
    char *err_msg = NULL;

    assert(db);

    if(0 > asprintf(&sql, REMOVE_CLIENT_REQUEST_TPL, nickname)) return;
    if (!sql) return;

    if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, &err_msg)) {
        free(err_msg);
        free(sql);
        return;
    }

    free(sql);
}

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
        memcpy(clients[row].keyword, table[idx + 2], sizeof(clients[row].keyword));
        memcpy(clients[row].address, table[idx + 3], sizeof(clients[row].address));
        memcpy(clients[row].port, table[idx + 4], sizeof(clients[row].port));
    }

    sqlite3_free_table(table);

    *clients_ = clients;
    return nrows;
}
