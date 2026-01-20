# Target State of HeimWatt

**Vision:** An extensible, local-first data platform for energy optimization that acts as a "Single Pane of Glass" for the entire home energy system.

---

## Core Philosophy

| Principle | Description |
|-----------|-------------|
| **Zero Domain Knowledge in Core** | Core is a pure broker — routes messages, stores data |
| **Plugin Composition** | Plugins interact via semantic types, not direct calls |
| **User Abstraction** | Users set comfort constraints; Solver handles power |
| **Simplicity First** | Analysis possible with Excel/Pandas from CSV |

---

## Storage Architecture: The "Wide CSV"

The entire system state is persisted to a single CSV file.

```
data/history.csv

timestamp,               atmosphere.temperature, energy.price.spot, home.power.total, ...
2026-01-20T10:00:00Z,   -5.2,                   0.45,              1250,             ...
2026-01-20T10:01:00Z,   -5.1,                   0.45,              1180,             ...
```

### Resampling Strategy

- **On Tick**: Core snapshots latest known value per semantic type
- **Write**: Snapshot written as new row
- **Null Handling**: Empty cell if no data received since startup

---

## The Solver (Brain)

> [!IMPORTANT]
> **Current Implementation**: Dynamic Programming with battery state discretization  
> **Target Implementation**: HiGHS (MILP) for multi-asset thermal optimization

### Inputs

| Tier | Source | Horizon | Confidence |
|------|--------|---------|------------|
| **Tier 1** | Published spot prices | 12-35h | 100% |
| **Tier 2** | Weather-based prediction | 35h-7d | 50-70% |

### Outputs

- `schedule.heat_pump.power` (W per interval)
- `schedule.battery.power` (+charge, -discharge)
- `schedule.ev.power` (charging schedule)

---

## House Physics (Self-Learning)

The RC thermal network model accuracy improves automatically:

| Stage | Data | Accuracy |
|-------|------|----------|
| Day 1 | Minimal | Default assumptions |
| Week 1 | Temperature patterns | Rough R estimate |
| Week 2+ | Heating cycles | R and C estimated |
| Month 1+ | Seasonal variation | Weather-adjusted |

---

## Hardware Integration

### Smart Meter (P1 Port)

| Device | Price | Connection | Notes |
|--------|-------|------------|-------|
| SlimmeLezer+ | €25 | WiFi → MQTT | Start here |
| Tibber Pulse | €100 | Cloud API | Already integrated |
| P1 USB Cable | €15 | Serial | Later |

### CT Clamps (Sub-Metering)

| Device | Price | Channels | Notes |
|--------|-------|----------|-------|
| Shelly EM | €35 | 2 | WiFi, good for single-phase |
| Shelly Pro 3EM | €90 | 3 | 3-phase monitoring |
| IotaWatt | €150 | 14 | Open source, highly accurate |

---

## Two Editors

### Device Definition Editor

- **Purpose**: Define WHAT a device IS (specs, protocol)
- **Output**: JSON file in `~/.heimwatt/devices/`

### Connection Editor (LiteGraph.js)

- **Purpose**: Wire YOUR devices in YOUR house
- **Nodes**: Zones, Devices, Sensors, Constraints
- **Edges**: "serves", "measures", "constrains"

---

## Open Design Questions

1. **LP vs DP**: When to use Dynamic Programming vs HiGHS MILP?
   - DP: Simple battery scheduling (101 states × 9 actions × 168 periods)
   - MILP: Thermal dynamics, multi-asset coupling, complex constraints

2. **SDK Push Messaging**: How to enable server-initiated messages to plugins?

3. **State Persistence**: SQLite vs JSON for runtime state between restarts?

4. **Plugin Trust**: How to implement signed plugin verification?

---

## File Locations (Target)

```
/usr/share/heimwatt/
├── plugins/              # Official signed plugins
└── devices/              # Official device definitions

~/.heimwatt/
├── config/
│   ├── system.json       # From connection editor
│   └── credentials.json  # Vault for API tokens
├── plugins/              # User/community plugins
├── devices/              # User device definitions
└── data/
    └── history.csv       # Wide CSV semantic store
```

---

## Related Documents

- [current_state.md](current_state.md) — Where we are now
- [roadmap.md](roadmap.md) — How we get there
- [ARCHITECTURE_REVIEW.md](../ARCHITECTURE_REVIEW.md) — Strengths/weaknesses analysis
- [old_docs/docs/DESIGN_SPEC.md](../old_docs/docs/DESIGN_SPEC.md) — Full specification (legacy)
- [old_docs/docs/SDK_SPEC.md](../old_docs/docs/SDK_SPEC.md) — SDK reference
- [old_docs/docs/Solver.md](../old_docs/docs/Solver.md) — MPC/MILP theory
