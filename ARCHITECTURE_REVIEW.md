# HeimWatt Architecture Review

> **Review Date**: 2026-01-20  
> **Status**: Active Development

---

## Greatest Strengths

### 1. Opaque Pointer Pattern
Consistent use of opaque pointers (`heimwatt_ctx`, `plugin_mgr`, `db_handle`, `http_server`, `lps_solver`) provides true encapsulation and ABI stability. Same pattern as SQLite, libuv, OpenSSL.

### 2. Plugin Architecture with Process Isolation
Forking plugins into separate processes provides:
- Crash isolation (plugin crash doesn't kill core)
- Security boundary for sandboxing
- Independent resource limits
- Same pattern as Chrome renderers, VSCode extensions

### 3. Semantic Type System with X-Macros
`semantic_types.h` generates enums, metadata tables, and string converters from single source of truth. Pattern used in professional game engines and embedded systems.

### 4. Symmetric Lifecycle Pairs
Double-pointer destroy pattern (`destroy(T**)`) prevents use-after-free by NULLing caller's pointer. Combined with `SAFE_FREE` macro - defensive programming done right.

### 5. Newline-Delimited JSON Protocol
IPC uses NDJSON protocol (same as Docker daemon APIs, Language Server Protocol). Simple, debuggable, robust.

---

## Weaknesses / Areas for Improvement

### 1. ⚠️ "LPS" is Actually Dynamic Programming

**Current**: `lps.c` implements Dynamic Programming with state discretization, not Linear Programming.

**Issue**: LP/MILP uses Simplex/Interior Point with arbitrary linear constraints. DP discretizes into 101 states with O(T×S×A) complexity.

**Actions**:
- [ ] Rename to `dp_solver` or `battery_scheduler`
- [ ] Consider integrating HiGHS for true LP/MILP when thermal dynamics needed
- [ ] Document that DP complexity grows exponentially with state dimensions

**Regarding LP constraint limits**: Real LP solvers (HiGHS, Gurobi) handle millions of constraints. The plugin architecture is well-suited for pluggable constraint providers.

> User desicion: Remove this solver and lets work on a real professional one as explained below. 

---

### 2. No Persistent State Between Restarts

- [ ] Add `state.json` or SQLite for runtime configuration
- [ ] Persist plugin credentials and scheduling state

> Good idea, but i feel implementation needs more discussion and details.
---

### 3. Single-Threaded Core with Blocking Patterns

- [ ] Consider worker thread pool for JSON parsing
- [ ] Use SIGCHLD for non-blocking plugin health checks
- [ ] (Low priority for home energy system)

> Blocking is unacceptable. Explain more about my options here. Both sound fine, give me more details. 
---

### 4. SDK IPC is Request-Response Only

Current synchronous pattern prevents:
- Server-initiated push
- Streaming data
- Parallel operations

- [ ] Enhance SDK for bidirectional messaging

> I would like to do a deep dive design study on the SDK. We need it to be perfect. It MUST be extremely easy toi extend the system with new plugins. For example, is in / out the correct abstraction? do we need more types? sub types? how do we integra t esensors, actuators, constraints, etc? How do we wrap APIs that can control devices, nibe uplink etc? The SDK and the COre needs to handle all these things from the start. 

---

### 5. Fixed Buffer Sizes Everywhere

- [ ] Document maximum sizes in spec
- [ ] Add runtime validation with clear errors
> What else should we do? I dont want malloc eveywhere. Suggest options. Larger buffers is just kicking the can down the road right?
---

## Assessment Summary

| Aspect | Rating | Notes |
|--------|--------|-------|
| Code Quality | 8/10 | Clean C, consistent patterns |
| Architecture | 7/10 | Plugin isolation excellent |
| Professional Standard | 7/10 | Comparable to early open source projects |
| Scalability | 6/10 | Would need work for 100+ plugins |
| Correctness | 8/10 | Energy balance verified, DP algorithm correct |

---

## Future Considerations

### True LP/MILP Integration

For thermal dynamics and multi-asset coupling, integrate HiGHS solver:

```
[Constraint Provider Plugin: Battery] → SoC constraints
[Constraint Provider Plugin: Thermal] → RC model constraints  
[Constraint Provider Plugin: Grid]    → Import limits
        ↓
[Core LP Solver (HiGHS)]
        ↓
Optimal schedule
```

HiGHS (MIT licensed) can handle millions of variables/constraints in seconds.

> ✅ **Response**: Agreed. HiGHS is the gold standard for open-source MILP.  
> **See**: [docs/design/sdk_plugin_taxonomy.md](docs/design/sdk_plugin_taxonomy.md) for constraint plugin architecture.

---

## Design Studies (Implementation Order)

| # | Topic | Document | Why This Order |
|---|-------|----------|----------------|
| **1** | Database Abstraction | [01_database_abstraction.md](docs/design/01_database_abstraction.md) | Foundation — everything else writes data through this |
| **2** | Memory Strategy | [02_memory_strategy.md](docs/design/02_memory_strategy.md) | Second foundation — defines how all components allocate |
| **3** | SDK Plugin Taxonomy | [03_sdk_plugin_taxonomy.md](docs/design/03_sdk_plugin_taxonomy.md) | API design — must be locked before adding more plugins |
| **4** | Concurrency | [04_concurrency_strategy.md](docs/design/04_concurrency_strategy.md) | Performance — can be retrofitted after SDK is stable |
| **5** | WebUI, Auth, Safety | [05_webui_auth_safety.md](docs/design/05_webui_auth_safety.md) | Polish — needs stable Core/SDK before building on top |

---

## User Questions (Quick Answers)

**Q: Should WebUI be a plugin?**  
A: Keep API in Core (truth authority), UI as static bundle. Core can't have "ghost" plugins if it controls `/api/plugins`.

**Q: How do we manage safety?**  
A: Defense-in-depth: User constraints → Solver constraints → Plugin clamping → Hardware failsafe. Never trust any single layer.

**Q: Login for WebUI?**  
A: First-run password generation + JWT sessions. Full details in security study.