/**
 * @file plugin_mgr.h
 * @brief Plugin lifecycle management
 *
 * Discovers, forks, and supervises plugins.
 */

#ifndef HEIMWATT_PLUGIN_MGR_H
#define HEIMWATT_PLUGIN_MGR_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct plugin_mgr plugin_mgr;
typedef struct plugin_handle plugin_handle;

/**
 * Plugin types.
 */
typedef enum
{
    PLUGIN_TYPE_IN, /**< Data plugin (inbound) */
    PLUGIN_TYPE_OUT /**< Calculator plugin (outbound) */
} plugin_type;

/**
 * Plugin states.
 */
typedef enum
{
    PLUGIN_STATE_STOPPED,
    PLUGIN_STATE_STARTING,
    PLUGIN_STATE_RUNNING,
    PLUGIN_STATE_STOPPING,
    PLUGIN_STATE_RESTARTING,
    PLUGIN_STATE_FAILED
} plugin_state;

/* ============================================================
 * MANAGER LIFECYCLE
 * ============================================================ */

/**
 * Initialize plugin manager.
 *
 * @param mgr         Output pointer for manager
 * @param plugins_dir Directory to scan for plugins
 * @param ipc_sock    Path to IPC socket
 * @return 0 on success, -1 on error
 */
int plugin_mgr_init(plugin_mgr **mgr, const char *plugins_dir, const char *ipc_sock);

/**
 * Destroy plugin manager and free resources.
 *
 * @param mgr Pointer to manager (set to NULL on return)
 */
void plugin_mgr_destroy(plugin_mgr **mgr);

/* ============================================================
 * DISCOVERY & LOADING
 * ============================================================ */

/**
 * Scan for plugins (find all manifest.json files).
 *
 * @param mgr Plugin manager
 * @return Number of plugins found, -1 on error
 */
int plugin_mgr_scan(plugin_mgr *mgr);

/**
 * Validate plugin dependencies.
 *
 * @param mgr Plugin manager
 * @return 0 if all valid, -1 if dependency errors
 */
int plugin_mgr_validate(plugin_mgr *mgr);

/**
 * Start all plugins (fork processes).
 *
 * @param mgr Plugin manager
 * @return 0 on success, -1 on error
 */
int plugin_mgr_start_all(plugin_mgr *mgr);

/**
 * Stop all plugins gracefully.
 *
 * @param mgr Plugin manager
 */
void plugin_mgr_stop_all(plugin_mgr *mgr);

/* ============================================================
 * INDIVIDUAL PLUGIN CONTROL
 * ============================================================ */

/**
 * Start a specific plugin.
 *
 * @param mgr       Plugin manager
 * @param plugin_id Plugin identifier
 * @return 0 on success, -1 on error
 */
int plugin_mgr_start(plugin_mgr *mgr, const char *plugin_id);

/**
 * Stop a specific plugin.
 *
 * @param mgr       Plugin manager
 * @param plugin_id Plugin identifier
 * @return 0 on success, -1 on error
 */
int plugin_mgr_stop(plugin_mgr *mgr, const char *plugin_id);

/**
 * Restart a specific plugin.
 *
 * @param mgr       Plugin manager
 * @param plugin_id Plugin identifier
 * @return 0 on success, -1 on error
 */
int plugin_mgr_restart(plugin_mgr *mgr, const char *plugin_id);

/* ============================================================
 * QUERY
 * ============================================================ */

/**
 * Get plugin handle by ID.
 *
 * @param mgr       Plugin manager
 * @param plugin_id Plugin identifier
 * @return Plugin handle or NULL if not found
 */
plugin_handle *plugin_mgr_get(plugin_mgr *mgr, const char *plugin_id);

/**
 * Get plugin state.
 */
plugin_state plugin_handle_state(const plugin_handle *h);

/**
 * Get plugin type.
 */
plugin_type plugin_handle_type(const plugin_handle *h);

/**
 * Get total number of plugins.
 */
int plugin_mgr_count(const plugin_mgr *mgr);

/**
 * Get plugin ID.
 */
const char *plugin_handle_id(const plugin_handle *h);

/**
 * Get plugin process ID.
 */
pid_t plugin_handle_pid(const plugin_handle *h);

/**
 * Get plugin interval in seconds.
 */
int plugin_handle_interval(const plugin_handle *h);

/**
 * Get plugin last run timestamp.
 */
time_t plugin_handle_last_run(const plugin_handle *h);

/**
 * Get plugin resource name.
 */
const char *plugin_handle_resource(const plugin_handle *h);

/**
 * Set plugin last run timestamp.
 *
 * @param mgr       Plugin manager
 * @param plugin_id Plugin identifier
 * @param ts        Timestamp
 */
void plugin_mgr_set_last_run(plugin_mgr *mgr, const char *plugin_id, time_t ts);

/* ============================================================
 * ITERATION
 * ============================================================ */

/**
 * Plugin iteration callback.
 */
typedef void (*plugin_iter_fn)(const plugin_handle *h, void *userdata);

/**
 * Iterate over all plugins.
 *
 * @param mgr      Plugin manager
 * @param fn       Callback function
 * @param userdata User data passed to callback
 */
void plugin_mgr_foreach(plugin_mgr *mgr, plugin_iter_fn fn, void *userdata);

/* ============================================================
 * SUPERVISION
 * ============================================================ */

/**
 * Check plugin health and restart dead plugins.
 * Called from main loop.
 *
 * @param mgr Plugin manager
 * @return Number of plugins restarted, -1 on error
 */
int plugin_mgr_check_health(plugin_mgr *mgr);

#endif /* HEIMWATT_PLUGIN_MGR_H */
