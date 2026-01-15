/**
 * @file ipc.h
 * @brief Core-side IPC server
 *
 * Unix domain socket server for plugin communication.
 * JSON messages, newline-delimited.
 */

#ifndef HEIMWATT_IPC_H
#define HEIMWATT_IPC_H

#include <stddef.h>

typedef struct ipc_server ipc_server;
typedef struct ipc_conn ipc_conn;

/* ============================================================
 * SERVER LIFECYCLE
 * ============================================================ */

/**
 * Initialize IPC server.
 *
 * @param srv         Output pointer for server
 * @param socket_path Path to Unix socket
 * @return 0 on success, -1 on error
 */
int ipc_server_init(ipc_server **srv, const char *socket_path);

/**
 * Destroy IPC server.
 *
 * @param srv Pointer to server (set to NULL on return)
 */
void ipc_server_destroy(ipc_server **srv);

/**
 * Accept a connection (blocks until connection or error).
 *
 * @param srv  IPC server
 * @param conn Output pointer for connection
 * @return 0 on success, -1 on error
 */
int ipc_server_accept(ipc_server *srv, ipc_conn **conn);

/* ============================================================
 * CONNECTION OPERATIONS
 * ============================================================ */

/**
 * Receive a message.
 *
 * @param conn Connection
 * @param msg  Output message (caller frees)
 * @param len  Output message length
 * @return 0 on success, -1 on error
 */
int ipc_conn_recv(ipc_conn *conn, char **msg, size_t *len);

/**
 * Send a message.
 *
 * @param conn Connection
 * @param msg  Message to send
 * @param len  Message length
 * @return 0 on success, -1 on error
 */
int ipc_conn_send(ipc_conn *conn, const char *msg, size_t len);

/**
 * Flush pending write buffer.
 *
 * @param conn Connection
 * @return 0 on success (fully flushed), 1 (partially flushed, still pending), -1 on error
 */
int ipc_conn_flush(ipc_conn *conn);

/**
 * Check if connection has pending output.
 *
 * @param conn Connection
 * @return 1 if pending data exists, 0 otherwise
 */
int ipc_conn_has_pending(const ipc_conn *conn);

/**
 * Destroy connection and free resources.
 *
 * @param conn Pointer to connection (set to NULL on return)
 */
void ipc_conn_destroy(ipc_conn **conn);

/* ============================================================
 * CONNECTION IDENTIFICATION
 * ============================================================ */

/**
 * Set plugin ID for a connection.
 *
 * @param conn      Connection
 * @param plugin_id Plugin identifier
 */
void ipc_conn_set_plugin_id(ipc_conn *conn, const char *plugin_id);

/**
 * Get plugin ID for a connection.
 *
 * @param conn Connection
 * @return Plugin ID or NULL
 */
const char *ipc_conn_plugin_id(const ipc_conn *conn);

/* ============================================================
 * FILE DESCRIPTORS (for poll/select/epoll)
 * ============================================================ */

/**
 * Get server file descriptor.
 */
int ipc_server_fd(const ipc_server *srv);

/**
 * Get connection file descriptor.
 */
int ipc_conn_fd(const ipc_conn *conn);

#endif /* HEIMWATT_IPC_H */
