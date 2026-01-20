# HeimWatt Project Roadmap

**Goal:** Bridge the gap between Alpha (Current) and V1.0 (Target).

## Phase 1: Foundation & Stability (Current)
**Focus:** Reliability, Performance, and Safety.
- [ ] **Core Stability:** Fix race conditions in HTTP-IPC bridge.
- [ ] **Infrastructure:** Implement "Wide CSV" backend logger with configurable intervals.
- [ ] **Cleanup:** Remove SQLite and legacy File-backend code.
- [ ] **Testing:** Expand Unit Test coverage > 80% for Core and SDK.
- [ ] **Simulator:** Finish `sim_bundle` (House, Weather, Grid, Battery) to enable dev without hardware.

## Phase 2: Data Ingestion (The "Eyes")
**Focus:** Getting real-world data into the system.
- [ ] **Smart Meter:** Implement `slimmelezer_plugin` (MQTT/HTTP P1 reader).
- [ ] **Weather:** Enhance SMHI plugin to robustly handle API failures/rate limits.
- [ ] **Prices:** Implement Nordpool/Spot price plugin (Tier 1 data).
- [ ] **Visualization:** Implement basic Grafana-style charts in Web UI to verify data quality.

## Phase 3: The Brain (Solver V0.1)
**Focus:** Closing the control loop.
- [ ] **Solver Integration:**
    - Create `solver_plugin`.
- [ ] **Optimization:**
    - Simple linear optimization for battery.
    - Basic constraints for heating.

## Phase 4: Device Control (The "Hands")
**Focus:** Actuating hardware based on Solver output.
- [ ] **Wrappers:**
    - Implement `melcloud_plugin` (Mitsubishi Heat Pumps).
    - Implement `shelly_plugin` (Relays/Plugs).
- [ ] **Feedback Loop:** Ensure `actual` semantic types are reported back to verify commands were executed.

## Phase 5: User Experience & Polish
**Focus:** Usability for non-developers.
- [ ] **Connection Editor:** Finalize LiteGraph.js integration for visual wiring.
- [ ] **Device Editor:** UI for creating device definitions.
- [ ] **Packaging:**
    - Stable Docker builds.
    - Setup Wizard (Welcome screen).

## Phase 6: Advanced Features (V2.0)
- [ ] **Physics Learning:** Implement RC Model parameter estimation.
- [ ] **Tier 2 Predictions:** AI-based price prediction models.
- [ ] **Community Store:** Plugin signing and distribution repo.
