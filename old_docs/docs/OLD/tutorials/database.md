# Database Tutorial

This guide covers usage of the HeimWatt Database Layer (`src/db/`).

## Basic Operations

```c
#include "db/sqlite.h"
#include "db/schema.h"
#include "db/queries.h"

int main(void) {
    db_conn *conn;
    
    // 1. Open database
    if (db_open(&conn, "./heimwatt.db") != DB_OK) {
        fprintf(stderr, "Failed to open database\n");
        return 1;
    }
    
    // 2. Initialize schema (safe to call repeatedly)
    if (schema_init(conn) != DB_OK) {
        fprintf(stderr, "Schema init failed: %s\n", db_errmsg(conn));
        db_close(&conn);
        return 1;
    }
    
    // 3. Insert Data (Tier 1)
    query_insert_tier1(conn, 
                       SEM_ATMOSPHERE_TEMPERATURE,
                       time(NULL),
                       15.5,
                       NULL,  // Not monetary
                       "com.heimwatt.smhi");
    
    // 4. Query Latest Value
    double value;
    int64_t ts;
    char currency[4];
    char source[256];
    
    if (query_select_latest_tier1(conn, SEM_ATMOSPHERE_TEMPERATURE,
                                   &value, &ts, currency, source) == DB_OK) {
        printf("Temperature: %.1f°C from %s\n", value, source);
    }
    
    db_close(&conn);
    return 0;
}
```

## Range Queries

Querying historical data is optimized with prepared statements.

```c
double *values;
int64_t *timestamps;
char **currencies;
size_t count;

int64_t now = time(NULL);
int64_t yesterday = now - 86400;

if (query_select_range_tier1(conn, SEM_ENERGY_PRICE_SPOT,
                              yesterday, now,
                              &values, &timestamps, &currencies,
                              &count) == DB_OK) {
    for (size_t i = 0; i < count; i++) {
        printf("Price at %lld: %.2f %s\n", 
               timestamps[i], values[i], 
               currencies[i] ? currencies[i] : "???");
    }
    
    // Important: Free the result arrays
    query_free_range_tier1(values, timestamps, currencies, count);
}
```

## Transactions

Always use transactions for batch operations to ensure performance and integrity.

```c
db_begin(conn);

for (int i = 0; i < 1000; i++) {
    query_insert_tier1(conn, SEM_ATMOSPHERE_TEMPERATURE,
                       base_ts + i * 3600, temps[i], NULL, source);
}

if (/* success */) {
    db_commit(conn);
} else {
    db_rollback(conn);
}
```
