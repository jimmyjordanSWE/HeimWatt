/**
 * @file test_http_server.c
 * @brief Unit tests for HTTP server
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "libs/unity/unity.h"
#include "net/http_server.h"

// --- Lifecycle Tests ---

void test_http_server_create_destroy(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 0);  // Port 0 = ephemeral
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(srv);

    http_server_destroy(&srv);
    TEST_ASSERT_NULL(srv);

    // Double destroy should be safe
    http_server_destroy(&srv);
}

void test_http_server_create_null_output(void)
{
    int ret = http_server_create(NULL, 8080);
    TEST_ASSERT_TRUE(ret < 0);
}

// --- Configuration Tests ---

void test_http_server_set_timeout(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 0);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should not crash even with extreme values
    http_server_set_timeout(srv, 1000);
    http_server_set_timeout(srv, 0);
    http_server_set_timeout(srv, 60000);

    http_server_destroy(&srv);
}

void test_http_server_set_max_connections(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 0);
    TEST_ASSERT_EQUAL_INT(0, ret);

    http_server_set_max_connections(srv, 100);
    http_server_set_max_connections(srv, 1);
    http_server_set_max_connections(srv, 10000);

    http_server_destroy(&srv);
}

// --- Handler Registration Tests ---

static int dummy_handler(const http_request *req, http_response *resp, void *ctx)
{
    (void) req;
    (void) resp;
    (void) ctx;
    return 0;
}

void test_http_server_set_handler(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 0);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should accept handler
    http_server_set_handler(srv, dummy_handler, NULL);

    // Should accept NULL handler (clears)
    http_server_set_handler(srv, NULL, NULL);

    http_server_destroy(&srv);
}

// --- State Tests ---

void test_http_server_not_running_initially(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 0);
    TEST_ASSERT_EQUAL_INT(0, ret);

    TEST_ASSERT_FALSE(http_server_is_running(srv));

    http_server_destroy(&srv);
}

void test_http_server_port_assigned(void)
{
    http_server *srv = NULL;
    int ret = http_server_create(&srv, 8888);  // Use fixed port
    TEST_ASSERT_EQUAL_INT(0, ret);

    int port = http_server_port(srv);
    TEST_ASSERT_EQUAL_INT(8888, port);  // Should match what we passed

    http_server_destroy(&srv);
}
