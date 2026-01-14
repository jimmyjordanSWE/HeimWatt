#ifndef DB_H
#define DB_H

#include <stdint.h>
#include <time.h>

#include "semantic_types.h"

/**
 * @brief Opaque database handle.
 */
typedef struct db_handle db_handle;

/**
 * @brief Create and open database (creates if not exists).
 *
 * @param db   Output pointer for handle
 * @param path Path to database file
 * @return 0 on success, negative errno on failure.
 */
int db_create(db_handle** db, const char* path);

/**
 * @brief Destroy database handle and close connection.
 *
 * @param db Pointer to handle (set to NULL on return)
 */
void db_destroy(db_handle** db);

/**
 * @brief Initialize database tables (schema migration).
 * @param db Database handle.
 * @return 0 on success, negative errno on failure.
 */
int db_init_tables(db_handle* db);

/* ============================================================================
 * Tier 1: Known Semantic Types (Fast, Indexable)
 * ============================================================================ */

/**
 * @brief Insert a Tier 1 data point.
 * @param db        Database handle.
 * @param type      Semantic type ID.
 * @param timestamp Unix timestamp.
 * @param value     Numeric value (canonical unit).
 * @param currency  ISO 4217 currency code (optional, can be NULL).
 * @param source_id Plugin ID source.
 */
int db_insert_tier1(db_handle* db, semantic_type type, int64_t timestamp, double value,
                    const char* currency, const char* source_id);

/**
 * @brief Query recent Tier 1 data.
 * @return 0 found, -1 not found.
 */
int db_query_latest_tier1(db_handle* db, semantic_type type, double* out_val, int64_t* out_ts);

/* ============================================================================
 * Tier 2: Raw Extension Data (Flexible / Logs)
 * ============================================================================ */

/**
 * @brief Insert a Tier 2 raw data point.
 * @param key Dot-separated key (e.g. "com.vendor.sensor1.temp").
 */
int db_insert_tier2(db_handle* db, const char* key, int64_t timestamp, const char* json_payload,
                    const char* source_id);

/**
 * @brief Query recent Tier 2 data.
 * @param out_json Pointer to char* (caller must free).
 */
int db_query_latest_tier2(db_handle* db, const char* key, char** out_json, int64_t* out_ts);

#endif /* DB_H */
