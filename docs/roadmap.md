# Roadmap & Planning

> **Version**: 2.0 (2026-01-15)

---

## Current Status

| Component | Status |
|-----------|--------|
| Core lifecycle | ✅ API defined |
| Plugin manager | ✅ API defined |
| Data store | ✅ API defined |
| Router | ✅ API defined |
| IPC protocol | ✅ Designed (ADR-001) |
| SDK | ✅ API defined |
| Database (SQLite) | ✅ Interface defined |
| Database (file debug) | ✅ Interface defined |

---

## Next Steps (Short-term)

1. [ ] Implement Core lifecycle (`core.c`)
2. [ ] Implement IPC server/client
3. [ ] Create stub implementations for all modules
4. [ ] Build first IN plugin (SMHI weather)
5. [ ] Build first OUT plugin (energy strategy)
6. [ ] Port existing LPS solver logic to plugin

---

## Future Roadmap

### Phase 1: Foundation (Current)
- Module API definitions
- Coding standards
- Build system (Makefile)
- Basic documentation

### Phase 2: Core Implementation
- Full Core broker implementation
- Plugin discovery and forking
- SQLite data store
- HTTP server + router
- IPC messaging

### Phase 3: First Plugins
- SMHI weather plugin (IN)
- Elpriset prices plugin (IN)
- Energy optimizer plugin (OUT)

### Phase 4: Hardening
- Unit testing framework
- Fuzzing (HTTP parser, JSON, IPC)
- CI pipeline
- >80% code coverage

### Phase 5: Polish
- Web UI (React)
- Configuration UI
- Plugin marketplace concept
- Monitoring & alerting

---

## Architectural Decisions (ADRs)

### ADR-001: Plugin IPC via Unix Sockets

**Status**: Accepted

**Decision**: Use Unix domain sockets with JSON-over-newline messages.

**Rationale**:
- Fast (no network overhead)
- Simple POSIX implementation
- Bidirectional in single connection
- Filesystem-based access control

**Trade-offs**:
- Linux-only (Windows would need named pipes)
- Local machine only
- JSON parsing overhead (acceptable)

---

## Testing Strategy

### Unit Testing

| Component | Strategy |
|-----------|----------|
| Config parsing | Valid/invalid JSON tests |
| Data store | In-memory SQLite |
| Router | Path matching tests |
| IPC protocol | Message parsing tests |

### Fuzzing Targets (Priority)

| Target | Risk | Tool |
|--------|------|------|
| HTTP parser | High | AFL++ |
| JSON parser | High | libFuzzer |
| IPC messages | High | AFL++ |
| Config loader | Medium | - |

### CI Pipeline (Planned)

```
PR:
  ├── Format check
  ├── Lint (clang-tidy)
  ├── Build (debug + release)
  ├── Unit tests (ASAN)
  └── Coverage report

Nightly:
  ├── Full test suite
  ├── Fuzz (1 hour/target)
  └── Valgrind memcheck
```

---

## Decisions Needed

Vendor HiGHS?

| Topic | Options | Status |
|-------|---------|--------|
| Unit test library | µnit, Unity, Check, cmocka | Pending |
| Primary fuzzer | AFL++ vs libFuzzer | Pending |
| Mock approach | Manual stubs vs framework | Pending |
| Web UI framework | React vs Vue vs plain | Pending |
