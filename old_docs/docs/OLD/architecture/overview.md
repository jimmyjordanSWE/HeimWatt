# HeimWatt: System Architecture

> **Version**: 3.0 (2026-01-14)  
> **Status**: ✅ Module Architecture Defined

---

## Vision

**HeimWatt** is an extensible data platform that:
1. **Ingests** time-series data from external APIs and sensors via plugins
2. **Stores** data indexed by semantic type
3. **Computes** answers to optimization questions via calculator plugins
4. **Exposes** results through a REST API

---

## Core Design Decisions

| Decision | Resolution |
|----------|------------|
| Data normalization | Plugins send data in canonical SI units. SDK validates, does not convert. |
| Plugin architecture | Two types: **IN Plugins** (ingest) and **OUT Plugins** (compute + serve via cores API) |
| Core responsibility | Pure data broker: store, index, route. No domain logic. |
| Extensibility | Tier 1 (known semantic types) + Tier 2 (raw extension data) |
| Currency handling | Value + currency code string. No conversion. Client displays. |
| IPC mechanism | JSON over Unix domain sockets. Plugins are forked subprocesses. |
| Modularity | Atomic modules: each `.c`/`.h` pair has a single, defined purpose. |

---

## System Architecture

```mermaid
flowchart TB
    subgraph Clients
        Web[Web App]
        Mobile[Mobile App]
    end

    subgraph Core [HeimWatt Core]
        PM[Plugin Manager]
        DS[(Data Store)]
        GW[HTTP Server + Router]
        IPC[IPC Server]
        
        PM --> IPC
        IPC --> DS
        GW --> IPC
    end

    subgraph InPlugins [IN Plugins]
        SMHI[SMHI Weather]
        Elpriset[Elpriset Prices]
        OpenMeteo[Open-Meteo]
    end

    subgraph OutPlugins [OUT Plugins]
        EnergyStrategy[Energy Strategy]
        PlantWater[Plant Watering]
    end

    %% Data flow
    InPlugins -->|IPC: REPORT| IPC
    IPC -->|IPC: HTTP_REQUEST| OutPlugins
    OutPlugins -->|IPC: QUERY| IPC
    Clients --> GW
```
---

## Plugin Types

### IN Plugins (Inbound Data)

**Purpose**: Fetch data from external sources, report to Core.

**Lifecycle**:
```
sdk_init() → sdk_run() → [REPORT loop] → sdk_fini()
```

**Example Usage**:
```c
sdk_metric_new(ctx)
    ->semantic(SEM_ATMOSPHERE_TEMPERATURE)
    ->value(15.5)
    ->floor(-50.0)
    ->report();
```

**Manifest**:
```json
{
  "type": "in",
  "id": "com.heimwatt.smhi",
  "provides": {
    "known": ["ATMOSPHERE_TEMPERATURE", "ATMOSPHERE_HUMIDITY"],
    "raw": ["smhi.wind_direction"]
  },
  "schedule": { "interval_seconds": 3600 }
}
```

### OUT Plugins (Outbound Compute)

**Purpose**: Query data, compute answers, serve via API endpoint.

**Lifecycle**:
```
sdk_init() → sdk_register_endpoint() → sdk_run() → [handle requests] → sdk_fini()
```

**Example Usage**:
```c
sdk_register_endpoint(ctx, "GET", "/api/energy-strategy", handle_strategy);

int handle_strategy(plugin_ctx *ctx, const sdk_request *req, sdk_response *resp) {
    sdk_data_point price;
    sdk_query_latest(ctx, SEM_ENERGY_PRICE_SPOT, &price);
    
    // Compute 48h strategy using LPS solver...
    
    sdk_response_json(resp, strategy_json);
    return 0;
}
```

**Manifest**:
```json
{
  "type": "out",
  "id": "com.heimwatt.energy-strategy",
  "requires": { "known": ["ENERGY_PRICE_SPOT", "STORAGE_SOC"] },
  "endpoints": [{ "method": "GET", "path": "/api/energy-strategy" }],
  "triggers": { "on_data": ["ENERGY_PRICE_SPOT"], "interval_seconds": 900 }
}
```

---

## Semantic Type System

### Hierarchy

```
<domain>.<measurement>[.<qualifier>]
```

### Domains

| Domain | Examples |
|--------|----------|
| `atmosphere` | temperature, humidity, pressure, precipitation |
| `solar` | ghi, dni, dhi, elevation |
| `space` | kp_index, solar_wind, xray_flux |
| `energy` | price.spot, grid_frequency, carbon_intensity |
| `storage` | soc, power, voltage, temperature |
| `vehicle` | soc, charging.power, charging.state |
| `soil` | temperature, moisture |
| `water` | temperature, ph, level, flow |
| `indoor` | temperature, humidity, co2, illuminance |
| `air` | aqi, pm2_5, pm10, no2 |

### Two-Tier Model

| Tier | Purpose | Validation |
|------|---------|------------|
| **Tier 1** | Known semantic types (enum) | Full: type, unit, format |
| **Tier 2** | Raw extension data (string key) | Minimal: key format only |

---

## IPC Protocol

JSON over Unix domain socket. Each message is newline-terminated.

### Message Types

| Direction | Type | Purpose |
|-----------|------|---------|
| Plugin → Core | `REPORT` | Submit Tier 1 data point |
| Plugin → Core | `REPORT_RAW` | Submit Tier 2 data |
| Plugin → Core | `QUERY_LATEST` | Request latest value |
| Plugin → Core | `QUERY_RANGE` | Request historical range |
| Plugin → Core | `REGISTER_ENDPOINT` | Declare HTTP endpoint |
| Core → Plugin | `HTTP_REQUEST` | Forward HTTP request to OUT plugin |
| Plugin → Core | `HTTP_RESPONSE` | Return HTTP response |
| Core → Plugin | `TRIGGER` | Wake plugin (scheduled/data event) |

---

## Data Flow

```mermaid
flowchart LR
    API[External API] --> InPlugin[IN Plugin]
    InPlugin -->|IPC: REPORT| Core[Core]
    Core -->|store| DB[(SQLite)]
    
    Client[Client] --> HTTP[HTTP Server]
    HTTP -->|route| Core
    Core -->|IPC: HTTP_REQUEST| OutPlugin[OUT Plugin]
    OutPlugin -->|IPC: QUERY| Core
    Core -->|result| OutPlugin
    OutPlugin -->|IPC: HTTP_RESPONSE| Core
    Core --> HTTP
    HTTP --> Client
```

---

## Core Components

| Component | Responsibility |
|-----------|----------------|
| **Plugin Manager** | Discover, fork, supervise plugin processes |
| **Data Store** | SQLite, indexed by semantic type + timestamp |
| **Router** | Map HTTP paths → plugin IDs |
| **HTTP Server** | Accept connections, parse HTTP, dispatch |
| **IPC Server** | Unix socket, JSON messages, Core ↔ Plugin |
| **SDK** | Plugin library: reporting, querying, endpoints |

---

## Document Index

| Path | Description |
|------|-------------|
| [architecture.md](./architecture.md) | This file — system overview |
| [modules/core/design.md](./modules/core/design.md) | Core module APIs |
| [modules/plugins/design.md](./modules/plugins/design.md) | Plugin system design |
| [modules/net/design.md](./modules/net/design.md) | Network stack APIs |
| [modules/db/design.md](./modules/db/design.md) | Database layer APIs |
| [modules/sdk/design.md](./modules/sdk/design.md) | SDK public API |

---

## Next Steps

1. ✅ Define module APIs (this document + children)
2. [ ] Create stub implementations for all modules
3. [ ] Implement Core lifecycle (`core.c`)
4. [ ] Implement IPC server/client
5. [ ] Port existing LPS logic to `energy_strategy` plugin
6. [ ] Build first IN plugin (SMHI or Elpriset)
