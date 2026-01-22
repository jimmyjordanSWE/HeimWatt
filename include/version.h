#ifndef HEIMWATT_VERSION_H
#define HEIMWATT_VERSION_H

/*
 * @file version.h
 * @brief API Version Information
 *
 * Version constants for API compatibility checking.
 * Essential for IPC protocol versioning between Core and plugins.
 *
 * ## Versioning Scheme
 *
 * - MAJOR: Breaking API changes (plugins must recompile)
 * - MINOR: Backwards-compatible additions
 * - PATCH: Bug fixes only
 *
 * ## IPC Protocol Version
 *
 * The IPC_VERSION is sent in the HELLO handshake.
 * Core will reject plugins with incompatible IPC versions.
 */

/* ============================================================================
 * Version Numbers
 * ============================================================================ */

/* Major version (breaking changes) */
#define HEIMWATT_VERSION_MAJOR 0

/* Minor version (backwards-compatible additions) */
#define HEIMWATT_VERSION_MINOR 1

/* Patch version (bug fixes) */
#define HEIMWATT_VERSION_PATCH 0

/* Version string */
#define HEIMWATT_VERSION_STRING "0.1.0"

/* ============================================================================
 * IPC Protocol Version
 * ============================================================================ */

/*
 * IPC protocol version.
 * Increment when message format changes.
 *
 * - v1: Initial protocol
 */
#define HEIMWATT_IPC_VERSION 1

/*
 * Minimum IPC version supported.
 * Older plugins will be rejected.
 */
#define HEIMWATT_IPC_VERSION_MIN 1

/* ============================================================================
 * Compile-Time Compatibility
 * ============================================================================ */

/*
 * SDK version that plugins were compiled against.
 * Plugins should call HEIMWATT_CHECK_SDK_VERSION() in their init.
 */
#define HEIMWATT_SDK_VERSION \
    ((HEIMWATT_VERSION_MAJOR << 16) | (HEIMWATT_VERSION_MINOR << 8) | HEIMWATT_VERSION_PATCH)

/*
 * Check SDK version compatibility at runtime.
 * Returns 0 if compatible, -1 if not.
 *
 * @param compiled_version The SDK version the plugin was compiled with.
 * @return 0 if compatible, -1 if major version mismatch.
 */
static inline int heimwatt_check_version(int compiled_version) {
    int compiled_major = (compiled_version >> 16) & 0xFF;
    int runtime_major = HEIMWATT_VERSION_MAJOR;
    return (compiled_major == runtime_major) ? 0 : -1;
}

/*
 * Macro for plugins to verify SDK compatibility.
 * Place at start of plugin's main function.
 */
#define HEIMWATT_CHECK_SDK_VERSION() heimwatt_check_version(HEIMWATT_SDK_VERSION)

#endif /* HEIMWATT_VERSION_H */
