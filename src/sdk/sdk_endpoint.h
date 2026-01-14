/**
 * @file sdk_endpoint.h
 * @brief Endpoint registration (internal)
 *
 * Internal endpoint registration and dispatch functions.
 */

#ifndef SDK_ENDPOINT_H
#define SDK_ENDPOINT_H

#include "heimwatt_sdk.h"

/**
 * Register endpoint with Core via IPC.
 *
 * @param ctx    Plugin context
 * @param method HTTP method
 * @param path   URL path
 * @return 0 on success, -1 on error
 */
int sdk_endpoint_register(plugin_ctx* ctx, const char* method, const char* path);

/**
 * Handle incoming HTTP_REQUEST message.
 *
 * @param ctx          Plugin context
 * @param request_json Raw JSON request from Core
 * @return 0 on success, -1 on error
 */
int sdk_endpoint_dispatch(plugin_ctx* ctx, const char* request_json);

#endif /* SDK_ENDPOINT_H */
