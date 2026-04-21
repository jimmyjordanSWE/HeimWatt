# HeimWatt

> Local-first platform for energy optimization, data routing, and flexible load coordination.
>
> Current status: work in progress.

## Overview

HeimWatt is a modular platform for home energy optimization. It is designed to collect and route system data, manage communication between components, and support scheduling decisions for flexible loads such as heat pumps, EV chargers, and batteries.

The long-term goal is to provide a robust local-first foundation for energy-aware automation and optimization, with a clear separation between core infrastructure and domain-specific logic.

## Documentation

- [Current State](docs/current_state.md) - architecture, recent performance work, and current implementation status
- [Target Vision](docs/target_state.md) - intended end-state architecture and solver direction
- [Roadmap](docs/roadmap.md) - planned path from current prototype to v1.0

## Quick Start

### Prerequisites

- Linux (POSIX)
- Clang or GCC
- Make

### Build and run
```bash
Build the project with `make`.
Run the server with `./build/heimwatt`.
For a debug build, run `make debug` and then start `./build/heimwatt`.
```
## Tech Stack

- Core: C99
- Event loop: `epoll`
- Storage: CSV (SQLite planned)
- Web UI: React + Vite
- Protocols: HTTP/1.1, Unix domain sockets

## Architecture

HeimWatt follows a broker-based architecture:

1. **Core**  
   Routes messages, stores semantic data, and manages lifecycle without embedding domain-specific logic.

2. **Plugins**  
   Separate processes that provide external data or device-specific behavior, such as weather, pricing, or device control.

3. **Solver**  
   Consumes structured inputs and produces optimized schedules for flexible energy usage.

This separation keeps the core generic and makes the system easier to extend over time.

## Current Focus

The project is currently focused on:

- core architecture
- async I/O and runtime behavior
- plugin boundaries
- data flow needed for future optimization logic

## License

Proprietary. See [LICENSE](LICENSE).
