/*
 * @file sdk_query.h
 * @brief Query implementation (internal)
 *
 * Internal query functions.
 */

#ifndef HEIMWATT_SDK_QUERY_H
#define HEIMWATT_SDK_QUERY_H

#include "heimwatt_sdk.h"

#include <errno.h>
#include <stddef.h>

/*
 * Send latest query and wait for response.
 *
 * @param ctx  Plugin context
 * @param type Semantic type to query
 * @param out  Output data point
 * @return 0 on success, negative errno if not found or error
 */
int sdk_query_send_latest(plugin_ctx* ctx, semantic_type type, sdk_data_point* out);

/*
 * Send range query and wait for response.
 *
 * @param ctx     Plugin context
 * @param type    Semantic type to query
 * @param from_ts Start timestamp
 * @param to_ts   End timestamp
 * @param out     Output data point array
 * @param count   Output count
 * @return 0 on success, negative errno on error
 */
int sdk_query_send_range(plugin_ctx* ctx, semantic_type type, int64_t from_ts, int64_t to_ts,
                         sdk_data_point** out, size_t* count);

#endif  // HEIMWATT_SDK_QUERY_H */
