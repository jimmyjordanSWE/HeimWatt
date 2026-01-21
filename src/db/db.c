/**
 * @file db.c
 * @brief Composite database dispatcher
 *
 * Manages multiple backends and dispatches operations:
 * - Writes broadcast to ALL backends
 * - Reads use PRIMARY backend only
 */

#include "db.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"
#include "db_backend.h"
#include "log.h"
#include "memory.h"

/* ============================================================================
 * Composite Handle
 * ============================================================================ */

struct db_handle
{
    db_backend *backends;
    size_t count;
    pthread_mutex_t lock;
};

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int db_open(db_handle **db_out, const struct config *cfg)
{
    if (!db_out || !cfg) return -EINVAL;

    db_handle *db = mem_alloc(sizeof(*db));
    if (!db) return -ENOMEM;

    size_t count = config_get_backend_count(cfg);
    if (count == 0)
    {
        /* No backends configured */
        log_warn("[DB] No backends configured");
        free(db);
        return -EINVAL; /* Policy: must have at least one backend? Or allow memory-only? */
    }

    db->backends = mem_alloc(count * sizeof(*db->backends));
    if (!db->backends)
    {
        mem_free(db);
        return -ENOMEM;
    }

    db->count = 0; /* Will count successfully opened backends */

    for (size_t i = 0; i < count; i++)
    {
        const storage_backend_config *bc = config_get_backend(cfg, i);
        if (!bc) continue;

        db_backend *be = &db->backends[db->count];  // Next slot

        // Dispatch based on type
        if (strcmp(bc->type, "csv") == 0)
        {
            be->ops = csv_backend_get_ops();
        }
        else if (strcmp(bc->type, "duckdb") == 0)
        {
            be->ops = duckdb_backend_get_ops();
        }
        else
        {
            log_warn("[DB] Unknown backend type '%s'", bc->type);
            continue;
        }

        if (snprintf(be->uri, sizeof(be->uri), "%s://%s", bc->type, bc->path) >=
            (int) sizeof(be->uri))
        {
            log_warn("[DB] URI truncated for backend type '%s'", bc->type);
        }
        be->is_primary = bc->primary;

        int ret = be->ops->open(&be->ctx, bc->path);
        if (ret == 0)
        {
            log_info("[DB] Opened backend: %s (primary=%d)", be->uri, be->is_primary);
            db->count++;
        }
        else
        {
            log_error("[DB] Failed to open backend %s: %d", be->uri, ret);
            // Continue to try other backends?
        }
    }

    if (db->count == 0)
    {
        log_fatal("[DB] Failed to open any storage backends");
        mem_free(db->backends);
        mem_free(db);
        return -EIO;
    }

    // Sort valid backends so primary is at index 0?
    // For now, linear scan for read ops is fine if count is small (1 or 2).
    // Or we just enforce that the first configured primary is used.
    // Let's ensure at least one primary exists, or default to the first one.

    int primary_idx = -1;
    for (size_t i = 0; i < db->count; i++)
    {
        if (db->backends[i].is_primary)
        {
            if (primary_idx == -1)
            {
                primary_idx = (int) i;
            }
            else
            {
                log_warn(
                    "[DB] Multiple primary backends defined. Using first encountered ('%s'), "
                    "ignoring '%s' as primary",
                    db->backends[primary_idx].uri, db->backends[i].uri);
                db->backends[i].is_primary = false;
            }
        }
    }

    if (primary_idx != -1)
    {
        /* Swap primary to index 0 for O(1) access */
        if (primary_idx != 0)
        {
            db_backend tmp = db->backends[0];
            db->backends[0] = db->backends[primary_idx];
            db->backends[primary_idx] = tmp;
        }
    }
    else if (db->count > 0)
    {
        log_warn("[DB] No primary backend defined, using first one (%s) as primary",
                 db->backends[0].uri);
        db->backends[0].is_primary = true;
    }

    if (pthread_mutex_init(&db->lock, NULL) != 0)
    {
        mem_free(db->backends);
        mem_free(db);
        return -ENOMEM;
    }

    *db_out = db;
    return 0;
}

void db_close(db_handle **db_ptr)
{
    if (!db_ptr || !*db_ptr) return;

    db_handle *db = *db_ptr;
    for (size_t i = 0; i < db->count; i++)
    {
        if (db->backends[i].ops && db->backends[i].ctx)
        {
            db->backends[i].ops->close(&db->backends[i].ctx);
        }
    }
    mem_free(db->backends);
    pthread_mutex_destroy(&db->lock);
    mem_free(db);
    *db_ptr = NULL;
}

/* ============================================================================
 * Tier 1: Semantic Time-Series
 * ============================================================================ */

int db_insert_tier1(db_handle *db, semantic_type type, int64_t timestamp, double value,
                    const char *currency, const char *source_id)
{
    if (!db || db->count == 0) return -EINVAL;

    pthread_mutex_lock(&db->lock);
    int primary_ret = 0;

    for (size_t i = 0; i < db->count; i++)
    {
        int ret = db->backends[i].ops->insert_tier1(db->backends[i].ctx, type, timestamp, value,
                                                    currency, source_id);
        if (i == 0)
        {
            primary_ret = ret; /* Primary determines success/failure */
        }
        else if (ret != 0 && ret != -EEXIST)
        {
            log_warn("[DB] Secondary '%s' write failed: %d", db->backends[i].uri, ret);
            /* Continue — don't fail the whole operation */
        }
    }
    pthread_mutex_unlock(&db->lock);

    return primary_ret;
}

int db_query_latest_tier1(db_handle *db, semantic_type type, double *out_val, int64_t *out_ts)
{
    if (!db || db->count == 0) return -EINVAL;
    /* Always use primary backend for reads - reads are thread safe typically */
    pthread_mutex_lock(&db->lock);
    int ret = db->backends[0].ops->query_latest_tier1(db->backends[0].ctx, type, out_val, out_ts);
    pthread_mutex_unlock(&db->lock);
    return ret;
}

int db_query_range_tier1(db_handle *db, semantic_type type, int64_t from_ts, int64_t to_ts,
                         double **out_values, int64_t **out_ts, size_t *out_count)
{
    if (!db || db->count == 0) return -EINVAL;
    pthread_mutex_lock(&db->lock);
    int ret = db->backends[0].ops->query_range_tier1(db->backends[0].ctx, type, from_ts, to_ts,
                                                     out_values, out_ts, out_count);
    pthread_mutex_unlock(&db->lock);
    return ret;
}

int db_query_point_exists_tier1(db_handle *db, semantic_type type, int64_t timestamp)
{
    if (!db || db->count == 0) return -EINVAL;
    return db->backends[0].ops->query_point_exists_tier1(db->backends[0].ctx, type, timestamp);
}

/* ============================================================================
 * Tier 2: Raw Extension Data
 * ============================================================================ */

int db_insert_tier2(db_handle *db, const char *key, int64_t timestamp, const char *json_payload,
                    const char *source_id)
{
    if (!db || db->count == 0) return -EINVAL;

    pthread_mutex_lock(&db->lock);
    int primary_ret = 0;

    for (size_t i = 0; i < db->count; i++)
    {
        int ret = db->backends[i].ops->insert_tier2(db->backends[i].ctx, key, timestamp,
                                                    json_payload, source_id);
        if (i == 0)
        {
            primary_ret = ret;
        }
        else if (ret != 0)
        {
            log_warn("[DB] Secondary '%s' tier2 write failed: %d", db->backends[i].uri, ret);
        }
    }
    pthread_mutex_unlock(&db->lock);

    return primary_ret;
}

int db_query_latest_tier2(db_handle *db, const char *key, char **out_json, int64_t *out_ts)
{
    if (!db || db->count == 0) return -EINVAL;
    pthread_mutex_lock(&db->lock);
    int ret = db->backends[0].ops->query_latest_tier2(db->backends[0].ctx, key, out_json, out_ts);
    pthread_mutex_unlock(&db->lock);
    return ret;
}

/* ============================================================================
 * Maintenance
 * ============================================================================ */

int db_tick(db_handle *db)
{
    if (!db) return -EINVAL;

    if (pthread_mutex_trylock(&db->lock) == 0)
    {
        /* Tick all backends */
        for (size_t i = 0; i < db->count; i++)
        {
            if (db->backends[i].ops->tick)
            {
                db->backends[i].ops->tick(db->backends[i].ctx);
            }
        }
        pthread_mutex_unlock(&db->lock);
        return 0;
    }
    return -EBUSY;
}

void db_set_interval(db_handle *db, int interval_sec)
{
    if (!db) return;

    for (size_t i = 0; i < db->count; i++)
    {
        if (db->backends[i].ops->set_interval)
        {
            db->backends[i].ops->set_interval(db->backends[i].ctx, interval_sec);
        }
    }
}

int db_prune_tier1(db_handle *db, semantic_type type, int64_t before_ts)
{
    if (!db || db->count == 0) return -EINVAL;

    int total = 0;
    for (size_t i = 0; i < db->count; i++)
    {
        if (db->backends[i].ops->prune_tier1)
        {
            int ret = db->backends[i].ops->prune_tier1(db->backends[i].ctx, type, before_ts);
            if (i == 0 && ret >= 0) total = ret;
        }
    }
    return total;
}

int db_is_empty(db_handle *db)
{
    if (!db || db->count == 0) return 1;
    if (db->backends[0].ops->is_empty)
    {
        return db->backends[0].ops->is_empty(db->backends[0].ctx);
    }
    return 1;
}

int db_maintenance(db_handle *db)
{
    if (!db) return -EINVAL;

    for (size_t i = 0; i < db->count; i++)
    {
        if (db->backends[i].ops->maintenance)
        {
            db->backends[i].ops->maintenance(db->backends[i].ctx);
        }
    }
    return 0;
}

void db_free(void *ptr) { mem_free(ptr); }
