/**
 * @file test_file_backend.c
 * @brief Unit tests for file-based database backend
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/config.h"
#include "db.h"
#include "libs/unity/unity.h"
#include "semantic_types.h"

static char test_dir[256];
static db_handle *db = NULL;
static config *cfg = NULL;

void file_backend_setUp(void)
{
    // Create unique temp directory for each test
    snprintf(test_dir, sizeof(test_dir), "/tmp/heimwatt_test_XXXXXX");
    char *result = mkdtemp(test_dir);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Failed to create temp dir");

    cfg = config_create();
    TEST_ASSERT_NOT_NULL(cfg);
    TEST_ASSERT_EQUAL_INT(0, config_add_backend(cfg, "csv", test_dir, true));

    int ret = db_open(&db, cfg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, ret, "db_open failed");
    TEST_ASSERT_NOT_NULL(db);
}

void file_backend_tearDown(void)
{
    if (db)
    {
        db_close(&db);
    }
    if (cfg)
    {
        config_destroy(&cfg);
    }
    // Cleanup temp dir
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    (void) system(cmd);
}

// --- db_open/db_close tests ---

void test_db_open_close(void)
{
    // setUp already opened, just verify it worked
    TEST_ASSERT_NOT_NULL(db);
}

void test_db_open_invalid_path(void)
{
    db_handle *bad_db = NULL;
    int ret = db_open(&bad_db, NULL);
    TEST_ASSERT_TRUE(ret < 0);
    TEST_ASSERT_NULL(bad_db);
}

// --- db_insert_tier1 tests ---

void test_db_insert_single(void)
{
    int ret = db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 1000, 22.5, NULL, "test");
    TEST_ASSERT_EQUAL_INT(0, ret);
}

void test_db_insert_duplicate(void)
{
    int ret = db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 2000, 22.5, NULL, "test");
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Same timestamp should fail
    ret = db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 2000, 23.0, NULL, "test");
    TEST_ASSERT_EQUAL_INT(-EEXIST, ret);
}

void test_db_insert_with_currency(void)
{
    int ret = db_insert_tier1(db, SEM_ENERGY_PRICE_SPOT, 3000, 0.85, "SEK", "test");
    TEST_ASSERT_EQUAL_INT(0, ret);
}

// --- db_query_latest_tier1 tests ---

void test_db_query_latest(void)
{
    // Insert multiple points
    TEST_ASSERT_EQUAL_INT(
        0, db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 1000, 20.0, NULL, "test"));
    TEST_ASSERT_EQUAL_INT(
        0, db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 2000, 21.0, NULL, "test"));
    TEST_ASSERT_EQUAL_INT(
        0, db_insert_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, 3000, 22.0, NULL, "test"));

    double val = 0;
    int64_t ts = 0;
    int ret = db_query_latest_tier1(db, SEM_ATMOSPHERE_TEMPERATURE, &val, &ts);

    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_EQUAL_INT64(3000, ts);
    TEST_ASSERT_EQUAL_DOUBLE(22.0, val);
}

void test_db_query_latest_empty(void)
{
    double val = 0;
    int64_t ts = 0;
    int ret = db_query_latest_tier1(db, SEM_SOLAR_GHI, &val, &ts);

    // Should fail - no data for this type
    TEST_ASSERT_TRUE(ret < 0);
}

// --- db_query_point_exists_tier1 tests ---

void test_db_point_exists(void)
{
    TEST_ASSERT_EQUAL_INT(0,
                          db_insert_tier1(db, SEM_ATMOSPHERE_HUMIDITY, 5000, 65.0, NULL, "test"));

    // Exists
    int ret = db_query_point_exists_tier1(db, SEM_ATMOSPHERE_HUMIDITY, 5000);
    TEST_ASSERT_TRUE(ret > 0);

    // Does not exist
    ret = db_query_point_exists_tier1(db, SEM_ATMOSPHERE_HUMIDITY, 9999);
    TEST_ASSERT_EQUAL_INT(0, ret);
}

// --- Index persistence test ---

void test_db_index_persistence(void)
{
    // Insert data
    TEST_ASSERT_EQUAL_INT(
        0, db_insert_tier1(db, SEM_ATMOSPHERE_PRESSURE, 7000, 1013.25, NULL, "test"));

    // Close and reopen
    db_close(&db);
    int ret = db_open(&db, cfg);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Should still find the point after reopen (index reloaded from file)
    ret = db_query_point_exists_tier1(db, SEM_ATMOSPHERE_PRESSURE, 7000);
    TEST_ASSERT_TRUE(ret > 0);
}
