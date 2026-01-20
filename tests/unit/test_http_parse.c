/**
 * @file test_http_parse.c
 * @brief Unit tests for HTTP parser
 */

#include <stdlib.h>
#include <string.h>

#include "libs/unity/unity.h"
#include "memory.h"
#include "net/http_parse.h"

// --- http_parse_request tests ---

static HwArena *test_arena;

static void setup_arena(void) { test_arena = hw_arena_create(4096); }

static void teardown_arena(void)
{
    if (test_arena) hw_arena_destroy(&test_arena);
}

void test_http_parse_simple_get(void)
{
    setup_arena();
    http_request req;
    const char *raw = "GET /api/status HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req, test_arena);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("GET", req.method);
    TEST_ASSERT_EQUAL_STRING("/api/status", req.path);
    TEST_ASSERT_NULL(req.query);
    http_request_destroy(&req);
    teardown_arena();
}

void test_http_parse_get_with_query(void)
{
    setup_arena();
    http_request req;
    const char *raw = "GET /api?foo=bar&baz=1 HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req, test_arena);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("GET", req.method);
    TEST_ASSERT_EQUAL_STRING("/api", req.path);
    TEST_ASSERT_EQUAL_STRING("foo=bar&baz=1", req.query);
    http_request_destroy(&req);
    teardown_arena();
}

void test_http_parse_with_headers(void)
{
    setup_arena();
    http_request req;
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req, test_arena);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(2, req.header_count);
    TEST_ASSERT_EQUAL_STRING("Host", req.headers[0].name);
    TEST_ASSERT_EQUAL_STRING("example.com", req.headers[0].value);
    http_request_destroy(&req);
    teardown_arena();
}

void test_http_parse_post_method(void)
{
    setup_arena();
    http_request req;
    const char *raw = "POST /data HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req, test_arena);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("POST", req.method);
    TEST_ASSERT_EQUAL_STRING("/data", req.path);
    http_request_destroy(&req);
    teardown_arena();
}

void test_http_parse_malformed_no_crlf(void)
{
    setup_arena();
    http_request req;
    const char *raw = "GARBAGE";
    int ret = http_parse_request(raw, strlen(raw), &req, test_arena);

    TEST_ASSERT_EQUAL_INT(-1, ret);
    teardown_arena();
}

void test_http_parse_null_input(void)
{
    setup_arena();
    http_request req;
    int ret = http_parse_request(NULL, 0, &req, test_arena);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    ret = http_parse_request("GET / HTTP/1.1\r\n\r\n", 20, NULL, test_arena);
    TEST_ASSERT_EQUAL_INT(-1, ret);
    teardown_arena();
}

// --- http_serialize_response tests ---

void test_http_serialize_response_basic(void)
{
    http_response resp;
    http_response_init(&resp);
    http_response_set_status(&resp, 200);
    http_response_set_json(&resp, "{\"ok\":true}");

    char *out = NULL;
    size_t out_len = 0;
    int ret = http_serialize_response(&resp, &out, &out_len);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_TRUE(out_len > 0);
    TEST_ASSERT_NOT_NULL(strstr(out, "HTTP/1.1 200 OK"));
    TEST_ASSERT_NOT_NULL(strstr(out, "Content-Type: application/json"));
    TEST_ASSERT_NOT_NULL(strstr(out, "{\"ok\":true}"));

    mem_free(out);
    http_response_destroy(&resp);
}
