# Task: Memory System Optimization

## Status
- [x] Finalize `docs/02_memory_optimization.md` plan <!-- id: 0 -->
- [x] Implement `src/core/memory` library <!-- id: 1 -->
    - [x] `mem_alloc` (malloc + memset) <!-- id: 2 -->
    - [x] `HwPool` (Fixed-size object pool) <!-- id: 3 -->
    - [x] `HwArena` (Bump allocator) <!-- id: 4 -->
- [x] Refactor Codebase to use `mem_alloc` <!-- id: 5 -->
    - [x] `src/core` (config, ipc, plugin_mgr) <!-- id: 6 -->
    - [x] `src/net` (http*, json, tcp) <!-- id: 7 -->
    - [x] `src/db` (db, csv, duckdb)
    - [x] `src/sdk` (query, config, state, lifecycle)
- [ ] Optimize Hot Paths <!-- id: 8 -->
    - [x] HTTP Response Serialization (Pool) <!-- id: 9 -->
    - [x] HTTP Client Buffer (HwBuffer) <!-- id: 10 -->
    - [x] JSON Parsing (Arena) <!-- id: 11 -->
- [ ] Verification <!-- id: 12 -->
    - [ ] Functional Tests <!-- id: 13 -->
    - [ ] Memory Leak Check (Valgrind) <!-- id: 14 -->
