#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include <pthread.h>

#include "config.h"
#include "db.h"
#include "types.h"

// --- Contexts ---
// Moved from types.h to enforce Opaque Pointer pattern
struct server_ctx {
    config cfg;                                 // Server configuration
    pthread_t pipeline_thread;                  // Data fetching thread
    volatile bool running;                      // Server run status
    int server_sock_fd;                         // Listening socket
    pthread_mutex_t lock;                       // State lock
    pthread_cond_t data_cond;                   // Update notification
    db_handle* db;                              // Database handle
    weather_data last_weather;                  // Most recent weather
    spot_price last_price;                      // Most recent spot price
    float last_forecast_prices[HOURS_PER_DAY];  // 24h price forecast
    energy_plan last_plan;                      // Current energy plan
    time_t next_weather_fetch;                  // Scheduled fetch time
    time_t next_price_fetch;                    // Scheduled fetch time
};

#endif  // SERVER_INTERNAL_H
