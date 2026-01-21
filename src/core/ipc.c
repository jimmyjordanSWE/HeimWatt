// #define _GNU_SOURCE (Defined in Makefile)
#include "ipc.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "memory.h"

struct ipc_server
{
    int fd;
    int epoll_fd;
    char *socket_path;
};

struct ipc_conn
{
    int fd;
    char *plugin_id;
    // Buffering for robust reads
    HwBuffer read_buf;

    // Output buffering for non-blocking writes
    HwBuffer out_buf;
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

    ipc_server *srv = mem_alloc(sizeof(*srv));
    if (!srv) return -ENOMEM;
    srv->fd = -1;
    srv->epoll_fd = -1;

    size_t path_len = strlen(socket_path);
    srv->socket_path = mem_alloc(path_len + 1);
    if (!srv->socket_path)
    {
        ret = -ENOMEM;
        goto cleanup;
    }
    memcpy(srv->socket_path, socket_path, path_len + 1);

    srv->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->fd < 0)
    {
        ret = -errno;
        goto cleanup;
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

    // Create epoll instance
    srv->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (srv->epoll_fd < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    // Add listen socket to epoll
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = srv};
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->fd, &ev) < 0)
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

    if (srv->epoll_fd >= 0) close(srv->epoll_fd);
    if (srv->fd >= 0) close(srv->fd);
    if (srv->socket_path)
    {
        unlink(srv->socket_path);
        mem_free(srv->socket_path);
    }

    mem_free(srv);
    *srv_ptr = NULL;
}

int ipc_server_accept(ipc_server *srv, ipc_conn **conn_out)
{
    if (!srv || !conn_out) return -EINVAL;

    int new_fd = accept(srv->fd, NULL, NULL);
    if (new_fd < 0) return -errno;

    ipc_conn *conn = mem_alloc(sizeof(*conn));
    if (!conn)
    {
        close(new_fd);
        return -ENOMEM;
    }
    conn->fd = new_fd;

    // Set non-blocking
    set_nonblocking(conn->fd);

    // Register with epoll (level-triggered for simplicity)
    struct epoll_event ev = {.events = EPOLLIN, .data.ptr = conn};
    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev) < 0)
    {
        int saved_errno = errno;
        close(new_fd);
        mem_free(conn);
        return -saved_errno;
    }

    *conn_out = conn;
    return 0;
}

int ipc_conn_recv(ipc_conn *conn, char **msg, size_t *len)
{
    if (!conn || !msg || !len) return -EINVAL;

    // 1. Check if we already have a newline in buffer
    while (1)
    {
        char *newline = NULL;
        if (conn->read_buf.len > 0 && conn->read_buf.data)
        {
            newline = memchr(conn->read_buf.data, '\n', conn->read_buf.len);
        }

        if (newline)
        {
            size_t msg_len = newline - conn->read_buf.data;
            char *ret_msg = mem_alloc(msg_len + 1);
            if (!ret_msg) return -ENOMEM;

            memcpy(ret_msg, conn->read_buf.data, msg_len);
            ret_msg[msg_len] = 0;

            *msg = ret_msg;
            *len = msg_len;

            // Remove line from buffer (consume)
            size_t consume = msg_len + 1;  // +1 for \n
            size_t remaining = conn->read_buf.len - consume;
            if (remaining > 0)
            {
                memmove(conn->read_buf.data, conn->read_buf.data + consume, remaining);
            }
            conn->read_buf.len = remaining;

            return 0;
        }

        // 2. Read from socket
        char tmp[4096];
        ssize_t n = read(conn->fd, tmp, sizeof(tmp));
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return -EAGAIN;  // Was continue/implicit return
            if (errno == EINTR) continue;
            return -errno;
        }
        if (n == 0) return -ECONNRESET;

        if (hw_buffer_append(&conn->read_buf, tmp, n) < 0)
        {
            return -ENOMEM;
        }
    }
}

int ipc_conn_send(ipc_conn *conn, const char *msg, size_t len)
{
    if (!conn || !msg) return -EINVAL;

    // Append to buffer
    int ret = hw_buffer_append(&conn->out_buf, msg, len);
    if (ret < 0) return ret;

    ret = hw_buffer_append(&conn->out_buf, "\n", 1);
    if (ret < 0) return ret;

    // Try to flush immediately
    return ipc_conn_flush(conn) < 0 ? -1 : 0;
}

int ipc_conn_flush(ipc_conn *conn)
{
    if (!conn) return -1;
    if (conn->out_pos >= conn->out_buf.len) return 0;  // Nothing to write

    ssize_t n =
        write(conn->fd, conn->out_buf.data + conn->out_pos, conn->out_buf.len - conn->out_pos);
    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 1;  // Pending
        return -errno;
    }

    conn->out_pos += n;

    if (conn->out_pos >= conn->out_buf.len)
    {
        // Fully flushed, clear buffer (reuses memory)
        hw_buffer_clear(&conn->out_buf);
        conn->out_pos = 0;
        return 0;
    }

    return 1;  // Partial
}

int ipc_conn_has_pending(const ipc_conn *conn)
{
    return (conn && conn->out_buf.len > conn->out_pos) ? 1 : 0;
}

void ipc_conn_destroy(ipc_conn **conn_ptr)
{
    if (!conn_ptr || !*conn_ptr) return;
    ipc_conn *conn = *conn_ptr;
    if (conn->fd >= 0) close(conn->fd);
    mem_free(conn->plugin_id);
    hw_buffer_free(&conn->read_buf);
    hw_buffer_free(&conn->out_buf);
    mem_free(conn);
    *conn_ptr = NULL;
}

void ipc_conn_set_plugin_id(ipc_conn *conn, const char *plugin_id)
{
    if (!conn) return;
    mem_free(conn->plugin_id);
    if (plugin_id)
    {
        size_t len = strlen(plugin_id);
        conn->plugin_id = mem_alloc(len + 1);
        if (conn->plugin_id) strcpy(conn->plugin_id, plugin_id);
    }
    else
    {
        conn->plugin_id = NULL;
    }
}

const char *ipc_conn_plugin_id(const ipc_conn *conn) { return conn ? conn->plugin_id : NULL; }

int ipc_server_fd(const ipc_server *srv) { return srv ? srv->fd : -1; }
int ipc_conn_fd(const ipc_conn *conn) { return conn ? conn->fd : -1; }

int ipc_server_get_epoll_fd(const ipc_server *srv) { return srv ? srv->epoll_fd : -1; }

int ipc_server_poll(ipc_server *srv, struct epoll_event *events, int max_events, int timeout_ms)
{
    if (!srv || !events || srv->epoll_fd < 0) return -EINVAL;
    int ret = epoll_wait(srv->epoll_fd, events, max_events, timeout_ms);
    return ret < 0 ? -errno : ret;
}

int ipc_server_unregister_conn(ipc_server *srv, ipc_conn *conn)
{
    if (!srv || !conn || srv->epoll_fd < 0 || conn->fd < 0) return -EINVAL;
    // EPOLL_CTL_DEL ignores ev parameter in Linux 2.6.9+, but pass NULL for clarity
    (void) epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    return 0;
}

int ipc_server_update_conn_events(ipc_server *srv, ipc_conn *conn, uint32_t events)
{
    if (!srv || !conn || srv->epoll_fd < 0 || conn->fd < 0) return -EINVAL;
    struct epoll_event ev = {.events = events, .data.ptr = conn};
    return epoll_ctl(srv->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev) < 0 ? -errno : 0;
}

bool ipc_server_is_listen_event(const ipc_server *srv, void *event_ptr)
{
    return srv && event_ptr == srv;
}
