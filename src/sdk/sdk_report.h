/**
 * @file sdk_report.h
 * @brief Metric reporting implementation (internal)
 *
 * Internal functions for metric reporting.
 */

#ifndef SDK_REPORT_H
#define SDK_REPORT_H

#include <stddef.h>

#include "heimwatt_sdk.h"

/**
 * Validate metric before sending.
 *
 * @param m Metric
 * @return 0 if valid, -1 if invalid
 */
int sdk_metric_validate(const sdk_metric_t* m);

/**
 * Serialize metric to IPC JSON message.
 *
 * @param m Metric
 * @return JSON string (caller frees) or NULL on error
 */
char* sdk_metric_to_json(const sdk_metric_t* m);

#endif /* SDK_REPORT_H */
