#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "log.h"
#include "semantic_types.h"

// "Wide" CSV: Timestamp + One column per semantic type
struct db_handle
{
    FILE *fp;
    char path[256];

    // In-memory buffer of latest known values (Tier 1)
    double values[SEM_TYPE_COUNT];
    int64_t last_ts[SEM_TYPE_COUNT];  // Helper for EEXIST and query_latest
    bool has_value[SEM_TYPE_COUNT];

    int interval_sec;
    time_t last_flush;
};

static time_t parse_iso(const char *s)
{
    struct tm tm = {0};
    tm.tm_isdst = -1;
    strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
}

// Generate header string
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

int db_open(db_handle **db_out, const char *path)
{
    if (!path) return -EINVAL;

    db_handle *db = calloc(1, sizeof(db_handle));
    if (!db) return -ENOMEM;

    snprintf(db->path, sizeof(db->path), "%s/history.csv", path);

    // Create directory recursively (mkdir -p behavior)
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
    mkdir(tmp, 0755);  // Create final directory

    bool needs_header = (access(db->path, F_OK) != 0);

    db->fp = fopen(db->path, "a+");
    if (!db->fp)
    {
        free(db);
        return -errno;
    }

    if (needs_header)
    {
        write_header(db->fp);
    }
    else
    {
        // Replay
        fseek(db->fp, 0, SEEK_SET);
        char line[4096];
        // Skip header
        if (fgets(line, sizeof(line), db->fp))
        {
            while (fgets(line, sizeof(line), db->fp))
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
                        db->values[i] = atof(token);
                        db->has_value[i] = true;
                        db->last_ts[i] = (int64_t) row_ts;
                    }
                }
            }
        }
    }

    db->interval_sec = 60;
    db->last_flush = time(NULL);

    *db_out = db;
    return 0;
}

// Forward decl
static void flush_row(db_handle *db, time_t now);

void db_close(db_handle **db_ptr)
{
    if (db_ptr && *db_ptr)
    {
        db_handle *db = *db_ptr;
        // Flush before close to ensure persistence
        if (db->fp)
        {
            time_t flush_ts = time(NULL);
            int64_t max_ts = 0;
            for (int i = 1; i < SEM_TYPE_COUNT; i++)
            {
                if (db->has_value[i] && db->last_ts[i] > max_ts) max_ts = db->last_ts[i];
            }
            if (max_ts > 0) flush_ts = (time_t) max_ts;

            flush_row(db, flush_ts);
            fclose(db->fp);
        }
        free(db);
        *db_ptr = NULL;
    }
}

const char *db_error_message(const db_handle *db)
{
    (void) db;
    if (!db) return "Database handle is NULL";
    return "Database operation failed";
}

int db_insert_tier1(db_handle *db, semantic_type type, int64_t timestamp, double value,
                    const char *currency, const char *source_id)
{
    (void) currency;
    (void) source_id;
    if (!db || type <= SEM_UNKNOWN || type >= SEM_TYPE_COUNT) return -EINVAL;

    if (db->has_value[type] && db->last_ts[type] == timestamp) return -EEXIST;

    db->values[type] = value;
    db->has_value[type] = true;
    db->last_ts[type] = timestamp;
    return 0;
}

static void flush_row(db_handle *db, time_t now)
{
    char time_buf[32];
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &tm_info);

    fprintf(db->fp, "%s", time_buf);

    for (int i = 1; i < SEM_TYPE_COUNT; i++)
    {
        fprintf(db->fp, ",");
        if (db->has_value[i]) fprintf(db->fp, "%.6g", db->values[i]);
    }
    fprintf(db->fp, "\n");
    fflush(db->fp);
}

int db_tick(db_handle *db)
{
    if (!db) return -EINVAL;
    time_t now = time(NULL);
    if (now - db->last_flush >= db->interval_sec)
    {
        flush_row(db, now);
        db->last_flush = now;
    }
    return 0;
}

void db_set_interval(db_handle *db, int interval_sec)
{
    if (db && interval_sec > 0) db->interval_sec = interval_sec;
}

int db_query_latest_tier1(db_handle *db, semantic_type type, double *out_val, int64_t *out_ts)
{
    if (!db || type >= SEM_TYPE_COUNT) return -EINVAL;
    if (db->has_value[type])
    {
        *out_val = db->values[type];
        *out_ts = db->last_ts[type];
        return 0;
    }
    return -2;
}

int db_query_range_tier1(db_handle *db, semantic_type type, int64_t from_ts, int64_t to_ts,
                         double **out_values, int64_t **out_ts, size_t *out_count)
{
    if (!db || !db->fp || !out_values || !out_ts || !out_count) return -EINVAL;

    fseek(db->fp, 0, SEEK_SET);
    char line[4096];
    if (!fgets(line, sizeof(line), db->fp)) return -1;

    int col_idx = -1;
    int current_col = 0;
    char *header_dup = strdup(line);
    char *token = strtok(header_dup, ",\n");

    const semantic_meta *meta = semantic_get_meta(type);
    if (!meta)
    {
        free(header_dup);
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
    free(header_dup);

    if (col_idx == -1)
    {
        *out_count = 0;
        *out_values = NULL;
        *out_ts = NULL;
        return 0;
    }

    size_t cap = 256;
    size_t count = 0;
    double *vals = malloc(cap * sizeof(double));
    int64_t *tss = malloc(cap * sizeof(int64_t));
    if (!vals || !tss)
    {
        free(vals);
        free(tss);
        return -ENOMEM;
    }

    while (fgets(line, sizeof(line), db->fp))
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

                        double *new_vals = realloc(vals, cap * sizeof(double));
                        if (!new_vals)
                        {
                            free(vals);
                            free(tss);
                            return -ENOMEM;
                        }
                        vals = new_vals;

                        int64_t *new_tss = realloc(tss, cap * sizeof(int64_t));
                        if (!new_tss)
                        {
                            free(vals);
                            free(tss);
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

int db_query_point_exists_tier1(db_handle *db, semantic_type type, int64_t timestamp)
{
    if (!db) return -EINVAL;
    if (db->has_value[type] && db->last_ts[type] == timestamp) return 1;
    return 0;
}

int db_insert_tier2(db_handle *db, const char *key, int64_t timestamp, const char *json_payload,
                    const char *source_id)
{
    (void) db;
    (void) key;
    (void) timestamp;
    (void) json_payload;
    (void) source_id;
    return 0;
}

int db_query_latest_tier2(db_handle *db, const char *key, char **out_json, int64_t *out_ts)
{
    (void) db;
    (void) key;
    (void) out_json;
    (void) out_ts;
    return -1;
}

void db_free(void *ptr) { free(ptr); }
int db_maintenance(db_handle *db)
{
    (void) db;
    return 0;
}
/**
 * Check if database is empty (no data rows).
 * Used for bootstrap detection.
 */
int db_is_empty(db_handle *db)
{
    if (!db || !db->fp) return 1;  // Treat invalid as empty

    // Check if file has more than just the header line
    FILE *fp = fopen(db->path, "r");
    if (!fp) return 1;

    // Skip header
    char line[8192];
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return 1;
    }

    // Try to read first data line
    int has_data = (fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);

    return !has_data;  // Empty if no data line
}
