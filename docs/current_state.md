# Current State of HeimWatt

**Date:** 2026-01-20
**Status:** Alpha / Heavy WIP

## Overview
HeimWatt is currently a high-performance, local-first energy optimization platform in active development. The core architecture is established as a "Pure Broker" model where the Core routing engine has zero domain knowledge, passing semantic data between decoupled plugins.

## Architecture

### Core System
- **Language:** C99 (Clang optimized)
- **Event Loop:** Custom `epoll`-based non-blocking architecture.
- **IPC:** Unix Domain Sockets with JSON framing. Recently refactored to be async and non-blocking to prevent server deadlocks.
- **Storage:** Transitioning to Flat CSV.
    - **Current:** Two-tier system (SQLite/File) is being deprecated.
    - **Target:** Single `history.csv` with configurable interval resampling.

### Network Stack
- **HTTP Server:** Custom non-blocking server handling 1000+ concurrent connections.
- **Bridge:** Async HTTP-to-IPC bridge using Request ID correlation to map incoming HTTP requests to plugin responses without blocking worker threads.

### Plugin System
- **Isolation:** Plugins run as separate processes, managed by `Plugin Manager`.
- **SDK:** C SDK provides abstraction for:
    - Lifecycle management (startup/teardown)
    - Configuration (from `manifest.json`)
    - Scheduling (Ticks, Cron, File Descriptors)
    - Semantic Data Reporting & Querying
    - HTTP/TLS abstraction with security policies.

### Web UI
- **Stack:** React, Vite.
- **Visual Programming:** `LiteGraph.js` integrated for defining connections between devices, zones, and constraints.
- **Status:** Functional scaffolding exists; node editor logic is being implemented.

## Key Components Status

| Component | Status | Notes |
|-----------|--------|-------|
| **Core Server** | **Beta** | High-perf rewrite complete. Verification in progress. |
| **SDK** | **Beta** |  |
| **Plugin: SMHI** | **Alpha** | Fetches weather data. Uses config for URLs. |
| **Plugin: Prices** | **Planned** | Infrastructure ready, needs implementation. |
| **Solver** | **Pending** | Optimization logic (MPC/MILP) not yet integrated. |
| **Physics Model** | **Pending** | RC Network logic defined but not implemented. |
| **Unit Tests** | **Active** | Unity framework integrated. 27+ tests passing (Parsers, DB, etc). |

## Known Issues
- **Race Condition:** Residual risk in `http_server.c` regarding connection freeing vs async completion (Fix proposed).
- **Web UI:** Dashboard data visualization is minimal.
- **Documentation:** Sprawling and fragmented (being consolidated now).
