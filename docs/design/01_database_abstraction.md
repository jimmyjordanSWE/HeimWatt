# Design Study: Time Series Database Abstraction

> **Status**: Draft  
> **Date**: 2026-01-20

## The Goal

Abstract storage so we can swap CSV → TimescaleDB/QuestDB/InfluxDB without changing Core.

---

## Current State

```c
// Scattered file operations in csv_backend.c
FILE *fp = fopen(path, "a");
fprintf(fp, "%s,%.2f,%.2f,...", timestamp, value1, value2);
```

---

## Proposed Interface

```c
// include/db.h

typedef struct db_handle db_handle;

// Lifecycle
int db_open(db_handle **db_out, const char *uri);
void db_close(db_handle **db);

// Write (Tier 1: semantic time-series)
int db_insert(db_handle *db, int64_t ts, semantic_type type, double value);
int db_insert_batch(db_handle *db, const db_point *points, size_t count);

// Read (Tier 1)
int db_query_latest(db_handle *db, semantic_type type, double *value, int64_t *ts);
int db_query_range(db_handle *db, semantic_type type, int64_t start, int64_t end,
                   db_point **points, size_t *count);

// Write (Tier 2: schedules/blobs)
int db_insert_blob(db_handle *db, const char *key, const void *data, size_t len);
int db_query_blob(db_handle *db, const char *key, void **data, size_t *len);

// Maintenance
int db_compact(db_handle *db);
int db_export_csv(db_handle *db, const char *path);
```

---

## Backend Options

| Database | License | Pros | Cons |
|----------|---------|------|------|
| **CSV** (current) | N/A | Simple, portable, Excel-friendly | Slow queries, no indexing |
| **SQLite** | Public Domain | Embedded, reliable | Not optimized for time-series |
| **QuestDB** | Apache 2.0 | Blazing fast TS, SQL | Separate process, 500MB+ memory |
| **TimescaleDB** | TimescaleDB License | PostgreSQL extension, mature | Heavy, requires Postgres |
| **InfluxDB v2** | MIT (v2 OSS) | Purpose-built for TS | Flux query language quirky |
| **DuckDB** | MIT | Embedded, columnar, fast analytics | Newer, less battle-tested |

---

## Recommendation: DuckDB

**Why DuckDB for HeimWatt**:

1. **Embedded** (like SQLite) — no separate process
2. **Columnar storage** — ideal for time-series analytics
3. **MIT License** — fully open source
4. **Parquet export** — industry-standard data format
5. **SQL interface** — familiar, powerful queries
6. **Small footprint** — ~20MB binary, low memory

```c
// Example with DuckDB C API
#include <duckdb.h>

duckdb_database db;
duckdb_open("data/heimwatt.duckdb", &db);

duckdb_connection con;
duckdb_connect(db, &con);

duckdb_query(con, 
    "INSERT INTO readings (ts, type_id, value) VALUES (?, ?, ?)",
    &result);
```

---

## Implementation Plan

### Phase 1: Abstraction Layer (Now)

```c
// src/db/db.c - Dispatches to backend
int db_open(db_handle **db_out, const char *uri) {
    if (strncmp(uri, "csv://", 6) == 0)
        return csv_backend_open(db_out, uri + 6);
    if (strncmp(uri, "duckdb://", 9) == 0)
        return duckdb_backend_open(db_out, uri + 9);
    return -EINVAL;
}
```

### Phase 2: Keep CSV Backend

Refactor current code into `src/db/csv_backend.c` behind the interface.

### Phase 3: Add DuckDB Backend

Implement `src/db/duckdb_backend.c` with same interface.

### Phase 4: Config Switch

```json
{
  "storage": {
    "uri": "duckdb://data/heimwatt.duckdb"
  }
}
```

---

## Schema for Time-Series

```sql
-- DuckDB schema
CREATE TABLE readings (
    ts         TIMESTAMP NOT NULL,
    type_id    SMALLINT NOT NULL,   -- semantic_type enum
    value      DOUBLE,
    currency   VARCHAR(3),          -- For price types
    PRIMARY KEY (ts, type_id)
);

-- Schedules as JSON blobs
CREATE TABLE schedules (
    key        VARCHAR PRIMARY KEY,
    data       JSON,
    created_at TIMESTAMP DEFAULT current_timestamp
);

-- Efficient time-range queries
CREATE INDEX idx_readings_ts ON readings (ts);
```
