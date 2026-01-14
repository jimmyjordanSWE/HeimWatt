#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_init_defaults(config* cfg) {
    if (!cfg) {
        return;
    }

    cfg->port = DEFAULT_PORT;
    cfg->data_refresh_interval_sec =
        DEFAULT_REFRESH_SEC;       // 15 min (SMHI updates ~1h, Elpriset daily)
    cfg->simulation_mode = false;  // Default: REAL data

    // Default: Tesla Powerwall 2 specs
    cfg->battery_capacity_kwh = DEFAULT_BATTERY_KWH;
    cfg->battery_charge_rate_kw = DEFAULT_CHARGE_RATE;
    cfg->battery_discharge_rate_kw = DEFAULT_DISCHARGE_RATE;
    cfg->efficiency = DEFAULT_EFFICIENCY;
}

int config_parse_args(config* cfg, int argc, char* argv[]) {
    if (!cfg) {
        return -1;
    }

    // Basic argument parsing loop
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            long val = strtol(argv[++i], NULL, BASE_10);
            if (val > 0 && val <= PORT_MAX) {
                cfg->port = (int)val;
            } else {
                (void)fprintf(stderr, "Invalid port: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "--mock") == 0) {
            cfg->simulation_mode = true;
            printf(">> MOCK/SIMULATION MODE ENABLED\n");  // Use printf for CLI output, log_msg for
                                                          // running logs
        } else {
            (void)fprintf(stderr, "Usage: %s [--port <port>] [--mock]\n", argv[0]);
            return -1;
        }
    }
    return 0;
}
