#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sdk_internal.h"

int sdk_ipc_send(plugin_ctx* ctx, const char* json_msg) {
    if (!ctx) return -1;

    // Lazy connect
    if (ctx->ipc_fd < 0) {
        if (sdk_ipc_connect(ctx) < 0) return -1;
    }

    if (ctx->ipc_fd < 0) return -1;

    // Simple protocol: Newline delimited JSON

    size_t len = strlen(json_msg);
    ssize_t sent = write(ctx->ipc_fd, json_msg, len);
    if (sent < 0) return -1;

    char newline = '\n';
    write(ctx->ipc_fd, &newline, 1);

    return 0;
}

int sdk_ipc_recv(plugin_ctx* ctx, char* buf, size_t len) {
    if (!ctx || ctx->ipc_fd < 0) return -1;

    // Blocking read for now (simple request-response)
    // In sdk_run, we poll. But for synchronous calls like get_config, we might block?
    // Mixed async/sync on same socket is hard.
    // Ideally get_config is done during init (sync) or via async ID matching.

    // For MVP, simple blocking read of one line.
    size_t received = 0;
    while (received < len - 1) {
        char c;
        ssize_t n = read(ctx->ipc_fd, &c, 1);
        if (n <= 0) return -1;

        if (c == '\n') break;
        buf[received++] = c;
    }
    buf[received] = 0;
    return 0;
}
