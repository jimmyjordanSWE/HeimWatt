/**
 * @file http_parse.h
 * @brief HTTP request/response parsing
 *
 * Stateless HTTP/1.1 request and response parsing.
 */

#ifndef HEIMWATT_HTTP_PARSE_H
#define HEIMWATT_HTTP_PARSE_H

#include <stddef.h>

/* Maximum sizes */
#define HTTP_MAX_METHOD 16
#define HTTP_MAX_PATH 2048
#define HTTP_MAX_HEADERS 64
#define HTTP_MAX_HEADER_NAME 64
#define HTTP_MAX_HEADER_VALUE 4096

/**
 * HTTP request structure.
 */
typedef struct {
    char method[HTTP_MAX_METHOD]; /**< "GET", "POST", etc. */
    char path[HTTP_MAX_PATH];     /**< "/api/plan" */
    char query[HTTP_MAX_PATH];    /**< "foo=bar" (without '?') */

    /* Headers */
    struct {
        char name[HTTP_MAX_HEADER_NAME];
        char value[HTTP_MAX_HEADER_VALUE];
    } headers[HTTP_MAX_HEADERS];
    size_t header_count;

    /* Body */
    char* body; /**< Heap-allocated, caller frees */
    size_t body_len;
} http_request;

/**
 * HTTP response structure.
 */
typedef struct {
    int status_code; /**< 200, 404, etc. */

    /* Headers */
    struct {
        char name[HTTP_MAX_HEADER_NAME];
        char value[HTTP_MAX_HEADER_VALUE];
    } headers[HTTP_MAX_HEADERS];
    size_t header_count;

    /* Body */
    char* body; /**< Heap-allocated, caller frees */
    size_t body_len;
} http_response;

/* ============================================================
 * PARSING
 * ============================================================ */

/**
 * Parse raw HTTP request.
 *
 * @param raw Raw HTTP data
 * @param len Data length
 * @param req Output request structure
 * @return 0 on success, -1 on parse error
 */
int http_parse_request(const char* raw, size_t len, http_request* req);

/**
 * Serialize HTTP response to wire format.
 * Caller frees *out.
 *
 * @param resp    Response to serialize
 * @param out     Output buffer
 * @param out_len Output length
 * @return 0 on success, -1 on error
 */
int http_serialize_response(const http_response* resp, char** out, size_t* out_len);

/* ============================================================
 * HEADER HELPERS
 * ============================================================ */

/**
 * Get request header by name (case-insensitive).
 *
 * @param req  Request
 * @param name Header name
 * @return Header value or NULL if not found
 */
const char* http_request_header(const http_request* req, const char* name);

/**
 * Set response header.
 *
 * @param resp  Response
 * @param name  Header name
 * @param value Header value
 */
void http_response_set_header(http_response* resp, const char* name, const char* value);

/* ============================================================
 * CONVENIENCE FUNCTIONS
 * ============================================================ */

/**
 * Set JSON body with Content-Type header.
 *
 * @param resp Response
 * @param json JSON string
 */
void http_response_set_json(http_response* resp, const char* json);

/**
 * Set status with reason phrase.
 *
 * @param resp Response
 * @param code HTTP status code
 */
void http_response_set_status(http_response* resp, int code);

/* ============================================================
 * CLEANUP
 * ============================================================ */

/**
 * Initialize request structure.
 */
void http_request_init(http_request* req);

/**
 * Initialize response structure.
 */
void http_response_init(http_response* resp);

/**
 * Free request resources.
 */
void http_request_destroy(http_request* req);

/**
 * Free response resources.
 */
void http_response_destroy(http_response* resp);

#endif /* HEIMWATT_HTTP_PARSE_H */
