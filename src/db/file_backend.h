/**
 * @file file_backend.h
 * @brief File-Based Database Backend (Debug)
 *
 * Simple file-based storage for debugging and development.
 * Each plugin gets its own append-only log file.
 *
 * ## Storage Layout
 *
 * ```
 * data/
 * ├── tier1/
 * │   ├── atmosphere.temperature.log
 * │   ├── energy.price.spot.log
 * │   └── ...
 * └── tier2/
 *     ├── plugin_weather.log
 *     ├── plugin_solver.log
 *     └── ...
 * ```
 *
 * ## File Format
 *
 * Each line is a tab-separated record:
 * ```
 * TIMESTAMP\tVALUE\tCURRENCY\tSOURCE_ID
 * ```
 *
 * For Tier 2 (raw JSON):
 * ```
 * TIMESTAMP\tKEY\tJSON_PAYLOAD\tSOURCE_ID
 * ```
 *
 * ## Use Cases
 *
 * - Development: Easy to inspect data with `cat`, `tail -f`, `grep`
 * - Debugging: See exactly what each plugin is reporting
 * - Testing: Simple to create test fixtures
 * - Portability: No external dependencies
 *
 * ## Limitations
 *
 * - No indexing, queries scan entire files
 * - Not suitable for production with large datasets
 * - No transactions or atomicity guarantees
 */

#ifndef HEIMWATT_FILE_BACKEND_H
#define HEIMWATT_FILE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"

/**
 * File backend handle.
 */
typedef struct file_db file_db;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Open file-based database.
 *
 * Creates the directory structure if it doesn't exist.
 *
 * @param db       Output pointer for handle.
 * @param base_dir Base directory for data files (e.g., "./data").
 * @return 0 on success, -1 on error.
 */
int file_db_open(file_db **db, const char *base_dir);

/**
 * Close file database and flush buffers.
 *
 * @param db Pointer to handle (set to NULL on return).
 */
void file_db_close(file_db **db);

/* ============================================================================
 * Tier 1: Semantic Data (one file per semantic type)
 * ============================================================================ */

/**
 * Append a Tier 1 data point.
 *
 * Writes to: {base_dir}/tier1/{semantic_id}.log
 *
 * @param db        Database handle.
 * @param type      Semantic type.
 * @param timestamp Unix timestamp.
 * @param value     Numeric value.
 * @param currency  Currency code (NULL if not monetary).
 * @param source_id Plugin ID.
 * @return 0 on success, -1 on error.
 */
int file_db_append_tier1(file_db *db, semantic_type type, int64_t timestamp, double value,
                         const char *currency, const char *source_id);

/**
 * Read latest value for a semantic type.
 *
 * Scans file from end to find most recent entry.
 *
 * @param db      Database handle.
 * @param type    Semantic type.
 * @param out_val Output value.
 * @param out_ts  Output timestamp.
 * @return 0 if found, -1 if not found or error.
 */
int file_db_read_latest_tier1(file_db *db, semantic_type type, double *out_val, int64_t *out_ts);

/* ============================================================================
 * Tier 2: Raw Data (one file per source plugin)
 * ============================================================================ */

/**
 * Append a Tier 2 raw data point.
 *
 * Writes to: {base_dir}/tier2/{source_id}.log
 *
 * @param db           Database handle.
 * @param key          Data key.
 * @param timestamp    Unix timestamp.
 * @param json_payload JSON payload.
 * @param source_id    Plugin ID.
 * @return 0 on success, -1 on error.
 */
int file_db_append_tier2(file_db *db, const char *key, int64_t timestamp, const char *json_payload,
                         const char *source_id);

/**
 * Read latest raw data for a key.
 *
 * @param db       Database handle.
 * @param key      Data key.
 * @param out_json Output JSON (caller frees).
 * @param out_ts   Output timestamp.
 * @return 0 if found, -1 if not found or error.
 */
int file_db_read_latest_tier2(file_db *db, const char *key, char **out_json, int64_t *out_ts);

/* ============================================================================
 * Debug Utilities
 * ============================================================================ */

/**
 * Flush all open file handles.
 *
 * Forces buffered data to disk.
 *
 * @param db Database handle.
 */
void file_db_flush(file_db *db);

/**
 * Get path to a specific log file.
 *
 * Useful for `tail -f` during development.
 *
 * @param db     Database handle.
 * @param type   Semantic type (or SEM_UNKNOWN for tier2).
 * @param key    Tier2 key (NULL for tier1).
 * @param buf    Output buffer.
 * @param buflen Buffer size.
 * @return 0 on success, -1 if buffer too small.
 */
int file_db_get_path(const file_db *db, semantic_type type, const char *key, char *buf,
                     size_t buflen);

#endif /* HEIMWATT_FILE_BACKEND_H */
