/**
 * @file http_parse.c
 * @brief HTTP parser implementation
 */

#include "http_parse.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void http_request_init(http_request *req)
{
    if (req) memset(req, 0, sizeof(*req));
}

void http_response_init(http_response *resp)
{
    if (resp)
    {
        memset(resp, 0, sizeof(*resp));
        resp->status_code = 200;
    }
}

void http_request_destroy(http_request *req)
{
    if (req)
    {
        free(req->body);
        req->body = NULL;
    }
}

void http_response_destroy(http_response *resp)
{
    if (resp)
    {
        free(resp->body);
        resp->body = NULL;
    }
}

// Simple parser for "METHOD /path?query HTTP/1.1"
int http_parse_request(const char *raw, size_t len, http_request *req)
{
    if (!raw || !req) return -1;
    http_request_init(req);

    // Make a copy to tokenize safely? Or scan.
    // For MVP, limit to headers.
    // Find first line
    const char *end_line = strstr(raw, "\r\n");
    if (!end_line) return -1;

    char line[1024];
    size_t line_len = end_line - raw;
    if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
    memcpy(line, raw, line_len);
    line[line_len] = 0;

    // METHOD PATH VER
    char *method = strtok(line, " ");
    char *path = strtok(NULL, " ");
    char *ver = strtok(NULL, " ");
    (void) ver;  // Unused for now

    if (!method || !path) return -1;

    snprintf(req->method, sizeof(req->method), "%s", method);

    // split path and query
    char *q = strchr(path, '?');
    if (q)
    {
        *q = 0;
        snprintf(req->path, sizeof(req->path), "%s", path);
        snprintf(req->query, sizeof(req->query), "%s", q + 1);
    }
    else
    {
        snprintf(req->path, sizeof(req->path), "%s", path);
        req->query[0] = 0;
    }

    // Headers - skip to next line
    const char *p = end_line + 2;
    while (p < raw + len)
    {
        // Check for empty line (end of headers)
        if (strncmp(p, "\r\n", 2) == 0)
        {
            p += 2;
            break;  // Body starts here
        }

        const char *eol = strstr(p, "\r\n");
        if (!eol) break;

        char hline[1024];
        size_t hlen = eol - p;
        if (hlen >= sizeof(hline)) hlen = sizeof(hline) - 1;
        memcpy(hline, p, hlen);
        hline[hlen] = 0;

        char *colon = strchr(hline, ':');
        if (colon)
        {
            *colon = 0;
            char *val = colon + 1;
            while (*val == ' ') val++;

            if (req->header_count < HTTP_MAX_HEADERS)
            {
                snprintf(req->headers[req->header_count].name, HTTP_MAX_HEADER_NAME, "%.*s",
                         HTTP_MAX_HEADER_NAME - 1, hline);
                snprintf(req->headers[req->header_count].value, HTTP_MAX_HEADER_VALUE, "%s", val);
                req->header_count++;
            }
        }
        p = eol + 2;
    }

    // Body?
    // Implementation of body reading is simplified for GET
    return 0;
}

int http_serialize_response(const http_response *resp, char **out, size_t *out_len)
{
    if (!resp || !out) return -1;

    // Buffer
    enum
    {
        BUF_SIZE = 16384
    };
    char *buf = malloc(BUF_SIZE);  // 16KB fixed for MVP
    if (!buf) return -1;

    size_t off = 0;
    size_t remaining = BUF_SIZE;
    int n = snprintf(buf + off, remaining, "HTTP/1.1 %d OK\r\n", resp->status_code);
    if (n > 0 && (size_t) n < remaining)
    {
        off += (size_t) n;
        remaining -= (size_t) n;
    }

    for (size_t i = 0; i < resp->header_count && remaining > 0; i++)
    {
        n = snprintf(buf + off, remaining, "%s: %s\r\n", resp->headers[i].name,
                     resp->headers[i].value);
        if (n > 0 && (size_t) n < remaining)
        {
            off += (size_t) n;
            remaining -= (size_t) n;
        }
    }

    n = snprintf(buf + off, remaining, "Content-Length: %zu\r\n", resp->body_len);
    if (n > 0 && (size_t) n < remaining)
    {
        off += (size_t) n;
        remaining -= (size_t) n;
    }

    n = snprintf(buf + off, remaining, "Connection: close\r\n");
    if (n > 0 && (size_t) n < remaining)
    {
        off += (size_t) n;
        remaining -= (size_t) n;
    }

    n = snprintf(buf + off, remaining, "\r\n");
    if (n > 0 && (size_t) n < remaining)
    {
        off += (size_t) n;
        remaining -= (size_t) n;
    }

    if (resp->body && resp->body_len > 0)
    {
        memcpy(buf + off, resp->body, resp->body_len);
        off += resp->body_len;
    }

    *out = buf;
    *out_len = off;
    return 0;
}

void http_response_set_json(http_response *resp, const char *json)
{
    if (!resp || !json) return;
    size_t len = strlen(json);
    resp->body = malloc(len + 1);
    // ...
    if (resp->body)
    {
        memcpy(resp->body, json, len + 1);
        resp->body_len = len;

        // Add header
        if (resp->header_count < HTTP_MAX_HEADERS)
        {
            snprintf(resp->headers[resp->header_count].name, sizeof(resp->headers[0].name),
                     "Content-Type");
            snprintf(resp->headers[resp->header_count].value, sizeof(resp->headers[0].value),
                     "application/json");
            resp->header_count++;
        }
    }
}

void http_response_set_status(http_response *resp, int code)
{
    if (resp) resp->status_code = code;
}
