/**
 * @file http_server.c
 * @brief High-performance HTTP server with epoll
 *
 * Non-blocking, event-driven HTTP server capable of handling
 * thousands of concurrent connections.
 */

#include "http_server.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "tcp_server.h"

enum
{
    MAX_EVENTS = 64,
    MAX_CONNECTIONS = 1024,
    READ_BUF_SIZE = 8192,
    WRITE_BUF_SIZE = 16384,
    REQUEST_ID_LEN = 37
};

// Connection state
typedef enum
{
    CONN_STATE_READING,
    CONN_STATE_PROCESSING,
    CONN_STATE_WRITING,
    CONN_STATE_CLOSING
} conn_state;

// Per-connection context
typedef struct http_conn
{
    int fd;
    conn_state state;

    // Read buffer
    HwBuffer read_buf;
    size_t read_pos;  // kept for compatibility if needed, or remove.
    // Actually http_parse needs data pointer. read_buf.len is the size.
    // read_pos in original code was tracking length. We can use read_buf.len.
    // But wait, existing code might use read_pos for incremental parsing state?
    // "conn->read_buf[conn->read_pos] = '\0'". read_pos is just used bytes.
    // So read_buf.len replaces read_pos.

    // Parsed request
    http_request req;
    int req_parsed;

    // Response
    http_response resp;
    HwBuffer write_buf;
    size_t write_pos;

    // Request ID for async correlation
    char request_id[REQUEST_ID_LEN];

    // Reference counting for async safety
    atomic_int ref_count;

    // Linked list for connection pool
    struct http_conn *next;

    // Request scoped arena
    HwArena *arena;
} http_conn;

struct http_server
{
    int listen_fd;
    int epoll_fd;
    int port;
    int timeout_ms;
    int max_conns;

    http_handler_fn handler;
    http_handler_async_fn async_handler;
    void *user_ctx;

    atomic_int running;

    // Connection tracking
    http_conn *connections[MAX_CONNECTIONS];
    int conn_count;

    // Connection Pool
    HwPool *conn_pool;
    HwPool *io_pool;
    pthread_mutex_t conn_lock;

    // Pending async responses (for bridge)
    struct
    {
        char request_id[REQUEST_ID_LEN];
        http_conn *conn;
    } pending[64];
    int pending_count;
    pthread_mutex_t pending_lock;
};

// Forward declarations
static int set_nonblocking(int fd);
static http_conn *conn_alloc(http_server *srv);
static void conn_unref(http_server *srv, http_conn *conn);
static void conn_ref(http_conn *conn);
static void conn_reset(http_conn *conn);
static int handle_accept(http_server *srv);
static int handle_read(http_server *srv, http_conn *conn);
static int handle_write(http_server *srv, http_conn *conn);
static void close_connection(http_server *srv, http_conn *conn);
static void generate_request_id(char *out);

/* ============================================================
 * LIFECYCLE
 * ============================================================ */

int http_server_create(http_server **srv, int port)
{
    if (!srv) return -1;

    http_server *s = mem_alloc(sizeof(*s));
    if (!s) return -ENOMEM;

    s->port = port;
    s->timeout_ms = 30000;
    s->max_conns = MAX_CONNECTIONS;
    s->listen_fd = -1;
    s->epoll_fd = -1;

    s->conn_pool = hw_pool_create(sizeof(http_conn), MAX_CONNECTIONS);
    s->io_pool = hw_pool_create(16384, MAX_CONNECTIONS);
    if (!s->conn_pool || !s->io_pool)
    {
        if (s->conn_pool) hw_pool_destroy(&s->conn_pool);
        if (s->io_pool) hw_pool_destroy(&s->io_pool);
        mem_free(s);
        return -ENOMEM;
    }

    pthread_mutex_init(&s->conn_lock, NULL);
    pthread_mutex_init(&s->pending_lock, NULL);

    *srv = s;
    return 0;
}

void http_server_destroy(http_server **srv)
{
    if (!srv || !*srv) return;
    http_server *s = *srv;

    http_server_stop(s);

    // Close all connections
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (s->connections[i])
        {
            if (s->connections[i]->fd >= 0) close(s->connections[i]->fd);
            if (s->connections[i]->arena) hw_arena_destroy(&s->connections[i]->arena);
            hw_buffer_free(&s->connections[i]->read_buf);
            hw_buffer_free(&s->connections[i]->write_buf);
            mem_free(s->connections[i]);
        }
    }

    hw_pool_destroy(&s->conn_pool);
    hw_pool_destroy(&s->io_pool);

    if (s->listen_fd >= 0) close(s->listen_fd);
    if (s->epoll_fd >= 0) close(s->epoll_fd);

    pthread_mutex_destroy(&s->conn_lock);
    pthread_mutex_destroy(&s->pending_lock);

    mem_free(s);
    *srv = NULL;
}

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

void http_server_set_handler(http_server *srv, http_handler_fn fn, void *ctx)
{
    if (srv)
    {
        srv->handler = fn;
        srv->user_ctx = ctx;
    }
}

void http_server_set_async_handler(http_server *srv, http_handler_async_fn fn, void *ctx)
{
    if (srv)
    {
        srv->async_handler = fn;
        srv->user_ctx = ctx;
    }
}

void http_server_set_timeout(http_server *srv, int timeout_ms)
{
    if (srv) srv->timeout_ms = timeout_ms;
}

void http_server_set_max_connections(http_server *srv, int max)
{
    if (srv && max > 0 && max <= MAX_CONNECTIONS) srv->max_conns = max;
}

/* ============================================================
 * ASYNC REQUEST COMPLETION
 * ============================================================ */

int http_server_complete_request(http_server *srv, const char *request_id,
                                 const http_response *resp)
{
    if (!srv || !request_id || !resp) return -1;

    pthread_mutex_lock(&srv->pending_lock);

    http_conn *conn = NULL;
    int found_idx = -1;

    for (int i = 0; i < srv->pending_count; i++)
    {
        if (strcmp(srv->pending[i].request_id, request_id) == 0)
        {
            conn = srv->pending[i].conn;
            found_idx = i;
            break;
        }
    }

    if (found_idx >= 0)
    {
        // Remove from pending
        srv->pending[found_idx] = srv->pending[srv->pending_count - 1];
        srv->pending_count--;
    }

    pthread_mutex_unlock(&srv->pending_lock);

    if (!conn)
    {
        log_warn("[HTTP] No pending request for ID: %.8s...", request_id);
        return -1;
    }

    // Check state - if client disconnected, don't write
    if (conn->state != CONN_STATE_PROCESSING)
    {
        // Connection closed or in bad state
        conn_unref(srv, conn);
        return -1;
    }

    // Copy response
    memcpy(&conn->resp, resp, sizeof(http_response));

    // Serialize response
    char *out = NULL;
    size_t out_len = 0;
    if (http_serialize_response(&conn->resp, &out, &out_len) == 0)
    {
        // Adopt the buffer or copy. Since we want to use generic HwBuffer logic:
        hw_buffer_clear(&conn->write_buf);
        if (hw_buffer_append(&conn->write_buf, out, out_len) < 0)
        {
            mem_free(out);
            close_connection(srv, conn);
            conn_unref(srv, conn);
            return -1;
        }
        mem_free(out);  // We copied it.

        conn->write_pos = 0;
        conn->state = CONN_STATE_WRITING;

        // Modify epoll to watch for EPOLLOUT
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;
        ev.data.ptr = conn;
        epoll_ctl(srv->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
    }
    else
    {
        close_connection(srv, conn);
    }

    // Release async reference
    conn_unref(srv, conn);

    return 0;
}

const char *http_server_get_request_id(const http_conn *conn)
{
    return conn ? conn->request_id : NULL;
}

/* ============================================================
 * MAIN LOOP
 * ============================================================ */

int http_server_run(http_server *srv)
{
    if (!srv) return -1;

    // Create listen socket
    srv->listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (srv->listen_fd < 0)
    {
        log_error("[HTTP] Failed to create socket: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(srv->port);

    if (bind(srv->listen_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        log_error("[HTTP] Bind failed: %s", strerror(errno));
        close(srv->listen_fd);
        return -1;
    }

    if (listen(srv->listen_fd, 128) < 0)
    {
        log_error("[HTTP] Listen failed: %s", strerror(errno));
        close(srv->listen_fd);
        return -1;
    }

    // Create epoll instance
    srv->epoll_fd = epoll_create1(0);
    if (srv->epoll_fd < 0)
    {
        log_error("[HTTP] epoll_create1 failed: %s", strerror(errno));
        close(srv->listen_fd);
        return -1;
    }

    // Add listen socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = NULL;  // NULL ptr means listen socket
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->listen_fd, &ev) < 0)
    {
        log_error("[HTTP] epoll_ctl failed: %s", strerror(errno));
        close(srv->epoll_fd);
        close(srv->listen_fd);
        return -1;
    }

    atomic_store(&srv->running, 1);
    log_info("[HTTP] Server listening on port %d (epoll, max %d connections)", srv->port,
             srv->max_conns);

    struct epoll_event events[MAX_EVENTS];

    while (atomic_load(&srv->running))
    {
        int nfds = epoll_wait(srv->epoll_fd, events, MAX_EVENTS, 1000);

        if (nfds < 0)
        {
            if (errno == EINTR) continue;
            log_error("[HTTP] epoll_wait error: %s", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.ptr == NULL)
            {
                // Listen socket - accept new connections
                while (handle_accept(srv) == 0);
            }
            else
            {
                http_conn *conn = (http_conn *) events[i].data.ptr;

                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    close_connection(srv, conn);
                    continue;
                }

                if (events[i].events & EPOLLIN)
                {
                    if (handle_read(srv, conn) < 0)
                    {
                        close_connection(srv, conn);
                        continue;
                    }
                }

                if (events[i].events & EPOLLOUT)
                {
                    if (handle_write(srv, conn) < 0)
                    {
                        close_connection(srv, conn);
                        continue;
                    }
                }
            }
        }
    }

    log_info("[HTTP] Server shutting down");
    return 0;
}

void http_server_stop(http_server *srv)
{
    if (srv)
    {
        atomic_store(&srv->running, 0);
    }
}

int http_server_is_running(const http_server *srv) { return srv ? atomic_load(&srv->running) : 0; }

int http_server_port(const http_server *srv) { return srv ? srv->port : 0; }

/* ============================================================
 * CONNECTION HANDLING
 * ============================================================ */

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static http_conn *conn_alloc(http_server *srv)
{
    pthread_mutex_lock(&srv->conn_lock);

    http_conn *conn = NULL;

    conn = hw_pool_alloc(srv->conn_pool);

    if (conn)
    {
        atomic_init(&conn->ref_count, 1);  // Initial ref by main loop
    }

    pthread_mutex_unlock(&srv->conn_lock);
    return conn;
}

static void conn_ref(http_conn *conn)
{
    if (conn) atomic_fetch_add(&conn->ref_count, 1);
}

static void conn_unref(http_server *srv, http_conn *conn)
{
    if (!conn) return;

    if (atomic_fetch_sub(&conn->ref_count, 1) > 1)
    {
        return;  // Still in use
    }

    // Ref count dropped to 0, actually free it
    pthread_mutex_lock(&srv->conn_lock);

    // Remove from active connections
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (srv->connections[i] == conn)
        {
            srv->connections[i] = NULL;
            srv->conn_count--;
            break;
        }
    }

    // Add to free list for reuse
    conn_reset(conn);
    hw_pool_free(srv->conn_pool, conn);

    pthread_mutex_unlock(&srv->conn_lock);
}

static void conn_reset(http_conn *conn)
{
    if (conn->fd >= 0) close(conn->fd);
    conn->fd = -1;
    conn->state = CONN_STATE_READING;
    hw_buffer_clear(&conn->read_buf);
    conn->req_parsed = 0;
    memset(&conn->req, 0, sizeof(conn->req));
    memset(&conn->resp, 0, sizeof(conn->resp));
    hw_buffer_clear(&conn->write_buf);
    conn->write_pos = 0;
    conn->request_id[0] = '\0';
    conn->next = NULL;
    if (conn->arena) hw_arena_reset(conn->arena);
}

static int handle_accept(http_server *srv)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(srv->listen_fd, (struct sockaddr *) &client_addr, &client_len);
    if (client_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -1;  // No more pending
        }
        log_warn("[HTTP] Accept failed: %s", strerror(errno));
        return -1;
    }

    if (srv->conn_count >= srv->max_conns)
    {
        log_warn("[HTTP] Max connections reached (%d), rejecting", srv->max_conns);
        close(client_fd);
        return 0;
    }

    if (set_nonblocking(client_fd) < 0)
    {
        close(client_fd);
        return 0;
    }

    http_conn *conn = conn_alloc(srv);
    if (!conn)
    {
        close(client_fd);
        return 0;
    }

    if (!conn->arena)
    {
        conn->arena = hw_arena_create(4096);
        if (!conn->arena)
        {
            // OOM
            close(client_fd);
            conn_unref(srv, conn);  // Returns to pool
            return 0;
        }
    }

    conn->fd = client_fd;
    conn->state = CONN_STATE_READING;
    generate_request_id(conn->request_id);

    // Add to epoll (edge-triggered)
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = conn;
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0)
    {
        log_warn("[HTTP] epoll_ctl add failed: %s", strerror(errno));
        conn_unref(srv, conn);
        return 0;
    }

    // Track connection
    pthread_mutex_lock(&srv->conn_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++)
    {
        if (!srv->connections[i])
        {
            srv->connections[i] = conn;
            srv->conn_count++;
            break;
        }
    }
    pthread_mutex_unlock(&srv->conn_lock);

    log_debug("[HTTP] New connection (fd=%d, id=%.8s...)", client_fd, conn->request_id);
    return 0;
}

static int handle_read(http_server *srv, http_conn *conn)
{
    while (1)
    {
        char tmp[8192];
        ssize_t n = read(conn->fd, tmp, sizeof(tmp));

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;  // No more data
            }
            return -1;  // Error
        }

        if (n == 0)
        {
            return -1;  // Client closed
        }

        if (hw_buffer_append(&conn->read_buf, tmp, n) < 0)
        {
            return -1;  // OOM
        }
    }

    // Try to parse request
    if (!conn->req_parsed)
    {
        if (http_parse_request(conn->read_buf.data, conn->read_buf.len, &conn->req, conn->arena) ==
            0)
        {
            conn->req_parsed = 1;
            conn->state = CONN_STATE_PROCESSING;

            log_debug("[HTTP] %s %s (id=%.8s...)", conn->req.method, conn->req.path,
                      conn->request_id);

            // Initialize response
            http_response_init(&conn->resp);

            // Call handler
            int handler_result = 0;

            if (srv->async_handler)
            {
                handler_result =
                    srv->async_handler(&conn->req, &conn->resp, conn->request_id, srv->user_ctx);

                if (handler_result == 1)
                {
                    // Async - add to pending
                    pthread_mutex_lock(&srv->pending_lock);
                    if (srv->pending_count < 64)
                    {
                        snprintf(srv->pending[srv->pending_count].request_id, REQUEST_ID_LEN, "%s",
                                 conn->request_id);
                        srv->pending[srv->pending_count].conn = conn;
                        conn_ref(conn);  // Async system holds a reference
                        srv->pending_count++;
                    }
                    pthread_mutex_unlock(&srv->pending_lock);
                    return 0;  // Wait for complete_request
                }
            }
            else if (srv->handler)
            {
                handler_result = srv->handler(&conn->req, &conn->resp, srv->user_ctx);
            }
            else
            {
                conn->resp.status_code = 404;
            }

            // Synchronous completion
            char *out = hw_pool_alloc(srv->io_pool);
            int from_pool = 1;
            if (!out)
            {
                out = mem_alloc(16384);
                from_pool = 0;
            }

            size_t out_len = 0;
            if (out && http_serialize_response_buf(&conn->resp, out, 16384, &out_len) == 0)
            {
                hw_buffer_clear(&conn->write_buf);
                if (hw_buffer_append(&conn->write_buf, out, out_len) < 0)
                {
                    if (from_pool)
                        hw_pool_free(srv->io_pool, out);
                    else
                        mem_free(out);
                    return -1;
                }

                if (from_pool)
                    hw_pool_free(srv->io_pool, out);
                else
                    mem_free(out);

                conn->write_pos = 0;
                conn->state = CONN_STATE_WRITING;

                // Switch to write mode
                struct epoll_event ev;
                ev.events = EPOLLOUT | EPOLLET;
                ev.data.ptr = conn;
                epoll_ctl(srv->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
            }
            else
            {
                if (from_pool)
                    hw_pool_free(srv->io_pool, out);
                else
                    mem_free(out);
                return -1;
            }
        }
    }
    /*
    // Previously we checked for buffer full here. Now we support infinite buffer.
    // We relies on OOM or overall system constraints.
    else if (conn->read_buf.len >= READ_BUF_SIZE - 1)
    {
         // HwBuffer full, can't parse - reject
         log_warn("[HTTP] Request too large");
         return -1;
    }
    */

    return 0;
}

static int handle_write(http_server *srv, http_conn *conn)
{
    (void) srv;

    while (conn->write_pos < conn->write_buf.len)
    {
        ssize_t n = write(conn->fd, conn->write_buf.data + conn->write_pos,
                          conn->write_buf.len - conn->write_pos);

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return 0;  // Try again later
            }
            return -1;  // Error
        }

        conn->write_pos += n;
    }

    // Done writing - close connection (HTTP/1.0 style for simplicity)
    log_debug("[HTTP] Response sent (id=%.8s...)", conn->request_id);
    return -1;  // Close connection
}

static void close_connection(http_server *srv, http_conn *conn)
{
    if (!conn) return;

    // Remove from epoll
    epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);

    // Remove from pending if applicable
    pthread_mutex_lock(&srv->pending_lock);
    for (int i = 0; i < srv->pending_count; i++)
    {
        if (srv->pending[i].conn == conn)
        {
            srv->pending[i] = srv->pending[srv->pending_count - 1];
            srv->pending_count--;
            break;
        }
    }
    pthread_mutex_unlock(&srv->pending_lock);

    // Clean up request/response
    http_request_destroy(&conn->req);
    http_response_destroy(&conn->resp);

    if (conn->arena) hw_arena_reset(conn->arena);

    conn_unref(srv, conn);
}

static void generate_request_id(char *out)
{
    // Simple incrementing ID with random component for uniqueness
    static atomic_int counter = 0;
    int c = atomic_fetch_add(&counter, 1);

    // Format: XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX (deterministic)
    snprintf(out, REQUEST_ID_LEN, "%08x-%04x-%04x-%04x-%08x%04x", (unsigned int) time(NULL),
             (unsigned int) ((c >> 16) & 0xFFFF), (unsigned int) (c & 0xFFFF),
             (unsigned int) (getpid() & 0xFFFF), (unsigned int) ((c * 2654435761U) & 0xFFFFFFFF),
             (unsigned int) ((c ^ getpid()) & 0xFFFF));
}
