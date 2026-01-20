/**
 * @file test_plugin_mgr.c
 * @brief Unit tests for plugin manager
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/plugin_mgr.h"
#include "libs/unity/unity.h"

static char test_dir[512];
static char test_sock[512];

void plugin_mgr_setUp(void)
{
    snprintf(test_dir, sizeof(test_dir), "/tmp/heimwatt_plugin_test_XXXXXX");
    char *result = mkdtemp(test_dir);
    TEST_ASSERT_NOT_NULL_MESSAGE(result, "Failed to create temp dir");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(test_sock, sizeof(test_sock), "%s/ipc.sock", test_dir);
#pragma GCC diagnostic pop
}

void plugin_mgr_tearDown(void)
{
    char cmd[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
#pragma GCC diagnostic pop
    (void) system(cmd);
}

// --- Lifecycle Tests ---

void test_plugin_mgr_init_destroy(void)
{
    plugin_mgr *mgr = NULL;
    int ret = plugin_mgr_init(&mgr, test_dir, test_sock);
    TEST_ASSERT_EQUAL_INT(0, ret);
    TEST_ASSERT_NOT_NULL(mgr);

    plugin_mgr_destroy(&mgr);
    TEST_ASSERT_NULL(mgr);

    // Double destroy should be safe
    plugin_mgr_destroy(&mgr);
}

void test_plugin_mgr_init_null_params(void)
{
    plugin_mgr *mgr = NULL;

    int ret = plugin_mgr_init(NULL, test_dir, test_sock);
    TEST_ASSERT_TRUE(ret < 0);

    ret = plugin_mgr_init(&mgr, NULL, test_sock);
    TEST_ASSERT_TRUE(ret < 0);

    ret = plugin_mgr_init(&mgr, test_dir, NULL);
    TEST_ASSERT_TRUE(ret < 0);
}

// --- Scan Tests ---

void test_plugin_mgr_scan_empty_dir(void)
{
    plugin_mgr *mgr = NULL;
    int ret = plugin_mgr_init(&mgr, test_dir, test_sock);
    TEST_ASSERT_EQUAL_INT(0, ret);

    // Scan should succeed but find nothing
    ret = plugin_mgr_scan(mgr);
    TEST_ASSERT_TRUE(ret >= 0);

    plugin_mgr_destroy(&mgr);
}

void test_plugin_mgr_scan_with_manifest(void)
{
    // Create subdirectories for plugins
    char in_dir[1024], plugin_dir[1024], manifest_path[1024];
    snprintf(in_dir, sizeof(in_dir), "%s/in", test_dir);
    mkdir(in_dir, 0755);
    snprintf(plugin_dir, sizeof(plugin_dir), "%s/in/test_plugin", test_dir);
    mkdir(plugin_dir, 0755);

    // Create minimal manifest.json
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", plugin_dir);
#pragma GCC diagnostic pop
    FILE *f = fopen(manifest_path, "w");
    TEST_ASSERT_NOT_NULL(f);
    fprintf(f, "{\"id\": \"com.test.plugin\", \"type\": \"in\", \"version\": \"1.0.0\"}");
    fclose(f);

    plugin_mgr *mgr = NULL;
    int ret = plugin_mgr_init(&mgr, test_dir, test_sock);
    TEST_ASSERT_EQUAL_INT(0, ret);

    ret = plugin_mgr_scan(mgr);
    TEST_ASSERT_TRUE(ret >= 0);

    plugin_mgr_destroy(&mgr);
}

// --- Query Tests ---

void test_plugin_mgr_get_nonexistent(void)
{
    plugin_mgr *mgr = NULL;
    int ret = plugin_mgr_init(&mgr, test_dir, test_sock);
    TEST_ASSERT_EQUAL_INT(0, ret);

    plugin_handle *h = plugin_mgr_get(mgr, "nonexistent.plugin");
    TEST_ASSERT_NULL(h);

    plugin_mgr_destroy(&mgr);
}
