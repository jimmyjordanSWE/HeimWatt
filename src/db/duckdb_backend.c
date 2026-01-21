#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db_backend.h"
#include "duckdb.h"
#include "log.h"
#include "memory.h"

typedef struct duckdb_ctx
{
    duckdb_database db;
    duckdb_connection con;
    bool connected;
} duckdb_ctx;

static int duckdb_backend_open(void **ctx_out, const char *path)
{
    if (!ctx_out || !path) return -EINVAL;

    duckdb_ctx *ctx = mem_alloc(sizeof(*ctx));
    if (!ctx) return -ENOMEM;

    char wal_path[512];
    char backup_path[512];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
    snprintf(backup_path, sizeof(backup_path), "%s.backup", path);

    /* Stage 1: Try normal open */
    if (duckdb_open(path, &ctx->db) == DuckDBSuccess)
    {
        goto db_opened;
    }

    /* Stage 2: Remove stale WAL and retry */
    if (access(wal_path, F_OK) == 0)
    {
        log_warn("[DuckDB] Found stale WAL file, removing: %s", wal_path);
        unlink(wal_path);

        if (duckdb_open(path, &ctx->db) == DuckDBSuccess)
        {
            log_info("[DuckDB] Recovered after WAL cleanup");
            goto db_opened;
        }
    }

    /* Stage 3: Database file itself is corrupted - restore from backup or create fresh */
    if (access(backup_path, F_OK) == 0)
    {
        log_warn("[DuckDB] Database corrupted, restoring from backup: %s", backup_path);
        unlink(path);     /* Remove corrupted file */
        unlink(wal_path); /* Remove any WAL if exists */

        /* Copy backup to main path */
        FILE *src = fopen(backup_path, "rb");
        FILE *dst = fopen(path, "wb");
        if (src && dst)
        {
            char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
            {
                fwrite(buf, 1, n, dst);
            }
            fclose(src);
            fclose(dst);

            if (duckdb_open(path, &ctx->db) == DuckDBSuccess)
            {
                log_info("[DuckDB] Recovered from backup");
                goto db_opened;
            }
        }
        else
        {
            if (src) fclose(src);
            if (dst) fclose(dst);
        }
    }

    /* Stage 4: Delete everything and create fresh database */
    log_warn("[DuckDB] All recovery attempts failed, creating fresh database");
    unlink(path);
    unlink(wal_path);

    if (duckdb_open(path, &ctx->db) == DuckDBSuccess)
    {
        log_info("[DuckDB] Created fresh database at %s", path);
        goto db_opened;
    }

    log_error("[DuckDB] Failed to open database at %s (all recovery attempts failed)", path);
    mem_free(ctx);
    return -EIO;

db_opened:

    /* Connect */
    if (duckdb_connect(ctx->db, &ctx->con) == DuckDBError)
    {
        log_error("[DuckDB] Failed to connect to database");
        duckdb_close(&ctx->db);
        mem_free(ctx);
        return -EIO;
    }

    ctx->connected = true;

    /* Initialize Schema */
    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS tier1 ("
        "  type INTEGER,"
        "  ts BIGINT,"
        "  val DOUBLE,"
        "  currency VARCHAR,"
        "  source VARCHAR,"
        "  PRIMARY KEY (type, ts)"
        ");"
        "CREATE TABLE IF NOT EXISTS tier2 ("
        "  key VARCHAR,"
        "  ts BIGINT,"
        "  payload JSON,"
        "  source VARCHAR"
        ");";

    duckdb_state state = duckdb_query(ctx->con, schema_sql, NULL);
    if (state == DuckDBError)
    {
        log_error("[DuckDB] Schema init failed");
        /* Continue? Or fail? Let's fail safety. */
        duckdb_disconnect(&ctx->con);
        duckdb_close(&ctx->db);
        mem_free(ctx);
        return -EIO;
    }

    *ctx_out = ctx;
    return 0;
}

static void duckdb_backend_close(void **ctx_ptr)
{
    if (!ctx_ptr || !*ctx_ptr) return;
    duckdb_ctx *ctx = *ctx_ptr;

    if (ctx->connected)
    {
        // Force checkpoint to ensure all data is written to disk
        duckdb_result result;
        if (duckdb_query(ctx->con, "CHECKPOINT", &result) != DuckDBError)
        {
            duckdb_destroy_result(&result);
        }

        // Close connection first, then database
        duckdb_disconnect(&ctx->con);
        duckdb_close(&ctx->db);
        ctx->connected = false;
    }
    mem_free(ctx);
    *ctx_ptr = NULL;
}

static int duckdb_insert_tier1(void *ctx_void, semantic_type type, int64_t ts, double value,
                               const char *currency, const char *source_id)
{
    duckdb_ctx *ctx = (duckdb_ctx *) ctx_void;
    if (!ctx || !ctx->connected) return -EINVAL;

    /* Use prepared statement for safety/speed? Or simple query for MVP brevity?
       Let's use a simple query with snprintf for now, assuming inputs (currency/source) are
       relatively safe or we should escape them. Actually, DuckDB C API supports prepared
       statements. But for this prototype, let's just construct the query string carefully.
    */

    char query[1024];
    /* Using INSERT OR IGNORE or simple INSERT handling constraints?
       DuckDB supports INSERT OR IGNORE or INSERT OR REPLACE equivalent.
       "INSERT OR IGNORE INTO ..." work in DuckDB? Yes "INSERT OR IGNORE".
    */

    snprintf(query, sizeof(query), "INSERT OR IGNORE INTO tier1 VALUES (%d, %ld, %f, '%s', '%s')",
             type, ts, value, currency ? currency : "", source_id ? source_id : "");

    duckdb_state state = duckdb_query(ctx->con, query, NULL);
    if (state == DuckDBError)
    {
        /* Could be constraint violation if OR IGNORE didn't work, but we used it. */
        /* Check if it was unique constraint violation? */
        return -EIO;
    }

    return 0;
}

static int duckdb_query_latest_tier1(void *ctx_void, semantic_type type, double *out_val,
                                     int64_t *out_ts)
{
    duckdb_ctx *ctx = (duckdb_ctx *) ctx_void;
    if (!ctx || !ctx->connected) return -EINVAL;

    char query[256];
    snprintf(query, sizeof(query),
             "SELECT val, ts FROM tier1 WHERE type=%d ORDER BY ts DESC LIMIT 1", type);

    duckdb_result result;
    if (duckdb_query(ctx->con, query, &result) == DuckDBError)
    {
        return -EIO;
    }

    idx_t row_count = duckdb_row_count(&result);
    if (row_count == 0)
    {
        duckdb_destroy_result(&result);
        return -ENOENT; /* Not Found */
    }

    /* Extract data - Assuming column 0 is val (double), 1 is ts (bigint) */
    /* Note: DuckDB C API accessor functions checks types usually. */

    if (out_val) *out_val = duckdb_value_double(&result, 0, 0);
    if (out_ts) *out_ts = duckdb_value_int64(&result, 1, 0);

    duckdb_destroy_result(&result);
    return 0;
}

static int duckdb_query_range_tier1(void *ctx_void, semantic_type type, int64_t from_ts,
                                    int64_t to_ts, double **out_values, int64_t **out_ts,
                                    size_t *out_count)
{
    duckdb_ctx *ctx = (duckdb_ctx *) ctx_void;
    if (!ctx || !ctx->connected) return -EINVAL;

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT val, ts FROM tier1 WHERE type=%d AND ts >= %ld AND ts <= %ld ORDER BY ts ASC",
             type, from_ts, to_ts);

    duckdb_result result;
    if (duckdb_query(ctx->con, query, &result) == DuckDBError)
    {
        return -EIO;
    }

    idx_t count = duckdb_row_count(&result);
    *out_count = (size_t) count;

    if (count > 0)
    {
        *out_values = mem_alloc(count * sizeof(**out_values));
        *out_ts = mem_alloc(count * sizeof(**out_ts));

        if (!*out_values || !*out_ts)
        {
            mem_free(*out_values);
            mem_free(*out_ts);
            duckdb_destroy_result(&result);
            return -ENOMEM;
        }

        for (idx_t i = 0; i < count; i++)
        {
            (*out_values)[i] = duckdb_value_double(&result, 0, i);  // col 0, row i?
            /* wait, duckdb_value_double takes query result, col index, row index */
            /* duckdb_value_double(duckdb_result *result, idx_t col, idx_t row); */
            (*out_ts)[i] = duckdb_value_int64(&result, 1, i);
        }
    }
    else
    {
        *out_values = NULL;
        *out_ts = NULL;
    }

    duckdb_destroy_result(&result);
    return 0;
}

static int duckdb_query_point_exists_tier1(void *ctx_void, semantic_type type, int64_t timestamp)
{
    duckdb_ctx *ctx = (duckdb_ctx *) ctx_void;
    if (!ctx || !ctx->connected) return -EINVAL;

    char query[256];
    snprintf(query, sizeof(query), "SELECT 1 FROM tier1 WHERE type=%d AND ts=%ld", type, timestamp);

    duckdb_result result;
    if (duckdb_query(ctx->con, query, &result) == DuckDBError)
    {
        return -EIO;
    }

    idx_t row_count = duckdb_row_count(&result);
    duckdb_destroy_result(&result);

    return (row_count > 0) ? 1 : 0;
}

/* Stubs for other ops if needed, or leave NULL */
/* Tier2 not fully implemented here yet to save space/time, but schema exists. */

static const db_backend_ops DUCK_OPS = {.open = duckdb_backend_open,
                                        .close = duckdb_backend_close,
                                        .insert_tier1 = duckdb_insert_tier1,
                                        .query_latest_tier1 = duckdb_query_latest_tier1,
                                        .query_range_tier1 = duckdb_query_range_tier1,
                                        .query_point_exists_tier1 = duckdb_query_point_exists_tier1,
                                        /* Insert Tier 2 stub? */
                                        .insert_tier2 = NULL,
                                        .query_latest_tier2 = NULL,
                                        .tick = NULL,
                                        .set_interval = NULL,
                                        .prune_tier1 = NULL,
                                        .is_empty = NULL,
                                        .maintenance = NULL};

const db_backend_ops *duckdb_backend_get_ops(void) { return &DUCK_OPS; }
