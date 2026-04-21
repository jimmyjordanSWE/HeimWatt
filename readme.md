# HeimWatt

> Local-first energy optimization platform for coordinating flexible loads and system data.
>
> Current status: work in progress.

## Overview

HeimWatt is a modular platform for home energy optimization. It is designed to collect system data, route messages between components, and support scheduling decisions for flexible loads such as heat pumps, EV chargers, and batteries.

The long-term goal is to provide a local-first foundation for energy-aware automation, with a clear separation between core infrastructure and domain-specific logic.

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
make
./build/heimwatt

# Debug build
make debug && ./build/heimwatt
