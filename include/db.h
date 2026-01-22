#ifndef HEIMWATT_DB_H
#define HEIMWATT_DB_H

/*
 * @file db.h
 * @brief Database Interface
 *
 * Backend-agnostic database API. The actual implementation (SQLite, PostgreSQL, etc.)
 * is selected at compile time. This header defines the contract that all backends
 * must implement.
 *
 * ## Architecture
 *
 * ```
 * ┌─────────────────────────────────────────────────────┐
 * │                 Public Interface                    │
 * │                   include/db.h                      │
 * │   db_open(), db_close(), db_insert_tier1(), etc.    │
 * └─────────────────────────────────────────────────────┘
 *                          │
 *                          ▼
 * ┌─────────────────────────────────────────────────────┐
 * │              Backend Implementation                 │
 * │                 src/db/sqlite.c                     │
 * │       (or future: postgres.c, duckdb.c, etc.)       │
 * └─────────────────────────────────────────────────────┘
 * ```
 *
 * ## Thread Safety
 *
 * Database handles are NOT thread-safe. Each thread should use its own handle,
 * or external synchronization must be used.
 */

struct config;

#include "semantic_types.h"

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Error Handling
 * ============================================================================
 *
 * All functions follow the project coding standard:
 * - SUCCESS: Returns 0
 * - FAILURE: Returns negative errno value (e.g., -EINVAL, -ENOMEM, -EIO)
 *
 * Common errors:
 * - -EINVAL:   Invalid argument (NULL pointer, bad parameter)
 * - -ENOMEM:   Memory allocation failed
 * - -ENOENT:   Record not found
 * - -EEXIST:   Record already exists (constraint violation)
 * - -EBUSY:    Database locked/busy
 * - -EIO:      I/O error or database corruption
 */

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/*
 * @brief Opaque database handle.
 *
 * The internal structure is defined by the selected backend.
 * Users interact only through the functions below.
 */
typedef struct db_handle db_handle;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/*
 * @brief Open a database connection.
 *
 * Creates the database file if it does not exist.
 * Initializes schema if needed.
 *
 * @return 0 on success, error code on failure.
 */
int db_open(db_handle** db, const struct config* cfg);

/*
 * @brief Close database connection and free resources.
 *
 * @param[in,out] db Pointer to handle (set to NULL on return)
 */
void db_close(db_handle** db);

/*
 * @brief Get last error message.
 *
 * Returns a human-readable error description for the most recent
 * failed operation. The string is valid until the next database call.
 *
 * @param[in] db Database handle.
 * @return Error message string, or NULL if no error.
 */
// DELETE: const char *db_error_message(const db_handle *db);

/* ============================================================================
 * Tier 1: Semantic Time-Series Data
 * ============================================================================
 *
 * Tier 1 data uses the strongly-typed semantic type system.
 * Optimized for time-series queries on known data types.
 */

/*
 * @brief Insert a Tier 1 data point.
 *
 * @param db        Database handle.
 * @param type      Semantic type ID.
 * @param timestamp Unix timestamp (seconds since epoch).
 * @param value     Numeric value in canonical unit.
 * @param currency  ISO 4217 currency code (e.g., "SEK"), or NULL if not monetary.
 * @param source_id Plugin ID that reported this data.
 * @return 0 on success, error code on failure.
 */
int db_insert_tier1(db_handle* db, semantic_type type, int64_t timestamp, double value,
                    const char* currency, const char* source_id);

/*
 * @brief Query the most recent value for a semantic type.
 *
 * @param[in]  db      Database handle.
 * @param[in]  type    Semantic type to query.
 * @param[out] out_val Output value (canonical unit).
 * @param[out] out_ts  Output timestamp.
 * @return 0 if found, DB_NOT_FOUND if no data, error code on failure.
 */
int db_query_latest_tier1(db_handle* db, semantic_type type, double* out_val, int64_t* out_ts);

/*
 * @brief Query values in a time range.
 *
 * @param[in]  db           Database handle.
 * @param[in]  type         Semantic type to query.
 * @param[in]  from_ts      Start timestamp (inclusive).
 * @param[in]  to_ts        End timestamp (inclusive).
 * @param[out] out_values   Output values array (caller frees via db_free).
 * @param[out] out_ts       Output timestamps array (caller frees via db_free).
 * @param[out] out_count    Number of results.
 * @return 0 on success (may return 0 results), error code on failure.
 */
int db_query_range_tier1(db_handle* db, semantic_type type, int64_t from_ts, int64_t to_ts,
                         double** out_values, int64_t** out_ts, size_t* out_count);

/*
 * @brief Check if a Tier 1 data point already exists.
 *
 * @param db        Database handle.
 * @param type      Semantic type ID.
 * @param timestamp Unix timestamp.
 * @return 1 if exists, 0 if not, negative error code on failure.
 */
int db_query_point_exists_tier1(db_handle* db, semantic_type type, int64_t timestamp);

/* ============================================================================
 * Tier 2: Raw Extension Data
 * ============================================================================
 *
 * Tier 2 stores arbitrary JSON payloads keyed by string.
 * Used for plugin-specific data not covered by semantic types.
 */

/*
 * @brief Insert a Tier 2 raw data point.
 *
 * @param db           Database handle.
 * @param key          Dot-separated key (e.g., "com.vendor.sensor.temp").
 * @param timestamp    Unix timestamp.
 * @param json_payload JSON payload string.
 * @param source_id    Plugin ID.
 * @return 0 on success, error code on failure.
 */
int db_insert_tier2(db_handle* db, const char* key, int64_t timestamp, const char* json_payload,
                    const char* source_id);

/*
 * @brief Query most recent Tier 2 data.
 *
 * @param[in]  db       Database handle.
 * @param[in]  key      Data key.
 * @param[out] out_json Output JSON string (caller frees via db_free).
 * @param[out] out_ts   Output timestamp.
 * @return 0 if found, DB_NOT_FOUND if no data, error code on failure.
 */
int db_query_latest_tier2(db_handle* db, const char* key, char** out_json, int64_t* out_ts);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/*
 * @brief Free memory allocated by database queries.
 *
 * Use this to free arrays returned by query functions.
 *
 * @param ptr Pointer to free (NULL is safe).
 */
void db_free(void* ptr);

/* ============================================================================
 * Maintenance
 * ============================================================================ */

/*
 * @brief Delete data older than specified timestamp.
 *
 * @param db        Database handle.
 * @param type      Semantic type (or SEM_UNKNOWN for all types).
 * @param before_ts Delete records before this timestamp.
 * @return Number of records deleted, or negative error code.
 */
int db_prune_tier1(db_handle* db, semantic_type type, int64_t before_ts);

/*
 * @brief periodic tick for time-based operations (like CSV flushing).
 *
 * @param db Database handle.
 * @return 0 on success.
 */
int db_tick(db_handle* db);

/*
 * @brief Set the flush interval for CSV backend.
 *
 * @param db Database handle.
 * @param interval_sec Seconds between flushes.
 */
void db_set_interval(db_handle* db, int interval_sec);

/*
 * @brief Check if database is empty (no data points).
 *
 * Used for bootstrap detection on server startup.
 *
 * @param db Database handle.
 * @return 1 if empty, 0 if has data, negative on error.
 */
int db_is_empty(db_handle* db);

int db_maintenance(db_handle* db);

#endif /* HEIMWATT_DB_H */
