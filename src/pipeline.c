

#include "pipeline.h"

#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "db.h"
#include "lps.h"
#include "server.h"
#include "utils.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Internal Prototypes ---
static int pipeline_fetch_weather(weather_data* out);
static int pipeline_fetch_prices(spot_price* out, float* forecast_arr);
static void pipeline_compute_plan(const config* cfg, lps_solver* solver,
                                  const weather_data* weather, const spot_price* price,
                                  const float* forecast_prices, energy_plan* out);
static double estimate_irradiance(time_t timestamp, double cloud_cover_octas);
static int parse_weather_smhi(const char* json, weather_data* out);
static int parse_prices_se(const char* json, spot_price* out, float hourly_prices[HOURS_PER_DAY]);
static void backfill_prices(server_ctx* ctx);
static void backfill_weather(server_ctx* ctx);

// --- Curl Helpers ---
typedef struct {
    char* memory;
    size_t size;
} curl_buffer;

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    curl_buffer* mem = (curl_buffer*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        // If realloc fails, original memory is still valid, but we can't append.
        // In this curl callback, returning 0 signals error to curl.
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;  // Null-terminate

    return realsize;
}

static char* fetch_url(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    curl_buffer chunk = {0};
    chunk.memory = malloc(1);  // will be grown as needed by realloc
    chunk.size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "LEOP-Prototype/0.1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);  // 10s timeout

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        log_msg(LOG_LEVEL_ERROR, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        free(chunk.memory);
        chunk.memory = NULL;
    }

    curl_easy_cleanup(curl);
    return chunk.memory;
}

// --- Implementation ---

void* pipeline_thread_func(void* arg) {
    server_ctx* ctx = (server_ctx*)arg;
    log_msg(LOG_LEVEL_INFO, "Pipeline thread started.");

    // Create LPS solver instance once
    lps_solver* solver = lps_solver_create();
    if (!solver) {
        log_msg(LOG_LEVEL_ERROR, "Failed to create LPS solver!");
        return NULL;
    }

    while (server_is_running(ctx)) {
        // 1. Calculate Next Target Times based on API update frequencies
        time_t now = time(NULL);
        struct tm tm_curr;
        localtime_r(&now, &tm_curr);  // Thread-safe

        // Weather (SMHI): Updates hourly - align to next full hour
        struct tm next_weather_tm = tm_curr;
        next_weather_tm.tm_sec = 0;
        next_weather_tm.tm_min = 0;
        next_weather_tm.tm_hour += 1;
        time_t next_weather = mktime(&next_weather_tm);

        // Price (Elpriset): Updates once daily at 13:00 with tomorrow's prices
        // If it's past 13:00, schedule for tomorrow at 13:00
        struct tm next_price_tm = tm_curr;
        next_price_tm.tm_sec = 0;
        next_price_tm.tm_min = 0;
        if (tm_curr.tm_hour >= PRICE_FETCH_HOUR) {
            // Already past 13:00 today, schedule for tomorrow
            next_price_tm.tm_mday += 1;
        }
        next_price_tm.tm_hour = PRICE_FETCH_HOUR;
        time_t next_price = mktime(&next_price_tm);

        // Update Shared State for API
        server_set_next_fetch(ctx, next_weather, next_price);

        // 2. Perform Data Fetching
        weather_data weather = {0};
        spot_price prices = {0};
        energy_plan plan = {0};
        float forecast_buf[HOURS_PER_DAY] = {0};

        const config* cfg = server_get_config(ctx);
        if (cfg && !cfg->simulation_mode) {
            log_msg(LOG_LEVEL_INFO, "Pipeline: Fetching REAL data...");

            pipeline_fetch_weather(&weather);
            // Fetch prices and populate 24h forecast buffer
            pipeline_fetch_prices(&prices, forecast_buf);

            // Persist to DB
            sqlite3* db_handle = server_get_db(ctx);
            db_insert_weather(db_handle, &weather);
            db_insert_price(db_handle, &prices);

            // Update Price & Forecast in Server
            server_update_price(ctx, &prices, forecast_buf);
            server_update_weather(ctx, &weather);

        } else {
            log_msg(LOG_LEVEL_DEBUG, "Pipeline: Running in SIMULATION MODE.");
            weather.temp_c = SIM_TEMP_WINTER;
            weather.irradiance_wm2 = SIM_IRRAD_MOON;
            prices.price_sek_kwh = SIM_PRICE_FIXED;
            // Mock forecast?
            for (int i = 0; i < HOURS_PER_DAY; i++) {
                forecast_buf[i] = SIM_PRICE_FIXED_F;
            }

            // Update Server for SIM mode too so UI sees it
            server_update_price(ctx, &prices, forecast_buf);
            server_update_weather(ctx, &weather);
        }

        // Compute plan (using latest data + LPS)
        log_msg(LOG_LEVEL_DEBUG, "Pipeline: Computing plan with LPS...");

        // Use the local buffers we just populated
        const config* cfg_ptr = server_get_config(ctx);
        if (cfg_ptr) {
            pipeline_compute_plan(cfg_ptr, solver, &weather, &prices, forecast_buf, &plan);
        }

        // Update Shared State (Thread-Safe)
        server_update_plan(ctx, &plan);

        // 3. Wait until next weather run (weather is more frequent)
        char next_w_str[TIME_STR_LEN];
        get_time_str(next_weather, next_w_str, sizeof(next_w_str));
        log_msg(LOG_LEVEL_INFO, "Pipeline: Next weather fetch at %s", next_w_str);

        while (time(NULL) < next_weather) {
            if (!server_is_running(ctx)) {
                break;
            }
            sleep(1);
        }
    }

    lps_solver_destroy(&solver);
    log_msg(LOG_LEVEL_INFO, "Pipeline thread stopped.");
    return NULL;
}

static int pipeline_fetch_weather(weather_data* out) {
    // Stockholm Central Coordinates: 59.3293 N, 18.0686 E
    const char* url =
        "https://opendata-download-metfcst.smhi.se/api/category/pmp3g/version/2/"
        "geotype/point/lon/18.0686/lat/59.3293/data.json";

    log_msg(LOG_LEVEL_DEBUG, "Fetching Weather from SMHI (Stockholm)...");
    char* json = fetch_url(url);

    if (!json) {
        log_msg(LOG_LEVEL_ERROR, "Failed to fetch weather data");
        return -1;
    }

    if (parse_weather_smhi(json, out) == 0) {
        log_msg(LOG_LEVEL_INFO, "Weather updated: %.1fC", out->temp_c);
    } else {
        log_msg(LOG_LEVEL_ERROR, "Failed to parse weather JSON");
    }

    free(json);
    return 0;
}

static int pipeline_fetch_prices(spot_price* out, float* forecast_arr) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char url[URL_BUF_SIZE];
    // https://www.elprisetjustnu.se/api/v1/prices/2025/01-13_SE3.json
    (void)snprintf(url, sizeof(url),
                   "https://www.elprisetjustnu.se/api/v1/prices/%04d/%02d-%02d_SE3.json",
                   tm_info.tm_year + TM_YEAR_BASE, tm_info.tm_mon + 1, tm_info.tm_mday);

    log_msg(LOG_LEVEL_DEBUG, "Fetching Prices from %s...", url);

    char* json = fetch_url(url);
    if (!json) {
        log_msg(LOG_LEVEL_ERROR, "Failed to fetch price data");
        return -1;
    }

    if (parse_prices_se(json, out, forecast_arr) < 0) {
        free(json);
        return -1;
    }

    free(json);
    return 0;
}

// Mock Sine Wave for demand (Peak at 8am and 8pm)
static float mock_demand_curve(int hour_of_day) {
    // Simple double-peak curve (Morning + Evening)
    const float base = DEMAND_BASE;

    // Gaussian-like peaks
    float morning =
        DEMAND_MORNING_PEAK * (float)exp(-pow(hour_of_day - MORNING_PEAK_HOUR, 2) / PEAK_WIDTH);
    float evening =
        DEMAND_EVENING_PEAK * (float)exp(-pow(hour_of_day - EVENING_PEAK_HOUR, 2) / PEAK_WIDTH);

    return base + morning + evening;
}

static void pipeline_compute_plan(const config* cfg, lps_solver* solver,
                                  const weather_data* weather, const spot_price* price,
                                  const float* forecast_prices, energy_plan* out) {
    out->generated_at = time(NULL);

    // 1. Setup Forecasts
    const int horizon = DEFAULT_HORIZON;
    float solar[DEFAULT_HORIZON];
    float prices[DEFAULT_HORIZON];
    float demand[DEFAULT_HORIZON];

    // Get current hour
    time_t now = time(NULL);
    struct tm now_tm;
    localtime_r(&now, &now_tm);
    int start_hour = now_tm.tm_hour;

    for (int i = 0; i < horizon; i++) {
        int hour_idx = (start_hour + i) % HOURS_PER_DAY;

        // Solar: Physics-based estimation using persistence cloud cover
        time_t future_time = now + ((time_t)i * SECS_PER_HOUR);
        double estimated_ghi = estimate_irradiance(future_time, weather->cloud_cover);

        // Simulating a 5kWp PV system (approx 25-30m^2, 20% eff)
        // Output (kW) = GHI (kW/m^2) * System Size Factor
        // Factor 5.0 means 1000 W/m^2 -> 5.0 kW output
        solar[i] = (float)(estimated_ghi / W_PER_KW) * PV_MAX_KW;

        // Price: Use real forecast from array
        if (forecast_prices && forecast_prices[hour_idx] > PRICE_EPSILON) {
            prices[i] = forecast_prices[hour_idx];
        } else {
            // Fallback if missing
            prices[i] = (float)price->price_sek_kwh;
        }

        // Demand: Mock user consumption
        demand[i] = mock_demand_curve(hour_idx);
    }

    // 2. Setup Problem
    lps_problem problem = {
        .num_periods = horizon,
        .solar_forecast_kwh = solar,
        .price_sek_kwh = prices,
        .demand_kwh = demand,
        .battery_capacity_kwh = cfg->battery_capacity_kwh,
        .battery_initial_kwh = cfg->battery_capacity_kwh * BATTERY_INITIAL_PCT,  // Start at 50%
        .charge_rate_kw = cfg->battery_charge_rate_kw,
        .discharge_rate_kw = cfg->battery_discharge_rate_kw,
        .efficiency = cfg->efficiency,
        .sell_price_ratio = SELL_PRICE_RATIO  // Grid fees
    };

    // 3. Solve
    lps_solution* solution = lps_solution_create(horizon);
    if (!solution) {
        log_msg(LOG_LEVEL_ERROR, "LPS: Failed to allocate solution");
        return;
    }

    int ret = lps_solve(solver, &problem, solution);
    if (ret != 0) {
        log_msg(LOG_LEVEL_ERROR, "LPS: Solve failed code %d", ret);
    } else {
        // 4. Copy data to output struct for API
        out->start_hour = start_hour;
        out->horizon = horizon;
        out->total_cost_sek = solution->total_cost_sek;

        // Copy forecast inputs
        memcpy(out->prices, prices, sizeof(float) * horizon);
        memcpy(out->solar, solar, sizeof(float) * horizon);
        memcpy(out->demand, demand, sizeof(float) * horizon);

        // Copy solution outputs
        memcpy(out->battery, solution->battery_level_kwh, sizeof(float) * horizon);
        memcpy(out->buy, solution->buy_kwh, sizeof(float) * horizon);
        memcpy(out->sell, solution->sell_kwh, sizeof(float) * horizon);
        memcpy(out->charge, solution->charge_kwh, sizeof(float) * horizon);
        memcpy(out->discharge, solution->discharge_kwh, sizeof(float) * horizon);

        // 5. Log immediate action
        float net_grid = solution->buy_kwh[0] - solution->sell_kwh[0];
        float batt_change = solution->charge_kwh[0] - solution->discharge_kwh[0];

        log_msg(LOG_LEVEL_INFO, "LPS PLAN [Hour 0]: NetGrid: %.2f kWh, Battery: %.2f kWh", net_grid,
                batt_change);

        if (batt_change > BATTERY_EPSILON) {
            log_msg(LOG_LEVEL_INFO, "ACTION: CHARGING BATTERY");
        } else if (batt_change < -BATTERY_EPSILON) {
            log_msg(LOG_LEVEL_INFO, "ACTION: DISCHARGING BATTERY");
        } else if (net_grid < -BATTERY_EPSILON) {
            log_msg(LOG_LEVEL_INFO, "ACTION: SELLING TO GRID");
        } else {
            log_msg(LOG_LEVEL_INFO, "ACTION: IDLE / SELF-CONSUMPTION");
        }

        log_msg(LOG_LEVEL_INFO, "LPS PLAN [Total]: Cost: %.2f SEK, Profit: %.2f",
                solution->total_cost_sek, -solution->total_cost_sek);
    }

    lps_solution_destroy(&solution);
}

static double estimate_irradiance(
    time_t timestamp, double cloud_cover_octas) {  // NOLINT(bugprone-easily-swappable-parameters)
    struct tm tm_t;
    localtime_r(&timestamp, &tm_t);
    int day_of_year = tm_t.tm_yday + 1;

    // Simple solar model (Stockholm)
    // Declination approx
    double decl =
        SOLAR_DECL_AMP * sin((2 * M_PI / DAYS_PER_YEAR) * (day_of_year - SOLAR_DECL_OFFSET));
    double lat = LATITUDE;

    // Hour angle
    double hour_angle = (tm_t.tm_hour + tm_t.tm_min / MIN_PER_HOUR - SOLAR_NOON_OFF) * DEG_PER_HOUR;

    double decl_rad = decl * M_PI / DEG_180;
    double lat_rad = lat * M_PI / DEG_180;
    double ha_rad = hour_angle * M_PI / DEG_180;

    double sin_elev = sin(lat_rad) * sin(decl_rad) + cos(lat_rad) * cos(decl_rad) * cos(ha_rad);
    double elev = asin(sin_elev) * DEG_180 / M_PI;

    if (elev <= 0) {
        return 0.0;
    }

    // Clear Sky Radiation (Ryan-Stolzenbach)
    double trans = ATM_TRANS;  // Atmospheric transmission estimate
    double clearsky_ghi = W_PER_KW * sin(elev * M_PI / DEG_180) * trans;
    if (clearsky_ghi < 0) {
        clearsky_ghi = 0;
    }

    // Cloud cover factor (linear attenuation)
    // 0 octas = 1.0, 8 octas = 0.25
    double cloud_factor = 1.0 - (CLOUD_ATTEN * (cloud_cover_octas / MAX_OCTAS));

    return clearsky_ghi * cloud_factor;
}

static int parse_weather_smhi(const char* json, weather_data* out) {
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        log_msg(LOG_LEVEL_ERROR, "JSON Parse Error: %s", cJSON_GetErrorPtr());
        return -1;
    }

    // Navigate: root -> timeSeries -> [0] -> parameters
    cJSON* timeSeries = cJSON_GetObjectItem(root, "timeSeries");
    if (!cJSON_IsArray(timeSeries)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON* first_hour = cJSON_GetArrayItem(timeSeries, 0);
    if (!first_hour) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON* params = cJSON_GetObjectItem(first_hour, "parameters");
    if (!cJSON_IsArray(params)) {
        cJSON_Delete(root);
        return -1;
    }

    double cloud_cover = MAX_OCTAS;  // Default to cloudy if not found
    cJSON* param = NULL;
    cJSON_ArrayForEach(param, params) {
        cJSON* name = cJSON_GetObjectItem(param, "name");
        cJSON* values = cJSON_GetObjectItem(param, "values");

        if (cJSON_IsString(name) && cJSON_IsArray(values)) {
            cJSON* first_val = cJSON_GetArrayItem(values, 0);
            if (!first_val) {
                continue;
            }
            double val = first_val->valuedouble;

            if (strcmp(name->valuestring, "t") == 0) {
                out->temp_c = val;
            } else if (strcmp(name->valuestring, "tcc_mean") == 0) {
                cloud_cover = val;
            }
        }
    }

    // Calculate irradiance based on time of day and cloud cover
    time_t now = time(NULL);
    out->cloud_cover = cloud_cover;
    out->irradiance_wm2 = estimate_irradiance(now, cloud_cover);
    out->timestamp = now;

    cJSON_Delete(root);
    return 0;
}

static int parse_prices_se(const char* json, spot_price* out, float hourly_prices[HOURS_PER_DAY]) {
    cJSON* root = cJSON_Parse(json);
    if (!root) {
        log_msg(LOG_LEVEL_ERROR, "JSON Parse Error: %s", cJSON_GetErrorPtr());
        return -1;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    // Initialize hourly array if provided
    if (hourly_prices) {
        for (int i = 0; i < HOURS_PER_DAY; i++) {
            hourly_prices[i] = 0.0F;
        }
    }

    // We match on the "hour" substring e.g. "T17:00"
    char time_str_fragment[TIME_STR_LEN];
    (void)snprintf(time_str_fragment, sizeof(time_str_fragment), "T%02d:00", tm_now.tm_hour);

    cJSON* item = NULL;
    bool found = false;
    cJSON_ArrayForEach(item, root) {
        cJSON* time_start = cJSON_GetObjectItem(item, "time_start");
        cJSON* sek = cJSON_GetObjectItem(item, "SEK_per_kWh");

        if (cJSON_IsString(time_start) && cJSON_IsNumber(sek)) {
            // Extract hour from time_start (format: "2026-01-13T17:00:00+01:00")
            const char* tpos = strstr(time_start->valuestring, "T");
            if (tpos && hourly_prices) {
                long val = strtol(tpos + 1, NULL, BASE_10);
                int hour = (int)val;
                if (hour >= 0 && hour < HOURS_PER_DAY) {
                    hourly_prices[hour] = (float)sek->valuedouble;
                }
            }

            if (strstr(time_start->valuestring, time_str_fragment) != NULL) {
                out->price_sek_kwh = sek->valuedouble;
                out->timestamp = now;
                found = true;
            }
        }
    }

    if (!found && cJSON_GetArraySize(root) > 0) {
        cJSON* last = cJSON_GetArrayItem(root, cJSON_GetArraySize(root) - 1);
        out->price_sek_kwh = cJSON_GetObjectItem(last, "SEK_per_kWh")->valuedouble;
        log_msg(LOG_LEVEL_WARN,
                "Current hour (%s) not found in price list. Using last attached entry.",
                time_str_fragment);
    }

    cJSON_Delete(root);
    return 0;
}

// Helper to reduce cognitive complexity of backfill_prices
static void parse_and_insert_prices(server_ctx* ctx, cJSON* root) {
    if (!cJSON_IsArray(root)) {
        return;
    }

    cJSON* item = NULL;
    cJSON_ArrayForEach(item, root) {
        cJSON* ts_item = cJSON_GetObjectItem(item, "time_start");
        cJSON* val_item = cJSON_GetObjectItem(item, "SEK_per_kWh");
        if (cJSON_IsNumber(val_item) && cJSON_IsString(ts_item)) {
            /* 2023-11-23T00:00:00 */
            struct tm tm_log = {0};
            if (strptime(ts_item->valuestring, "%Y-%m-%dT%H:%M:%S", &tm_log)) {
                tm_log.tm_isdst = -1;
                time_t log_ts = mktime(&tm_log);

                spot_price price_rec = {.price_sek_kwh = val_item->valuedouble,
                                        .timestamp = log_ts};
                db_insert_price(server_get_db(ctx), &price_rec);
            }
        }
    }
}

// Backfill historical data if missing
static void backfill_prices(server_ctx* ctx) {
    // 1. Backfill Price History (approx 24-48 hours from today)
    // We will try to fetch yesterday's prices.
    time_t now = time(NULL);
    struct tm now_tm;
    localtime_r(&now, &now_tm);
    (void)now_tm;

    // Go back 1 day
    time_t yesterday = now - ((time_t)HOURS_PER_DAY * SECS_PER_HOUR);
    struct tm tm_y;
    localtime_r(&yesterday, &tm_y);

    char url[URL_BUF_SIZE];
    (void)snprintf(url, sizeof(url),
                   "https://www.elprisetjustnu.se/api/v1/prices/%04d/%02d-%02d_SE3.json",
                   tm_y.tm_year + TM_YEAR_BASE, tm_y.tm_mon + 1, tm_y.tm_mday);

    log_msg(LOG_LEVEL_INFO, "Backfilling history from %s", url);

    char* json = fetch_url(url);
    if (json) {
        cJSON* root = cJSON_Parse(json);
        if (root) {
            parse_and_insert_prices(ctx, root);
            cJSON_Delete(root);
        }
        free(json);
    } else {
        log_msg(LOG_LEVEL_WARN, "Backfill fetch failed for %s", url);
    }
}

static void backfill_weather(server_ctx* ctx) {
    // 2. Backfill Weather (Synthetic History)
    // 72 hours of simulated data
    time_t now = time(NULL);

    // Seed for rand_r (thread-safe)
    unsigned int seed = (unsigned int)now;

    for (int i = 0; i < BACKFILL_HOURS; i++) {
        time_t hist_time = now - ((time_t)i * SECS_PER_HOUR);
        hist_time -= (hist_time % SECS_PER_HOUR);  // Align to hour

        // Random cloud cover 0-8
        double cloud = (double)(rand_r(&seed) % CLOUD_COVER_MOD);

        double irrad = estimate_irradiance(hist_time, cloud);

        // Synthetic Temp: Daily sine wave 0-6 C
        struct tm tm_h;
        localtime_r(&hist_time, &tm_h);
        // Peak at 14:00 (14/24 * 2PI approx 3.66 rad)
        double hour_rad = ((double)tm_h.tm_hour - PEAK_HOUR) * (M_PI / HOUR_SCALE);
        double temp = TEMP_BASE + TEMP_AMP * cos(hour_rad);  // 0 to 6 range

        weather_data weather = {
            .timestamp = hist_time, .temp_c = temp, .irradiance_wm2 = irrad, .cloud_cover = cloud};
        db_insert_weather(server_get_db(ctx), &weather);
    }
}

void pipeline_backfill(server_ctx* ctx) {
    if (!ctx) {
        return;
    }
    sqlite3* db_handle = server_get_db(ctx);
    if (!db_handle) {
        return;
    }

    // Check if we already have data
    int count = db_count_rows(db_handle, "price_log");
    if (count >= MAX_BACKFILL_EXISTING_ROWS) {
        log_msg(LOG_LEVEL_INFO, "Database already has data (%d rows). Skipping backfill.", count);
        return;
    }

    log_msg(LOG_LEVEL_INFO, "Performing initial backfill of historical data...");
    backfill_prices(ctx);
    backfill_weather(ctx);

    log_msg(LOG_LEVEL_INFO, "Backfill complete.");
}
