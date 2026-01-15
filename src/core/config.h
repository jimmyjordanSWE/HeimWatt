/**
 * @file config.h
 * @brief Configuration parsing for HeimWatt core
 *
 * Parses heimwatt.json configuration file.
 */

#ifndef HEIMWATT_CORE_CONFIG_H
#define HEIMWATT_CORE_CONFIG_H

#include "types.h"

/**
 * Core configuration structure.
 */
typedef struct config config;

/**
 * Load configuration from JSON file.
 *
 * @param cfg  Configuration structure to populate
 * @param path Path to configuration file
 * @return 0 on success, -1 on error
 */
int config_load(config *cfg, const char *path);

/**
 * Destroy and free configuration resources.
 *
 * @param cfg Pointer to configuration pointer (set to NULL on return)
 */
void config_destroy(config **cfg);

/**
 * Initialize configuration with default values.
 *
 * @param cfg Configuration structure
 */
void config_init_defaults(config *cfg);

#endif /* HEIMWATT_CORE_CONFIG_H */
