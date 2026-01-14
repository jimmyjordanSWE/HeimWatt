#ifndef HEIMWATT_SDK_H
#define HEIMWATT_SDK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "semantic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Types
// ============================================================================

/**
 * Opaque handle to the plugin connection context.
 * Represents the connection to the HeimWatt Core.
 */
typedef struct plugin_ctx plugin_ctx;

/**
 * Log levels for sdk_log()
 */
typedef enum { SDK_LOG_DEBUG, SDK_LOG_INFO, SDK_LOG_WARN, SDK_LOG_ERROR } sdk_log_level;

// ============================================================================
// Lifecycle Management
// ============================================================================

/**
 * Create and initialize the SDK.
 * Allocates and parses command-line args passed by Core (socket paths, IDs).
 * Must be the first call in main().
 *
 * @param ctx_out Pointer to receive the allocated context.
 * @param argc    Argument count from main.
 * @param argv    Argument vector from main.
 * @return 0 on success, negative errno on failure.
 */
int sdk_create(plugin_ctx** ctx_out, int argc, char** argv);

/**
 * Main event loop.
 * Blocks until the Core sends a shutdown signal or a fatal error occurs.
 *
 * @param ctx The active plugin context.
 * @return 0 on clear shutdown, non-zero on error.
 */
int sdk_run(plugin_ctx* ctx);

/**
 * Destroy SDK and free all resources.
 * Should be called before returning from main().
 *
 * @param ctx_ptr Pointer to the context pointer (will be set to NULL).
 */
void sdk_destroy(plugin_ctx** ctx_ptr);

/**
 * Log a message to the centralized Core log.
 */
void sdk_log(plugin_ctx* ctx, sdk_log_level level, const char* fmt, ...);

// ============================================================================
// Data Reporting API (Inbound / Data Plugins)
// ============================================================================

typedef struct {
    // REQUIRED: What kind of data is this?
    semantic_type semantic;

    // REQUIRED: The raw value in the CANONICAL UNIT for the type (see semantic_types.h).
    double value;

    // OPTIONAL: Time of measurement (Unix epoch). 0 = Now.
    int64_t timestamp;

    // OPTIONAL: Currency code (ISO 4217, e.g., "SEK", "EUR").
    // MUST be provided if the semantic type implies monetary value (e.g. ENERGY_PRICE).
    const char* currency;

    // OPTIONAL: Constraints
    double floor_val;
    bool has_floor;

    double cap_val;
    bool has_cap;

} sdk_metric;

/**
 * Report a standardized metric to Core.
 * Validates the semantic type and unit compliance (via documentation contract).
 */
int sdk_report(plugin_ctx* ctx, const sdk_metric* metric);

/**
 * Report raw extension data (Tier 2).
 * For use cases not yet covered by Semantic Types.
 *
 * @param key       String ID (e.g. "myvendor.soil_radon")
 * @param json_fmt  Printf-style format string for the JSON payload
 */
int sdk_report_raw(plugin_ctx* ctx, const char* key, int64_t timestamp, const char* json_fmt, ...);

// ============================================================================
// Calculator API (Outbound / Calculator Plugins)
// ============================================================================

// Forward declarations for API handling
typedef struct sdk_req sdk_req;
typedef struct sdk_resp sdk_resp;

/**
 * Callback function signature for HTTP API handlers.
 */
typedef int (*sdk_api_handler)(plugin_ctx* ctx, const sdk_req* req, sdk_resp* resp);

/**
 * Register an HTTP endpoint.
 *
 * @param method  "GET", "POST", etc.
 * @param path    "/api/my-calc/plan"
 * @param handler Function to call
 */
int sdk_register_endpoint(plugin_ctx* ctx, const char* method, const char* path,
                          sdk_api_handler handler);

/**
 * Declare a dependency on a semantic type.
 * Should be called during initialization.
 * Core uses this to valid configuration health.
 */
int sdk_require_semantic(plugin_ctx* ctx, semantic_type type);

// -- Data Querying --

typedef struct {
    int64_t timestamp;
    double value;
    char currency[4];  // Empty string if not monetary
} sdk_data_point;

/**
 * Get the most recent value for a type.
 * @return 0 found, -1 not found.
 */
int sdk_query_latest(plugin_ctx* ctx, semantic_type type, sdk_data_point* out);

/**
 * Get history.
 * Caller must free the *out_array using sdk_data_point_destroy().
 */
int sdk_query_history(plugin_ctx* ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                      sdk_data_point** out_array, size_t* out_count);

/**
 * Free a data point array.
 * @param points_ptr Pointer to the array pointer (will be set to NULL).
 */
void sdk_data_point_destroy(sdk_data_point** points_ptr);

// ============================================================================
// Response Helpers
// ============================================================================

void sdk_resp_set_status(sdk_resp* resp, int code);  // 200, 400, etc.
void sdk_resp_set_json(sdk_resp* resp, const char* json_body);
void sdk_resp_set_header(sdk_resp* resp, const char* key, const char* val);

#ifdef __cplusplus
}
#endif

#endif  // HEIMWATT_SDK_H
