/**
 * @file sqlite.h
 * @brief SQLite wrapper
 *
 * Low-level wrapper around SQLite3. Hides SQLite types from the rest
 * of the codebase.
 */

#ifndef HEIMWATT_SQLITE_H
#define HEIMWATT_SQLITE_H

#include <stddef.h>
#include <stdint.h>

typedef struct db_conn db_conn;
typedef struct db_stmt db_stmt;

/* Error codes (compatible with SQLite) */
#define DB_OK 0
#define DB_ERROR 1
#define DB_ROW 100  /**< sqlite3_step() has another row */
#define DB_DONE 101 /**< sqlite3_step() is complete */

/* Column type constants */
#define DB_TYPE_INTEGER 1
#define DB_TYPE_FLOAT 2
#define DB_TYPE_TEXT 3
#define DB_TYPE_BLOB 4
#define DB_TYPE_NULL 5

/* ============================================================
 * CONNECTION
 * ============================================================ */

/**
 * Create database connection (creates if not exists).
 *
 * @param conn Output pointer for connection
 * @param path Path to database file
 * @return DB_OK on success, DB_ERROR on failure
 */
int db_create(db_conn** conn, const char* path);

/**
 * Destroy database connection.
 *
 * @param conn Pointer to connection (set to NULL on return)
 */
void db_destroy(db_conn** conn);

/**
 * Execute SQL without result (for DDL, inserts).
 *
 * @param conn Connection
 * @param sql  SQL statement
 * @return DB_OK on success, DB_ERROR on failure
 */
int db_exec(db_conn* conn, const char* sql);

/**
 * Get last error message.
 *
 * @param conn Connection
 * @return Error message string
 */
const char* db_errmsg(const db_conn* conn);

/**
 * Get last insert rowid.
 *
 * @param conn Connection
 * @return Row ID of last insert
 */
int64_t db_last_insert_id(const db_conn* conn);

/**
 * Get affected row count.
 *
 * @param conn Connection
 * @return Number of rows affected by last statement
 */
int db_changes(const db_conn* conn);

/* ============================================================
 * PREPARED STATEMENTS
 * ============================================================ */

/**
 * Prepare a statement.
 *
 * @param conn Connection
 * @param sql  SQL statement
 * @param stmt Output pointer for statement
 * @return DB_OK on success, DB_ERROR on failure
 */
int db_prepare(db_conn* conn, const char* sql, db_stmt** stmt);

/**
 * Finalize (free) a statement.
 *
 * @param stmt Pointer to statement (set to NULL on return)
 */
void db_finalize(db_stmt** stmt);

/**
 * Reset statement for re-use.
 *
 * @param stmt Statement
 * @return DB_OK on success
 */
int db_reset(db_stmt* stmt);

/**
 * Clear bindings.
 *
 * @param stmt Statement
 * @return DB_OK on success
 */
int db_clear_bindings(db_stmt* stmt);

/* ============================================================
 * BINDING (1-indexed)
 * ============================================================ */

int db_bind_int(db_stmt* stmt, int idx, int64_t val);
int db_bind_double(db_stmt* stmt, int idx, double val);
int db_bind_text(db_stmt* stmt, int idx, const char* val);
int db_bind_blob(db_stmt* stmt, int idx, const void* data, size_t len);
int db_bind_null(db_stmt* stmt, int idx);

/* ============================================================
 * EXECUTION
 * ============================================================ */

/**
 * Step through results.
 *
 * @param stmt Statement
 * @return DB_ROW (more data), DB_DONE (finished), or DB_ERROR
 */
int db_step(db_stmt* stmt);

/* ============================================================
 * COLUMN ACCESS (0-indexed, after db_step returns DB_ROW)
 * ============================================================ */

int db_column_count(const db_stmt* stmt);
int db_column_type(const db_stmt* stmt, int col);
int64_t db_column_int(const db_stmt* stmt, int col);
double db_column_double(const db_stmt* stmt, int col);
const char* db_column_text(const db_stmt* stmt, int col);
const void* db_column_blob(const db_stmt* stmt, int col, size_t* len);
int db_column_is_null(const db_stmt* stmt, int col);

/* ============================================================
 * TRANSACTIONS
 * ============================================================ */

int db_begin(db_conn* conn);
int db_commit(db_conn* conn);
int db_rollback(db_conn* conn);

#endif /* HEIMWATT_SQLITE_H */
