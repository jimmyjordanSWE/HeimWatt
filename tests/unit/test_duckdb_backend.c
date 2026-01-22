/*
 * @file test_duckdb_backend.c
 * @brief Unit tests for DuckDB backend
 */

#include "db.h"
#include "semantic_types.h"

#include <stdlib.h>
#include <unistd.h>

#include "core/config.h"
#include "libs/unity/unity.h"

static char test_db_path[256];
static db_handle *db = NULL;
static config *cfg = NULL;

void duckdb_backend_setUp(void) {
    // Unique DB file
    snprintf(test_db_path, sizeof(test_db_path), "/tmp/heimwatt_test_duck.db");
    unlink(test_db_path); /* Ensure start fresh */

    cfg = config_create();
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(0, config_add_backend(cfg, "duckdb", test_db_path, true));

    int ret = db_open(&db, cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret, "db_open failed (is DuckDB lib available?)");
    TEST_ASSERT_NOT_NULL(db);
}

void duckdb_backend_tearDown(void) {
    if (db) {
        db_close(&db);
    }
    if (cfg) {
        config_destroy(&cfg);
    }
    unlink(test_db_path);
}

void test_duckdb_insert_query(void) {
    int ret = db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 1000, 25.5, NULL, "test");
    TEST_ASSERT_EQUAL_INT(0, ret);

    double val = 0;
    int64_t ts = 0;
    ret = db_query_latest_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, &val, &ts);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_DOUBLE(25.5, val);
    TEST_ASSERT_EQUAL_INT64(1000, ts);
}

void test_duckdb_persistence(void) {
    TEST_ASSERT_EQUAL_INT(0,
                          db_insert_tier1(db, SEM_ENERGY_PRICE_SPOT, 2000, 100.0, "EUR", "test"));
    db_close(&db);
    db = NULL;

    /* Reopen */
    int ret = db_open(&db, cfg);
    TEST_ASSERT_EQUAL_INT(0, ret);

    double val = 0;
    int64_t ts = 0;
    ret = db_query_latest_tier1(db, SEM_ENERGY_PRICE_SPOT, &val, &ts);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_DOUBLE(100.0, val);
}

void test_duckdb_query_range(void) {
    /* Insert 3 points */
    db_insert_tier1(db, SEM_SOLAR_GHI, 100, 10.0, NULL, "t");
    db_insert_tier1(db, SEM_SOLAR_GHI, 200, 20.0, NULL, "t");
    db_insert_tier1(db, SEM_SOLAR_GHI, 300, 30.0, NULL, "t");

    double *vals = NULL;
    int64_t *tss = NULL;
    size_t count = 0;

    int ret = db_query_range_tier1(db, SEM_SOLAR_GHI, 150, 350, &vals, &tss, &count);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT(2, count);

    TEST_ASSERT_EQUAL_DOUBLE(20.0, vals[0]);
    TEST_ASSERT_EQUAL_INT64(200, tss[0]);
    TEST_ASSERT_EQUAL_DOUBLE(30.0, vals[1]);
    TEST_ASSERT_EQUAL_INT64(300, tss[1]);

    db_free(vals);
    db_free(tss);
}
