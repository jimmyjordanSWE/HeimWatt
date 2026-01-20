/**
 * @file tcp_server.c
 * @brief Raw TCP socket operations implementation
 */

#include "tcp_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "memory.h"

struct tcp_socket
{
    int fd;
    int port;
};

int tcp_listen(tcp_socket **sock, int port, int backlog)
{
    if (!sock) return -EINVAL;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -errno;

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        int err = errno;
        close(fd);
        return -err;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        int err = errno;
        close(fd);
        return -err;
    }

    if (listen(fd, backlog) < 0)
    {
        int err = errno;
        close(fd);
        return -err;
    }

    tcp_socket *s = mem_alloc(sizeof(*s));
    if (!s)
    {
        close(fd);
        return -ENOMEM;
    }
    s->fd = fd;
    s->port = port;
    *sock = s;
    return 0;
}

int tcp_accept(tcp_socket *sock, tcp_socket **client)
{
    if (!sock || !client) return -EINVAL;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = accept(sock->fd, (struct sockaddr *) &addr, &len);
    if (fd < 0) return -errno;

    tcp_socket *c = mem_alloc(sizeof(*c));
    if (!c)
    {
        close(fd);
        return -ENOMEM;
    }
    c->fd = fd;
    c->port = ntohs(addr.sin_port);
    *client = c;
    return 0;
}

void tcp_close(tcp_socket **sock)
{
    if (sock && *sock)
    {
        if ((*sock)->fd >= 0) close((*sock)->fd);
        mem_free(*sock);
        *sock = NULL;
    }
}

int tcp_recv(tcp_socket *sock, char *buf, size_t len)
{
    if (!sock || !buf) return -EINVAL;
    ssize_t n = recv(sock->fd, buf, len, 0);
    if (n < 0) return -errno;
    return (int) n;
}

int tcp_send(tcp_socket *sock, const char *buf, size_t len)
{
    if (!sock || !buf) return -EINVAL;
    size_t total = 0;
    while (total < len)
    {
        ssize_t n = send(sock->fd, buf + total, len - total, 0);
        if (n < 0) return -errno;
        total += n;
    }
    return (int) total;
}

int tcp_set_nonblocking(tcp_socket *sock, int enable)
{
    if (!sock) return -EINVAL;
    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) return -errno;
    if (enable)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(sock->fd, F_SETFL, flags) < 0) return -errno;
    return 0;
}

int tcp_set_reuseaddr(tcp_socket *sock, int enable)
{
    if (!sock) return -EINVAL;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    return 0;
}

int tcp_fd(const tcp_socket *sock) { return sock ? sock->fd : -1; }

int tcp_peer_addr(const tcp_socket *sock, char *buf, size_t len)
{
    if (!sock || !buf) return -EINVAL;
    struct sockaddr_in addr;
    socklen_t l = sizeof(addr);
    if (getpeername(sock->fd, (struct sockaddr *) &addr, &l) < 0) return -errno;
    inet_ntop(AF_INET, &addr.sin_addr, buf, len);
    return 0;
}
