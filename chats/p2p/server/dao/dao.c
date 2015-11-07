#include "dao.h"

#define _GNU_SOURCE
#include <sqlite3.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
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

    if(0 > asprintf(&sql, REMOVE_CLIENT_REQUEST_TPL, nickname)) return false;
    if (!sql) return false;

    if (SQLITE_OK != sqlite3_exec(db, sql, NULL, NULL, &err_msg)) {
        free(err_msg);
        free(sql);
        return false;
    }

    free(sql);
    return true;
}

size_t dao_list_clients(dao_t *dao, struct dao_client **clients) {
    assert(0);
    sqlite3_get_table();
    sqlite3_free_table();
    /* TODO */
#warning "Not implemented"
}















