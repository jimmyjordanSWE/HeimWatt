/**
 * @file tcp_client.h
 * @brief TCP client (outbound connections)
 *
 * Raw TCP socket operations for connecting to remote hosts.
 * Can be used directly or as a base for http_client on embedded targets.
 */

#ifndef HEIMWATT_TCP_CLIENT_H
#define HEIMWATT_TCP_CLIENT_H

#include <stddef.h>

typedef struct tcp_client_socket tcp_client_socket;

/* ============================================================
 * CONNECTION
 * ============================================================ */

/**
 * Connect to a remote host.
 *
 * @param sock     Output pointer for socket
 * @param host     Hostname or IP address
 * @param port     Port number
 * @return 0 on success, -1 on error (check errno)
 */
int tcp_client_connect(tcp_client_socket** sock, const char* host, int port);

/**
 * Close connection.
 *
 * @param sock Pointer to socket (set to NULL on return)
 */
void tcp_client_close(tcp_client_socket** sock);

/* ============================================================
 * I/O (blocking)
 * ============================================================ */

/**
 * Receive data (blocking).
 *
 * @param sock Socket
 * @param buf  Buffer to receive into
 * @param len  Buffer size
 * @return Bytes received, 0 on connection closed, -1 on error
 */
int tcp_client_recv(tcp_client_socket* sock, char* buf, size_t len);

/**
 * Send data (blocking).
 *
 * @param sock Socket
 * @param buf  Data to send
 * @param len  Data length
 * @return Bytes sent, -1 on error
 */
int tcp_client_send(tcp_client_socket* sock, const char* buf, size_t len);

/* ============================================================
 * NON-BLOCKING I/O
 * ============================================================ */

int tcp_client_recv_nonblock(tcp_client_socket* sock, char* buf, size_t len);
int tcp_client_send_nonblock(tcp_client_socket* sock, const char* buf, size_t len);

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

/**
 * Set socket timeout.
 *
 * @param sock       Socket
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return 0 on success, -1 on error
 */
int tcp_client_set_timeout(tcp_client_socket* sock, int timeout_ms);

/**
 * Set non-blocking mode.
 *
 * @param sock   Socket
 * @param enable 1 to enable, 0 to disable
 * @return 0 on success, -1 on error
 */
int tcp_client_set_nonblocking(tcp_client_socket* sock, int enable);

/**
 * Set TCP_NODELAY (disable Nagle's algorithm).
 */
int tcp_client_set_nodelay(tcp_client_socket* sock, int enable);

/* ============================================================
 * INFO
 * ============================================================ */

/**
 * Get underlying file descriptor.
 */
int tcp_client_fd(const tcp_client_socket* sock);

/**
 * Check if connected.
 */
int tcp_client_is_connected(const tcp_client_socket* sock);

#endif /* HEIMWATT_TCP_CLIENT_H */
