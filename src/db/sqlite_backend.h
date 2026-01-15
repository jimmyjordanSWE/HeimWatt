/**
 * @file sqlite_backend.h
 * @brief SQLite Database Backend (Internal)
 *
 * SQLite-specific implementation of the database interface.
 * This header is internal and should not be included by plugins.
 *
 * The public API is defined in include/db.h.
 */

#ifndef HEIMWATT_SQLITE_BACKEND_H
#define HEIMWATT_SQLITE_BACKEND_H

#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * Internal Types
 * ============================================================================
 *
 * These types are used only within the SQLite backend implementation.
 * External code uses the opaque db_handle from include/db.h.
 */

/**
 * SQLite connection wrapper.
 * Hides sqlite3* from the rest of the codebase.
 */
typedef struct db_conn db_conn;

/**
 * SQLite prepared statement wrapper.
 * Hides sqlite3_stmt* from the rest of the codebase.
 */
typedef struct db_stmt db_stmt;

/* ============================================================================
 * SQLite-Specific Error Codes
 * ============================================================================
 *
 * These map to SQLite return codes and are used internally.
 * The public API uses db_result from include/db.h.
 */

#define SQLITE_OK 0
#define SQLITE_ERROR 1
#define SQLITE_ROW 100
#define SQLITE_DONE 101

/* Column type constants */
#define SQLITE_TYPE_INTEGER 1
#define SQLITE_TYPE_FLOAT 2
#define SQLITE_TYPE_TEXT 3
#define SQLITE_TYPE_BLOB 4
#define SQLITE_TYPE_NULL 5

/* ============================================================================
 * Low-Level Connection API
 * ============================================================================ */

/**
 * Create a raw SQLite connection.
 *
 * @param conn Output pointer for connection
 * @param path Path to database file
 * @return SQLITE_OK on success, SQLITE_ERROR on failure
 *
 * @note Prefer db_open() from include/db.h for normal use.
 */
int sqlite_conn_create(db_conn **conn, const char *path);

/**
 * Destroy SQLite connection.
 *
 * @param conn Pointer to connection (set to NULL on return)
 */
void sqlite_conn_destroy(db_conn **conn);

/**
 * Execute SQL without result (for DDL, inserts).
 *
 * @param conn Connection
 * @param sql  SQL statement
 * @return SQLITE_OK on success, SQLITE_ERROR on failure
 */
int sqlite_exec(db_conn *conn, const char *sql);

/**
 * Get last error message.
 *
 * @param conn Connection
 * @return Error message string
 */
const char *sqlite_errmsg(const db_conn *conn);

/**
 * Get last insert rowid.
 */
int64_t sqlite_last_insert_id(const db_conn *conn);

/**
 * Get affected row count.
 */
int sqlite_changes(const db_conn *conn);

/* ============================================================================
 * Prepared Statements
 * ============================================================================ */

int sqlite_prepare(db_conn *conn, const char *sql, db_stmt **stmt);
void sqlite_finalize(db_stmt **stmt);
int sqlite_reset(db_stmt *stmt);
int sqlite_clear_bindings(db_stmt *stmt);

/* ============================================================================
 * Binding (1-indexed)
 * ============================================================================ */

int sqlite_bind_int(db_stmt *stmt, int idx, int64_t val);
int sqlite_bind_double(db_stmt *stmt, int idx, double val);
int sqlite_bind_text(db_stmt *stmt, int idx, const char *val);
int sqlite_bind_blob(db_stmt *stmt, int idx, const void *data, size_t len);
int sqlite_bind_null(db_stmt *stmt, int idx);

/* ============================================================================
 * Execution
 * ============================================================================ */

/**
 * Step through results.
 *
 * @param stmt Statement
 * @return SQLITE_ROW (more data), SQLITE_DONE (finished), or SQLITE_ERROR
 */
int sqlite_step(db_stmt *stmt);

/* ============================================================================
 * Column Access (0-indexed, after sqlite_step returns SQLITE_ROW)
 * ============================================================================ */

int sqlite_column_count(const db_stmt *stmt);
int sqlite_column_type(const db_stmt *stmt, int col);
int64_t sqlite_column_int(const db_stmt *stmt, int col);
double sqlite_column_double(const db_stmt *stmt, int col);
const char *sqlite_column_text(const db_stmt *stmt, int col);
const void *sqlite_column_blob(const db_stmt *stmt, int col, size_t *len);
int sqlite_column_is_null(const db_stmt *stmt, int col);

/* ============================================================================
 * Transactions
 * ============================================================================ */

int sqlite_begin(db_conn *conn);
int sqlite_commit(db_conn *conn);
int sqlite_rollback(db_conn *conn);

#endif /* HEIMWATT_SQLITE_BACKEND_H */
