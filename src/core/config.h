/**
 * @file config.h
 * @brief Configuration parsing for HeimWatt core
 *
 * Parses heimwatt.json configuration file.
 */

#ifndef HEIMWATT_CORE_CONFIG_H
#define HEIMWATT_CORE_CONFIG_H

/**
 * Core configuration structure.
 */
typedef struct {
    char* database_path;     /**< Path to SQLite database */
    char* plugins_dir;       /**< Directory containing plugins */
    char* ipc_socket_path;   /**< Path to IPC Unix socket */
    int http_port;           /**< HTTP server port */
    int plugin_timeout_ms;   /**< Plugin response timeout */
    int plugin_max_restarts; /**< Max restart attempts for failed plugins */
} heimwatt_config;

/**
 * Load configuration from JSON file.
 *
 * @param cfg  Configuration structure to populate
 * @param path Path to configuration file
 * @return 0 on success, -1 on error
 */
int config_load(heimwatt_config* cfg, const char* path);

/**
 * Finalize and free configuration resources.
 *
 * @param cfg Pointer to configuration pointer (set to NULL on return)
 */
void config_fini(heimwatt_config** cfg);

/**
 * Initialize configuration with default values.
 *
 * @param cfg Configuration structure
 */
void config_init_defaults(heimwatt_config* cfg);

#endif /* HEIMWATT_CORE_CONFIG_H */
