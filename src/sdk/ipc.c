#include "sdk_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int sdk_ipc_send(plugin_ctx* ctx, const char* json_msg) {
    if (!ctx)
        return -EINVAL;

    // Lazy connect
    if (ctx->ipc_fd < 0) {
        if (sdk_ipc_connect(ctx) < 0)
            return -ENOTCONN;
    }

    if (ctx->ipc_fd < 0)
        return -ENOTCONN;

    // Simple protocol: Newline delimited JSON
    size_t len = strlen(json_msg);
    ssize_t sent = write(ctx->ipc_fd, json_msg, len);
    if (sent < 0)
        return -errno;

    char newline = '\n';
    ssize_t ret = write(ctx->ipc_fd, &newline, 1);
    (void) ret;  // Ignore partial write for newline

    return 0;
}

// Buffered receive - reads efficiently in bulk, extracts newline-delimited messages
int sdk_ipc_recv(plugin_ctx* ctx, char* buf, size_t len) {
    if (!ctx || ctx->ipc_fd < 0 || !buf || len == 0)
        return -EINVAL;

    while (1) {
        // 1. Check if we already have a complete line in buffer
        char* newline = memchr(ctx->ipc_buf + ctx->ipc_rpos, '\n', ctx->ipc_wpos - ctx->ipc_rpos);
        if (newline) {
            size_t msg_len = newline - (ctx->ipc_buf + ctx->ipc_rpos);

            // Check if output buffer is large enough
            if (msg_len >= len) {
                msg_len = len - 1;  // Truncate
            }

            memcpy(buf, ctx->ipc_buf + ctx->ipc_rpos, msg_len);
            buf[msg_len] = '\0';

            // Advance read position past newline
            ctx->ipc_rpos += msg_len + 1;
            return 0;
        }

        // 2. No complete line - need more data
        // If buffer is full and no newline, we have a problem
        if (ctx->ipc_wpos == SDK_IPC_BUFFER_SIZE) {
            if (ctx->ipc_rpos == 0) {
                // Message too big - reset buffer
                ctx->ipc_wpos = 0;
                return -EMSGSIZE;
            }

            // Compact buffer: move active data to front
            size_t active = ctx->ipc_wpos - ctx->ipc_rpos;
            memmove(ctx->ipc_buf, ctx->ipc_buf + ctx->ipc_rpos, active);
            ctx->ipc_rpos = 0;
            ctx->ipc_wpos = active;
        }

        // 3. Read from socket in bulk
        ssize_t n =
            read(ctx->ipc_fd, ctx->ipc_buf + ctx->ipc_wpos, SDK_IPC_BUFFER_SIZE - ctx->ipc_wpos);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (n == 0)
            return -ECONNRESET;

        ctx->ipc_wpos += n;
    }
}

bool sdk_ipc_check_data(plugin_ctx* ctx, int64_t ts) {
    if (!ctx)
        return false;

    char buf[256];
    snprintf(buf, sizeof(buf), "{\"cmd\":\"check_data\", \"ts\":%ld}", ts);

    if (sdk_ipc_send(ctx, buf) < 0)
        return false;

    char resp[256];
    if (sdk_ipc_recv(ctx, resp, sizeof(resp)) < 0)
        return false;

    // Expect {"exists": true/false}
    if (strstr(resp, "\"exists\":true") || strstr(resp, "\"exists\": true")) {
        return true;
    }

    return false;
}
