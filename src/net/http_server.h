/**
 * @file http_server.h
 * @brief HTTP server
 *
 * Accept loop and request dispatching.
 */

#ifndef HEIMWATT_HTTP_SERVER_H
#define HEIMWATT_HTTP_SERVER_H

#include "http_parse.h"

typedef struct http_server http_server;

/**
 * Handler callback signature.
 *
 * @param req HTTP request
 * @param resp HTTP response (pre-initialized with 200 OK)
 * @param ctx User context
 * @return 0 on success, -1 on error (will send 500)
 */
typedef int (*http_handler_fn)(const http_request* req, http_response* resp, void* ctx);

/* ============================================================
 * LIFECYCLE
 * ============================================================ */

/**
 * Initialize HTTP server.
 *
 * @param srv  Output pointer for server
 * @param port Port to listen on
 * @return 0 on success, -1 on error
 */
int http_server_init(http_server** srv, int port);

/**
 * Destroy HTTP server.
 *
 * @param srv Pointer to server (set to NULL on return)
 */
void http_server_destroy(http_server** srv);

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

/**
 * Set request handler.
 *
 * @param srv Server
 * @param fn  Handler function
 * @param ctx User context passed to handler
 */
void http_server_set_handler(http_server* srv, http_handler_fn fn, void* ctx);

/**
 * Set connection timeout.
 *
 * @param srv        Server
 * @param timeout_ms Timeout in milliseconds
 */
void http_server_set_timeout(http_server* srv, int timeout_ms);

/**
 * Set maximum concurrent connections.
 *
 * @param srv Server
 * @param max Maximum connections
 */
void http_server_set_max_connections(http_server* srv, int max);

/* ============================================================
 * RUN
 * ============================================================ */

/**
 * Run server (blocks until stopped).
 *
 * @param srv Server
 * @return 0 on normal shutdown, -1 on error
 */
int http_server_run(http_server* srv);

/**
 * Stop server (thread-safe, can be called from signal handler).
 *
 * @param srv Server
 */
void http_server_stop(http_server* srv);

/* ============================================================
 * STATUS
 * ============================================================ */

/**
 * Check if server is running.
 */
int http_server_is_running(const http_server* srv);

/**
 * Get server port.
 */
int http_server_port(const http_server* srv);

#endif /* HEIMWATT_HTTP_SERVER_H */
