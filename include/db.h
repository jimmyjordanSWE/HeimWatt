#ifndef DB_H
#define DB_H

#include <time.h>

#include "types.h"

/**
 * @brief Initialize database tables.
 * Creates necessary tables if they do not exist.
 * @param db SQLite database handle.
 * @return 0 on success, -1 on failure.
 */
int db_init_tables(sqlite3* db);

/**
 * @brief Insert weather data into the log.
 * @param db SQLite database handle.
 * @param w Weather data to insert.
 * @return 0 on success, -1 on failure.
 */
int db_insert_weather(sqlite3* db, const weather_data* w);

/**
 * @brief Insert spot price data into the log.
 * @param db SQLite database handle.
 * @param p Spot price data to insert.
 * @return 0 on success, -1 on failure.
 */
int db_insert_price(sqlite3* db, const spot_price* p);

/**
 * @brief Count rows in a table.
 * @param db SQLite database handle.
 * @param table Table name.
 * @return Number of rows, or 0 on error (logs error).
 */
int db_count_rows(sqlite3* db, const char* table);

/**
 * @brief Query table data with pagination.
 * @param db SQLite database handle.
 * @param table Table name.
 * @param limit Maximum number of rows to return.
 * @param offset Row offset.
 * @return JSON string of results (caller must free), or NULL on error.
 */
char* db_query_table(sqlite3* db, const char* table, int limit, int offset);

/**
 * @brief Retrieve historical price and solar data.
 * @param db SQLite database handle.
 * @param start_ts Start timestamp.
 * @param hours Number of hours to retrieve.
 * @param out_prices Output array for prices (must be size `hours`).
 * @param out_solar Output array for solar (must be size `hours`), can be NULL.
 */
void db_get_history(sqlite3* db, time_t start_ts, int hours, float* out_prices, float* out_solar);

#endif  // DB_H
