# System Architecture

> **Version**: 2.0 (2026-01-15)

---

## Core Principles

| Principle | Description |
|-----------|-------------|
| **Core as Broker** | Core manages plugins, stores data, routes requests. No domain logic. |
| **Semantic Data** | All data uses semantic types (`ATMOSPHERE_TEMPERATURE`, `ENERGY_PRICE_SPOT`). |
| **Plugin Isolation** | Plugins run as separate processes, communicate via IPC. |
| **Opaque APIs** | Internal state hidden behind handles. No direct struct access. |

---

## System Components

```
heimwatt/
├── include/          # Public headers (7 files)
│   ├── db.h          # Database interface
│   ├── server.h      # Core lifecycle
│   ├── heimwatt_sdk.h# Plugin SDK
│   ├── semantic_types.h
│   ├── types.h
│   ├── utils.h
│   └── version.h
│
├── src/
│   ├── core/         # Central broker
│   │   ├── core.h        - Lifecycle, orchestration
│   │   ├── config.h      - Configuration parsing
│   │   ├── plugin_mgr.h  - Plugin fork/supervise
│   │   ├── data_store.h  - Semantic data storage
│   │   ├── router.h      - HTTP → plugin dispatch
│   │   ├── ipc.h         - Core-side IPC
│   │   └── pipeline.h    - Internal data pipeline
│   │
│   ├── db/           # Database layer
│   │   ├── sqlite_backend.h  - SQLite implementation
│   │   ├── file_backend.h    - Debug file backend
│   │   ├── schema.h          - Table creation
│   │   └── queries.h         - Prepared statements
│   │
│   ├── net/          # Network stack
│   │   ├── tcp_server.h  - Raw sockets
│   │   ├── http_server.h - Accept loop
│   │   ├── http_parse.h  - Request/response parsing
│   │   ├── http_client.h - Outbound requests
│   │   └── json.h        - JSON encode/decode
│   │
│   ├── sdk/          # Plugin SDK internals
│   │   ├── sdk_core.h    - Context lifecycle
│   │   ├── sdk_ipc.h     - Plugin-side IPC
│   │   ├── sdk_report.h  - Data reporting
│   │   ├── sdk_query.h   - Query API
│   │   └── sdk_endpoint.h- Endpoint registration
│   │
│   └── util/         # Shared utilities
│       ├── mem.h         - Safe allocation
│       ├── signal_util.h - Signal handling
│       └── time_util.h   - Timestamp helpers
│
└── plugins/
    ├── in/           # Data ingestion plugins
    └── out/          # Compute/API plugins
```

---

## Data Flow

### 1. Ingestion (IN Plugin → Core)

```
External API → IN Plugin → sdk_report() → IPC:REPORT → Core → Data Store
```

### 2. Query (OUT Plugin ← Core)

```
OUT Plugin → sdk_query_latest() → IPC:QUERY → Core → Data Store → Response
```

### 3. HTTP Request (Client → OUT Plugin)

```
Client → HTTP Server → Router → IPC:HTTP_REQUEST → OUT Plugin
                                                     ↓
Client ← HTTP Server ← IPC:HTTP_RESPONSE ←──────────┘
```

---

## Database Architecture

Two-tier storage model:

| Tier | Purpose | Storage |
|------|---------|---------|
| **Tier 1** | Semantic types (enum ID + value + timestamp) | Indexed, fast |
| **Tier 2** | Raw extension data (string key + JSON + timestamp) | Flexible |

**Backends**:
- `sqlite_backend.h` — Production (SQLite)
- `file_backend.h` — Debug (append-only log files)

The interface in `include/db.h` is backend-agnostic. Swap implementations at compile time.

---

## IPC Protocol

JSON over Unix domain sockets. Newline-delimited messages.

| Direction | Message | Purpose |
|-----------|---------|---------|
| Plugin → Core | `HELLO` | Handshake with plugin ID |
| Plugin → Core | `REPORT` | Submit Tier 1 data point |
| Plugin → Core | `REPORT_RAW` | Submit Tier 2 data |
| Plugin → Core | `QUERY_LATEST` | Get most recent value |
| Plugin → Core | `QUERY_RANGE` | Get historical data |
| Plugin → Core | `REGISTER_ENDPOINT` | Claim HTTP route |
| Core → Plugin | `HTTP_REQUEST` | Forward web request |
| Plugin → Core | `HTTP_RESPONSE` | Return web response |

**Versioning**: Protocol version in HELLO handshake (see `version.h`).

---

## Semantic Type System

Hierarchy: `<domain>.<measurement>[.<qualifier>]`

| Domain | Examples |
|--------|----------|
| `atmosphere` | temperature, humidity, pressure |
| `solar` | ghi, dni, dhi, irradiance |
| `energy` | price.spot, demand, carbon_intensity |
| `storage` | soc, power, capacity, temperature |
| `vehicle` | soc, charging.power, charging.state |

Defined in `include/semantic_types.h` using X-Macros.

---

## Error Handling

Per coding standard:
- **Success**: Return `0`
- **Failure**: Return negative errno (`-EINVAL`, `-ENOMEM`, `-EIO`, etc.)

```c
int result = db_open(&db, "/path/to/db");
if (result < 0) {
    fprintf(stderr, "Failed: %s\n", strerror(-result));
}
```
