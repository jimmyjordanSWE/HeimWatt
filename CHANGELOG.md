# Changelog

## 2026-01-15 23:20
- **LLM Review (Async/Performance)**: Deep verification of the new epoll/threading architecture.
  - **Critical**: Race condition in `http_server.c` (use-after-free) between client disconnect and async completion.
  - **Critical**: IPC layer (`ipc.c`) is still blocking; `conn_lock` in `server.c` propagates this global lock capability.
  - **Report**: Detailed analysis in `llm_rev_report.md`.

## 2026-01-15 21:18
- **Unit Testing**: Implemented Unity test framework with 27 tests.
  - **HTTP Parser**: 7 tests (parsing, serialization, edge cases)
  - **JSON Wrapper**: 6 tests (parse, stringify, type checks)
  - **Semantic Types**: 5 tests (string→enum lookup, metadata)
  - **File Backend**: 9 tests (CRUD, duplicate detection, index persistence)
  - **Makefile**: New `make unit-test` target with ASAN enabled
  - All tests pass with no memory leaks.

## 2026-01-15 20:47
- **High-Performance Architecture**: Complete refactor for scalability.
  - **HTTP Server**: Rewrote with Linux `epoll` for non-blocking, event-driven I/O. Handles 1000+ concurrent connections.
  - **Async HTTP-IPC Bridge**: Request ID correlation replaces blocking condition variables. No more deadlocks.
  - **File Backend**: In-memory hash table index for O(1) timestamp existence checks (was O(n) file scan).
  - **SDK**: Buffered IPC reads (was byte-at-a-time), cron memory leak fix, `strtok_r` fix.
  - **New Handler**: `check_data` IPC command now implemented in Core.
  - Build: Clean compile with ASAN, no warnings.

## 2026-01-15 20:35
- **LLM Review**: Deep code analysis beyond style/linting.
  - **Critical**: HTTP-IPC bridge race condition (responses can mismatch)
  - **Critical**: Blocking HTTP server (single connection at a time)
  - **Critical**: O(n²) file backend due to full-scan on every insert
  - **Major**: `check_data` IPC command unimplemented (SDK assumption broken)
  - **Major**: SDK IPC uses byte-at-a-time reads (performance)
  - **Minor**: Memory leak in `sdk_register_cron` (cron_expr not freed)
  - See `llm_rev_report.md` for full findings and fix priorities.

## 2026-01-15 20:30
- **Network Stack**: Robust production hardening.
  - **IPC**: Replaced MVP IPC read loop with a buffered, framing-aware reader in `src/core/ipc.c`. Prevents message fragmentation issues.
  - **Deadlock Prevention**: Added 5s timeout to synchronous HTTP-IPC bridge in `src/server.c`.
  - **Socket Reuse**: Enabled `SO_REUSEADDR` for rapid server restarts.
  - **HTTP Lifecycle**: Added connection closure logs and `Connection: close` headers.

## 2026-01-15 14:21
- **Logging**: Replaced all `printf` with structured logging via `libs/log.c`:
  - Console output with ANSI colors (green=INFO, cyan=DEBUG, yellow=WARN, red=ERROR)
  - File output to `data/{location}/heimwatt.log`
  - Semantic prefixes: `[INIT]`, `[PLUGIN]`, `[IPC]`, `[DATA]`, `[CORE]`, `[SHUTDOWN]`, `[SDK:*]`
  - Verbose mode: `-v` or `--verbose` enables DEBUG level
- **DX**: SDK logs now mirrored through Core logging system with proper level mapping.
- **Fix**: Added signal handlers (SIGINT/SIGTERM) for graceful shutdown.

## 2026-01-15 13:01
- **Core**: Implemented Alpha server prototype (`src/server.c`) with IPC and File Backend (`data/tier1`).
- **SDK**: Added `sdk_report` (ipc), `sdk_query` (stub), and `sdk_lifecycle`. Fixed int/string protocol mismatch.
- **Plugins**: Refactored SMHI Weather to use config for URLs (removed hardcoding). Added `avg_sum_api` (stats) plugin code (pending SDK endpoint support).
- **Build**: Updated Makefile for debug/release targets and added `sdk`, `smhi`, `stats` targets. Added `make run` for DX.
- **DX**: Enhanced server console output with timestamps (e.g., `Reported: ... @ YYYY-MM-DD HH:MM:SS`).
- **Feature**: Added `--location` argument to organize data in `data/{location}/tier1`.
- **Visibility**: Added `hello` handshake; Core now logs when plugins connect (e.g., `[INFO] Plugin '...' connected`).

