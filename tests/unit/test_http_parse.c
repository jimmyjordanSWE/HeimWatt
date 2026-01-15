/**
 * @file test_http_parse.c
 * @brief Unit tests for HTTP parser
 */

#include <stdlib.h>
#include <string.h>

#include "libs/unity/unity.h"
#include "net/http_parse.h"

// --- http_parse_request tests ---

void test_http_parse_simple_get(void)
{
    http_request req;
    const char *raw = "GET /api/status HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("GET", req.method);
    TEST_ASSERT_EQUAL_STRING("/api/status", req.path);
    TEST_ASSERT_EQUAL_STRING("", req.query);
    http_request_destroy(&req);
}

void test_http_parse_get_with_query(void)
{
    http_request req;
    const char *raw = "GET /api?foo=bar&baz=1 HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("GET", req.method);
    TEST_ASSERT_EQUAL_STRING("/api", req.path);
    TEST_ASSERT_EQUAL_STRING("foo=bar&baz=1", req.query);
    http_request_destroy(&req);
}

void test_http_parse_with_headers(void)
{
    http_request req;
    const char *raw =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(2, req.header_count);
    TEST_ASSERT_EQUAL_STRING("Host", req.headers[0].name);
    TEST_ASSERT_EQUAL_STRING("example.com", req.headers[0].value);
    http_request_destroy(&req);
}

void test_http_parse_post_method(void)
{
    http_request req;
    const char *raw = "POST /data HTTP/1.1\r\n\r\n";
    int ret = http_parse_request(raw, strlen(raw), &req);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_STRING("POST", req.method);
    TEST_ASSERT_EQUAL_STRING("/data", req.path);
    http_request_destroy(&req);
}

void test_http_parse_malformed_no_crlf(void)
{
    http_request req;
    const char *raw = "GARBAGE";
    int ret = http_parse_request(raw, strlen(raw), &req);

    TEST_ASSERT_EQUAL_INT(-1, ret);
}

void test_http_parse_null_input(void)
{
    http_request req;
    int ret = http_parse_request(NULL, 0, &req);
    TEST_ASSERT_EQUAL_INT(-1, ret);

    ret = http_parse_request("GET / HTTP/1.1\r\n\r\n", 20, NULL);
    TEST_ASSERT_EQUAL_INT(-1, ret);
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

    free(out);
    http_response_destroy(&resp);
}
