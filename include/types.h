#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define SECS_PER_HOUR 3600
#define HOURS_PER_DAY 24

// --- Macros (moved from prototype) ---
#define ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// --- Integer Constants (per coding standards: enum for integers) ---
enum {
    MAX_BUFFER_SIZE = 4096,
    MAX_LOG_MSG = 1024,
    JSON_RESP_SIZE = 512,
    LARGE_BUF_SIZE = 2048,
    EVENT_BUF_SIZE = 2200,
    SMALL_BUF_SIZE = 64,
    SECS_PER_HOUR = 3600,
    HISTORY_HOURS = 72,
    DEFAULT_HORIZON = 48,
    DEFAULT_LIMIT = 50,
    MAX_LIMIT = 500,
    BASE_10 = 10,
    TIMEOUT_SEC = 15,
    URL_BUF_SIZE = 256,
    HOURS_PER_DAY = 24,
    MAX_BACKFILL_EXISTING_ROWS = 50,
    BACKFILL_HOURS = 72,
    TM_YEAR_BASE = 1900,
    EXPECTED_SSCANF_ITEMS = 6,
    CLOUD_COVER_MOD = 9,
    BACKLOG_SIZE = 5,
    SOLAR_DECL_OFFSET = 284,
    MORNING_PEAK_HOUR = 8,
    EVENING_PEAK_HOUR = 20,
    SQL_BUF_SIZE = 512,
    TIME_STR_LEN = 20,
    PIPELINE_TIME_BUF_SIZE = 32,
    PORT_MAX = 65535,
    PRICE_FETCH_HOUR = 13,
    DEFAULT_PORT = 8080,
    DEFAULT_REFRESH_SEC = 900
};

// --- Float/Double Constants (per coding standards: static const for typed values) ---
static const double PEAK_HOUR = 14.0;
static const double HOUR_SCALE = 12.0;
static const double TEMP_BASE = 3.0;
static const double TEMP_AMP = 3.0;
static const float BATTERY_EPSILON = 0.1f;
static const double W_PER_KW = 1000.0;
static const float PV_MAX_KW = 5.0f;
static const float PRICE_EPSILON = 0.0001f;
static const float BATTERY_INITIAL_PCT = 0.5f;
static const float SELL_PRICE_RATIO = 0.8f;
static const double LATITUDE = 59.33;
static const double DAYS_PER_YEAR = 365.0;
static const double DEG_180 = 180.0;
static const double DEG_PER_HOUR = 15.0;
static const double ATM_TRANS = 0.8;
static const double CLOUD_ATTEN = 0.75;
static const double MAX_OCTAS = 8.0;
static const double SOLAR_DECL_AMP = 23.45;
static const double MIN_PER_HOUR = 60.0;
static const double SOLAR_NOON_OFF = 11.0;
static const float DEMAND_BASE = 0.5f;
static const float DEMAND_MORNING_PEAK = 2.5f;
static const float DEMAND_EVENING_PEAK = 3.5f;
static const double PEAK_WIDTH = 8.0;
static const double SIM_TEMP_WINTER = -2.0;
static const double SIM_IRRAD_MOON = 50.0;
static const double SIM_PRICE_FIXED = 1.50;
static const float SIM_PRICE_FIXED_F = 1.50f;
static const float DEFAULT_BATTERY_KWH = 13.5F;
static const float DEFAULT_CHARGE_RATE = 5.0F;
static const float DEFAULT_DISCHARGE_RATE = 5.0F;
static const float DEFAULT_EFFICIENCY = 0.90F;
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
