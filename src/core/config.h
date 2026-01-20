/**
 * @file config.h
 * @brief Configuration parsing for HeimWatt core
 *
 * Parses heimwatt.json configuration file.
 */

#ifndef HEIMWATT_CORE_CONFIG_H
#define HEIMWATT_CORE_CONFIG_H

#include "types.h"

typedef struct config config;

config *config_create(void);
int config_get_csv_interval(const config *cfg);
const char *config_get_loc_name(const config *cfg);
double config_get_lat(const config *cfg);
double config_get_lon(const config *cfg);
const char *config_get_area(const config *cfg);

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
