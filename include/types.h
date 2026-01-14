#ifndef TYPES_H
#define TYPES_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// #include "sqlite3.h" // Removed to hide implementation details
typedef struct sqlite3 sqlite3;

// --- Macros (moved from prototype) ---
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))
#define MAX_BUFFER_SIZE 4096
#define MAX_LOG_MSG 1024
#define JSON_RESP_SIZE 512
#define LARGE_BUF_SIZE 2048
#define EVENT_BUF_SIZE 2200
#define SMALL_BUF_SIZE 64
#define SECS_PER_HOUR 3600
#define HISTORY_HOURS 72
#define DEFAULT_HORIZON 48
#define DEFAULT_LIMIT 50
#define MAX_LIMIT 500
#define BASE_10 10
#define TIMEOUT_SEC 15
#define URL_BUF_SIZE 256
#define HOURS_PER_DAY 24
#define MAX_BACKFILL_EXISTING_ROWS 50
#define BACKFILL_HOURS 72
#define TM_YEAR_BASE 1900
#define EXPECTED_SSCANF_ITEMS 6
#define CLOUD_COVER_MOD 9
#define PEAK_HOUR 14.0
#define HOUR_SCALE 12.0
#define TEMP_BASE 3.0
#define TEMP_AMP 3.0
#define BATTERY_EPSILON 0.1f
#define BACKLOG_SIZE 5
#define W_PER_KW 1000.0
#define PV_MAX_KW 5.0f
#define PRICE_EPSILON 0.0001f
#define BATTERY_INITIAL_PCT 0.5f
#define SELL_PRICE_RATIO 0.8f
#define LATITUDE 59.33
#define DAYS_PER_YEAR 365.0
#define DEG_180 180.0
#define DEG_PER_HOUR 15.0
#define ATM_TRANS 0.8
#define CLOUD_ATTEN 0.75
#define MAX_OCTAS 8.0
#define SOLAR_DECL_AMP 23.45
#define SOLAR_DECL_OFFSET 284
#define MIN_PER_HOUR 60.0
#define SOLAR_NOON_OFF 11.0
#define DEMAND_BASE 0.5f
#define DEMAND_MORNING_PEAK 2.5f
#define DEMAND_EVENING_PEAK 3.5f
#define MORNING_PEAK_HOUR 8
#define EVENING_PEAK_HOUR 20
#define PEAK_WIDTH 8.0
#define SQL_BUF_SIZE 512
#define TIME_STR_LEN 20
#define PIPELINE_TIME_BUF_SIZE 32
#define SIM_TEMP_WINTER -2.0
#define SIM_IRRAD_MOON 50.0
#define SIM_PRICE_FIXED 1.50
#define SIM_PRICE_FIXED_F 1.50f
#define PORT_MAX 65535
#define PRICE_FETCH_HOUR 13

#define DEFAULT_PORT 8080
#define DEFAULT_REFRESH_SEC 900
#define DEFAULT_BATTERY_KWH 13.5F
#define DEFAULT_CHARGE_RATE 5.0F
#define DEFAULT_DISCHARGE_RATE 5.0F
#define DEFAULT_EFFICIENCY 0.90F
/**
 * @brief Log severity levels.
 */
typedef enum { LOG_LEVEL_DEBUG = 0, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR } log_level;

/**
 * @brief Pipeline execution status.
 */
typedef enum {
    PIPE_STATUS_IDLE,
    PIPE_STATUS_RUNNING,
    PIPE_STATUS_DONE,
    PIPE_STATUS_ERROR
} pipeline_status;

// --- Data Models ---

typedef struct {
    double temp_c;          // Temperature in Celsius
    double irradiance_wm2;  // Solar irradiance in W/m^2
    double cloud_cover;     // Cloud cover in octas (0-8)
    time_t timestamp;       // Timestamp of observation
} weather_data;

typedef struct {
    double price_sek_kwh;  // Price in SEK per kWh
    time_t timestamp;      // Timestamp of price point
} spot_price;

typedef struct {
    time_t generated_at;   // Timestamp when plan was generated
    int start_hour;        // Hour of day plan starts (0-23)
    int horizon;           // Number of periods (48)
    float prices[48];      // SEK/kWh per hour
    float solar[48];       // Solar production kWh
    float demand[48];      // Demand kWh
    float battery[48];     // Battery level kWh at start of each hour
    float buy[48];         // Grid purchase kWh
    float sell[48];        // Grid sale kWh
    float charge[48];      // Battery charge kWh
    float discharge[48];   // Battery discharge kWh
    float total_cost_sek;  // Total planned cost
} energy_plan;

typedef struct {
    int port;                         // Server listening port
    int data_refresh_interval_sec;    // Data fetch interval
    bool simulation_mode;             // If true, use mock data
    float battery_capacity_kwh;       // Battery capacity
    float battery_charge_rate_kw;     // Max charge rate
    float battery_discharge_rate_kw;  // Max discharge rate
    float efficiency;                 // Round-trip efficiency
} config;

// --- Contexts ---
// Forward declaration to avoid circular deps if needed,
// but defined here for simplicity as it was in prototype.
// --- Contexts ---
// Opaque pointer forward declaration
typedef struct server_ctx server_ctx;

#endif  // TYPES_H
