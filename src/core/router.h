/**
 * @file router.h
 * @brief HTTP request routing
 *
 * Maps HTTP paths to plugin IDs for request dispatch.
 */

#ifndef HEIMWATT_ROUTER_H
#define HEIMWATT_ROUTER_H

#include <stdbool.h>

typedef struct router router;

/* ============================================================
 * LIFECYCLE
 * ============================================================ */

/**
 * Initialize router.
 *
 * @param r Output pointer for router
 * @return 0 on success, -1 on error
 */
int router_init(router **r);

/**
 * Destroy router.
 *
 * @param r Pointer to router (set to NULL on return)
 */
void router_destroy(router **r);

/* ============================================================
 * REGISTRATION
 * ============================================================ */

/**
 * Register a route.
 * Called when OUT plugins declare endpoints.
 *
 * @param r         Router
 * @param method    HTTP method (GET, POST, etc.)
 * @param path      URL path
 * @param plugin_id Plugin to handle this route
 * @return 0 on success, -1 on error (duplicate route)
 */
int router_register(router *r, const char *method, const char *path, const char *plugin_id);

/**
 * Unregister all routes for a plugin.
 *
 * @param r         Router
 * @param plugin_id Plugin ID
 * @return 0 on success, -1 on error
 */
int router_unregister(router *r, const char *plugin_id);

/* ============================================================
 * DISPATCH
 * ============================================================ */

/**
 * Look up plugin for a route.
 *
 * @param r      Router
 * @param method HTTP method
 * @param path   URL path
 * @return Plugin ID or NULL if not found
 */
const char *router_lookup(const router *r, const char *method, const char *path);

/**
 * Check if a route exists.
 *
 * @param r      Router
 * @param method HTTP method
 * @param path   URL path
 * @return true if route exists
 */
bool router_has_route(const router *r, const char *method, const char *path);

/* ============================================================
 * DEBUG
 * ============================================================ */

/**
 * Log all registered routes.
 *
 * @param r Router
 */
void router_dump(const router *r);

#endif /* HEIMWATT_ROUTER_H */
