/*
 * @file config.h
 * @brief Configuration parsing for HeimWatt core
 *
 * Parses heimwatt.json configuration file.
 */

#ifndef HEIMWATT_CORE_CONFIG_H
#define HEIMWATT_CORE_CONFIG_H

#include "types.h"

#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * Storage Backend Configuration
 * ============================================================================ */

#define CONFIG_MAX_BACKENDS 4

typedef struct storage_backend_config {
    char type[32];  /*< Backend type: "csv", "duckdb" */
    char path[256]; /*< Backend-specific path */
    bool primary;   /*< If true, used for reads */
} storage_backend_config;

typedef struct config config;

config* config_create(void);
int config_get_csv_interval(const config* cfg);
const char* config_get_loc_name(const config* cfg);
double config_get_lat(const config* cfg);
double config_get_lon(const config* cfg);
const char* config_get_area(const config* cfg);

/* Storage backend accessors */
size_t config_get_backend_count(const config* cfg);
const storage_backend_config* config_get_backend(const config* cfg, size_t idx);
int config_get_disk_write_interval(const config* cfg);

int config_add_backend(config* cfg, const char* type, const char* path, bool primary);

/*
 * Load configuration from JSON file.
 *
 * @param cfg  Configuration structure to populate
 * @param path Path to configuration file
 * @return 0 on success, -1 on error
 */
int config_load(config* cfg, const char* path);

/*
 * Destroy and free configuration resources.
 *
 * @param cfg Pointer to configuration pointer (set to NULL on return)
 */
void config_destroy(config** cfg);

/*
 * Initialize configuration with default values.
 *
 * @param cfg Configuration structure
 */
void config_init_defaults(config* cfg);

#endif /* HEIMWATT_CORE_CONFIG_H */
