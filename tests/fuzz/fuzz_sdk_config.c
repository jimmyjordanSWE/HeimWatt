/**
 * @file fuzz_sdk_config.c
 * @brief AFL++ fuzz harness for SDK variable substitution
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sdk/sdk_internal.h"

#ifdef __AFL_HAVE_MANUAL_CONTROL
__AFL_FUZZ_INIT();
#endif

// Stubs for linker dependencies (we don't use sdk_get_config, only substitution)
int sdk_ipc_send(plugin_ctx *ctx, const char *json_msg)
{
    (void) ctx;
    (void) json_msg;
    return 0;
}
int sdk_ipc_recv(plugin_ctx *ctx, char *buf, size_t len)
{
    (void) ctx;
    (void) buf;
    (void) len;
    return 0;
}

int main(void)
{
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000))
    {
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
#else
    char buf[4096];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0) return 0;
    size_t len = (size_t) n;
#endif

        // Null terminate input
        char *input = malloc(len + 1);
        if (!input)
        {
#ifdef __AFL_HAVE_MANUAL_CONTROL
            continue;
#else
        return 0;
#endif
        }
        memcpy(input, buf, len);
        input[len] = 0;

        // Use a fixed timestamp or derive from payload?
        // Fixed is fine to fuzz the parsing logic.
        time_t now = 1735689600;  // 2025-01-01

        char *output = NULL;
        int ret = sdk_substitute_config_vars(input, now, &output);

        if (ret == 0 && output)
        {
            free(output);
        }

        // Also free the duplicate if no replacement occured (implementation detail:
        // substitute_vars strdups if no match found, so output always needs free if ret==0)
        // Wait, my implementation strdups if no match? Yes.
        // So output is always allocated on success.

        free(input);

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif

    return 0;
}
