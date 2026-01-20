/**
 * @file csv_backend.c
 * @brief CSV storage backend implementation
 *
 * "Wide" CSV format: Timestamp + One column per semantic type
 */

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "db_backend.h"
#include "log.h"
#include "memory.h"
#include "semantic_types.h"

/* ============================================================================
 * Internal Context
 * ============================================================================ */

typedef struct csv_ctx
{
    FILE *fp;
    char path[256];

    /* In-memory buffer of latest known values (Tier 1) */
    double values[SEM_TYPE_COUNT];
    int64_t last_ts[SEM_TYPE_COUNT];
    bool has_value[SEM_TYPE_COUNT];

    int interval_sec;
    time_t last_flush;
} csv_ctx;

/* ============================================================================
 * Helpers
 * ============================================================================ */

static time_t parse_iso(const char *s)
{
    struct tm tm = {0};
    tm.tm_isdst = -1;
    strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
}

static void write_header(FILE *fp)
{
    fprintf(fp, "timestamp");
    for (int i = 1; i < SEM_TYPE_COUNT; i++)
    {
        const semantic_meta *meta = semantic_get_meta((semantic_type) i);
        if (meta)
            fprintf(fp, ",%s", meta->id);
        else
            fprintf(fp, ",unknown_%d", i);
    }
    fprintf(fp, "\n");
    fflush(fp);
}

static void flush_row(csv_ctx *ctx, time_t now)
{
    char time_buf[32];
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &tm_info);

    fprintf(ctx->fp, "%s", time_buf);

    for (int i = 1; i < SEM_TYPE_COUNT; i++)
    {
        fprintf(ctx->fp, ",");
        if (ctx->has_value[i]) fprintf(ctx->fp, "%.6g", ctx->values[i]);
    }
    fprintf(ctx->fp, "\n");
    fflush(ctx->fp);
}

/* ============================================================================
 * Backend Ops Implementation
 * ============================================================================ */

static int csv_open(void **ctx_out, const char *path)
{
    if (!path) return -EINVAL;

    csv_ctx *ctx = mem_alloc(sizeof(*ctx));
    if (!ctx) return -ENOMEM;

    snprintf(ctx->path, sizeof(ctx->path), "%s/history.csv", path);

    /* Create directory recursively (mkdir -p behavior) */
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);

    bool needs_header = (access(ctx->path, F_OK) != 0);

    ctx->fp = fopen(ctx->path, "a+");
    if (!ctx->fp)
    {
        mem_free(ctx);
        return -errno;
    }

    if (needs_header)
    {
        write_header(ctx->fp);
    }
    else
    {
        /* Replay existing data to populate in-memory state */
        fseek(ctx->fp, 0, SEEK_SET);
        char line[4096];
        if (fgets(line, sizeof(line), ctx->fp)) /* Skip header */
        {
            while (fgets(line, sizeof(line), ctx->fp))
            {
                line[strcspn(line, "\r\n")] = 0;
                char *ptr = line;
                char *token = strsep(&ptr, ",");
                if (!token) continue;
                time_t row_ts = parse_iso(token);

                for (int i = 1; i < SEM_TYPE_COUNT; i++)
                {
                    token = strsep(&ptr, ",");
                    if (token && *token != '\0')
                    {
                        char *endptr = NULL;
                        double val = strtod(token, &endptr);
                        if (endptr != token)
                        {
                            ctx->values[i] = val;
                            ctx->has_value[i] = true;
                            ctx->last_ts[i] = (int64_t) row_ts;
                        }
                    }
                }
            }
        }
    }

    ctx->interval_sec = 60;
    ctx->last_flush = time(NULL);

    *ctx_out = ctx;
    return 0;
}

static void csv_close(void **ctx_ptr)
{
    if (!ctx_ptr || !*ctx_ptr) return;

    csv_ctx *ctx = *ctx_ptr;
    if (ctx->fp)
    {
        /* Flush before close to ensure persistence */
        time_t flush_ts = time(NULL);
        int64_t max_ts = 0;
        for (int i = 1; i < SEM_TYPE_COUNT; i++)
        {
            if (ctx->has_value[i] && ctx->last_ts[i] > max_ts) max_ts = ctx->last_ts[i];
        }
        if (max_ts > 0) flush_ts = (time_t) max_ts;

        flush_row(ctx, flush_ts);
        fclose(ctx->fp);
    }
    mem_free(ctx);
    *ctx_ptr = NULL;
}

static int csv_insert_tier1(void *ctx_ptr, semantic_type type, int64_t timestamp, double value,
                            const char *currency, const char *source_id)
{
    (void) currency;
    (void) source_id;
    csv_ctx *ctx = ctx_ptr;
    if (!ctx || type <= SEM_UNKNOWN || type >= SEM_TYPE_COUNT) return -EINVAL;

    if (ctx->has_value[type] && ctx->last_ts[type] == timestamp) return -EEXIST;

    ctx->values[type] = value;
    ctx->has_value[type] = true;
    ctx->last_ts[type] = timestamp;
    return 0;
}

static int csv_query_latest_tier1(void *ctx_ptr, semantic_type type, double *out_val,
                                  int64_t *out_ts)
{
    csv_ctx *ctx = ctx_ptr;
    if (!ctx || type >= SEM_TYPE_COUNT) return -EINVAL;
    if (ctx->has_value[type])
    {
        *out_val = ctx->values[type];
        *out_ts = ctx->last_ts[type];
        return 0;
    }
    return -2; /* Not found */
}

static int csv_query_range_tier1(void *ctx_ptr, semantic_type type, int64_t from_ts, int64_t to_ts,
                                 double **out_values, int64_t **out_ts, size_t *out_count)
{
    csv_ctx *ctx = ctx_ptr;
    if (!ctx || !ctx->fp || !out_values || !out_ts || !out_count) return -EINVAL;

    fseek(ctx->fp, 0, SEEK_SET);
    char line[4096];
    if (!fgets(line, sizeof(line), ctx->fp)) return -1;

    int col_idx = -1;
    int current_col = 0;
    char *header_dup = mem_alloc(strlen(line) + 1);
    if (header_dup) strcpy(header_dup, line);
    char *token = strtok(header_dup, ",\n");

    const semantic_meta *meta = semantic_get_meta(type);
    if (!meta)
    {
        mem_free(header_dup);
        return -EINVAL;
    }

    while (token)
    {
        if (strcmp(token, meta->id) == 0)
        {
            col_idx = current_col;
            break;
        }
        token = strtok(NULL, ",\n");
        current_col++;
    }
    mem_free(header_dup);

    if (col_idx == -1)
    {
        *out_count = 0;
        *out_values = NULL;
        *out_ts = NULL;
        return 0;
    }

    size_t cap = 256;
    size_t count = 0;
    double *vals = mem_alloc(cap * sizeof(*vals));
    int64_t *tss = mem_alloc(cap * sizeof(*tss));
    if (!vals || !tss)
    {
        mem_free(vals);
        mem_free(tss);
        return -ENOMEM;
    }

    while (fgets(line, sizeof(line), ctx->fp))
    {
        char *ptr = line;
        char *comma = strchr(ptr, ',');
        if (!comma) continue;
        *comma = '\0';

        time_t row_ts = parse_iso(ptr);
        if (row_ts >= from_ts && row_ts <= to_ts)
        {
            char *row_cursor = comma + 1;
            int c = 1;
            while (c < col_idx)
            {
                char *next_comma = strchr(row_cursor, ',');
                if (!next_comma)
                {
                    row_cursor = NULL;
                    break;
                }
                row_cursor = next_comma + 1;
                c++;
            }

            if (row_cursor)
            {
                char *val_end = strchr(row_cursor, ',');
                if (val_end)
                    *val_end = '\0';
                else
                {
                    val_end = strchr(row_cursor, '\n');
                    if (val_end) *val_end = '\0';
                }

                if (*row_cursor != '\0')
                {
                    double v = strtod(row_cursor, NULL);
                    if (count >= cap)
                    {
                        cap *= 2;

                        double *new_vals = mem_realloc(vals, cap * sizeof(double));
                        if (!new_vals)
                        {
                            mem_free(vals);
                            mem_free(tss);
                            return -ENOMEM;
                        }
                        vals = new_vals;

                        int64_t *new_tss = mem_realloc(tss, cap * sizeof(int64_t));
                        if (!new_tss)
                        {
                            mem_free(vals);
                            mem_free(tss);
                            return -ENOMEM;
                        }
                        tss = new_tss;
                    }
                    vals[count] = v;
                    tss[count] = (int64_t) row_ts;
                    count++;
                }
            }
        }
    }
    *out_values = vals;
    *out_ts = tss;
    *out_count = count;
    return 0;
}

static int csv_query_point_exists_tier1(void *ctx_ptr, semantic_type type, int64_t timestamp)
{
    csv_ctx *ctx = ctx_ptr;
    if (!ctx) return -EINVAL;
    if (ctx->has_value[type] && ctx->last_ts[type] == timestamp) return 1;
    return 0;
}

static int csv_insert_tier2(void *ctx_ptr, const char *key, int64_t timestamp,
                            const char *json_payload, const char *source_id)
{
    (void) ctx_ptr;
    (void) key;
    (void) timestamp;
    (void) json_payload;
    (void) source_id;
    return 0; /* Tier 2 not implemented for CSV */
}

static int csv_query_latest_tier2(void *ctx_ptr, const char *key, char **out_json, int64_t *out_ts)
{
    (void) ctx_ptr;
    (void) key;
    (void) out_json;
    (void) out_ts;
    return -1; /* Tier 2 not implemented for CSV */
}

static int csv_tick(void *ctx_ptr)
{
    csv_ctx *ctx = ctx_ptr;
    if (!ctx) return -EINVAL;
    time_t now = time(NULL);
    if (now - ctx->last_flush >= ctx->interval_sec)
    {
        flush_row(ctx, now);
        ctx->last_flush = now;
    }
    return 0;
}

static void csv_set_interval(void *ctx_ptr, int interval_sec)
{
    csv_ctx *ctx = ctx_ptr;
    if (ctx && interval_sec > 0) ctx->interval_sec = interval_sec;
}

static int csv_is_empty(void *ctx_ptr)
{
    csv_ctx *ctx = ctx_ptr;
    if (!ctx || !ctx->fp) return 1;

    FILE *fp = fopen(ctx->path, "r");
    if (!fp) return 1;

    char line[8192];
    /* Skip header */
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return 1;
    }

    /* Try to read first data line */
    int has_data = (fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);

    return !has_data;
}

static const char *csv_error_message(void *ctx_ptr)
{
    (void) ctx_ptr;
    return "CSV backend operation failed";
}

/* ============================================================================
 * Ops Table Export
 * ============================================================================ */

static const db_backend_ops CSV_OPS = {
    .open = csv_open,
    .close = csv_close,
    .insert_tier1 = csv_insert_tier1,
    .query_latest_tier1 = csv_query_latest_tier1,
    .query_range_tier1 = csv_query_range_tier1,
    .query_point_exists_tier1 = csv_query_point_exists_tier1,
    .insert_tier2 = csv_insert_tier2,
    .query_latest_tier2 = csv_query_latest_tier2,
    .tick = csv_tick,
    .set_interval = csv_set_interval,
    .prune_tier1 = NULL, /* Not implemented */
    .is_empty = csv_is_empty,
    .maintenance = NULL, /* Not implemented */
    .error_message = csv_error_message,
};

const db_backend_ops *csv_backend_get_ops(void) { return &CSV_OPS; }
