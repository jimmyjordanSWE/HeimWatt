

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "db.h"
#include "pipeline.h"
#include "server.h"
#include "utils.h"

int main(int argc, char* argv[]) {
    int exit_code = EXIT_SUCCESS;

    // 0. Global Lib Init
    curl_global_init(CURL_GLOBAL_ALL);

    // 1. Setup Signal Handling
    setup_signals();

    // 2. Initialize Logging
    log_msg(LOG_LEVEL_INFO, "Starting LEOP Server Prototype...");

    // 3. Create Server Context (opaque)
    server_ctx* server = server_create();
    if (!server) {
        log_msg(LOG_LEVEL_ERROR, "Failed to allocate server context.");
        curl_global_cleanup();
        return EXIT_FAILURE;
    }

    // 4. Load Config
    config* cfg = server_get_config_mutable(server);
    config_init_defaults(cfg);
    if (config_parse_args(cfg, argc, argv) != 0) {
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    // 5. Init Server Resources
    log_msg(LOG_LEVEL_INFO, "Initializing server on port %d...", cfg->port);
    if (server_init(server) != 0) {
        log_msg(LOG_LEVEL_ERROR, "Failed to initialize server.");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    // 6. Init Database
    if (db_init_tables(server_get_db(server)) != 0) {
        log_msg(LOG_LEVEL_ERROR, "Failed to create DB tables.");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    // 7. Perform Initial Backfill (Blocking, before pipeline starts)
    pipeline_backfill(server);

    // 8. Start Pipeline Thread
    server_set_running(server, true);
    pthread_t* pipeline_thread = server_get_pipeline_thread(server);
    if (pthread_create(pipeline_thread, NULL, pipeline_thread_func, server) != 0) {
        log_msg(LOG_LEVEL_ERROR, "Failed to start pipeline thread.");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    // 9. Run Main Loop
    server_run(server);

    // 10. Shutdown
    log_msg(LOG_LEVEL_INFO, "Shutting down...");
    server_set_running(server, false);
    // Wake up pipeline thread if it's sleeping
    // In a real system we would use a condition variable or signal, but
    // since the pipeline loop checks `running` and sleeps for 1 sec,
    // it will exit shortly. We just join.
    pthread_join(*pipeline_thread, NULL);

cleanup:
    server_destroy(&server);
    curl_global_cleanup();

    if (exit_code == EXIT_SUCCESS) {
        log_msg(LOG_LEVEL_INFO, "Goodbye.");
    }
    return exit_code;
}
