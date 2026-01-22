/*
 * @file http_parse.c
 * @brief HTTP parser implementation
 */

#include "http_parse.h"
#include "memory.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void http_request_init(http_request* req) {
    if (req)
        memset(req, 0, sizeof(*req));
}

void http_response_init(http_response* resp) {
    if (resp) {
        memset(resp, 0, sizeof(*resp));
        resp->status_code = 200;
    }
}

void http_request_destroy(http_request* req) {
    // No-op if using arena (arena reset handles it)
    // But struct itself might be on stack/heap.
    // If we used direct mem_alloc for body in some manual cases, we might leak?
    // We assume http_request lifecycle is bound to arena now.
    (void) req;
}

void http_response_destroy(http_response* resp) {
    if (resp) {
        mem_free(resp->body);
        resp->body = NULL;
    }
}

// Simple parser for "METHOD /path?query HTTP/1.1"
static int parse_request_line(char* line, http_request* req, HwArena* arena) {
    char* method = strtok(line, " ");
    char* path = strtok(NULL, " ");
    char* ver = strtok(NULL, " ");
    (void) ver;

    if (!method || !path)
        return -1;

    size_t mlen = strlen(method);
    req->method = hw_arena_alloc(arena, mlen + 1);
    if (!req->method)
        return -1;
    memcpy(req->method, method, mlen + 1);

    char* q = strchr(path, '?');
    if (q) {
        *q = 0;
        size_t plen = strlen(path);
        req->path = hw_arena_alloc(arena, plen + 1);
        if (req->path)
            memcpy(req->path, path, plen + 1);

        size_t qlen = strlen(q + 1);
        req->query = hw_arena_alloc(arena, qlen + 1);
        if (req->query)
            memcpy(req->query, q + 1, qlen + 1);
    } else {
        size_t plen = strlen(path);
        req->path = hw_arena_alloc(arena, plen + 1);
        if (req->path)
            memcpy(req->path, path, plen + 1);
        req->query = NULL;
    }
    return 0;
}

static int parse_headers(const char* p, const char* end, http_request* req, HwArena* arena) {
    while (p < end) {
        if (strncmp(p, "\r\n", 2) == 0) {
            break;
        }

        const char* eol = strstr(p, "\r\n");
        if (!eol)
            break;

        char hline[1024];
        size_t hlen = eol - p;
        if (hlen >= sizeof(hline))
            hlen = sizeof(hline) - 1;
        memcpy(hline, p, hlen);
        hline[hlen] = 0;

        char* colon = strchr(hline, ':');
        if (colon) {
            *colon = 0;
            char* val = colon + 1;
            while (*val == ' ')
                val++;

            if (req->header_count < HTTP_MAX_HEADERS) {
                size_t nlen = strlen(hline);
                req->headers[req->header_count].name = hw_arena_alloc(arena, nlen + 1);
                if (req->headers[req->header_count].name)
                    memcpy(req->headers[req->header_count].name, hline, nlen + 1);

                size_t vlen = strlen(val);
                req->headers[req->header_count].value = hw_arena_alloc(arena, vlen + 1);
                if (req->headers[req->header_count].value)
                    memcpy(req->headers[req->header_count].value, val, vlen + 1);

                req->header_count++;
            }
        }
        p = eol + 2;
    }
    return 0;
}

int http_parse_request(const char* raw, size_t len, http_request* req, HwArena* arena) {
    if (!raw || !req || !arena)
        return -1;
    http_request_init(req);

    const char* end_line = strstr(raw, "\r\n");
    if (!end_line)
        return -1;

    char line[1024];
    size_t line_len = end_line - raw;
    if (line_len >= sizeof(line))
        line_len = sizeof(line) - 1;
    memcpy(line, raw, line_len);
    line[line_len] = 0;

    if (parse_request_line(line, req, arena) < 0)
        return -1;

    // Headers start after first line
    const char* p = end_line + 2;
    if (parse_headers(p, raw + len, req, arena) < 0)
        return -1;

    return 0;
}

int http_serialize_response_buf(const http_response* resp, char* buf, size_t cap, size_t* len_out) {
    if (!resp || !buf || !len_out || cap == 0)
        return -1;

    size_t off = 0;
    size_t remaining = cap;

    // Helper macro to append safely without logic duplication
#define APPEND_FMT(fmt, ...)                                        \
    do {                                                            \
        int n = snprintf(buf + off, remaining, fmt, ##__VA_ARGS__); \
        if (n < 0 || (size_t) n >= remaining)                       \
            return -1;                                              \
        off += (size_t) n;                                          \
        remaining -= (size_t) n;                                    \
    } while (0)

    APPEND_FMT("HTTP/1.1 %d OK\r\n", resp->status_code);

    for (size_t i = 0; i < resp->header_count; i++) {
        APPEND_FMT("%s: %s\r\n", resp->headers[i].name, resp->headers[i].value);
    }

    APPEND_FMT("Content-Length: %zu\r\n", resp->body_len);
    APPEND_FMT("Connection: close\r\n");
    APPEND_FMT("\r\n");

    if (resp->body && resp->body_len > 0) {
        if (resp->body_len > remaining)
            return -1;
        memcpy(buf + off, resp->body, resp->body_len);
        off += resp->body_len;
    }

    *len_out = off;
    return 0;
#undef APPEND_FMT
}

int http_serialize_response(const http_response* resp, char** out, size_t* out_len) {
    if (!resp || !out)
        return -1;

    size_t cap = 16384;
    char* buf = mem_alloc(cap);
    if (!buf)
        return -1;

    if (http_serialize_response_buf(resp, buf, cap, out_len) < 0) {
        mem_free(buf);
        return -1;
    }

    *out = buf;
    return 0;
}

void http_response_set_json(http_response* resp, const char* json) {
    if (!resp || !json)
        return;
    size_t len = strlen(json);
    resp->body = mem_alloc(len + 1);
    // ...
    if (resp->body) {
        memcpy(resp->body, json, len + 1);
        resp->body_len = len;

        // Add header
        if (resp->header_count < HTTP_MAX_HEADERS) {
            snprintf(resp->headers[resp->header_count].name, sizeof(resp->headers[0].name),
                     "Content-Type");
            snprintf(resp->headers[resp->header_count].value, sizeof(resp->headers[0].value),
                     "application/json");
            resp->header_count++;
        }
    }
}

void http_response_set_status(http_response* resp, int code) {
    if (resp)
        resp->status_code = code;
}
