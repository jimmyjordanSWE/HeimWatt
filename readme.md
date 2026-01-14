# HeimWatt

> **Vision**: An extensible, local-first data platform for energy optimization.

HeimWatt is a modular broker that ingests data from **IN plugins** (weather, prices, sensors), stores it by semantic type, and routes queries to **OUT plugins** (optimizers, schedulers) that serve results via REST API.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        HeimWatt Core                            │
│   ┌──────────┐     ┌──────────┐     ┌──────────┐               │
│   │ Plugin   │────▶│  Data    │◀────│   HTTP   │               │
│   │ Manager  │     │  Store   │     │  Server  │               │
│   └──────────┘     └──────────┘     └──────────┘               │
│         │               ▲                 │                     │
│         │    IPC        │      IPC        │                     │
└─────────┼───────────────┼─────────────────┼─────────────────────┘
          │               │                 │
    ┌─────▼─────┐   ┌─────┴─────┐     ┌─────▼─────┐
    │ IN Plugin │   │ IN Plugin │     │OUT Plugin │
    │  (SMHI)   │   │(Elpriset) │     │ (Energy)  │
    └───────────┘   └───────────┘     └───────────┘
```

See [System Architecture](docs/design/architecture.md) for the full design.

---

## Directory Structure

```
heimwatt/
├── src/                    # Core source code
│   ├── core/               # Broker, routing, plugin management
│   ├── net/                # TCP, HTTP, JSON
│   ├── db/                 # SQLite abstraction
│   └── util/               # Logging, time, signals
├── include/                # Public headers
├── sdk/                    # Plugin SDK (libheimwatt-sdk.so)
├── libs/                   # Vendored libraries (cJSON, SQLite, curl)
├── plugins/                # Plugin source
│   ├── in/                 # Data plugins (inbound)
│   └── out/                # Calculator plugins (outbound)
├── webui/                  # Web UI (React, future)
├── config/                 # Configuration files
├── data/                   # SQLite database
├── logs/                   # Log files
└── docs/                   # Documentation
```

---

## Modules

| Layer | Modules |
|-------|---------|
| **Core** | `core`, `config`, `plugin_mgr`, `data_store`, `router`, `ipc` |
| **Network** | `tcp`, `http_parse`, `http_server`, `json` |
| **Database** | `sqlite`, `schema`, `queries` |
| **SDK** | `sdk_core`, `sdk_report`, `sdk_query`, `sdk_endpoint`, `sdk_ipc` |
| **Utilities** | `log`, `time_util`, `signal_util`, `mem` |

---

## Documentation

| Document | Description |
|----------|-------------|
| [Architecture](docs/architecture/overview.md) | System overview, data flow, IPC protocol |
| [Core Module](docs/architecture/modules/core/design.md) | Core APIs |
| [Plugins](docs/architecture/modules/plugins/design.md) | Plugin lifecycle, manifest schema |
| [Network](docs/architecture/modules/net/design.md) | Network stack APIs |
| [Database](docs/architecture/modules/db/design.md) | Database layer APIs |
| [SDK](docs/architecture/modules/sdk/design.md) | Plugin SDK public API |
| [Semantic Types](docs/reference/semantic_types.md) | Data type vocabulary |
| [Coding Standards](docs/standards/coding.md) | Code style, conventions |
| [Contributing](CONTRIBUTING.md) | How to contribute |

---

## Quick Start

```bash
# Build
make

# Run
./build/heimwatt --config config/heimwatt.json

# Run tests
make test
```

---

## Status

🔄 **Design Phase** — Architecture defined, implementation in progress.

| Component | Status |
|-----------|--------|
| Architecture | ✅ Complete |
| Module APIs | ✅ Defined |
| Core Implementation | 🔄 In Progress |
| IN Plugins | ⏳ Planned |
| OUT Plugins | ⏳ Planned |
| Web UI | ⏳ Future |

---

## License

Proprietary. See [LICENSE](LICENSE).


