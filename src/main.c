// #define _GNU_SOURCE (Defined in Makefile)
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "server.h"

static volatile sig_atomic_t g_shutdown_requested = 0;

static void signal_handler(int sig)
{
    (void) sig;
    g_shutdown_requested = 1;
}

int main(int argc, char **argv)
{
    // Set default log level (will be overridden by init if verbose)
    log_set_level(LOG_INFO);

    log_info("[MAIN] HeimWatt Server starting...");
    log_info("[MAIN] Version: 0.1.0 (Alpha)");

    char data_path[256];
    snprintf(data_path, sizeof(data_path), "data/default");

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--location") == 0 && i + 1 < argc)
        {
            snprintf(data_path, sizeof(data_path), "data/%s", argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
        {
            log_set_level(LOG_DEBUG);
            log_debug("[MAIN] Verbose mode enabled");
        }
    }

    log_info("[MAIN] Storage path: %s", data_path);

    // Setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;  // DISABLE SA_RESTART
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction SIGINT");
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1)
    {
        perror("sigaction SIGTERM");
    }

    signal(SIGPIPE, SIG_IGN);

    // Initialize Core
    heimwatt_ctx *ctx = heimwatt_create();
    if (!ctx)
    {
        log_fatal("[MAIN] Failed to create context");
        return 1;
    }

    if (heimwatt_init(ctx, data_path) < 0)
    {
        log_fatal("[MAIN] Initialization failed");
        heimwatt_destroy(&ctx);
        return 1;
    }

    // Run (Blocking)
    heimwatt_run_with_shutdown_flag(ctx, &g_shutdown_requested);

    log_info("[MAIN] Exit");
    heimwatt_destroy(&ctx);
    return 0;
}
