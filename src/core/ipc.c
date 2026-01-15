// #define _GNU_SOURCE (Defined in Makefile)
#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct ipc_server
{
    int fd;
    char *socket_path;
};

struct ipc_conn
{
    int fd;
    char *plugin_id;
    // Buffering for robust reads
    char buf[4096];
    size_t rpos;
    size_t wpos;

    // Output buffering for non-blocking writes
    char *out_buf;
    size_t out_len;
    size_t out_pos;
};

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int ipc_server_init(ipc_server **srv_out, const char *socket_path)
{
    int ret = 0;
    if (!srv_out || !socket_path) return -EINVAL;

    ipc_server *srv = malloc(sizeof(*srv));
    if (!srv) return -ENOMEM;
    memset(srv, 0, sizeof(*srv));

    srv->socket_path = strdup(socket_path);
    if (!srv->socket_path)
    {
        ret = -ENOMEM;
        goto cleanup;
    }

    srv->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->fd < 0)
    {
        free(srv->socket_path);
        free(srv);
        return -errno;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    (void) snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    unlink(socket_path);  // Remove existing
    if (bind(srv->fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    if (listen(srv->fd, 10) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    *srv_out = srv;
    return 0;

cleanup:
    ipc_server_destroy(&srv);
    return ret;
}

void ipc_server_destroy(ipc_server **srv_ptr)
{
    if (!srv_ptr || !*srv_ptr) return;
    ipc_server *srv = *srv_ptr;

    if (srv->fd >= 0) close(srv->fd);
    if (srv->socket_path)
    {
        unlink(srv->socket_path);
        free(srv->socket_path);
    }

    free(srv);
    *srv_ptr = NULL;
}

int ipc_server_accept(ipc_server *srv, ipc_conn **conn_out)
{
    if (!srv || !conn_out) return -EINVAL;

    int new_fd = accept(srv->fd, NULL, NULL);
    if (new_fd < 0) return -errno;

    ipc_conn *conn = malloc(sizeof(*conn));
    if (!conn)
    {
        close(new_fd);
        return -ENOMEM;
    }
    memset(conn, 0, sizeof(*conn));
    conn->fd = new_fd;

    // Set non-blocking
    set_nonblocking(conn->fd);

    *conn_out = conn;
    return 0;
}

int ipc_conn_recv(ipc_conn *conn, char **msg, size_t *len)
{
    if (!conn || !msg || !len) return -EINVAL;

    // 1. Check if we already have a newline in buffer
    while (1)
    {
        char *newline = memchr(conn->buf + conn->rpos, '\n', conn->wpos - conn->rpos);
        if (newline)
        {
            size_t msg_len = newline - (conn->buf + conn->rpos);
            char *ret_msg = malloc(msg_len + 1);
            if (!ret_msg) return -ENOMEM;

            memcpy(ret_msg, conn->buf + conn->rpos, msg_len);
            ret_msg[msg_len] = 0;

            *msg = ret_msg;
            *len = msg_len;

            // Advance rpos past newline
            conn->rpos += msg_len + 1;
            return 0;
        }

        // 2. No newline, need more data.
        // If buffer is full and no newline, we have a problem (msg too big)
        if (conn->wpos == sizeof(conn->buf))
        {
            // If rpos is 0, message is larger than 4KB. Drop/Error.
            if (conn->rpos == 0)
            {
                // Reset buffer, lost sync
                conn->wpos = 0;
                return -EMSGSIZE;
            }

            // Compact buffer: move active data to front
            size_t active = conn->wpos - conn->rpos;
            memmove(conn->buf, conn->buf + conn->rpos, active);
            conn->rpos = 0;
            conn->wpos = active;
        }

        // 3. Read from socket
        ssize_t n = read(conn->fd, conn->buf + conn->wpos, sizeof(conn->buf) - conn->wpos);
        if (n < 0)
        {
            if (errno == EINTR) continue;
            return -errno;
        }
        if (n == 0) return -ECONNRESET;

        conn->wpos += n;
    }
}

int ipc_conn_send(ipc_conn *conn, const char *msg, size_t len)
{
    if (!conn || !msg) return -EINVAL;

    // Check limits (1MB max output buffer)
    const size_t max_out_buffer = (size_t) 1024 * 1024;
    if (conn->out_len + len + 1 > max_out_buffer)
    {
        return -ENOBUFS;  // Buffer full
    }

    // Append to buffer
    size_t new_len = conn->out_len + len + 1;  // +1 for newline
    char *new_buf = realloc(conn->out_buf, new_len);
    if (!new_buf) return -ENOMEM;

    conn->out_buf = new_buf;
    memcpy(conn->out_buf + conn->out_len, msg, len);
    conn->out_buf[conn->out_len + len] = '\n';
    conn->out_len += len + 1;

    // Try to flush immediately
    return ipc_conn_flush(conn) < 0 ? -1 : 0;
}

int ipc_conn_flush(ipc_conn *conn)
{
    if (!conn) return -1;
    if (conn->out_pos >= conn->out_len) return 0;  // Nothing to write

    ssize_t n = write(conn->fd, conn->out_buf + conn->out_pos, conn->out_len - conn->out_pos);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;  // Pending
        return -errno;
    }

    conn->out_pos += n;

    if (conn->out_pos >= conn->out_len)
    {
        // Fully flushed, free buffer
        free(conn->out_buf);
        conn->out_buf = NULL;
        conn->out_len = 0;
        conn->out_pos = 0;
        return 0;
    }

    return 1;  // Partial
}

int ipc_conn_has_pending(const ipc_conn *conn)
{
    return (conn && conn->out_len > conn->out_pos) ? 1 : 0;
}

void ipc_conn_destroy(ipc_conn **conn_ptr)
{
    if (!conn_ptr || !*conn_ptr) return;
    ipc_conn *conn = *conn_ptr;
    if (conn->fd >= 0) close(conn->fd);
    free(conn->plugin_id);
    free(conn->out_buf);
    free(conn);
    *conn_ptr = NULL;
}

void ipc_conn_set_plugin_id(ipc_conn *conn, const char *plugin_id)
{
    if (!conn) return;
    free(conn->plugin_id);
    conn->plugin_id = plugin_id ? strdup(plugin_id) : NULL;
}

const char *ipc_conn_plugin_id(const ipc_conn *conn) { return conn ? conn->plugin_id : NULL; }

int ipc_server_fd(const ipc_server *srv) { return srv ? srv->fd : -1; }
int ipc_conn_fd(const ipc_conn *conn) { return conn ? conn->fd : -1; }
