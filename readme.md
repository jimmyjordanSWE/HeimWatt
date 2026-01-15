# HeimWatt

> **Vision**: An extensible, local-first data platform for energy optimization.

HeimWatt is a modular broker that ingests data from **IN plugins** (weather, prices, sensors), stores it by semantic type, and routes queries to **OUT plugins** (optimizers, schedulers) that serve results via REST API.

## Status

🔄 **Design Phase**
| Component | Status |
|-----------|--------|
| Architecture | ✅ Complete |
| Module APIs | 🔄 In Progress |
| Core Implementation |⏳ Planned |
| IN Plugins | ⏳ Planned |
| OUT Plugins | ⏳ Planned |
| Web UI | ⏳ Future |

---

## Tech Stack

| Component | Choice |
|-----------|--------|
| Language | C99 |
| Compiler | Clang |
| Build | Makefile |
| Target | Linux (POSIX) |

| Library | Purpose |
|---------|---------|
| cJSON | JSON parsing |
| SQLite | Database |
| libcurl | HTTP client |

---

## Quick Start

```bash
# Build
make

# Run
./build/heimwatt

# Run with debug
make debug && ./build/heimwatt
```

---

## License

Proprietary. See [LICENSE](LICENSE).
