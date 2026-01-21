#ifndef HEIMWATT_IPC_HANDLERS_H
#define HEIMWATT_IPC_HANDLERS_H

#include "core/ipc.h"   /* For ipc_conn */
#include "libs/cJSON.h" /* For cJSON */
#include "server.h"     /* For heimwatt_ctx */

/**
 * @brief Dispatch an IPC command to the appropriate handler.
 *
 * @param ctx Server context
 * @param conn IPC connection source
 * @param json Validated JSON object containing the command
 */
int handle_ipc_command(heimwatt_ctx *ctx, ipc_conn *conn, cJSON *json);

#endif /* HEIMWATT_IPC_HANDLERS_H */
