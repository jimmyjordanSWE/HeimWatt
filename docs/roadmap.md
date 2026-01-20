# HeimWatt Project Roadmap

**Goal:** Bridge the gap between Alpha (Current) and V1.0 (Target).

---

## Phase 1: Foundation & Stability (Current)

**Focus:** Reliability, Performance, and Safety.

- [x] Core Stability: High-performance epoll/async architecture
- [x] IPC: Non-blocking Unix socket with NDJSON framing
- [x] HTTP Server: 1000+ concurrent connections
- [ ] **Infrastructure:** Implement "Wide CSV" backend logger with configurable intervals
- [ ] **Cleanup:** Remove legacy SQLite code paths
- [ ] **Testing:** Expand Unit Test coverage > 80% for Core and SDK
- [ ] **Simulator:** Finish `sim_bundle` (House, Weather, Grid, Battery)
- [ ] **Solver Rename:** Rename "LPS" to "DP Solver" (it's Dynamic Programming, not LP)

## Phase 2: Data Ingestion (The "Eyes")

**Focus:** Getting real-world data into the system.

- [ ] **Smart Meter:** Implement `slimmelezer_plugin` (MQTT/HTTP P1 reader)
- [ ] **Weather:** Enhance SMHI plugin for API failures/rate limits
- [ ] **Prices:** Implement Nordpool/Spot price plugin
- [ ] **Visualization:** Basic charts in Web UI to verify data quality

## Phase 3: The Brain (Solver V0.1)

**Focus:** Closing the control loop.

> [!IMPORTANT]
> Current "LPS" is Dynamic Programming, not Linear Programming. 
> For thermal dynamics (RC model), consider integrating HiGHS (true MILP).

- [ ] **Solver Plugin:** Create output plugin with semantic type consumption
- [ ] **Optimization:**
  - Simple DP for battery charge/discharge (current)
  - Basic heating constraints
- [ ] **(Optional) HiGHS Integration:** For multi-asset coupling

## Phase 4: Device Control (The "Hands")

**Focus:** Actuating hardware based on Solver output.

- [ ] **Wrappers:**
  - Implement `melcloud_plugin` (Mitsubishi Heat Pumps)
  - Implement `shelly_plugin` (Relays/Plugs)
- [ ] **Feedback Loop:** Report `actual` semantic types to verify commands

## Phase 5: User Experience & Polish

**Focus:** Usability for non-developers.

- [ ] **Connection Editor:** Finalize LiteGraph.js integration
- [ ] **Device Editor:** UI for creating device definitions
- [ ] **Packaging:**
  - Stable Docker builds
  - Setup Wizard

## Phase 6: Advanced Features (V2.0)

- [ ] **Physics Learning:** RC Model parameter estimation
- [ ] **Tier 2 Predictions:** AI-based price prediction
- [ ] **Community Store:** Plugin signing and distribution
- [ ] **True LP Solver:** HiGHS integration for complex multi-asset optimization

---

## Architecture Debt (from Review)

| Issue | Severity | Phase |
|-------|----------|-------|
| Rename "LPS" → "DP Solver" | Medium | 1 |
| Add persistent state (SQLite/JSON) | Medium | 1 |
| SDK push messaging support | Low | 3 |
| Document buffer size limits | Low | 5 |
