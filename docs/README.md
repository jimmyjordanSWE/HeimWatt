# Documentation

Welcome to the HeimWatt documentation. This index helps you navigate to the right place.

---

## Getting Started

| Document | For |
|----------|-----|
| [README](../readme.md) | Project overview, quick start |
| [CONTRIBUTING](../CONTRIBUTING.md) | How to contribute |
| [Architecture Overview](architecture/overview.md) | System design |

---

## Architecture

Technical design documents for the system.

| Document | Description |
|----------|-------------|
| [Overview](architecture/overview.md) | High-level architecture, data flow, IPC protocol |
| [Core Module](architecture/modules/core/design.md) | Plugin manager, data store, router, IPC |
| [Database Layer](architecture/modules/db/design.md) | SQLite wrapper, schema, queries |
| [Network Stack](architecture/modules/net/design.md) | TCP, HTTP, JSON |
| [Plugin System](architecture/modules/plugins/design.md) | IN/OUT plugins, manifest schema |
| [SDK](architecture/modules/sdk/design.md) | Plugin development API |

---

## Standards

Project coding and workflow standards.

| Document | Description |
|----------|-------------|
| [Coding Standards](standards/coding.md) | C99 style, patterns, idioms |
| [Development Workflow](standards/workflow.md) | Git, CI, branches |

---

## Reference

Quick-reference material and lookup tables.

| Document | Description |
|----------|-------------|
| [Tech Stack](reference/tech_stack.md) | Libraries, tools, build setup |
| [Hardware Options](reference/hardware.md) | Deployment hardware recommendations |
| [Backend Swapping](reference/backend_swapping.md) | How to swap implementations |
| [Semantic Types](reference/semantic_types.md) | Data type vocabulary |

### External API References

| API | Document |
|-----|----------|
| SMHI | [reference/external_apis/smhi.md](reference/external_apis/smhi.md) |
| Open-Meteo | [reference/external_apis/open_meteo.md](reference/external_apis/open_meteo.md) |
| Elpriset | [reference/external_apis/prices.md](reference/external_apis/prices.md) |

---

## Decision Records

Architectural decisions and their rationale.

| ADR | Title |
|-----|-------|
| [ADR-001](adr/001-plugin-ipc.md) | Plugin IPC via Unix sockets |

---

## Generated Documentation

- **Doxygen API Docs**: Run `make docs` to generate HTML documentation from source comments.
