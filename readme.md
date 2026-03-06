# HeimWatt

> **Vision**: An extensible, local-first data platform for energy optimization.

> **Current state**: WIP

## Documentation

- **[Current State](docs/current_state.md)**: Technical architecture, recent performance updates (epoll/async), and status.
- **[Target Vision](docs/target_state.md)**: The "Single Pane of Glass" concept, Solver logic, and final architecture.
- **[Roadmap](docs/roadmap.md)**: The plan to get from Alpha to V1.0.

## Quick Start

### Prerequisites
- Linux (POSIX)
- Clang / GCC
- Make

### Build & Run
```bash
# Build the project
make

# Run the server
./build/heimwatt

# Run with debug logging
make debug && ./build/heimwatt
```

## Tech Stack

| Component | Choice |
|-----------|--------|
| **Core** | C99 (Clang optimized) |
| **I/O** | `epoll` non-blocking event loop |
| **Database** | CSV (Configurable Interval) |
| **Web UI** | React + Vite |
| **Protocols** | HTTP/1.1, Unix Domain Sockets |

## Architecture Overview

HeimWatt operates on a **Pure Broker** model:
1.  **Core**: Routes messages, stores semantic data, manages lifecycle. Zero domain knowledge.
2.  **Plugins**: Separate processes that provide intelligence (Weather, Pricing, Device Control).
3.  **Solver**: Optimizes power schedules based on inputs.

## License

Proprietary. See [LICENSE](LICENSE).
