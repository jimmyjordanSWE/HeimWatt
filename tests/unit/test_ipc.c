/**
 * @file test_ipc.c
 * @brief Unit tests for IPC server/connection layer
 */

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/ipc.h"
#include "libs/unity/unity.h"

static const char *TEST_SOCKET = "/tmp/heimwatt_test_ipc.sock";

// --- Server Lifecycle Tests ---

void test_ipc_server_init_destroy(void)
{
    ipc_server *srv = NULL;
    int ret = ipc_server_init(&srv, TEST_SOCKET);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(srv);
    TEST_ASSERT_TRUE(ipc_server_fd(srv) >= 0);

    ipc_server_destroy(&srv);
    TEST_ASSERT_NULL(srv);

    // Double destroy should be safe
    ipc_server_destroy(&srv);
}

void test_ipc_server_init_null_params(void)
{
    ipc_server *srv = NULL;

    // NULL output pointer
    int ret = ipc_server_init(NULL, TEST_SOCKET);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);

    // NULL socket path
    ret = ipc_server_init(&srv, NULL);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
    TEST_ASSERT_NULL(srv);
}

// --- Connection Tests (require client thread) ---

typedef struct
{
    const char *socket_path;
    const char *msg_to_send;
    char *msg_received;
    int connected;
    int done;
} client_ctx;

static void *client_thread_fn(void *arg)
{
    client_ctx *ctx = (client_ctx *) arg;

    // Connect to server
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", ctx->socket_path);

    usleep(50000);  // Wait for server to be ready

    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        close(fd);
        return NULL;
    }
    ctx->connected = 1;

    // Send message if provided
    if (ctx->msg_to_send)
    {
        write(fd, ctx->msg_to_send, strlen(ctx->msg_to_send));
        write(fd, "\n", 1);  // Delimiter
    }

    // Read response
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0)
    {
        buf[n] = 0;
        ctx->msg_received = strdup(buf);
    }

    close(fd);
    ctx->done = 1;
    return NULL;
}

void test_ipc_conn_send_recv_roundtrip(void)
{
    ipc_server *srv = NULL;
    int ret = ipc_server_init(&srv, TEST_SOCKET);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Start client thread
    client_ctx ctx = {
        .socket_path = TEST_SOCKET, .msg_to_send = "hello", .connected = 0, .done = 0};

    pthread_t client;
    pthread_create(&client, NULL, client_thread_fn, &ctx);

    // Accept connection
    ipc_conn *conn = NULL;
    ret = ipc_server_accept(srv, &conn);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(conn);

    // Receive message from client
    char *msg = NULL;
    size_t len = 0;
    ret = ipc_conn_recv(conn, &msg, &len);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("hello", msg);
    free(msg);

    // Send response
    ret = ipc_conn_send(conn, "world", 5);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Cleanup
    pthread_join(client, NULL);
    ipc_conn_destroy(&conn);
    ipc_server_destroy(&srv);

    TEST_ASSERT_NOT_NULL(ctx.msg_received);
    TEST_ASSERT_TRUE(strstr(ctx.msg_received, "world") != NULL);
    free(ctx.msg_received);
}

void test_ipc_conn_plugin_id(void)
{
    ipc_server *srv = NULL;
    int ret = ipc_server_init(&srv, TEST_SOCKET);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Start client that just connects
    client_ctx ctx = {.socket_path = TEST_SOCKET, .msg_to_send = "test", .connected = 0, .done = 0};

    pthread_t client;
    pthread_create(&client, NULL, client_thread_fn, &ctx);

    // Accept and set plugin ID
    ipc_conn *conn = NULL;
    ret = ipc_server_accept(srv, &conn);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ipc_conn_set_plugin_id(conn, "se.smhi.weather");
    TEST_ASSERT_EQUAL_STRING("se.smhi.weather", ipc_conn_plugin_id(conn));

    // Change plugin ID
    ipc_conn_set_plugin_id(conn, "com.tibber.price");
    TEST_ASSERT_EQUAL_STRING("com.tibber.price", ipc_conn_plugin_id(conn));

    // Read message to unblock client
    char *msg = NULL;
    size_t len = 0;
    (void) ipc_conn_recv(conn, &msg, &len);
    free(msg);

    // Send response so client can finish
    ipc_conn_send(conn, "ok", 2);

    pthread_join(client, NULL);
    ipc_conn_destroy(&conn);
    ipc_server_destroy(&srv);
    free(ctx.msg_received);
}

void test_ipc_conn_recv_null_params(void)
{
    char *msg = NULL;
    size_t len = 0;

    int ret = ipc_conn_recv(NULL, &msg, &len);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_ipc_conn_send_null_params(void)
{
    int ret = ipc_conn_send(NULL, "test", 4);
    TEST_ASSERT_EQUAL_INT(-EINVAL, ret);
}

void test_ipc_conn_fd_null(void)
{
    int fd = ipc_conn_fd(NULL);
    TEST_ASSERT_EQUAL_INT(-1, fd);
}

void test_ipc_server_fd_null(void)
{
    int fd = ipc_server_fd(NULL);
    TEST_ASSERT_EQUAL_INT(-1, fd);
}
