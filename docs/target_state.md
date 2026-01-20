# Target State of HeimWatt

**Vision:** An extensible, local-first data platform for energy optimization that acts as a "Single Pane of Glass" for the entire home energy system.

## Storage Architecture: The "Wide CSV"

The entire system state is persisted to a single CSV file, prioritizing simplicity and portability over traditional database normality.

### 1. Structure
- **File:** `data/history.csv`
- **Rows:** One row per time interval (e.g., 1 minute). This interval is configurable in the Core config.
- **Columns:** One column per Canonical Unit (Semantic Type).
    - `timestamp` (ISO8601)
    - `atmosphere.temperature`
    - `energy.price.spot`
    - `home.power.total`
    - ...and so on for every semantic type.

### 2. Resampling Strategy
Plugins report data asynchronously. The Core buffers these values.
- **On Tick:** The Core snapshots the **latest known value** for each semantic type.
- **Write:** This snapshot is written as a new row to the CSV.
- **Null Handling:** If no data has been received for a type since startup, the cell is empty.

## Core Philosophy
1.  **Zero Domain Knowledge in Core:** The Core remains a pure broker.
2.  **Simplicity First:** Analysis should be possible with Excel/Pandas directly from the CSV.
3.  **Semantic Composition:** Plugins interact only via standardized columns in the global state.

## The User Experience (Target)

### 1. Unified Control ("Single Pane of Glass")
The user replaces 5+ vendor apps (Heat pump, EV charger, Solar inverter, Grid owner, Smart home hub) with HeimWatt.
- **Dashboard:** Real-time view of all energy flows, costs, and accumulated savings.
- **Control:** Centralized setting of comfort preferences (e.g., "Ready to drive by 07:00").

### 2. Intelligent Automation
- **Price Optimization:** The system automatically shifts heavy loads (EV charging, heating) to cheapest hours.
- **Event Resilience:**
    - **Storm Warnings:** Automatically pre-charges batteries and pre-heats house if a storm is forecast (Smhi Warnings).
    - **Price Spikes:** Asks user for "Profit Mode" (export to grid) vs "Survival Mode".
- **Hardware Agnostic:** Works with any hardware that has a plugin (Official, Community, or Local).

## Technical Architecture (Target)

### 1. The Solver (Brain)
- **Model:** MPC (Model Predictive Control) or MILP (Mixed-Integer Linear Programming).
- **Inputs:** 
    - **Tier 1 Intelegence (Known):** Published spot prices (next 12-35h).
    - **Tier 2 Intelligence (Predicted):** AI/Heuristic predictions of future prices (35h - 7 days) based on weather, wind, and trends.
    - House Physics Model (RC Network).
- **Outputs:** optimal power schedules published as semantic blobs (`schedule.heat_pump.power`).

### 2. House Physics (Self-Learning)
- **Initial:** Uses rough defaults based on house size/age.
- **Learning:** Continuously refines R (Resistance) and C (Capacitance) values by observing how the house reacts to heat input and weather.
- **Result:** Accuracy improves automatically over time, allowing for more aggressive "freewheeling" (turning off heat) during price spikes without losing comfort.

### 3. Hardware Integration
- **Smart Meter (P1):** Real-time grid import/export via SlimmeLezer+ (WiFi) or Tibber Pulse.
- **Sub-metering:** CT Clamps (Shelly EM) for specific heavy loads.
- **Device Control:**
    - **Heat Pumps:** Native API wrappers (MELCloud, Nibe Uplink).
    - **EVs:** Cloud APIs (Tesla Fleet) or Local Chargers (OCPP).
    - **Batteries:** Standardized interface for charge/discharge control.

### 4. Editors
- **Device Definition Editor:** Web UI to create JSON specs for new hardware (supported by LLM for spec lookup).
- **Connection Editor (LiteGraph):** Drag-and-drop wiring of devices to zones.
    - *Example:* [Heat Pump] --(heats)--> [Living Room] --(measured_by)--> [Thermometer].

## Ecosystem
- **Plugin Store:** Official signed plugins (Trust Level: High) vs Community plugins (Trust Level: Warn).
- **Simulation Bundle:** Full virtual house environment for developing and testing plugins without hardware.
