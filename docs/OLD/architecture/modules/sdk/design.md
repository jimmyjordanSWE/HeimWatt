# SDK Design

> **Version**: 3.0 (2026-01-14)  
> **Status**: ✅ API Defined  
> **Scope**: Plugin SDK public API and internal implementation

---

## Overview

The **SDK** (`libheimwatt-sdk.so`) is the library that plugin developers link against. It provides:

1. **Context management**: `sdk_init()`, `sdk_fini()`, `sdk_run()`
2. **Data reporting**: Builder pattern for submitting metrics (IN plugins)
3. **Data querying**: Request data from Core (OUT plugins)
4. **Endpoint registration**: Declare HTTP routes (OUT plugins)
5. **IPC**: Communication with Core (transparent to plugin author)

---

## Directory Structure

```
include/
└── heimwatt_sdk.h      # Public API (shipped to plugin devs)

src/sdk/
├── sdk_core.h / sdk_core.c       # Context lifecycle
├── sdk_report.h / sdk_report.c   # Data reporting
├── sdk_query.h / sdk_query.c     # Data querying
├── sdk_endpoint.h / sdk_endpoint.c # Endpoint registration
└── sdk_ipc.h / sdk_ipc.c         # IPC client
```

---

## Public API: heimwatt_sdk.h

This is the only header plugin developers need.

> **Source**: [include/heimwatt_sdk.h](../../../../include/heimwatt_sdk.h)

---

## Internal Modules

### sdk_core.h

Context structure and lifecycle.

> **Source**: [src/sdk/sdk_core.h](../../../../src/sdk/sdk_core.h)

### sdk_ipc.h

Plugin-side IPC client.

> **Source**: [src/sdk/sdk_ipc.h](../../../../src/sdk/sdk_ipc.h)

### sdk_report.h

Metric builder implementation.

> **Source**: [src/sdk/sdk_report.h](../../../../src/sdk/sdk_report.h)

### sdk_query.h

Query implementation.

> **Source**: [src/sdk/sdk_query.h](../../../../src/sdk/sdk_query.h)

### sdk_endpoint.h

Endpoint registration and dispatch.

> **Source**: [src/sdk/sdk_endpoint.h](../../../../src/sdk/sdk_endpoint.h)

---

## Usage Examples

> **See [Plugin Development Tutorial](../../../../tutorials/plugins.md).**

---

## Error Handling

All SDK functions return:
- `0` on success
- `-1` on error

Use `SDK_ERROR()` to log errors before returning.

---

> **Document Map**:
> - [Architecture Overview](../architecture.md)
> - [Plugin System](../plugins/design.md)
> - [Semantic Types](../../semantic_types_reference.md)
