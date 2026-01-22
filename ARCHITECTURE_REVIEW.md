# HeimWatt Architecture Review

> **Review Date**: 2026-01-21  
> **Status**: Active Development  
> **Last Audit**: LLM Code Review (2026-01-21)

---

## Executive Summary

The HeimWatt architecture demonstrates solid C99 fundamentals with consistent opaque pointer patterns, process-isolated plugins, and a semantic type system. However, code review and design analysis reveal **gaps between documented architecture and implementation reality**.

Key findings:
1. **Design Studies Not Created**: The 5 design studies referenced below don't exist yet
2. **Core Coupling**: `heimwatt_ctx` aggregates all system state, creating implicit dependencies
3. **Event Loop Complexity**: `heimwatt_run_with_shutdown_flag` (270 lines) is a God Unit
4. **Concurrency Bug**: `db_query_point_exists_tier1` missing mutex lock

---

## Greatest Strengths

### 1. Opaque Pointer Pattern
Consistent use of opaque pointers (`heimwatt_ctx`, `plugin_mgr`, `db_handle`, `http_server`) provides true encapsulation and ABI stability. Same pattern as SQLite, libuv, OpenSSL.

### 2. Plugin Architecture with Process Isolation
Forking plugins into separate processes provides:
- Crash isolation (plugin crash doesn't kill core)
- Security boundary for sandboxing
- Independent resource limits
- Same pattern as Chrome renderers, VSCode extensions

### 3. Semantic Type System with X-Macros
`semantic_types.h` generates enums, metadata tables, and string converters from single source of truth. Pattern used in professional game engines and embedded systems.

### 4. Symmetric Lifecycle Pairs
Double-pointer destroy pattern (`destroy(T**)`) prevents use-after-free by NULLing caller's pointer. Combined with `SAFE_FREE` macro — defensive programming done right.

### 5. Newline-Delimited JSON Protocol
IPC uses NDJSON protocol (same as Docker daemon APIs, Language Server Protocol). Simple, debuggable, robust.

---

## Weaknesses / Areas for Improvement

### 1. ⚠️ Core Event Loop is a God Unit

> **Code Location**: [server.c#L451-L720](src/server.c#L451-L720)  
> **Severity**: High

**Current State**: `heimwatt_run_with_shutdown_flag` (270 lines) handles:
- Signal handling via `signalfd`
- epoll event loop setup
- HTTP thread lifecycle
- IPC connection management (accept/read/write/disconnect)
- Periodic tasks (`plugin_mgr_check_health`, `db_tick`)

**Problem**: Changes to any subsystem risk breaking others. Implicit coupling through shared epoll and connection arrays makes the code fragile.

**Design Decision Required**:

| Option | Pros | Cons |
|--------|------|------|
| **A: Reactor Pattern** | Clean separation, testable components | More indirection, slightly higher complexity |
| **B: Keep Monolithic** | Simple to trace, all logic in one place | Fragile, hard to test, God Unit anti-pattern |

**Recommendation**: Option A — Factor into distinct reactors:
- `signal_reactor_poll()` — reads signalfd, sets shutdown flag
- `ipc_reactor_poll()` — handles plugin connections
- `supervision_tick()` — health checks and DB maintenance

---

### 2. ⚠️ `heimwatt_ctx` Aggregates All State

> **Code Location**: [server_internal.h#L21-L57](src/server_internal.h#L21-L57)  
> **Severity**: Medium

**Current State**: `heimwatt_ctx` contains:
- Database handle (`db_handle*`)
- IPC server (`ipc_server*`)
- Plugin manager (`plugin_mgr*`)
- HTTP server (`http_server*`)
- Thread pool (`thread_pool*`)
- Connection array and registry
- Global configuration (`lat`, `lon`, `area`)

**Problem**: Every function receiving `heimwatt_ctx` has access to *all* system resources. This creates:
- Implicit coupling (function touching `conns[]` can also touch `db`)
- Testing difficulty (must mock entire context)
- Violation of interface segregation principle

**Design Decision Required**:

| Option | Pros | Cons |
|--------|------|------|
| **A: Sub-Contexts** | Clean interfaces, explicit dependencies | More parameter passing |
| **B: Keep Unified** | Simple initialization, everything accessible | Coupling, testing difficulty |

**Recommendation**: Option A — Break into scoped contexts:
```
heimwatt_ctx (lifecycle owner only)
├── server_stats (for API handlers)
├── ipc_ctx (for IPC handlers)  
└── plugin_ctx (for plugin management)
```

---

### 3. ⚠️ Database Locking Strategy

> **Code Location**: [db.c#L30](src/db/db.c#L30)  
> **Severity**: Medium (Design) + High (Bug)

**Current State**: Single `pthread_mutex_t` serializes all reads and writes across all backends.

**Known Bug**: `db_query_point_exists_tier1` (L235-239) accesses backend without acquiring mutex — data race.

**Design Decision Required**:

| Option | Pros | Cons |
|--------|------|------|
| **A: Fix Bug, Keep Coarse Lock** | Simple, correct, sufficient for 1-2 backends | Bottleneck if network backend added |
| **B: Read-Write Lock** | Concurrent reads, acceptable for current scale | Complexity increase |
| **C: Per-Backend Locks** | Maximum parallelism | Complex deadlock avoidance needed |

**Recommendation**: Option A for now — fix the bug, document that coarse locking is intentional for simplicity. Revisit when network backends are added.

---

### 4. Design Studies Created

The following design studies are available for iteration:

| # | Topic | Document | Priority |
|---|-------|----------|----------|
| 1 | Core Event Loop | [01_core_event_loop.md](docs/design/01_core_event_loop.md) | P1 — Address God Unit |
| 2 | Context Decomposition | [02_context_decomposition.md](docs/design/02_context_decomposition.md) | P2 — Reduce coupling |
| 3 | Database Concurrency | [03_database_concurrency.md](docs/design/03_database_concurrency.md) | P0 — Contains bug fix |
| 4 | SDK Plugin Taxonomy | [04_sdk_plugin_taxonomy.md](docs/design/04_sdk_plugin_taxonomy.md) | P1 — API design |
| 5 | Solver Integration | [05_solver_integration.md](docs/design/05_solver_integration.md) | P2 — HiGHS migration |

---

## Assessment Summary

| Aspect | Rating | Notes |
|--------|--------|-------|
| Code Quality | 8/10 | Clean C, consistent patterns |
| Architecture | 6/10 | Plugin isolation excellent, core coupling needs work |
| Documentation | 7/10 | Manuals good, design specs now exist |
| Professional Standard | 7/10 | Comparable to early open source projects |
| Scalability | 6/10 | Would need work for 100+ plugins |
| Correctness | 7/10 | One concurrency bug found |

---

## Open Design Questions

1. **Reactor vs Monolithic**: Should the event loop be refactored into separate reactors?
   → See [01_core_event_loop.md](docs/design/01_core_event_loop.md)

2. **Context Decomposition**: Should `heimwatt_ctx` be broken into sub-contexts?
   → See [02_context_decomposition.md](docs/design/02_context_decomposition.md)

3. **DB Locking Strategy**: Fix bug and keep coarse lock, or migrate to RW lock?
   → See [03_database_concurrency.md](docs/design/03_database_concurrency.md)

4. **Plugin Taxonomy**: Capability-based or type-based plugin model?
   → See [04_sdk_plugin_taxonomy.md](docs/design/04_sdk_plugin_taxonomy.md)

5. **Solver Direction**: Commit to HiGHS integration or maintain DP solver?
   → See [05_solver_integration.md](docs/design/05_solver_integration.md)

---

## User Questions (Quick Answers)

**Q: Should WebUI be a plugin?**  
A: Keep API in Core (truth authority), UI as static bundle. Core can't have "ghost" plugins if it controls `/api/plugins`.

**Q: How do we manage safety?**  
A: Defense-in-depth: User constraints → Solver constraints → Plugin clamping → Hardware failsafe. Never trust any single layer.

**Q: Login for WebUI?**  
A: First-run password generation + JWT sessions. Full details in security study.