/**
 * @file data_store.h
 * @brief Semantic data storage abstraction
 *
 * High-level API for storing/querying semantic data.
 * Wraps the database layer with semantic type awareness.
 */

#ifndef HEIMWATT_DATA_STORE_H
#define HEIMWATT_DATA_STORE_H

#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"

typedef struct data_store data_store;

/* ============================================================
 * LIFECYCLE
 * ============================================================ */

/**
 * Open data store.
 *
 * @param ds      Output pointer for data store
 * @param db_path Path to database file
 * @return 0 on success, -1 on error
 */
int data_store_open(data_store** ds, const char* db_path);

/**
 * Close data store.
 *
 * @param ds Pointer to data store (set to NULL on return)
 */
void data_store_close(data_store** ds);

/* ============================================================
 * TIER 1: KNOWN SEMANTIC TYPES
 * ============================================================ */

/**
 * Data point for Tier 1 storage.
 */
typedef struct {
    int64_t timestamp;     /**< Unix timestamp */
    double value;          /**< Numeric value */
    char currency[4];      /**< Optional ISO currency code */
    const char* source_id; /**< Plugin ID that reported this */
} data_point;

/**
 * Insert a Tier 1 data point.
 *
 * @param ds   Data store
 * @param type Semantic type
 * @param pt   Data point to insert
 * @return 0 on success, -1 on error
 */
int data_store_insert(data_store* ds, semantic_type type, const data_point* pt);

/**
 * Query most recent value for a semantic type.
 *
 * @param ds   Data store
 * @param type Semantic type
 * @param out  Output data point
 * @return 0 if found, -1 if not found or error
 */
int data_store_query_latest(data_store* ds, semantic_type type, data_point* out);

/**
 * Query values in time range.
 *
 * @param ds      Data store
 * @param type    Semantic type
 * @param from_ts Start timestamp (inclusive)
 * @param to_ts   End timestamp (inclusive)
 * @param out     Output array (caller must call data_store_free_points)
 * @param count   Output count
 * @return 0 on success, -1 on error
 */
int data_store_query_range(data_store* ds, semantic_type type, int64_t from_ts, int64_t to_ts,
                           data_point** out, size_t* count);

/**
 * Free data points returned by query_range.
 *
 * @param pts Data points array
 */
void data_store_free_points(data_point* pts);

/* ============================================================
 * TIER 2: RAW EXTENSION DATA
 * ============================================================ */

/**
 * Insert raw extension data.
 *
 * @param ds           Data store
 * @param key          Dot-separated key (e.g., "vendor.sensor")
 * @param timestamp    Unix timestamp
 * @param json_payload JSON payload
 * @param source_id    Plugin ID
 * @return 0 on success, -1 on error
 */
int data_store_insert_raw(data_store* ds, const char* key, int64_t timestamp,
                          const char* json_payload, const char* source_id);

/**
 * Query most recent raw data.
 *
 * @param ds       Data store
 * @param key      Data key
 * @param json_out Output JSON (caller frees)
 * @param ts_out   Output timestamp
 * @return 0 if found, -1 if not found or error
 */
int data_store_query_raw_latest(data_store* ds, const char* key, char** json_out, int64_t* ts_out);

#endif /* HEIMWATT_DATA_STORE_H */
