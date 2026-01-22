#ifndef HEIMWATT_SERVER_INTERNAL_H
#define HEIMWATT_SERVER_INTERNAL_H

#include "db.h"
#include "server.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

#include "core/ipc.h"
#include "core/plugin_mgr.h"
#include "net/http_server.h"
#include "util/thread_pool.h"

enum { MAX_PLUGIN_CONNECTIONS = 32, REQUEST_ID_LEN = 37 };

struct heimwatt_ctx {
    db_handle* db;
    ipc_server* ipc;
    plugin_mgr* plugins;
    atomic_int running;

    // Connections managed here for Alpha
    ipc_conn* conns[MAX_PLUGIN_CONNECTIONS];
    int conn_count;

    // Logging
    FILE* log_file;

    // HTTP Server
    http_server* http;
    pthread_t http_thread;

    // Thread Pool
    thread_pool* pool;

    // Endpoint Registry
    struct {
        char path[128];
        char plugin_id[64];
        char method[16];
    } registry[32];
    int registry_count;

    // Connection Lock (Protect conns array and registry)
    pthread_mutex_t conn_lock;
    // Global Config
    double lat;
    double lon;
    char area[16];
};

#endif /* HEIMWATT_SERVER_INTERNAL_H */
