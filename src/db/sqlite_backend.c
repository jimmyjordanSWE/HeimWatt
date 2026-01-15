/**
 * @file sqlite_backend.c
 * @brief SQLite Database Backend Implementation
 */

// _GNU_SOURCE is defined in Makefile
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "db.h"
#include "libs/log.h"
#include "libs/sqlite3.h"

// Internal handle structure
struct db_handle
{
    sqlite3 *conn;
    char last_error[256];
};

static void set_error(db_handle *db, const char *fmt, ...)
{
    if (db)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(db->last_error, sizeof(db->last_error), fmt, args);
        va_end(args);
    }
}

const char *db_error_message(const db_handle *db) { return db ? db->last_error : NULL; }

static int init_schema(db_handle *db)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS tier1_data ("
        "  timestamp INTEGER NOT NULL,"
        "  type INTEGER NOT NULL,"
        "  value REAL NOT NULL,"
        "  currency TEXT,"
        "  source_id TEXT,"
        "  PRIMARY KEY (timestamp, type)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_tier1_ts_type ON tier1_data(timestamp, type);"
        "CREATE TABLE IF NOT EXISTS tier2_data ("
        "  key TEXT NOT NULL,"
        "  timestamp INTEGER NOT NULL,"
        "  json TEXT NOT NULL,"
        "  source_id TEXT,"
        "  PRIMARY KEY (key, timestamp)"
        ");";

    char *err_msg = NULL;
    int rc = sqlite3_exec(db->conn, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Schema init failed: %s", err_msg);
        sqlite3_free(err_msg);
        return -EIO;
    }
    return 0;
}

int db_open(db_handle **db_out, const char *path)
{
    if (!db_out || !path) return -EINVAL;

    // Ensure directory exists
    struct stat st = {0};
    // Extract dir
    char *dir = strdup(path);
    if (!dir) return -ENOMEM;

    char *slash = strrchr(dir, '/');
    if (slash)
    {
        *slash = 0;
        if (stat(dir, &st) == -1)
        {
            if (mkdir(dir, 0755) < 0 && errno != EEXIST)
            {
                free(dir);
                return -errno;
            }
        }
    }
    free(dir);

    // Full path for DB file
    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s/heimwatt.db", path);

    db_handle *db = malloc(sizeof(*db));
    if (!db) return -ENOMEM;
    memset(db, 0, sizeof(*db));

    int rc = sqlite3_open(db_path, &db->conn);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Failed to open SQLite: %s", sqlite3_errmsg(db->conn));
        sqlite3_close(db->conn);
        free(db);
        return -EIO;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(db->conn, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(db->conn, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    int ret = init_schema(db);
    if (ret != 0)
    {
        db_close(&db);
        return ret;
    }

    *db_out = db;
    return 0;
}

void db_close(db_handle **db_ptr)
{
    if (!db_ptr || !*db_ptr) return;
    db_handle *db = *db_ptr;

    if (db->conn)
    {
        sqlite3_close(db->conn);
    }
    free(db);
    *db_ptr = NULL;
}

int db_insert_tier1(db_handle *db, semantic_type type, int64_t timestamp, double value,
                    const char *currency, const char *source_id)
{
    if (!db) return -EINVAL;

    const char *sql =
        "INSERT INTO tier1_data (timestamp, type, value, currency, source_id) "
        "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Prepare failed: %s", sqlite3_errmsg(db->conn));
        return -EIO;
    }

    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_int(stmt, 2, type);
    sqlite3_bind_double(stmt, 3, value);
    if (currency)
        sqlite3_bind_text(stmt, 4, currency, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 4);

    if (source_id)
        sqlite3_bind_text(stmt, 5, source_id, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 5);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
    {
        if (rc == SQLITE_CONSTRAINT)
        {
            sqlite3_finalize(stmt);
            return -EEXIST;
        }
        set_error(db, "Insert failed: %s", sqlite3_errmsg(db->conn));
        return -EIO;
    }

    return 0;
}

int db_query_latest_tier1(db_handle *db, semantic_type type, double *out_val, int64_t *out_ts)
{
    if (!db || !out_val || !out_ts) return -EINVAL;

    const char *sql =
        "SELECT value, timestamp FROM tier1_data WHERE type = ? ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -EIO;

    sqlite3_bind_int(stmt, 1, type);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        *out_val = sqlite3_column_double(stmt, 0);
        *out_ts = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -2;  // DB_NOT_FOUND generic mapping (todo: define proper codes)
}

int db_query_range_tier1(db_handle *db, semantic_type type, int64_t from_ts, int64_t to_ts,
                         double **out_values, int64_t **out_ts, size_t *out_count)
{
    if (!db || !out_values || !out_ts || !out_count) return -EINVAL;

    const char *sql =
        "SELECT value, timestamp FROM tier1_data WHERE type = ? AND timestamp BETWEEN ? AND ? "
        "ORDER BY timestamp ASC;";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Prepare failed: %s", sqlite3_errmsg(db->conn));
        return -EIO;
    }

    sqlite3_bind_int(stmt, 1, type);
    sqlite3_bind_int64(stmt, 2, from_ts);
    sqlite3_bind_int64(stmt, 3, to_ts);

    // Initial allocation estimate (can grow)
    size_t cap = 256;
    size_t count = 0;
    double *vals = malloc(cap * sizeof(double));
    int64_t *tss = malloc(cap * sizeof(int64_t));

    if (!vals || !tss)
    {
        free(vals);
        free(tss);
        sqlite3_finalize(stmt);
        return -ENOMEM;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if (count >= cap)
        {
            size_t new_cap = cap * 2;
            double *v_tmp = realloc(vals, new_cap * sizeof(double));
            if (!v_tmp)
            {
                free(vals);
                free(tss);
                sqlite3_finalize(stmt);
                return -ENOMEM;
            }
            vals = v_tmp;

            int64_t *t_tmp = realloc(tss, new_cap * sizeof(int64_t));
            if (!t_tmp)
            {
                free(vals);
                free(tss);
                sqlite3_finalize(stmt);
                return -ENOMEM;
            }
            tss = t_tmp;
            cap = new_cap;
        }

        vals[count] = sqlite3_column_double(stmt, 0);
        tss[count] = sqlite3_column_int64(stmt, 1);
        count++;
    }

    sqlite3_finalize(stmt);

    *out_values = vals;
    *out_ts = tss;
    *out_count = count;
    return 0;
}

int db_query_point_exists_tier1(db_handle *db, semantic_type type, int64_t timestamp)
{
    if (!db) return -EINVAL;
    const char *sql = "SELECT 1 FROM tier1_data WHERE type = ? AND timestamp = ? LIMIT 1;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -EIO;

    sqlite3_bind_int(stmt, 1, type);
    sqlite3_bind_int64(stmt, 2, timestamp);

    rc = sqlite3_step(stmt);
    int exists = (rc == SQLITE_ROW) ? 1 : 0;
    sqlite3_finalize(stmt);
    return exists;
}

int db_insert_tier2(db_handle *db, const char *key, int64_t timestamp, const char *json_payload,
                    const char *source_id)
{
    if (!db || !key || !json_payload) return -EINVAL;

    const char *sql =
        "INSERT OR REPLACE INTO tier2_data (key, timestamp, json, source_id) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Prepare failed: %s", sqlite3_errmsg(db->conn));
        return -EIO;
    }

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, timestamp);
    sqlite3_bind_text(stmt, 3, json_payload, -1, SQLITE_STATIC);
    if (source_id)
        sqlite3_bind_text(stmt, 4, source_id, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 4);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -EIO;
    return 0;
}

int db_query_latest_tier2(db_handle *db, const char *key, char **out_json, int64_t *out_ts)
{
    if (!db || !key || !out_json || !out_ts) return -EINVAL;

    const char *sql =
        "SELECT json, timestamp FROM tier2_data WHERE key = ? ORDER BY timestamp DESC LIMIT 1;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -EIO;

    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        *out_json = strdup((const char *) sqlite3_column_text(stmt, 0));
        *out_ts = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);
        return *out_json ? 0 : -ENOMEM;
    }
    sqlite3_finalize(stmt);
    return -2;  // Not found
}

void db_free(void *ptr) { free(ptr); }

int db_prune_tier1(db_handle *db, semantic_type type, int64_t before_ts)
{
    if (!db) return -EINVAL;
    char sql[256];

    if (type == SEM_UNKNOWN)
    {
        snprintf(sql, sizeof(sql), "DELETE FROM tier1_data WHERE timestamp < %ld;", before_ts);
    }
    else
    {
        snprintf(sql, sizeof(sql), "DELETE FROM tier1_data WHERE type = %d AND timestamp < %ld;",
                 type, before_ts);
    }

    char *err = NULL;
    int rc = sqlite3_exec(db->conn, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK)
    {
        set_error(db, "Prune failed: %s", err);
        sqlite3_free(err);
        return -EIO;
    }
    return sqlite3_changes(db->conn);
}

int db_maintenance(db_handle *db)
{
    if (!db) return -EINVAL;
    sqlite3_exec(db->conn, "VACUUM;", NULL, NULL, NULL);
    return 0;
}
