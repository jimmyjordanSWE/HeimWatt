/*
 * @file tcp.h
 * @brief Raw TCP socket operations
 *
 * Lowest level of the network stack. Pure POSIX socket operations.
 */

#ifndef HEIMWATT_TCP_H
#define HEIMWATT_TCP_H

#include <stddef.h>

typedef struct tcp_socket tcp_socket;

/* ============================================================
 * SERVER OPERATIONS
 * ============================================================ */

/*
 * Create and bind a listening socket.
 *
 * @param sock    Output pointer for socket
 * @param port    Port to listen on
 * @param backlog Connection queue size
 * @return 0 on success, -1 on error (check errno)
 */
int tcp_listen(tcp_socket** sock, int port, int backlog);

/*
 * Accept a client connection.
 *
 * @param sock   Listening socket
 * @param client Output pointer for client socket
 * @return 0 on success, -1 on error
 */
int tcp_accept(tcp_socket* sock, tcp_socket** client);

/*
 * Close a socket.
 *
 * @param sock Pointer to socket (set to NULL on return)
 */
void tcp_close(tcp_socket** sock);

/* ============================================================
 * I/O (blocking)
 * ============================================================ */

/*
 * Receive data (blocking).
 *
 * @param sock Socket
 * @param buf  Buffer to receive into
 * @param len  Buffer size
 * @return Bytes received, 0 on connection closed, -1 on error
 */
int tcp_recv(tcp_socket* sock, char* buf, size_t len);

/*
 * Send data (blocking).
 *
 * @param sock Socket
 * @param buf  Data to send
 * @param len  Data length
 * @return Bytes sent, -1 on error
 */
int tcp_send(tcp_socket* sock, const char* buf, size_t len);

/* ============================================================
 * NON-BLOCKING I/O
 * ============================================================ */

/*
 * Receive data (non-blocking).
 *
 * @return Bytes received, 0 on connection closed, -1 on EAGAIN/error
 */
int tcp_recv_nonblock(tcp_socket* sock, char* buf, size_t len);

/*
 * Send data (non-blocking).
 *
 * @return Bytes sent, -1 on EAGAIN/error
 */
int tcp_send_nonblock(tcp_socket* sock, const char* buf, size_t len);

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

/*
 * Set non-blocking mode.
 *
 * @param sock   Socket
 * @param enable 1 to enable, 0 to disable
 * @return 0 on success, -1 on error
 */
int tcp_set_nonblocking(tcp_socket* sock, int enable);

/*
 * Set SO_REUSEADDR option.
 */
int tcp_set_reuseaddr(tcp_socket* sock, int enable);

/*
 * Set TCP_NODELAY option.
 */
int tcp_set_nodelay(tcp_socket* sock, int enable);

/* ============================================================
 * INFO
 * ============================================================ */

/*
 * Get underlying file descriptor.
 */
int tcp_fd(const tcp_socket* sock);

/*
 * Get peer address as string.
 *
 * @param sock Socket
 * @param buf  Buffer for address string
 * @param len  Buffer size
 * @return 0 on success, -1 on error
 */
int tcp_peer_addr(const tcp_socket* sock, char* buf, size_t len);

/*
 * Get local port number.
 */
int tcp_local_port(const tcp_socket* sock);

#endif /* HEIMWATT_TCP_H */
