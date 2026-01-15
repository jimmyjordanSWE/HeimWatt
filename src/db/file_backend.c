// #define _GNU_SOURCE (Defined in Makefile)
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "libs/log.h"

// ============================================================================
// Hash Table for O(1) Timestamp Lookup
// ============================================================================

// Simple hash table entry
typedef struct ts_entry
{
    int64_t timestamp;
    struct ts_entry *next;
} ts_entry;

enum
{
    HASH_BUCKETS = 4096,      // Power of 2 for fast modulo
    MAX_SEMANTIC_TYPES = 256  // Max number of semantic types to track
};

typedef struct
{
    ts_entry *buckets[HASH_BUCKETS];
    size_t count;
    bool loaded;
} ts_index;

// ============================================================================
// Internal Types
// ============================================================================

struct db_handle
{
    char *base_dir;
    char last_error[256];

    // In-memory timestamp indexes for O(1) existence checks
    ts_index *indexes[MAX_SEMANTIC_TYPES];
};

static void set_error(db_handle *db, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(db->last_error, sizeof(db->last_error), fmt, args);
    va_end(args);
}

// Hash function for 64-bit timestamps
static inline size_t hash_ts(int64_t ts)
{
    // Simple but effective hash
    uint64_t x = (uint64_t) ts;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x & (HASH_BUCKETS - 1);
}

// Create new timestamp index
static ts_index *ts_index_create(void)
{
    ts_index *idx = calloc(1, sizeof(*idx));
    return idx;
}

// Destroy timestamp index
static void ts_index_destroy(ts_index *idx)
{
    if (!idx) return;

    for (size_t i = 0; i < HASH_BUCKETS; i++)
    {
        ts_entry *e = idx->buckets[i];
        while (e)
        {
            ts_entry *next = e->next;
            free(e);
            e = next;
        }
    }
    free(idx);
}

// Insert timestamp into index
static int ts_index_insert(ts_index *idx, int64_t ts)
{
    if (!idx) return -1;

    size_t bucket = hash_ts(ts);

    // Check if already exists
    ts_entry *e = idx->buckets[bucket];
    while (e)
    {
        if (e->timestamp == ts) return 0;  // Already exists
        e = e->next;
    }

    // Insert new entry
    ts_entry *new_entry = malloc(sizeof(*new_entry));
    if (!new_entry) return -ENOMEM;

    new_entry->timestamp = ts;
    new_entry->next = idx->buckets[bucket];
    idx->buckets[bucket] = new_entry;
    idx->count++;

    return 0;
}

// Check if timestamp exists in index
static bool ts_index_contains(ts_index *idx, int64_t ts)
{
    if (!idx) return false;

    size_t bucket = hash_ts(ts);
    ts_entry *e = idx->buckets[bucket];

    while (e)
    {
        if (e->timestamp == ts) return true;
        e = e->next;
    }

    return false;
}

// Load index from file (lazy loading)
static int ts_index_load_from_file(ts_index *idx, const char *path)
{
    if (!idx || idx->loaded) return 0;

    FILE *f = fopen(path, "r");
    if (!f)
    {
        idx->loaded = true;  // No file = empty index
        return 0;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int64_t ts;
        if (sscanf(line, "%ld", &ts) == 1)
        {
            ts_index_insert(idx, ts);
        }
    }

    fclose(f);
    idx->loaded = true;
    log_debug("[DB] Loaded %zu timestamps from %s", idx->count, path);
    return 0;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Recursive mkdir
static int ensure_dir(const char *path)
{
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) < 0 && errno != EEXIST) return -1;
    return 0;
}

// Get path for semantic type log
static int get_tier1_path(db_handle *db, semantic_type type, char *out, size_t size)
{
    const semantic_meta *meta = semantic_get_meta(type);
    if (!meta)
    {
        set_error(db, "Invalid semantic type");
        return -1;
    }
    snprintf(out, size, "%s/tier1/%s.log", db->base_dir, meta->id);
    return 0;
}

// Get or create index for semantic type
static ts_index *get_or_create_index(db_handle *db, semantic_type type, const char *path)
{
    if ((int) type < 0 || (int) type >= MAX_SEMANTIC_TYPES) return NULL;

    if (!db->indexes[type])
    {
        db->indexes[type] = ts_index_create();
        if (db->indexes[type])
        {
            ts_index_load_from_file(db->indexes[type], path);
        }
    }

    return db->indexes[type];
}

// ============================================================================
// Lifecycle
// ============================================================================

int db_open(db_handle **db_out, const char *path)
{
    int ret = 0;
    db_handle *db = NULL;

    if (!db_out || !path) return -EINVAL;

    db = malloc(sizeof(*db));
    if (!db) return -ENOMEM;
    memset(db, 0, sizeof(*db));

    db->base_dir = strdup(path);
    if (!db->base_dir)
    {
        ret = -ENOMEM;
        goto cleanup;
    }

    // Create directories
    if (ensure_dir(db->base_dir) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    char buf[1024];
    (void) snprintf(buf, sizeof(buf), "%s/tier1", db->base_dir);
    if (ensure_dir(buf) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    (void) snprintf(buf, sizeof(buf), "%s/tier2", db->base_dir);
    if (ensure_dir(buf) < 0)
    {
        ret = -errno;
        goto cleanup;
    }

    log_info("[DB] Opened: %s (with in-memory index)", path);
    *db_out = db;
    return 0;

cleanup:
    db_close(&db);
    return ret;
}

void db_close(db_handle **db_ptr)
{
    if (!db_ptr || !*db_ptr) return;
    db_handle *db = *db_ptr;

    // Free all indexes
    for (int i = 0; i < MAX_SEMANTIC_TYPES; i++)
    {
        if (db->indexes[i])
        {
            ts_index_destroy(db->indexes[i]);
        }
    }

    free(db->base_dir);
    free(db);
    *db_ptr = NULL;
}

const char *db_error_message(const db_handle *db) { return db ? db->last_error : "Invalid Handle"; }

// ============================================================================
// Tier 1: Time-Series Data with O(1) Existence Check
// ============================================================================

int db_insert_tier1(db_handle *db, semantic_type type, int64_t timestamp, double value,
                    const char *currency, const char *source_id)
{
    if (!db) return -EINVAL;

    char path[1024];
    if (get_tier1_path(db, type, path, sizeof(path)) < 0) return -EINVAL;

    // O(1) check using in-memory index
    ts_index *idx = get_or_create_index(db, type, path);
    if (idx && ts_index_contains(idx, timestamp))
    {
        return -EEXIST;  // Already cached
    }

    // Append to file
    FILE *f = fopen(path, "a");
    if (!f)
    {
        set_error(db, "Failed to open %s: %s", path, strerror(errno));
        return -errno;
    }

    (void) fprintf(f, "%ld\t%.6f\t%s\t%s\n", timestamp, value, currency ? currency : "-",
                   source_id ? source_id : "-");
    (void) fclose(f);

    // Update index
    if (idx)
    {
        ts_index_insert(idx, timestamp);
    }

    return 0;
}

int db_query_latest_tier1(db_handle *db, semantic_type type, double *out_val, int64_t *out_ts)
{
    if (!db || !out_val || !out_ts) return -EINVAL;

    char path[1024];
    if (get_tier1_path(db, type, path, sizeof(path)) < 0) return -EINVAL;

    FILE *f = fopen(path, "r");
    if (!f)
    {
        return -ENOENT;
    }

    // Read from end would be ideal but requires seeking
    // For now, scan all (TODO: optimize with indexed file format)
    char line[1024];
    int64_t last_ts = -1;
    double last_val = 0;
    int found = 0;

    while (fgets(line, sizeof(line), f))
    {
        int64_t ts;
        double val;
        if (sscanf(line, "%ld\t%lf", &ts, &val) == 2)
        {
            last_ts = ts;
            last_val = val;
            found = 1;
        }
    }
    (void) fclose(f);

    if (!found) return -ENOENT;

    *out_val = last_val;
    *out_ts = last_ts;
    return 0;
}

int db_query_range_tier1(db_handle *db, semantic_type type, int64_t from_ts, int64_t to_ts,
                         double **out_values, int64_t **out_ts_arr, size_t *out_count)
{
    if (!db || !out_values || !out_ts_arr || !out_count) return -EINVAL;

    char path[1024];
    if (get_tier1_path(db, type, path, sizeof(path)) < 0) return -EINVAL;

    FILE *f = fopen(path, "r");
    if (!f)
    {
        *out_count = 0;
        *out_values = NULL;
        *out_ts_arr = NULL;
        return 0;  // Empty is success
    }

    // Single-pass: collect matching entries into dynamic arrays
    size_t capacity = 128;
    size_t count = 0;
    double *vals = malloc(capacity * sizeof(double));
    int64_t *tss = malloc(capacity * sizeof(int64_t));

    if (!vals || !tss)
    {
        free(vals);
        free(tss);
        fclose(f);
        return -ENOMEM;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        int64_t ts;
        double val;
        if (sscanf(line, "%ld\t%lf", &ts, &val) == 2)
        {
            if (ts >= from_ts && ts <= to_ts)
            {
                // Grow arrays if needed
                if (count >= capacity)
                {
                    capacity *= 2;
                    double *new_vals = realloc(vals, capacity * sizeof(double));
                    int64_t *new_tss = realloc(tss, capacity * sizeof(int64_t));
                    if (!new_vals || !new_tss)
                    {
                        // Free the new pointers if they succeeded, else the old ones
                        free(new_vals ? new_vals : vals);
                        free(new_tss ? new_tss : tss);
                        fclose(f);
                        return -ENOMEM;
                    }
                    vals = new_vals;
                    tss = new_tss;
                }

                vals[count] = val;
                tss[count] = ts;
                count++;
            }
        }
    }
    (void) fclose(f);

    *out_values = vals;
    *out_ts_arr = tss;
    *out_count = count;
    return 0;
}

int db_query_point_exists_tier1(db_handle *db, semantic_type type, int64_t timestamp)
{
    if (!db) return -EINVAL;

    char path[1024];
    if (get_tier1_path(db, type, path, sizeof(path)) < 0) return -EINVAL;

    // O(1) check using in-memory index
    ts_index *idx = get_or_create_index(db, type, path);
    if (idx)
    {
        return ts_index_contains(idx, timestamp) ? 1 : 0;
    }

    // Fallback to file scan if index failed
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f))
    {
        int64_t ts;
        if (sscanf(line, "%ld", &ts) == 1)
        {
            if (ts == timestamp)
            {
                found = 1;
                break;
            }
        }
    }
    (void) fclose(f);
    return found;
}

// ============================================================================
// Tier 2 (Stubs)
// ============================================================================

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
    return -1;  // Not impl
}

// ============================================================================
// Maintenance
// ============================================================================

void db_free(void *ptr) { free(ptr); }

int db_prune_tier1(db_handle *db, semantic_type type, int64_t before_ts)
{
    (void) db;
    (void) type;
    (void) before_ts;
    return 0;
}

int db_maintenance(db_handle *db)
{
    (void) db;
    return 0;
}
