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
 * Initialize the SDK.
 * Parses command-line args passed by Core (socket paths, IDs).
 * Must be the first call in main().
 *
 * @param ctx_out Pointer to receive the allocated context.
 * @param argc    Argument count from main.
 * @param argv    Argument vector from main.
 * @return 0 on success, non-zero error code on failure.
 */
int sdk_init(plugin_ctx** ctx_out, int argc, char** argv);

/**
 * Main event loop.
 * Blocks until the Core sends a shutdown signal or a fatal error occurs.
 *
 * @param ctx The active plugin context.
 * @return 0 on clear shutdown, non-zero on error.
 */
int sdk_run(plugin_ctx* ctx);

/**
 * Cleanup and free SDK resources.
 * Should be called before returning from main().
 *
 * @param ctx_ptr Pointer to the context pointer (will be set to NULL).
 */
void sdk_fini(plugin_ctx** ctx_ptr);

/**
 * Log a message to the centralized Core log.
 */
void sdk_log(plugin_ctx* ctx, sdk_log_level level, const char* fmt, ...);

// ============================================================================
// Data Reporting API (Inbound / Data Plugins)
// ============================================================================

/**
 * Data metric structure.
 * Designed for C99 designated initializers to act as a "Builder".
 *
 * Usage:
 *   sdk_report(ctx, &(sdk_metric_t){
 *       .semantic = SEM_ATMOSPHERE_TEMPERATURE,
 *       .value = 23.5,
 *       .floor = -40.0, // Optional constraints
 *       .cap = 60.0
 *   });
 */
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

    // OPTIONAL: Constraints (use NAN to indicate unset if manual init,
    // but 0.0 is valid for floor/cap, so we might need flags or just rely on sane defaults logic).
    // Note: The SDK currently treats 0.0 as a value, so for optional constraints
    // we might prefer separate fields or flags. For simplicity in V1,
    // we assume the user sets these only if needed.
    // To explicitly disable, we can't use NAN checks easily in C99 struct init without header
    // macros. For now, let's assume specific "has_xxx" flags or rely on builder helpers if needed
    // later. A simpler approach for C structs:
    double floor_val;
    bool has_floor;

    double cap_val;
    bool has_cap;

} sdk_metric_t;

/**
 * Report a standardized metric to Core.
 * Validates the semantic type and unit compliance (via documentation contract).
 */
int sdk_report(plugin_ctx* ctx, const sdk_metric_t* metric);

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
typedef int (*sdk_api_handler_t)(plugin_ctx* ctx, const sdk_req* req, sdk_resp* resp);

/**
 * Register an HTTP endpoint.
 *
 * @param method  "GET", "POST", etc.
 * @param path    "/api/my-calc/plan"
 * @param handler Function to call
 */
int sdk_register_endpoint(plugin_ctx* ctx, const char* method, const char* path,
                          sdk_api_handler_t handler);

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
} sdk_data_point_t;

/**
 * Get the most recent value for a type.
 * @return 0 found, -1 not found.
 */
int sdk_query_latest(plugin_ctx* ctx, semantic_type type, sdk_data_point_t* out);

/**
 * Get history.
 * Caller must free the *out_array using sdk_free_points().
 */
int sdk_query_history(plugin_ctx* ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                      sdk_data_point_t** out_array, size_t* out_count);

void sdk_free_points(sdk_data_point_t* points);

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
