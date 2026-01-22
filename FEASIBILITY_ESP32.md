# Feasibility Study: HeimWatt on ESP32-C6

## 1. Executive Summary
**Feasible?** Yes, but requires a significant architectural pivot ("Port to Embedded").

Running the **exact** current codebase (Linux-based Server) on ESP32-C6 is **NOT feasible** due to dependencies on:
- **DuckDB** (Too heavy, requires OS)
- **Linux/POSIX APIs** (`epoll`, `AF_UNIX` sockets, `dlopen`/`dlsym` for plugins)
- **Multi-Process Model** (ESP32 is single-process, multi-task)

However, achieving the **User Goal** (Run WebUI + Core Logic on ESP32 dongle) is **Highly Feasible** by adapting the architecture.

## 2. Resource Analysis

### Hardware: ESP32-C6
- **Architecture**: RISC-V 32-bit
- **Flash**: 4MB - 16MB (Typically 8MB for modules)
- **RAM**: ~512KB SRAM (usable ~300KB) + LP SRAM
- **Connectivity**: WiFi 6 + BLE 5 (Zigbee/Thread capable)
- **Peripherals**: UART (for P1 port), SPI, I2C

### Software Requirements (Current vs. ESP32)

| Component | Current Implementation | ESP32 Constraint | Solution |
|-----------|------------------------|------------------|----------|
| **WebUI** | React/Vite App (284KB dist) | Flash Storage | **Feasible**. Serve static GZIP files from SPIFFS/LittleFS partition. |
| **Database** | DuckDB + CSV | No OS, Low RAM | **Rewrite**. Use SQLite (supported but heavy) or simple Ring-Buffer in Flash/NVS for time-series. |
| **Networking** | `epoll` based TCP server | LwIP (Sockets/Netconn) | **Rewrite**. Use `esp_http_server` component (robust, supports WebSockets). |
| **Plugins** | `.so` Dynamic Libraries | No `dlopen` | **Refactor**. Statically link plugins at build time or use MicroPython/Lua for dynamic logic. |
| **IPC** | Unix Domain Sockets | No Filesystem Sockets | **Refactor**. Use FreeRTOS Queues and direct function calls. |
| **Threading** | `pthread` | FreeRTOS Tasks | **Adapt**. ESP-IDF supports `pthread` wrapper, but native Tasks are better. |

## 3. Proposed Architecture (ESP32 Edition)

To run "HeimWatt" on a dongle, we propose the **"HeimWatt Embedded"** architecture:

### A. The "Dongle" (ESP32-C6)
1.  **Core Logic**:
    -   Reads P1/RJ12 data via **UART ISR**.
    -   Decodes DSMR/OBIS telegrams (Lightweight parser).
    -   Stores recent history (e.g., last 24h) in **Ring Buffer (RAM/RTC RAM)**.
    -   Stores aggregated stats (hourly/daily) in **NVS/LittleFS**.
2.  **Web Server**:
    -   Uses `esp_http_server`.
    -   Serves the **exact same WebUI** (HTML/JS/CSS) stored in Flash.
    -   Exposes REST/WebSocket API compatible with the WebUI.
3.  **Connectivity**:
    -   WiFi Station (connects to home router).
    -   mDNS (`heimwatt.local`).

### B. Changes Required

#### 1. Build System
-   Switch from `Makefile` to `idf.py` (CMake based ESP-IDF build system).
-   WebUI build process remains `npm run build`, but adds a step to pack `dist/` into a LittleFS image.

#### 2. Codebase Adaptation
-   **`src/net/http_server.c`**: Replace with `httpd_start()` wrappers.
-   **`src/db/db.c`**: Implement a new backend `src/db/embedded_backend.c` using Flash/NVS. drop DuckDB.
-   **`src/core/ipc.c`**: Remove. Intrazonal communication becomes direct function calls.

## 4. Feasibility Rating
-   **WebUI**: 10/10 (Already small enough)
-   **P1/RJ12 Reading**: 10/10 (Native hardware support)
-   **Data Storage**: 5/10 (Limited by Flash endurance/size; need aggressive aggregation or SD card)
-   **Performance**: 8/10 (Sufficient for serving UI and reading meter)

## 5. Recommendation
Proceed with a **Proof of Concept**.
1.  Set up ESP-IDF project.
2.  Flash the `webui/dist` to SPIFFS.
3.  Implement a mock API returning static JSON.
4.  Verify phone/desktop browser can load the HeimWatt UI from the generic ESP32 chip.

## 6. Advanced Computation: Linear Programming (LPS)

The current energy storage optimization uses a **Dynamic Programming (DP)** algorithm.
-   **Complexity**: $O(T \times S \times A)$ where $T$ is time steps, $S$ is battery states, $A$ is actions.
-   **Current Settings**:
    -   $S = 101$ (0-100% capacity)
    -   $DP\_Entry = 28$ bytes
    -   $T = 24$ (typical) to $168$ (max)

### RAM Usage Analysis
| Horizon | Memory Required | Feasibility on ESP32-C6 (300KB RAM) |
| :--- | :--- | :--- |
| **24 Hours** | ~66 KB | **Feasible** (Leaves ~230KB for WiFi/App) |
| **48 Hours** | ~132 KB | **Risky** (May conflict with TLS buffers) |
| **1 Week** | ~464 KB | **Impossible** without PSRAM |

### CPU Performance Analysis
-   **Algorithm**: Dynamic Programming (Iterative, predictable).
-   **Complexity**: ~$8.7 \times 10^6$ operations per 24h solve (Hourly steps, 101 precise states).
-   **Processor**: ESP32-C6 (RISC-V 32-bit @ 160MHz). **No Hardware FPU**.
-   **Time Estimate**:
    -   Software Floating Point: ~10-20 cycles per op.
    -   Total Cycles: ~$1.7 \times 10^8$.
    -   Execution Time: **~1.1 seconds** at 160MHz.
-   **Verdict**: **Pass**. Solving once per 15 minutes is trivial (< 0.2% CPU load).

### Recommendation
1.  **Restrict Horizon**: Limit optimization to 24-36 hours.
2.  **Reduce Precision**: Lower `BATTERY_STATES` from 101 to 50 (2% steps) to halve memory usage.
3.  **Use PSRAM**: If 7-day lookahead is required, use an ESP32-C6 module with 2MB PSRAM.

## 7. Plugin System & Concurrency

The user correctly identified that the Linux **Process-based Plugin Model** cannot work on ESP32.

### Constraints
-   **No `fork()`/`exec()`**: Cannot spawn separate processes.
-   **No `dlopen()`**: Cannot load shared libraries (`.so`) at runtime.
-   **No Memory Isolation**: A crash in a plugin crashes the entire device.

### Proposed Architecture: "Monolithic Firmware"
1.  **Static Linking**: All "core" plugins (Source, pricing, LPS) are compiled directly into the `main` firmware image.
2.  **FreeRTOS Tasks**: instead of a "yielding state machine", we can use **FreeRTOS Tasks** (threads).
    -   Core: High priority.
    -   Plugins: Low priority tasks.
    -   *Risk*: If a plugin gets stuck in a `while(1)`, the Watchdog Timer (WDT) will reset the system.
3.  **Scripting (Future)**: For user-defined plugins, embed a lightweight interpreter (e.g., **Lua** or **MicroPython**). This allows "safe" execution without recompiling firmware, but costs RAM.

### Verdict
-   **First-Party Plugins**: Port to C, statically link. Feasible.
-   **First-Party Plugins**: Port to C, statically link. Feasible.
-   **Third-Party Plugins**: Disable for MVP. Future: WebAssembly (WASM3) or Lua.

## 8. Hardware Alternatives

**Baseline**: ESP32-C6 (160MHz RISC-V, No FPU, ~300KB RAM). Capable of LP solver < 36h horizon.

### Steps Down (Lower Cost/Power/Performance)
1.  **ESP32-C3** (-1 Step):
    -   **Pros**: Pin-compatible, cheaper.
    -   **Cons**: Same performance (160MHz No-FPU), fewer GPIOs, older WiFi 4.
    -   **Verdict**: **Possible**, but no benefit over C6.
2.  **RP2040 (Pico W)** (-2 Steps):
    -   **Pros**: Excellent documentation, dual-core.
    -   **Cons**: Slower (133MHz), **No FPU**, software float is ~60x slower than ESP32 hardware float.
    -   **Verdict**: **Avoid** for math-heavy LP solver tasks.
3.  **nRF52840** (-3 Steps):
    -   **Pros**: Utra-low power, has FPU (Cortex-M4F).
    -   **Cons**: Very slow (64MHz), expensive.
    -   **Verdict**: **Fail**. Too slow for heavy computation.

### Steps Up (Higher Performance/Precision)
1.  **ESP32-S3** (+1 Step):
    -   **Pros**: 240MHz Dual Core, **Hardware FPU (Single Precision)**, SIMD instructions, PSRAM support (up to 8MB).
    -   **Cons**: Higher power consumption.
    -   **Verdict**: **Highly Recommended**. The FPU speeds up LP solver 10x+. PSRAM enables 7-day horizon.
2.  **Teensy 4.1 (i.MX RT1062)** (+2 Steps):
    -   **Pros**: 600MHz Cortex-M7, **Double Precision FPU**.
    -   **Cons**: No built-in WiFi (needs dongle), expensive, different ecosystem.
    -   **Verdict**: **Overkill** unless we need sub-second solving of massive problems.
3.  **Raspberry Pi Zero 2 W** (+3 Steps):
    -   **Pros**: 1GHz Quad Core, 512MB RAM, Runs Linux (Original Code works 100%).
    -   **Cons**: High power (>100mA idle), long boot time, SD card corruption risk.
    -   **Cons**: High power (>100mA idle), long boot time, SD card corruption risk.
    -   **Verdict**: **Best for Compatibility**. No porting required, but loses "embedded reliability".

## 9. Future Scalability: 1000+ Units

The requirement to control **thousands of units** (sensors/machines) with complex constraints fundamentally changes the hardware verification.

### Analysis
-   **Algorithm**: "Full-featured LP Solver" (e.g., Simplex/Interior Point). Requires solving generic matrix equations.
-   **Memory**: A problem with 1000+ variables/constraints typically requires **MegaBytes of RAM** (e.g., Sparse Matrix overhead, factorization tables).
-   **Compute**: Solving large LPs allows $O(n^2)$ or $O(n^3)$ complexity.

### Hardware Verdict
-   **ESP32-C6/S3**: **Impossible**. Too little RAM (even with 8MB PSRAM, it's slow).
-   **Teensy 4.1**: **Plausible** but hard to implement (custom firmware).
-   **Raspberry Pi Zero 2**: **Feasible**. Linux handles virtual memory and standard solvers (GLPK, CBC, HiGHS) run natively.

### Strategic Recommendation
If the goal is "Thousands of Units":
1.  **Drop ESP32** for the central coordinator. Use it only as a "dumb" satellite node (Input/Output).
2.  **Target Linux**: Use Raspberry Pi Zero 2 W (or CM4) as the "HeimWatt Pro" controller.
3.  **Hybrid Architecture**:
    -   *Edge (ESP32)*: Reads sensors, executes simple commands, safety fallbacks.
    -   *Edge (ESP32)*: Reads sensors, executes simple commands, safety fallbacks.
    -   *Core (Pi/Server)*: Runs the heavy LP solver for the swarm of 1000 units.

## 10. Cloud Offloading (The "Thin Client" Model)

Since the device **must** connect to the Internet to fetch Price (Nordpool) and Weather (SMHI) data anyway, offloading the optimization logic to the cloud is a strong architectural candidate.

### Architecture
-   **ESP32 (Dongle)**:
    -   Role: Telemetry & Actuation only.
    -   Logic: "Measure P1 -> Send MQTT/HTTP -> Receive Setpoint -> Actuate".
    -   Fallback: If cloud unreachable > 15m, revert to simple "Charge if SoC < 50%" logic.
-   **Cloud Server (Lambda/VPS)**:
    -   Role: The Brain.
    -   Logic: Fetches prices, weather, and device telemetry. Runs heavy LP/Simplex solver (Python/Gurobi/CBC). Returns optimal schedule.

### Pros & Cons
| Feature | Edge Computing (On-Chip) | Cloud Computing (Offload) |
| :--- | :--- | :--- |
| **Scalability** | Limited (1 unit) | **Unlimited** (1000s of units) |
| **Hardware cost** | Higher (Needs S3/P4) | **Lowest** (C6/C3 is sufficient) |
| **Reliability** | **High** (Works offline) | Low (Needs 100% uptime) |
| **Maintenance** | Hard (OTA Updates) | **Easy** (Server deploy) |
| **Privacy** | **High** (Local data) | Low (Data leaves premise) |

### Verdict
For the "Thousands of Units" goal, **Cloud Offloading is the only viable path** if you want to use cheap hardware (ESP32-C6). It allows you to use full Python/SciPy solvers without memory constraints.

## 11. Business Case: The "Freemium" Model

The user's insight is critical: **The Solver does not just save money; it ensures physical safety and comfort.**

### A. The "Solver" is actually a MILP (Mixed-Integer Linear Program)
Reading the `old_docs/docs/Solver.md` reveals the system uses **Model Predictive Control (MPC)** to manage:
1.  **Thermal Mass**: Pre-heating the concrete floor (Underfloor Heating) to store energy.
2.  **Safety Constraints**: STRICT limits on $T_{floor} < 29^\circ C$ (to prevent cracking) and $T_{room}$ comfort bounds.
3.  **Complex Couplings**: Coordinating Heat Pump (COP curves) + Battery + Solar + Grid Price.

**This assumes a "Basic Dongle" heuristic (e.g., "Charge at night") is UNSAFE or INEFFECTIVE because it lacks the thermal model ($R/C$ parameters) to prevent overheating.**

### B. Value Proposition Refined
*   **The Product**: A "Comfort & Savings Engine", not just a switch.
*   **The Dongle (Free Tier)**:
    *   **Logic**: **Rule-Based Control (RBC)**. User sets simple "If-Then" automations.
    *   **Examples**: *"If Price > 2 SEK, set Heat to 18°C"*, *"If Price < 0.5 SEK, turn on Water Heater"*.
    *   **Pros**: Transparency, total user control, works offline.
    *   **Cons**: User must manually tune rules. Sub-optimal for complex thermal storage.
*   **The Cloud (Premium Tier)**:
    *   **Logic**: **Model Predictive Control (MPC)**. Solver calculates optimal path 24h ahead.
    *   **Examples**: *System pre-heats floor at 04:00 AM because it knows price spikes at 08:00 AM and it's cold outside.*
    *   **Value**:
        *   **Arbritrage**: ~450 SEK/month savings.
        *   **Comfort**: Stable temps, no overheating.
        *   **Safety**: Protects floor infrastructure.
    *   **Unlock**: Users **must** subscribe to get the "Smart" features because the computation (MILP) cannot run on the ESP32.

### C. Pricing Logic
*   **Hardware**: $39 USD (One-time). "Gateway Drug".
*   **Subscription**: **59 - 79 SEK/month**.
    *   *Justification*: It's not just "extra savings"; it's the specific feature that makes the system work as an "AI Energy Manager". Without it, it's just a dumb thermostat.

## 12. Conclusion & Roadmap
1.  **Phase 1 (MVP)**: Develop ESP32-C6 Firmware (Thin Client) + Basic Cloud Orchestrator.
2.  **Phase 2**: Port existing Python Solver (`EMHASS`/MILP) to a scalable Cloud Service (Lambda/Docker).
3.  **Phase 3**: Ship Dongles.

## 13. Competitive Landscape (Nordics & Germany)

Does this product ("Smart Dongle + MPC Subscription") already exist? **Yes**, but with different business models.

### A. The "Full Service" Energy Companies
*   **Tibber (Nordics/Germany)**:
    *   **Model**: Sell electricity (Dynamic Price). Smart control is a "free" perk to attract customers.
    *   **Hardware**: "Tibber Pulse" (P1 reader) + Third-party integrations (Nibe/Heat pumps).
    *   **Gap**: Customers **must** switch electricity provider to Tibber. You cannot use Tibber's smart control with Vattenfall/E.ON.
*   **1Komma5° (Germany/Nordics)**:
    *   **Model**: High-end ecosystem. "Heartbeat" device (599 EUR) + 15 EUR/month AI fee.
    *   **Gap**: Very expensive hardware entry point. Focused on selling batteries/solar panels.

### B. The "Hardware + Service" Players
*   **Ngenic Tune (Sweden)**:
    *   **Model**: Hardware Rental Subscription (~99 SEK/month). Includes gateway + indoor sensor.
    *   **Tech**: Offsets the outdoor temperature sensor of the heat pump.
    *   **Gap**: Hardware is "rented", not owned. Higher monthly fee.
*   **Tado (Germany/Europe)**:
    *   **Model**: Sell Hardware (Thermostats). Optional "Auto-Assist" subscription (~3 EUR/month).
    *   **Gap**: Focused on Radiators/AC, less on "Whole House Thermal Mass" or P1-meter optimization.

### C. The HeimWatt Opportunity
Your competitive edge is the **Unbundled Model**:
1.  **Provider Agnostic**: Works with *any* energy provider (unlike Tibber).
2.  **Low Capex**: Buy hardware for $39 (unlike 1Komma5's 600 EUR).
3.  **Specific Niche**: "Underfloor Heating Thermal Mass Optimization". Ngenic does this, but their business model is rental-based. Tado ignores thermal mass.

**Verdict**: The specific proposition of "Cheap Owned Hardware + Advanced MPC Subscription" has a valid gap in the market, especially for users who want optimization *without* switching electricity providers.
