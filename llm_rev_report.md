# LLM Semantic & Architectural Code Review

> **Review Date**: 2026-01-20  
> **Scope**: `src/` and `include/`  
> **Reviewer**: Semantic LLM Review (per `/.agent/workflows/llm_review.md`)

---

## Executive Summary

The HeimWatt codebase demonstrates a **mature architectural foundation** with strong adherence to project standards. The opaque pointer pattern is consistently used, lifecycle pairs are symmetric (`create/destroy`, `init/fini`), and error handling follows the `-errno` convention throughout.

However, this review identified **3 medium-severity** architectural issues and **2 minor** observations that warrant attention. No critical issues were found that would cause data loss or security vulnerabilities under normal operation.

---

## Findings

### 1. 🟡 **Static Buffer Reuse in `plugin_get_provided_types` and `find_providers_for_type`**

| Severity | Standard Violated |
|----------|-------------------|
| **Medium** | [Resource Safety §5.3](file:///home/jimmy/HeimWatt/old_docs/docs/coding_standards.md#L583-590) - Thread Safety |

**Location**: [plugin_mgr.c:606-648](file:///home/jimmy/HeimWatt/src/core/plugin_mgr.c#L606-648), [plugin_mgr.c:654-683](file:///home/jimmy/HeimWatt/src/core/plugin_mgr.c#L654-683)

**Issue**: Both functions return pointers to `static` arrays:

```c
static const char *types[65];  // Line 608
static const char *providers[33];  // Line 656
```

These static buffers are overwritten on each call, creating a **race condition** if the system ever becomes multi-threaded. Additionally, calling `plugin_get_provided_types` from within a loop while iterating over `find_providers_for_type` corrupts the returned data:

```c
for (int i = 0; i < mgr->count && count < 32; i++) {
    const char **types = plugin_get_provided_types(h);  // Overwrites static buffer
    // Inner loop uses `types` which may be corrupted by next iteration
}
```

**Recommendation**: Either:
1. Pass a caller-owned buffer as an output parameter, or
2. Return dynamically allocated memory (caller frees), or
3. Accept the limitation and **document** that these are not thread-safe and must not be nested.

---

### 2. 🟡 **Missing `strncpy` NULL-Termination Guarantee in HTTP Server**

| Severity | Standard Violated |
|----------|-------------------|
| **Medium** | [Safe Functions §5.4](file:///home/jimmy/HeimWatt/old_docs/docs/coding_standards.md#L675-693) - Banned Functions |

**Location**: [http_server.c:643-644](file:///home/jimmy/HeimWatt/src/net/http_server.c#L643-644)

**Issue**: The async pending request handling uses `strncpy` without guaranteed null-termination:

```c
strncpy(srv->pending[srv->pending_count].request_id, conn->request_id, REQUEST_ID_LEN);
```

If `conn->request_id` is exactly `REQUEST_ID_LEN` bytes, the destination is not null-terminated. Coding standards explicitly list `strncpy()` as discouraged due to this behavior.

**Recommendation**: Use `snprintf` for safe copying:
```c
snprintf(srv->pending[srv->pending_count].request_id, REQUEST_ID_LEN, "%s", conn->request_id);
```

---

### 3. 🟡 **Potential Unbounded IPC Read in SDK**

| Severity | Standard Violated |
|----------|-------------------|
| **Medium** | [Resource Safety](file:///home/jimmy/HeimWatt/old_docs/docs/coding_standards.md#L578) - Defensive Programming |

**Location**: [lifecycle.c:275-283](file:///home/jimmy/HeimWatt/src/sdk/lifecycle.c#L275-283)

**Issue**: The SDK's IPC read does not use the buffered read pattern implemented in the core IPC module:

```c
ssize_t n = read(ctx->ipc_fd, buf, sizeof(buf) - 1);
// ...
cJSON *json = cJSON_Parse(buf);
```

This assumes a complete JSON message arrives in a single read. Unlike the core's `ipc_conn_recv` which buffers and looks for newline delimiters, the SDK may receive partial messages or concatenated messages.

**Recommendation**: Port the buffered read logic from `ipc.c:ipc_conn_recv` to the SDK, or use the `ctx->ipc_buf` / `ipc_rpos` / `ipc_wpos` fields that are already defined in `sdk_internal.h` but not utilized.

---

### 4. 🟢 **Duplicate Logging Initialization in `heimwatt_init`**

| Severity | Standard Violated |
|----------|-------------------|
| **Minor** | [Core Principles §1](file:///home/jimmy/HeimWatt/old_docs/docs/coding_standards.md#L16-22) - Single Responsibility |

**Location**: [server.c:315-346](file:///home/jimmy/HeimWatt/src/server.c#L315-346)

**Issue**: Logging is initialized twice:

```c
// Line 316-322
log_set_level(LOG_INFO);
FILE *log_fp = fopen("heimwatt.log", "a");
if (log_fp) {
    log_add_fp(log_fp, LOG_TRACE);
    ctx->log_file = log_fp;
}

// Line 335-346 (again)
log_set_level(LOG_INFO);  // Duplicate
char log_path[256];
snprintf(log_path, sizeof(log_path), "%s/heimwatt.log", path);
ctx->log_file = fopen(log_path, "a");  // Overwrites previous handle!
```

The first log file handle opened at line 317 leaks when overwritten at line 341.

**Recommendation**: Remove the first logging block (lines 316-322) since the second one at line 335+ uses the proper path-based location.

---

### 5. 🟢 **Forward Declaration Duplication in `http_server.c`**

| Severity | Standard Violated |
|----------|-------------------|
| **Minor** | Code Quality |

**Location**: [http_server.c:108-109](file:///home/jimmy/HeimWatt/src/net/http_server.c#L108-109)

**Issue**: `conn_alloc` is forward-declared twice:

```c
static http_conn *conn_alloc(http_server *srv);
static http_conn *conn_alloc(http_server *srv);  // Duplicate
```

**Recommendation**: Remove the duplicate declaration.

---

## Positive Observations

The following patterns demonstrate strong adherence to the coding standards:

### ✅ Opaque Pointer Pattern

All major modules correctly use forward declarations in headers with struct definitions hidden in `.c` files:

- `heimwatt_ctx` in `server.h` / `server.c`
- `plugin_mgr`, `plugin_handle` in `plugin_mgr.h` / `plugin_mgr.c`
- `ipc_server`, `ipc_conn` in `ipc.h` / `ipc.c`
- `http_server` in `http_server.h` / `http_server.c`
- `db_handle` in `db.h` / `csv_backend.c`
- `config` in `config.h` / `config.c`
- `plugin_ctx` in `heimwatt_sdk.h` / `sdk_internal.h`

### ✅ Symmetric Lifecycle Pairs

All `create/destroy` pairs correctly use double-pointers to NULL the caller's handle:

```c
void heimwatt_destroy(heimwatt_ctx **ctx_ptr);
void plugin_mgr_destroy(plugin_mgr **mgr);
void ipc_server_destroy(ipc_server **srv_ptr);
void http_server_destroy(http_server **srv);
void config_destroy(config **cfg);
void sdk_destroy(plugin_ctx **ctx_ptr);
```

### ✅ GOTO-Cleanup Pattern

Multi-resource functions correctly use centralized cleanup:

- [ipc_server_init](file:///home/jimmy/HeimWatt/src/core/ipc.c#L41-89)
- [http_server_run](file:///home/jimmy/HeimWatt/src/net/http_server.c#L300-413)
- [sdk_create](file:///home/jimmy/HeimWatt/src/sdk/lifecycle.c#L16-55)

### ✅ Const Correctness

Public APIs consistently use `const` for read-only inputs:

```c
const char *plugin_handle_id(const plugin_handle *h);
int db_query_latest_tier1(db_handle *db, semantic_type type, double *out_val, int64_t *out_ts);
```

### ✅ Thread-Safe Atomics for State

The HTTP server and main context use `<stdatomic.h>` correctly:

```c
atomic_int running;  // In http_server and heimwatt_ctx
atomic_int ref_count;  // In http_conn
```

---

## Summary

| Category | Count |
|----------|-------|
| 🔴 Critical | 0 |
| 🟡 Medium | 3 |
| 🟢 Minor | 2 |

The codebase is well-structured and follows a consistent architectural philosophy. The identified issues are localized and can be addressed without major refactoring.

---

## Changelog Entry

> The following should be appended to `CHANGELOG.md` if this review marks completion of a unit of work:

```
2026-01-20 18:59: - LLM semantic review completed. Identified 3 medium-severity issues: static buffer reuse in plugin metadata accessors, strncpy null-termination risk, SDK IPC buffering gap. 2 minor issues: duplicate log init, duplicate forward decl.
```
