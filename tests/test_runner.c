/**
 * @file test_runner.c
 * @brief Main test runner for all unit tests
 */

#include "libs/unity/unity.h"

// Module-specific setup/teardown
extern void file_backend_setUp(void);
extern void file_backend_tearDown(void);
extern void plugin_mgr_setUp(void);
extern void plugin_mgr_tearDown(void);
extern void duckdb_backend_setUp(void);
extern void duckdb_backend_tearDown(void);
extern void test_duckdb_insert_query(void);
extern void test_duckdb_persistence(void);
extern void test_duckdb_query_range(void);

static int current_test_group = 0;
enum
{
    GROUP_DEFAULT = 0,
    GROUP_FILE_BACKEND,
    GROUP_PLUGIN_MGR,
    GROUP_DUCKDB_BACKEND
};

void setUp(void)
{
    switch (current_test_group)
    {
        case GROUP_FILE_BACKEND:
            file_backend_setUp();
            break;
        case GROUP_PLUGIN_MGR:
            plugin_mgr_setUp();
            break;
        case GROUP_DUCKDB_BACKEND:
            duckdb_backend_setUp();
            break;
        default:
            break;
    }
}

void tearDown(void)
{
    switch (current_test_group)
    {
        case GROUP_FILE_BACKEND:
            file_backend_tearDown();
            break;
        case GROUP_PLUGIN_MGR:
            plugin_mgr_tearDown();
            break;
        case GROUP_DUCKDB_BACKEND:
            duckdb_backend_tearDown();
            break;
        default:
            break;
    }
}

// --- test_http_parse.c ---
extern void test_http_parse_simple_get(void);
extern void test_http_parse_get_with_query(void);
extern void test_http_parse_with_headers(void);
extern void test_http_parse_post_method(void);
extern void test_http_parse_malformed_no_crlf(void);
extern void test_http_parse_null_input(void);
extern void test_http_serialize_response_basic(void);

// --- test_json.c ---
extern void test_json_parse_object(void);
extern void test_json_parse_array(void);
extern void test_json_parse_nested(void);
extern void test_json_parse_invalid(void);
extern void test_json_stringify_roundtrip(void);
extern void test_json_type_checks(void);

// --- test_semantic_types.c ---
extern void test_semantic_from_string_valid(void);
extern void test_semantic_from_string_invalid(void);
extern void test_semantic_get_meta_valid(void);
extern void test_semantic_get_meta_unknown(void);
extern void test_semantic_type_count(void);

// --- test_file_backend.c ---
extern void test_db_open_close(void);
extern void test_db_open_invalid_path(void);
extern void test_db_insert_single(void);
extern void test_db_insert_duplicate(void);
extern void test_db_insert_with_currency(void);
extern void test_db_query_latest(void);
extern void test_db_query_latest_empty(void);
extern void test_db_point_exists(void);
extern void test_db_index_persistence(void);

// --- test_ipc.c ---
extern void test_ipc_server_init_destroy(void);
extern void test_ipc_server_init_null_params(void);
extern void test_ipc_conn_send_recv_roundtrip(void);
extern void test_ipc_conn_plugin_id(void);
extern void test_ipc_conn_recv_null_params(void);
extern void test_ipc_conn_send_null_params(void);
extern void test_ipc_conn_fd_null(void);
extern void test_ipc_server_fd_null(void);

// --- test_plugin_mgr.c ---
extern void test_plugin_mgr_init_destroy(void);
extern void test_plugin_mgr_init_null_params(void);
extern void test_plugin_mgr_scan_empty_dir(void);
extern void test_plugin_mgr_scan_with_manifest(void);
extern void test_plugin_mgr_get_nonexistent(void);
extern void test_plugin_mgr_manifest_capabilities(void);

// --- test_http_server.c ---
extern void test_http_server_create_destroy(void);
extern void test_http_server_create_null_output(void);
extern void test_http_server_set_timeout(void);
extern void test_http_server_set_max_connections(void);
extern void test_http_server_set_handler(void);
extern void test_http_server_not_running_initially(void);
extern void test_http_server_port_assigned(void);

// --- test_lps.c ---
extern void test_lps_solver_create_destroy(void);
extern void test_lps_solution_create_destroy(void);
extern void test_lps_solution_create_invalid(void);
extern void test_lps_validate_null_problem(void);
extern void test_lps_validate_null_arrays(void);
extern void test_lps_validate_invalid_battery(void);
extern void test_lps_validate_invalid_efficiency(void);
extern void test_lps_validate_valid_problem(void);
extern void test_lps_no_battery_case(void);
extern void test_lps_constant_price_with_solar(void);
extern void test_lps_price_arbitrage(void);
extern void test_lps_excess_solar_sell(void);
extern void test_lps_single_period(void);
extern void test_lps_full_battery_start(void);
extern void test_lps_storm_mode(void);
extern void test_lps_performance_48h(void);

// --- test_log_ring.c ---
extern void test_ring_buffer_basics(void);
extern void test_ring_buffer_overwrite(void);
extern void test_ring_buffer_json(void);

int main(void)
{
    UNITY_BEGIN();

    // ======== HTTP Parser Tests ========
    RUN_TEST(test_http_parse_simple_get);
    RUN_TEST(test_http_parse_get_with_query);
    RUN_TEST(test_http_parse_with_headers);
    RUN_TEST(test_http_parse_post_method);
    RUN_TEST(test_http_parse_malformed_no_crlf);
    RUN_TEST(test_http_parse_null_input);
    RUN_TEST(test_http_serialize_response_basic);

    // ======== JSON Tests ========
    RUN_TEST(test_json_parse_object);
    RUN_TEST(test_json_parse_array);
    RUN_TEST(test_json_parse_nested);
    RUN_TEST(test_json_parse_invalid);
    RUN_TEST(test_json_stringify_roundtrip);
    RUN_TEST(test_json_type_checks);

    // ======== Semantic Types Tests ========
    RUN_TEST(test_semantic_from_string_valid);
    RUN_TEST(test_semantic_from_string_invalid);
    RUN_TEST(test_semantic_get_meta_valid);
    RUN_TEST(test_semantic_get_meta_unknown);
    RUN_TEST(test_semantic_type_count);

    // ======== File Backend Tests ========
    current_test_group = GROUP_FILE_BACKEND;
    RUN_TEST(test_db_open_close);
    RUN_TEST(test_db_open_invalid_path);
    RUN_TEST(test_db_insert_single);
    RUN_TEST(test_db_insert_duplicate);
    RUN_TEST(test_db_insert_with_currency);
    RUN_TEST(test_db_query_latest);
    RUN_TEST(test_db_query_latest_empty);
    RUN_TEST(test_db_point_exists);
    RUN_TEST(test_db_index_persistence);
    current_test_group = GROUP_DEFAULT;

    // ======== IPC Tests ========
    RUN_TEST(test_ipc_server_init_destroy);
    RUN_TEST(test_ipc_server_init_null_params);
    RUN_TEST(test_ipc_conn_send_recv_roundtrip);
    RUN_TEST(test_ipc_conn_plugin_id);
    RUN_TEST(test_ipc_conn_recv_null_params);
    RUN_TEST(test_ipc_conn_send_null_params);
    RUN_TEST(test_ipc_conn_fd_null);
    RUN_TEST(test_ipc_server_fd_null);

    // ======== Plugin Manager Tests ========
    current_test_group = GROUP_PLUGIN_MGR;
    RUN_TEST(test_plugin_mgr_init_destroy);
    RUN_TEST(test_plugin_mgr_init_null_params);
    RUN_TEST(test_plugin_mgr_scan_empty_dir);
    RUN_TEST(test_plugin_mgr_scan_with_manifest);
    RUN_TEST(test_plugin_mgr_get_nonexistent);
    RUN_TEST(test_plugin_mgr_manifest_capabilities);
    current_test_group = GROUP_DEFAULT;

    // ======== HTTP Server Tests ========
    RUN_TEST(test_http_server_create_destroy);
    RUN_TEST(test_http_server_create_null_output);
    RUN_TEST(test_http_server_set_timeout);
    RUN_TEST(test_http_server_set_max_connections);
    RUN_TEST(test_http_server_set_handler);
    RUN_TEST(test_http_server_not_running_initially);
    RUN_TEST(test_http_server_port_assigned);

    // ======== LPS Solver Tests ========
    RUN_TEST(test_lps_solver_create_destroy);
    RUN_TEST(test_lps_solution_create_destroy);
    RUN_TEST(test_lps_solution_create_invalid);
    RUN_TEST(test_lps_validate_null_problem);
    RUN_TEST(test_lps_validate_null_arrays);
    RUN_TEST(test_lps_validate_invalid_battery);
    RUN_TEST(test_lps_validate_invalid_efficiency);
    RUN_TEST(test_lps_validate_valid_problem);
    RUN_TEST(test_lps_no_battery_case);
    RUN_TEST(test_lps_constant_price_with_solar);
    RUN_TEST(test_lps_price_arbitrage);
    RUN_TEST(test_lps_excess_solar_sell);
    RUN_TEST(test_lps_single_period);
    RUN_TEST(test_lps_full_battery_start);
    RUN_TEST(test_lps_storm_mode);
    RUN_TEST(test_lps_performance_48h);

    // ======== DuckDB Tests ========
    current_test_group = GROUP_DUCKDB_BACKEND;
    RUN_TEST(test_duckdb_insert_query);
    RUN_TEST(test_duckdb_persistence);
    RUN_TEST(test_duckdb_query_range);
    current_test_group = GROUP_DEFAULT;

    // ======== Log Ring Buffer Tests ========
    RUN_TEST(test_ring_buffer_basics);
    RUN_TEST(test_ring_buffer_overwrite);
    RUN_TEST(test_ring_buffer_json);

    return UNITY_END();
}
