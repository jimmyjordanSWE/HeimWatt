/**
 * @file schema.h
 * @brief Database schema management
 *
 * Table creation and migrations.
 */

#ifndef HEIMWATT_SCHEMA_H
#define HEIMWATT_SCHEMA_H

#include "sqlite.h"

/** Current schema version */
#define SCHEMA_VERSION_CURRENT 1

/**
 * Initialize schema (creates tables if not exist).
 * Idempotent: safe to call on every startup.
 *
 * @param conn Database connection
 * @return DB_OK on success, DB_ERROR on failure
 */
int schema_init(db_conn *conn);

/**
 * Get current schema version.
 *
 * @param conn Database connection
 * @return Schema version number, or -1 on error
 */
int schema_version(db_conn *conn);

/**
 * Migrate to target version.
 *
 * @param conn           Database connection
 * @param target_version Target schema version
 * @return DB_OK on success, DB_ERROR on failure
 */
int schema_migrate(db_conn *conn, int target_version);

#endif /* HEIMWATT_SCHEMA_H */
