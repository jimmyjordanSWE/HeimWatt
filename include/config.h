#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

/**
 * @brief Initialize configuration with default values.
 * @param cfg Pointer to config structure to initialize.
 */
void config_init_defaults(config* cfg);

/**
 * @brief Parse command line arguments to override defaults.
 * @param cfg Pointer to config structure.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, -1 on failure (and prints usage).
 */
int config_parse_args(config* cfg, int argc, char* argv[]);

#endif  // CONFIG_H
