# HeimWatt Changelog

All notable changes to this project will be documented in this file.

---

## 2026-01-20

### Reviews & Analysis
- **18:59**: LLM semantic review completed. Identified 3 medium-severity issues: static buffer reuse in plugin metadata accessors, strncpy null-termination risk, SDK IPC buffering gap. 2 minor issues: duplicate log init, duplicate forward decl.

### Fixes
- **19:14**: Fixed all issues from LLM review:
  - `plugin_mgr.c/h`: Refactored `plugin_get_provided_types` and `find_providers_for_type` to use caller-owned buffers (thread-safe, nesting-safe)
  - `http_server.c`: Replaced `strncpy` with `snprintf` for request_id copy
  - `sdk/config.c`: Replaced `strncpy`/`strcpy` with `memcpy` + explicit null termination
  - `sdk/state.c`: Replaced `strncpy` with `memcpy` + explicit null termination  
  - `http_parse.c`: Replaced `sprintf` with bounded `snprintf`
  - `csv_backend.c`: Replaced `atof` with `strtod` for proper error detection
  - `sdk/lifecycle.c`: Now uses buffered `sdk_ipc_recv` for atomic IPC message framing
  - `server.c`: Removed duplicate logging initialization that leaked file handle
  - `http_server.c`: Removed duplicate forward declaration of `conn_alloc`
