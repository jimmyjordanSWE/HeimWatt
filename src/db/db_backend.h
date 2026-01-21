/**
 * @file db_backend.h
 * @brief Internal backend operations interface
 *
 * This defines the contract that all database backends must implement.
 * Not part of the public API — used only by db.c
 */

#ifndef HEIMWATT_DB_BACKEND_H
#define HEIMWATT_DB_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"

/* ============================================================================
 * Backend Operations Interface
 * ============================================================================ */

/**
 * @brief Function pointers for backend implementations.
 *
 * Each backend (CSV, DuckDB, SQLite) provides its own ops struct.
 */
typedef struct db_backend_ops
{
    /* Lifecycle */
    int (*open)(void **ctx, const char *path);
    void (*close)(void **ctx);

    /* Tier 1: Semantic time-series */
    int (*insert_tier1)(void *ctx, semantic_type type, int64_t ts, double value,
                        const char *currency, const char *source_id);
    int (*query_latest_tier1)(void *ctx, semantic_type type, double *out_val, int64_t *out_ts);
    int (*query_range_tier1)(void *ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                             double **out_values, int64_t **out_ts, size_t *out_count);
    int (*query_point_exists_tier1)(void *ctx, semantic_type type, int64_t ts);

    /* Tier 2: Raw extension data */
    int (*insert_tier2)(void *ctx, const char *key, int64_t ts, const char *json,
                        const char *source);
    int (*query_latest_tier2)(void *ctx, const char *key, char **out_json, int64_t *out_ts);

    /* Maintenance */
    int (*tick)(void *ctx);
    void (*set_interval)(void *ctx, int interval_sec);
    int (*prune_tier1)(void *ctx, semantic_type type, int64_t before_ts);
    int (*is_empty)(void *ctx);
    int (*maintenance)(void *ctx);

} db_backend_ops;

/**
 * @brief A single backend instance.
 *
 * Holds context and ops for one storage engine.
 */
typedef struct db_backend
{
    void *ctx;                 /**< Backend-specific context (opaque) */
    const db_backend_ops *ops; /**< Function pointers */
    char uri[256];             /**< Config URI for logging */
    bool is_primary;           /**< If true, used for reads */
} db_backend;

/* ============================================================================
 * Backend Registration
 * ============================================================================ */

/**
 * @brief Get CSV backend operations.
 * @return Pointer to static ops struct.
 */
const db_backend_ops *csv_backend_get_ops(void);

/**
 * @brief Get DuckDB backend operations (when available).
 * @return Pointer to static ops struct, or NULL if not compiled in.
 */
const db_backend_ops *duckdb_backend_get_ops(void);

#endif /* HEIMWATT_DB_BACKEND_H */
