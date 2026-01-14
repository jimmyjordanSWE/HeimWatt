/**
 * @file queries.h
 * @brief Named prepared statements
 *
 * Prepared statement wrappers for common operations.
 */

#ifndef HEIMWATT_QUERIES_H
#define HEIMWATT_QUERIES_H

#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"
#include "sqlite.h"

/* ============================================================
 * TIER 1: KNOWN SEMANTIC TYPES
 * ============================================================ */

/**
 * Insert a Tier 1 data point.
 *
 * @param conn      Database connection
 * @param type      Semantic type
 * @param timestamp Unix timestamp
 * @param value     Numeric value
 * @param currency  ISO currency code (can be NULL)
 * @param source_id Plugin ID
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_insert_tier1(db_conn* conn, semantic_type type, int64_t timestamp, double value,
                       const char* currency, const char* source_id);

/**
 * Get most recent value for a semantic type.
 *
 * @param conn         Database connection
 * @param type         Semantic type
 * @param out_value    Output value
 * @param out_timestamp Output timestamp
 * @param out_currency Output currency buffer (min 4 bytes)
 * @param out_source   Output source buffer (min 256 bytes)
 * @return DB_OK if found, DB_DONE if no data, DB_ERROR on error
 */
int query_select_latest_tier1(db_conn* conn, semantic_type type, double* out_value,
                              int64_t* out_timestamp, char* out_currency, char* out_source);

/**
 * Get values in time range.
 * Caller must free outputs via query_free_range_tier1().
 *
 * @param conn           Database connection
 * @param type           Semantic type
 * @param from_ts        Start timestamp (inclusive)
 * @param to_ts          End timestamp (inclusive)
 * @param out_values     Output values array
 * @param out_timestamps Output timestamps array
 * @param out_currencies Output currencies array
 * @param out_count      Output count
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_select_range_tier1(db_conn* conn, semantic_type type, int64_t from_ts, int64_t to_ts,
                             double** out_values, int64_t** out_timestamps, char*** out_currencies,
                             size_t* out_count);

/**
 * Free range query results.
 */
void query_free_range_tier1(double* values, int64_t* timestamps, char** currencies, size_t count);

/**
 * Get distinct sources for a semantic type.
 *
 * @param conn        Database connection
 * @param type        Semantic type
 * @param out_sources Output source array
 * @param out_count   Output count
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_select_sources_tier1(db_conn* conn, semantic_type type, char*** out_sources,
                               size_t* out_count);

/**
 * Delete old data (before timestamp).
 *
 * @param conn      Database connection
 * @param type      Semantic type
 * @param before_ts Delete records before this timestamp
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_delete_before_tier1(db_conn* conn, semantic_type type, int64_t before_ts);

/* ============================================================
 * TIER 2: RAW EXTENSION DATA
 * ============================================================ */

/**
 * Insert raw data.
 *
 * @param conn         Database connection
 * @param key          Data key
 * @param timestamp    Unix timestamp
 * @param json_payload JSON payload
 * @param source_id    Plugin ID
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_insert_tier2(db_conn* conn, const char* key, int64_t timestamp, const char* json_payload,
                       const char* source_id);

/**
 * Get most recent raw data.
 * Caller must free *out_json.
 *
 * @param conn          Database connection
 * @param key           Data key
 * @param out_json      Output JSON string
 * @param out_timestamp Output timestamp
 * @return DB_OK if found, DB_DONE if not found, DB_ERROR on error
 */
int query_select_latest_tier2(db_conn* conn, const char* key, char** out_json,
                              int64_t* out_timestamp);

/**
 * Get raw data in time range.
 *
 * @param conn           Database connection
 * @param key            Data key
 * @param from_ts        Start timestamp
 * @param to_ts          End timestamp
 * @param out_json       Output JSON array
 * @param out_timestamps Output timestamps array
 * @param out_count      Output count
 * @return DB_OK on success, DB_ERROR on failure
 */
int query_select_range_tier2(db_conn* conn, const char* key, int64_t from_ts, int64_t to_ts,
                             char*** out_json, int64_t** out_timestamps, size_t* out_count);

/**
 * Free range query results.
 */
void query_free_range_tier2(char** json, int64_t* timestamps, size_t count);

/* ============================================================
 * AGGREGATION
 * ============================================================ */

/**
 * Get count of records for a semantic type.
 */
int query_count_tier1(db_conn* conn, semantic_type type, size_t* out_count);

/**
 * Get time range bounds.
 */
int query_time_bounds_tier1(db_conn* conn, semantic_type type, int64_t* out_min_ts,
                            int64_t* out_max_ts);

/**
 * Get average value in range.
 */
int query_avg_tier1(db_conn* conn, semantic_type type, int64_t from_ts, int64_t to_ts,
                    double* out_avg);

#endif /* HEIMWATT_QUERIES_H */
