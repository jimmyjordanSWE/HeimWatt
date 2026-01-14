#ifndef DB_H
#define DB_H

#include <time.h>

#include "types.h"

/**
 * @brief Opaque database handle.
 */
typedef struct db_handle db_handle;

/**
 * @brief Open database (creates if not exists).
 *
 * @param db   Output pointer for handle
 * @param path Path to database file
 * @return 0 on success, negative errno on failure.
 */
int db_open(db_handle** db, const char* path);

/**
 * @brief Close database.
 *
 * @param db Pointer to handle (set to NULL on return)
 */
void db_close(db_handle** db);

/**
 * @brief Initialize database tables.
 * Creates necessary tables if they do not exist.
 * @param db Database handle.
 * @return 0 on success, negative errno on failure.
 */
int db_init_tables(db_handle* db);

/**
 * @brief Insert weather data into the log.
 * @param db Database handle.
 * @param w Weather data to insert.
 * @return 0 on success, negative errno on failure.
 */
int db_insert_weather(db_handle* db, const weather_data* w);

/**
 * @brief Insert spot price data into the log.
 * @param db Database handle.
 * @param p Spot price data to insert.
 * @return 0 on success, negative errno on failure.
 */
int db_insert_price(db_handle* db, const spot_price* p);

/**
 * @brief Count rows in a table.
 * @param db Database handle.
 * @param table Table name.
 * @return Number of rows, or 0 on error (logs error).
 */
int db_count_rows(db_handle* db, const char* table);

/**
 * @brief Query table data with pagination.
 * @param db Database handle.
 * @param table Table name.
 * @param limit Maximum number of rows to return.
 * @param offset Row offset.
 * @return JSON string of results (caller must free), or NULL on error.
 */
char* db_query_table(db_handle* db, const char* table, int limit, int offset);

/**
 * @brief Retrieve historical price and solar data.
 * @param db Database handle.
 * @param start_ts Start timestamp.
 * @param hours Number of hours to retrieve.
 * @param out_prices Output array for prices (must be size `hours`).
 * @param out_solar Output array for solar (must be size `hours`), can be NULL.
 */
void db_get_history(db_handle* db, time_t start_ts, int hours, float* out_prices, float* out_solar);

#endif /* DB_H */
