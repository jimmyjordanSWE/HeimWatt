/**
 * @file http_client.h
 * @brief HTTP client (wraps curl)
 *
 * Provides a simple HTTP client for outbound requests.
 * Implementation uses libcurl but can be swapped for embedded targets.
 */

#ifndef HEIMWATT_HTTP_CLIENT_H
#define HEIMWATT_HTTP_CLIENT_H

#include <stddef.h>

typedef struct http_client http_client;

/* ============================================================
 * LIFECYCLE
 * ============================================================ */

/**
 * Create and initialize HTTP client.
 *
 * @param client Output pointer for client
 * @return 0 on success, -1 on error
 */
int http_client_create(http_client **client);

/**
 * Destroy HTTP client.
 *
 * @param client Pointer to client (set to NULL on return)
 */
void http_client_destroy(http_client **client);

/* ============================================================
 * CONFIGURATION
 * ============================================================ */

/**
 * Set request timeout.
 *
 * @param client     HTTP client
 * @param timeout_ms Timeout in milliseconds
 */
void http_client_set_timeout(http_client *client, int timeout_ms);

/**
 * Set a default header for all requests.
 *
 * @param client HTTP client
 * @param name   Header name
 * @param value  Header value
 */
void http_client_set_header(http_client *client, const char *name, const char *value);

/**
 * Clear all default headers.
 *
 * @param client HTTP client
 */
void http_client_clear_headers(http_client *client);

/* ============================================================
 * REQUESTS (blocking)
 * ============================================================ */

/**
 * Perform HTTP GET request.
 *
 * @param client   HTTP client
 * @param url      Request URL
 * @param body_out Output response body (caller frees)
 * @param len_out  Output response length
 * @return HTTP status code, or -1 on error
 */
int http_get(http_client *client, const char *url, char **body_out, size_t *len_out);

/**
 * Perform HTTP POST with JSON body.
 *
 * @param client       HTTP client
 * @param url          Request URL
 * @param json_body    Request body (JSON string)
 * @param response_out Output response body (caller frees)
 * @param len_out      Output response length
 * @return HTTP status code, or -1 on error
 */
int http_post_json(http_client *client, const char *url, const char *json_body, char **response_out,
                   size_t *len_out);

/**
 * Perform HTTP POST with form data.
 *
 * @param client       HTTP client
 * @param url          Request URL
 * @param form_data    URL-encoded form data
 * @param response_out Output response body (caller frees)
 * @param len_out      Output response length
 * @return HTTP status code, or -1 on error
 */
int http_post_form(http_client *client, const char *url, const char *form_data, char **response_out,
                   size_t *len_out);

#endif /* HEIMWATT_HTTP_CLIENT_H */
