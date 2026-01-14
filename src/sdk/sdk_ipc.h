/**
 * @file sdk_ipc.h
 * @brief Plugin-side IPC client (internal)
 *
 * IPC client for plugin-to-Core communication.
 */

#ifndef SDK_IPC_H
#define SDK_IPC_H

#include <errno.h>
#include <stddef.h>

typedef struct ipc_client ipc_client;

/**
 * Create IPC client and connect to Core's socket.
 *
 * @param client      Output pointer for client
 * @param socket_path Path to Unix socket
 * @return 0 on success, negative errno on error
 */
int ipc_client_create(ipc_client** client, const char* socket_path);

/**
 * Destroy IPC client and disconnect from Core.
 *
 * @param client Pointer to client (set to NULL on return)
 */
void ipc_client_destroy(ipc_client** client);

/**
 * Send message (blocks until sent).
 *
 * @param client Client
 * @param msg    Message to send
 * @param len    Message length
 * @return 0 on success, negative errno on error
 */
int ipc_client_send(ipc_client* client, const char* msg, size_t len);

/**
 * Receive message (blocks until received or timeout).
 * Caller frees *msg.
 *
 * @param client     Client
 * @param msg        Output message
 * @param len        Output message length
 * @param timeout_ms Timeout in milliseconds (-1 = forever)
 * @return 0 on success, negative errno on error/timeout
 */
int ipc_client_recv(ipc_client* client, char** msg, size_t* len, int timeout_ms);

/**
 * Get file descriptor (for poll/select).
 *
 * @param client Client
 * @return File descriptor
 */
int ipc_client_fd(const ipc_client* client);

#endif /* SDK_IPC_H */
