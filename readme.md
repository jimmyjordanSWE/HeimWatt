# HeimWatt

> **Vision**: An extensible, local-first data platform for energy optimization.

## How it works
HeimWatt is a modular broker that ingests data from **IN plugins** (weather, prices, sensors), stores it by semantic type, and routes queries to **OUT plugins** (Solvers, controllers, schedulers) that serve results via REST API.

# Default plugins
- OUT: Complete energy strategy solver as described in [Solver.md](Solver.md)
- live and historical grid energy usage via connection to ?( there are two .png files in root describing the box you connect to in the main house electricity connection box for getting this data. Its hardware, not a api unfortunately. Suggest solutions here. What hardware should we support? IS there a simplr way? put a box in the house instead of at the pole on the street? We must have the actual usage.)
- IN: Nordic and german weather data
- IN: Nordic and german spot prices
- IN: Complete set of simulated sensors and appliances for a standard swedish "villa" house ( For simulation and testing )
- house physics are derived automatically when we have enough sensors and data. (heat loss etc) But provide options to run scheduled tests. For example homeowner is away over the weekend. Measure heat loss for each room and speed of heating at X powerlevels etc.  

## Funcitonality
### Web user interface
All functionality of the program is governed by json config files. These are created graphically in the UI. So EVERYTHING in the program is controlled via the web UI. 
Goal is to make it extremely simple to: 
- Specify hardware, "luftvärmepump" "varmvattenberedare", "golvärme"etc. Create a plugin enable plugin and connect tu ti the appliance.
- specify in data sources. LLM integration to generate plugins from external API documentation. For data ingestion. 
- The solver needs a complex set of inputs, machines availible constraints, sensors etc. All is defined and connected in the nodemased editor. You make devices then bind them together in rooms and set your constraints. (car must be charged at 07:00 every weekday, but can be 40% on weekends, hallway can be 18c, but babys room must be 23c, etc.)

## Status
> HEAVY WIP. Moving fast, breaking things.

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
| vite |web build tool|
| react |web UI|
| litegraph.js | Node editor |


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
