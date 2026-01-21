// #define _GNU_SOURCE (Defined in Makefile)
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "server.h"

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

    /*
     * BLOCK SIGINT/SIGTERM GLOBALLY
     * We use signalfd() in the main event loop to handle these signals synchronously.
     * For signalfd to work, the signals MUST be blocked in all threads.
     * Since this is the main thread and we haven't spawned threads yet,
     * this mask will be inherited by all future threads.
     */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGPIPE);  // Ignore SIGPIPE usually, blocking is also fine if handled

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0)
    {
        perror("pthread_sigmask");
        return 1;
    }

    // Initialize Core (this creates DuckDB threads, thread pool, etc.)
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

    // Run (Blocking) - Handles signals via signalfd
    heimwatt_run(ctx);

    log_info("[MAIN] Exit");
    heimwatt_destroy(&ctx);
    return 0;
}
