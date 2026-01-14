

#include "db.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "utils.h"

int db_init_tables(sqlite3* db_handle) {
    char* err = NULL;
    const char* sql =
        // Actual observations
        "CREATE TABLE IF NOT EXISTS weather_log ("
        "  timestamp INTEGER PRIMARY KEY,"
        "  temp_c REAL,"
        "  irradiance_wm2 REAL"
        ");"
        "CREATE TABLE IF NOT EXISTS price_log ("
        "  timestamp INTEGER PRIMARY KEY,"
        "  price_sek REAL"
        ");"
        // Forecasts (for charts and accuracy tracking)
        "CREATE TABLE IF NOT EXISTS price_forecast ("
        "  fetched_at INTEGER,"
        "  target_hour INTEGER,"
        "  price_sek REAL,"
        "  PRIMARY KEY (fetched_at, target_hour)"
        ");"
        "CREATE TABLE IF NOT EXISTS weather_forecast ("
        "  fetched_at INTEGER,"
        "  target_hour INTEGER,"
        "  predicted_temp REAL,"
        "  predicted_irradiance REAL,"
        "  PRIMARY KEY (fetched_at, target_hour)"
        ");";

    if (sqlite3_exec(db_handle, sql, NULL, NULL, &err) != SQLITE_OK) {
        log_msg(LOG_LEVEL_ERROR, "DB Init Error: %s", err);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

int db_insert_weather(sqlite3* db_handle, const weather_data* weather) {
    sqlite3_stmt* stmt;
    const char* sql =
        "INSERT OR REPLACE INTO weather_log (timestamp, temp_c, "
        "irradiance_wm2) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)weather->timestamp);
    sqlite3_bind_double(stmt, 2, weather->temp_c);
    sqlite3_bind_double(stmt, 3, weather->irradiance_wm2);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_msg(LOG_LEVEL_ERROR, "DB Insert Weather Failed");
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_insert_price(sqlite3* db_handle, const spot_price* price) {
    sqlite3_stmt* stmt;
    const char* sql = "INSERT OR REPLACE INTO price_log (timestamp, price_sek) VALUES (?, ?);";
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)price->timestamp);
    sqlite3_bind_double(stmt, 2, price->price_sek_kwh);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_msg(LOG_LEVEL_ERROR, "DB Insert Price Failed");
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_count_rows(sqlite3* db_handle, const char* table) {
    char sql[SQL_BUF_SIZE];
    (void)snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", table);

    sqlite3_stmt* stmt;
    int count = 0;

    // Prepare
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        // Step
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    } else {
        log_msg(LOG_LEVEL_ERROR, "DB Count Error: %s", sqlite3_errmsg(db_handle));
    }
    sqlite3_finalize(stmt);
    return count;
}

char* db_query_table(sqlite3* db_handle, const char* table, int limit, int offset) {
    // Whitelist tables for security
    if (strcmp(table, "weather_log") != 0 && strcmp(table, "price_log") != 0) {
        return NULL;
    }

    char sql[SQL_BUF_SIZE];
    (void)snprintf(sql, sizeof(sql), "SELECT * FROM %s ORDER BY timestamp DESC LIMIT %d OFFSET %d;",
                   table, limit, offset);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        log_msg(LOG_LEVEL_ERROR, "DB Query Error: %s", sqlite3_errmsg(db_handle));
        return NULL;
    }

    // Get total count for pagination info
    int total = db_count_rows(db_handle, table);

    // Build JSON using cJSON
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "table", table);
    cJSON_AddNumberToObject(root, "total", total);
    cJSON_AddNumberToObject(root, "limit", limit);
    cJSON_AddNumberToObject(root, "offset", offset);

    cJSON* rows = cJSON_CreateArray();

    int col_count = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON* row = cJSON_CreateObject();
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite3_column_name(stmt, i);
            int col_type = sqlite3_column_type(stmt, i);

            switch (col_type) {
                case SQLITE_INTEGER:
                    cJSON_AddNumberToObject(row, col_name, (double)sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    cJSON_AddNumberToObject(row, col_name, sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT:
                    cJSON_AddStringToObject(row, col_name,
                                            (const char*)sqlite3_column_text(stmt, i));
                    break;
                case SQLITE_NULL:
                    cJSON_AddNullToObject(row, col_name);
                    break;
                default:
                    cJSON_AddStringToObject(row, col_name, "?");
                    break;
            }
        }
        cJSON_AddItemToArray(rows, row);
    }

    cJSON_AddItemToObject(root, "rows", rows);
    sqlite3_finalize(stmt);

    char* json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void db_get_history(sqlite3* db_handle, time_t start_ts, int hours, float* out_prices,
                    float* out_solar) {
    if (!db_handle) {
        return;
    }

    for (int i = 0; i < hours; i++) {
        out_prices[i] = 0.0F;
        if (out_solar) {
            out_solar[i] = 0.0F;
        }
    }

    // SQLite query to get raw data
    // We'll perform a query and fill the arrays
    time_t end_ts = (time_t)(start_ts + ((time_t)hours * SECS_PER_HOUR) + SECS_PER_HOUR);

    char sql[SQL_BUF_SIZE];
    (void)snprintf(sql, sizeof(sql),
                   "SELECT timestamp, price_sek FROM price_log WHERE timestamp >= %ld AND "
                   "timestamp < %ld ORDER BY timestamp ASC;",
                   start_ts, end_ts);

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            time_t timestamp = (time_t)sqlite3_column_int64(stmt, 0);
            // Calculate index (0 to 23/47/etc)
            int idx = (int)((timestamp - start_ts) / SECS_PER_HOUR);

            if (idx >= 0 && idx < hours) {
                out_prices[idx] = (float)sqlite3_column_double(stmt, 1);
            }
        }
    }
    sqlite3_finalize(stmt);

    // Solar
    const char* sql_w =
        "SELECT timestamp, irradiance_wm2 FROM weather_log WHERE timestamp >= ? AND timestamp < ? "
        "ORDER BY timestamp ASC";
    if (sqlite3_prepare_v2(db_handle, sql_w, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, (sqlite3_int64)start_ts);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)end_ts);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            time_t timestamp = (time_t)sqlite3_column_int64(stmt, 0);
            // Calculate index (0 to 23/47/etc)
            int idx = (int)((timestamp - start_ts) / SECS_PER_HOUR);

            if (idx >= 0 && idx < hours) {
                double val = sqlite3_column_double(stmt, 1);
                // Convert W/m2 to kW output (using same factor 5.0 as forecast)
                if (out_solar) {
                    out_solar[idx] = (float)((val / W_PER_KW) * PV_MAX_KW);
                }
            }
        }
        sqlite3_finalize(stmt);
    }
}
