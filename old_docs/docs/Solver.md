# **Comprehensive Optimization Architectures for Residential Microgrids: Integrating Model Predictive Control with Complex Thermal Dynamics and Variable Pricing Markets**

## **1\. Introduction: The Evolution of Residential Energy Management**

The global energy landscape is undergoing a fundamental paradigm shift, transitioning from a centralized, unidirectional generation model to a decentralized, bi-directional network defined by the proliferation of distributed energy resources (DERs). Within this evolving ecosystem, the residential sector has emerged as a critical node of flexibility. The modern electrified residence is no longer merely a passive sink of energy but a complex microgrid characterized by stochastic generation (photovoltaics), intermediate storage (electrochemical batteries), and flexible, high-inertia thermal loads (heat pumps, underfloor heating, water heaters).

The user's query—how to calculate optimal energy usage given a specific set of assets including insulation factors, temperature sensors, battery storage, solar generation, and grid connectivity—touches upon the frontier of building energy management. The complexity of this problem space is compounded by the specific market constraint of day-ahead dynamic pricing, where electricity buy/sell prices are released daily at 13:00 for the subsequent 24-hour period. This creates a discontinuous information horizon that renders traditional heuristic control strategies obsolete.

This report posits that the only viable methodology to achieve true optimality in such a system is the implementation of **Model Predictive Control (MPC)** formulated as a **Mixed-Integer Linear Programming (MILP)** problem. Unlike Rule-Based Control (RBC), which relies on static "if-then" logic, MPC utilizes a mathematical model of the building's physics and the efficiency curves of its mechanical systems to predict future states over a finite horizon.1 By minimizing a cost function subject to constraints—such as thermal comfort bounds and battery state-of-charge limits—MPC effectively transforms the home into a virtual power plant capable of arbitrage, peak shaving, and self-consumption maximization.

The following analysis provides an exhaustive technical roadmap for designing, modeling, and deploying such a system. It synthesizes advanced thermodynamic modeling of building envelopes, polynomial characterization of heat pump efficiency, and the software architecture required to solve high-dimensional optimization problems on standard home automation hardware.

## ---

**2\. Mathematical Framework of the Optimization Problem**

To calculate optimal energy usage, the physical reality of the house must be abstracted into a mathematical formulation solvable by computational algorithms. The standard approach for this class of problem is Mixed-Integer Linear Programming (MILP), which allows for the simultaneous optimization of continuous variables (e.g., power flow in Watts) and binary variables (e.g., the on/off state of a heat pump to prevent short-cycling).

### **2.1 The Objective Function**

The core of the MPC controller is the objective function ($J$), a scalar value representing the total cost of operation over a prediction horizon ($H$). The solver’s goal is to minimize this value. Given the user's access to 15-minute interval grid prices, the horizon is discretized into time steps ($k$) of length $\Delta t = 0.25$ hours.

The general form of the economic objective function is expressed as:

$$J = \min_{\mathbf{u}} \sum_{k=0}^{H} \left( C_{grid}^{buy}(k) \cdot P_{grid}^{buy}(k) - C_{grid}^{sell}(k) \cdot P_{grid}^{sell}(k) + C_{deg}(P_{batt}(k)) + \alpha \cdot \Psi_{comfort}(k) \right) \cdot \Delta t$$

Where:

* $P_{grid}^{buy}(k)$ and $P_{grid}^{sell}(k)$ represent power imported and exported from the grid, respectively.4  
* $C_{grid}^{buy}(k)$ is the forecasted cost of electricity imports. This vector is populated by the 13:00 price release (e.g., Nord Pool spot prices plus transmission fees and taxes).6  
* $C_{grid}^{sell}(k)$ is the revenue from exports, often the spot price minus a margin, or a feed-in tariff.7  
* $C_{deg}$ represents the cost of battery degradation, preventing the solver from engaging in "micro-cycling" where the marginal profit is lower than the wear on the cell chemistry.8  
* $\Psi_{comfort}(k)$ is a penalty term for thermal discomfort, weighted by factor $\alpha$. This allows the system to treat temperature bounds as "soft constraints," permitting slight deviations for significant economic gain, a technique shown to enhance solver feasibility.1

### **2.2 System Constraints and Power Balance**

The optimization is bound by physical and operational constraints. The most fundamental is the electrical power balance equation, which must hold true at every time step $k$. This equation ensures that the sum of all power sources equals the sum of all power sinks:

$$P_{PV}(k) + P_{grid}^{buy}(k) + P_{batt}^{dis}(k) = P_{load}^{base}(k) + P_{HP}(k) + P_{DHW}(k) + P_{batt}^{ch}(k) + P_{grid}^{sell}(k)$$

Here, $P_{load}^{base}(k)$ represents the forecasted non-shiftable load (lighting, cooking, entertainment), while $P_{HP}(k)$ and $P_{DHW}(k)$ are the decision variables for the heat pump and water heater, respectively.4

**Table 1: Variable Classifications in MILP Formulation**

| Variable Category | Examples | Mathematical Nature | Source |
| :---- | :---- | :---- | :---- |
| **Disturbances (Inputs)** | Ambient Temp ($T_{amb}$), Solar Irradiance ($I_{sol}$), Base Load ($P_{load}$) | Deterministic Forecasts | Weather API / ML Model |
| **Decision Variables (Outputs)** | Grid Import ($P_{grid}$), Battery Charge ($P_{batt}^{ch}$), Heat Pump Power ($P_{HP}$) | Continuous ($\mathbb{R}_{\geq 0}$) | Optimization Solver |
| **State Variables** | Indoor Temp ($T_{in}$), Battery SoC ($SoC$), Tank Temp ($T_{tank}$) | Dependent Variables | System Dynamics ($x_{k+1} = Ax_k + Bu_k$) |
| **Binary Variables** | HP Status (On/Off), Defrost Cycles | Binary ($\{0, 1\}$) | Mixed-Integer Logic |

### **2.3 The "13:00 Gap" and Rolling Horizon Strategy**

A unique constraint identified in the user requirements is the release mechanism of grid prices: "Each day 13:00 24h 15m interval grid buy sell prices are released." This creates a variable information horizon that the MPC strategies must accommodate. Standard MPC uses a fixed receding horizon (e.g., always looking 24 hours ahead), but in this market structure, the known pricing horizon shrinks as the day progresses until the next update.

The Shrinking vs. Expanding Horizon:  
At 12:00 on Day $D$, the system only knows prices until 23:59 on Day $D$ (12 hours of visibility). At 13:00 on Day $D$, prices for Day $D+1$ are released, suddenly expanding the visibility to 35 hours (remainder of Day $D$ + all of Day $D+1$).  
To optimize effectively, the system must employ a **dynamic horizon strategy**:

1. **Day-Ahead Optimization (Global Planner):** Triggered immediately upon price release (e.g., 13:05). This solves for the full available horizon (up to 35 hours). Its primary purpose is to establish the optimal State of Charge (SoC) trajectory for the battery and the thermal mass of the floor at the end of the current day ($SoC_{final}$), ensuring the system does not deplete its reserves right before a price spike the next morning.6  
2. **Intra-Day MPC (Local Tracker):** Triggered every 15 minutes. It solves a shorter horizon problem (e.g., 12-24 hours) to correct for forecast errors in solar generation or consumption. Crucially, it must respect the terminal constraints set by the Global Planner.

This dual-layer approach allows the system to be both strategic (planning for tomorrow's prices) and reactive (adjusting for passing clouds).6

## ---

**3\. Thermodynamic Modeling of the Building Envelope**

The user explicitly mentions "insulation factor," "temp sensors," and "underfloor heating." To calculate optimal energy usage, these physical properties must be translated into a dynamic thermal model. In the context of MPC, this is achieved using **Grey-Box Modeling**, specifically Resistance-Capacitance (RC) networks. This method sits between "White-Box" models (complex physical simulations like EnergyPlus, computationally too heavy for real-time control) and "Black-Box" models (pure neural networks, which lack physical interpretability).14

### **3.1 The RC Network Analogy**

The building is modeled as an electrical circuit where:

* **Temperature ($T$)** is analogous to Voltage ($V$).  
* **Heat Flow ($\dot{Q}$)** is analogous to Current ($I$).  
* **Thermal Resistance ($R$)** represents the insulation properties (Inverse of U-value).  
* **Thermal Capacitance ($C$)** represents the thermal mass (concrete slab, drywall, furniture).

While a simple 1R1C model (one resistor, one capacitor) can approximate a lightweight structure, a house with **Underfloor Heating (UFH)** requires a higher-order model to capture the delay between heat injection into the floor slab and the rise in air temperature. Literature suggests a **3R2C** or **5R5C** model offers the optimal trade-off between accuracy and computational load for residential buildings.14

### **3.2 The 2R2C State-Space Formulation**

A robust baseline for the user's scenario is the 2R2C model, which separates the indoor air node ($T_{i}$) from the building envelope/structure node ($T_{m}$). This separation is critical for UFH systems because the heating element interacts directly with the structure, not the air.

The system dynamics are described by the following coupled differential equations:

$$C_i \frac{dT_i}{dt} = \frac{T_a - T_i}{R_{win}} + \frac{T_m - T_i}{R_{int}} + gA_{win}I_{sol} + \phi_{int}$$

$$C_m \frac{dT_m}{dt} = \frac{T_i - T_m}{R_{int}} + \frac{T_a - T_m}{R_{ext}} + (1-g)A_{win}I_{sol} + \phi_{heat}$$

Where:

* $T_a$: Ambient outdoor temperature (from the 73h weather forecast).  
* $R_{win}$: Thermal resistance of windows/infiltration.  
* $R_{ext}$: Thermal resistance of opaque walls (the user's "insulation factor").  
* $R_{int}$: Thermal resistance between the mass (walls/floor) and the air.  
* $C_i$: Heat capacity of the air volume.  
* $C_m$: Heat capacity of the thermal mass (the "battery" effect of the house).  
* $\phi_{heat}$: Heating power injected by the UFH. Notice it charges $T_m$ (the mass) first, explaining the system's latency.10

### **3.3 System Identification and Parameter Estimation**

While the user "knows" their insulation factor (likely the $U$-value or $R$-value of the walls), the thermal capacitance ($C_m$) is rarely known precisely. It represents the aggregate heat storage capability of tonnes of concrete, wood, and gypsum. To calculate optimal energy usage, these parameters must be estimated from data—a process known as **System Identification**.

The modestpy Workflow:  
The user can employ Python tools like modestpy or scipy.optimize.least_squares to derive these parameters using historical data collected from the "temp sensors in rooms".18

1. **Data Collection:** Gather 10-14 days of data containing:  
   * Indoor Temperature ($T_{room}$)  
   * Outdoor Temperature ($T_{amb}$)  
   * Global Horizontal Irradiance ($I_{sol}$)  
   * Heating Power Input ($P_{heat}$)  
2. **Excitation:** The data should ideally contain periods of heating and periods of free-floating cooling to allow the solver to distinguish between insulation (steady-state loss) and capacitance (transient decay).20  
3. **Optimization:** The algorithm adjusts the $R$ and $C$ values in the differential equations until the modeled temperature matches the historical measured temperature with minimal error (RMSE).  
4. **Result:** The output is a tuned physics model specific to the user's house, enabling the MPC to predict exactly how long the house will stay warm if heating is turned off—essential for bridging the price peaks.

## ---

**4\. Asset Characterization: Heat Pumps and Underfloor Heating**

The optimization engine requires accurate models of the energy conversion devices. Treating a heat pump (HP) as a fixed-efficiency heater (e.g., COP = 3.0) leads to suboptimal decisions, as efficiency varies drastically with ambient conditions.

### **4.1 Polynomial Curve Fitting for Heat Pump COP**

The Coefficient of Performance (COP) of an Air-Source Heat Pump (ASHP) is a non-linear function of the source temperature ($T_{amb}$) and the sink temperature ($T_{supply}$, determined by the UFH flow temperature). To integrate this into a MILP framework, the COP is typically approximated using a polynomial regression based on manufacturer datasheets or standard libraries like hplib.21

The general biquadratic approximation used in simulation is:

$$\text{COP}(T_{amb}, T_{supply}) = c_0 + c_1 T_{amb} + c_2 T_{supply} + c_3 T_{amb}^2 + c_4 T_{supply}^2 + c_5 T_{amb} T_{supply}$$

Refrigerant-Specific Efficiencies:  
The specific refrigerant used in the HP significantly impacts this curve. Research indicates that R32 units exhibit 5-9% higher capacity and efficiency compared to R410A units under identical conditions due to superior thermodynamic properties.23 If the user's manual specifies the refrigerant, this factor should be included in the curve selection.  
Linearization for MILP:  
Since dividing by a variable COP in the cost function ($Cost = \frac{Q_{heat}}{COP} \cdot Price$) creates a non-linear problem, a common technique is to pre-calculate the COP vector for the entire prediction horizon.

1. Read the 73h weather forecast for $T_{amb}$.  
2. Assume a weather-compensated flow temperature for $T_{supply}$ (e.g., $35^\circ$C at $0^\circ$C ambient, $30^\circ$C at $10^\circ$C ambient).  
3. Compute the COP for every 15-minute interval in the horizon.  
4. Pass this fixed time-series vector to the MILP solver, rendering the problem linear.25

### **4.2 Underfloor Heating as Thermal Storage**

Underfloor heating (UFH) systems are characterized by high thermal inertia. The concrete screed (often 5-10 cm thick) acts as a massive thermal battery.

* **Strategy:** The MPC can "pre-charge" this mass by running the HP when electricity is cheap (e.g., 03:00-05:00) or when solar is abundant. The heat is stored in the floor and slowly radiates into the room, maintaining comfort during evening peak hours (17:00-20:00) without running the HP.  
* **Capacity:** A 100 $m^2$ concrete slab of 10 cm thickness can store roughly 5-7 kWh of thermal energy for every 1°C rise in temperature. This is often comparable to or larger than the electrical battery capacity.27  
* **Constraints:** To avoid user discomfort or floor damage, the surface temperature must be constrained (typically $T_{floor} < 29^\circ$C), and the room temperature change rate limited.

## ---

**5\. Energy Storage Systems: Chemical and Thermal Integration**

Optimality is achieved by coordinating the chemical battery (BESS) and the thermal battery (UFH/DHW).

### **5.1 Battery Energy Storage System (BESS) Modeling**

The BESS allows for electrical arbitrage (buy low, sell high) and solar self-consumption. The model must track the State of Charge (SoC) evolution:

$$SoC(k+1) = SoC(k) + \left( \eta_{ch} P_{batt}^{ch}(k) - \frac{P_{batt}^{dis}(k)}{\eta_{dis}} \right) \cdot \frac{\Delta t}{E_{cap}}$$

Where:

* $\eta_{ch}, \eta_{dis}$: Charging and discharging efficiencies (typically 0.95, resulting in ~0.90 round-trip).  
* $E_{cap}$: Total energy capacity (kWh).

Degradation Costs:  
To ensure the battery is used economically, a degradation cost is added to the objective function. If the profit from a charge/discharge cycle is less than the cost of the wear on the battery, the solver should choose inaction.

$$C_{deg} = \frac{\text{Replacement Cost (€)}}{\text{Cycle Life} \times 2 \times E_{cap}}$$

This typically results in a "hurdle rate" of roughly €0.05 - €0.10 per kWh cycled.8

### **5.2 Coupled Optimization**

The power of MPC lies in the coupling of assets.

* **Scenario:** It is a sunny winter day. Grid prices are low at night (03:00) and high in the evening (18:00).  
* **Decoupled/Rule-Based Action:** Charge battery from solar. Heat house when cold.  
* **Coupled MPC Action:**  
  1. **03:00:** Pre-heat the UFH using grid power (high efficiency, low cost) to $22^\circ$C.  
  2. **12:00:** Use Solar PV to charge the electrical battery and top up the Domestic Hot Water (DHW) to $60^\circ$C (maximum thermal storage).  
  3. **18:00:** Discharge electrical battery to run lights/appliances. The UFH is off, but the house remains warm due to the thermal mass charged at 03:00.  
  4. **Result:** Minimal grid import during peak prices, maximum utilization of cheap grid and free solar.29

## ---

**6\. Forecasting Architectures**

The quality of the optimal plan is strictly limited by the quality of the forecasts. The user has access to a 73h weather forecast, which is the foundational dataset.

### **6.1 Solar Irradiance to Power**

The weather forecast provides Global Horizontal Irradiance (GHI) or Direct Normal Irradiance (DNI). This must be converted to electrical power ($P_{PV}$) using a geometric model of the solar array (Azimuth, Tilt, Efficiency). The pvlib Python library is the industry standard for this conversion, accounting for the angle of incidence and temperature coefficient of the panels.7

### **6.2 Load Forecasting with Machine Learning**

Predicting the "base load" (non-shiftable consumption like lights and cooking) is difficult due to human stochasticity. However, distinct patterns exist (morning peaks, evening peaks).

* **Method:** Utilizing the **EMHASS ML Forecaster**, a regressor (e.g., Random Forest or K-Nearest Neighbors) is trained on the historical load data (which the user possesses via Home Assistant) and the weather features.  
* **Input Features:** Hour of day, Day of week, Ambient Temperature, Solar Irradiance.  
* **Performance:** Machine learning approaches generally reduce forecast error (RMSE) by 15-20% compared to naive "persistence" models (assuming tomorrow = today), directly translating to better MPC decisions.32

## ---

**7\. Software Implementation: EMHASS and Home Assistant**

To answer the user's specific request ("How do I calculate..."), we must recommend a concrete implementation pathway. The most aligned open-source tool for this specific set of constraints—Home Assistant integration, Python-based, optimization-focused—is **EMHASS (Energy Management for Home Assistant)**.2

### **7.1 Architecture Overview**

The system architecture consists of three layers:

1. **The Data Layer (Home Assistant):** Collects sensor data (room temps, battery SoC, power flows) and external data (Nord Pool prices, weather forecast).  
2. **The Optimization Layer (EMHASS):** A Python container running a web server. It receives data via API, builds the MILP matrices using the PuLP library, passes them to a solver (COIN-OR CBC or HiGHS), and returns the optimal schedule.  
3. **The Control Layer (Automations):** Home Assistant automations that trigger the optimization and dispatch the setpoints to the physical devices via Modbus or vendor APIs.36

### **7.2 Configuration for the "13:00 Price Gap"**

The user's constraint regarding the 13:00 price release requires a specific automation strategy in the automation.yaml file of Home Assistant.

**Table 2: Automation Logic for Variable Horizon**

| Trigger Time | Action | Description | Horizon Configuration |
| :---- | :---- | :---- | :---- |
| **05:30** | shell_command.mpc_optim | Intra-day adjustment. Corrects for overnight drain or weather changes. | ~18 hours (Until midnight) |
| **13:05** | shell_command.dayahead_optim | **The Global Plan.** Runs immediately after prices for tomorrow are released. Optimizes battery and UFH strategy for the next 35 hours. | ~35 hours (Until midnight tomorrow) |
| **Every 15m** | shell_command.mpc_optim | **The Tracker.** Re-runs optimization to handle immediate cloud cover or load spikes, respecting the SoC targets from the 13:05 plan. | Receding (Variable) |

To implement this, the shell command must dynamically calculate the prediction horizon based on the current time, or use the robust "Day-Ahead + Naive MPC" combination supported by EMHASS.11

### **7.3 Solver Selection**

The choice of solver impacts the calculation speed and stability.

* **COIN-OR CBC:** The default open-source solver. Reliable but can be slow for large horizons (e.g., 48h at 15m intervals = 192 steps).  
* **HiGHS:** A newer, high-performance open-source solver. Benchmarks suggest it solves residential MILP problems 2-5x faster than CBC. For a 15-minute interval system, HiGHS is recommended to ensure the calculation completes well within the time step.39

## ---

**8\. Comparative Analysis: MILP vs. Reinforcement Learning**

The user might encounter literature suggesting Reinforcement Learning (RL) for this task. It is crucial to understand why MPC/MILP is preferred for this specific residential application.

**Table 3: Comparison of Control Strategies for Residential HEMS**

| Feature | Rule-Based Control (RBC) | Model Predictive Control (MPC) | Reinforcement Learning (RL) |
| :---- | :---- | :---- | :---- |
| **Optimality** | Low (Heuristic) | High (Mathematically Proven) | Variable (Convergence dependent) |
| **Data Requirements** | None | System Parameters ($R, C, COP$) | Massive Training Data (Months/Years) |
| **Constraint Handling** | Rigid (Hard limits) | **Guaranteed (Hard Constraints)** | Soft (Penalty-based violations) |
| **Interpretability** | High | High (Traceable Logic) | Low (Black Box) |
| **Setup Effort** | Low | Medium (Modeling) | High (Training/Tuning) |

**Why MILP Wins:**

1. **Price Spikes:** In a dynamic pricing market, a single hour of extreme prices (e.g., €2.00/kWh) requires absolute certainty that the battery will not discharge. MPC guarantees this via constraints. RL, which learns probabilities, might "explore" a discharge action during a spike, incurring massive costs.40  
2. **Thermodynamic Safety:** Underfloor heating has strict limits to prevent floor cracking or user discomfort. MPC treats these as hard constraints. RL typically treats them as negative rewards, meaning the agent might violate temperature limits during the learning phase.41  
3. **Data Efficiency:** The user has a single house. MPC can be calibrated in 2 weeks using modestpy. RL would require the house to run sub-optimally for a full year to experience all seasonal variations before it learns an optimal policy.42

## ---

**9\. Advanced Implementation Strategies**

### **9.1 Handling Forecast Uncertainty**

Forecasts are never perfect. A "Day-Ahead" plan generated at 13:00 might assume a sunny afternoon tomorrow. If the weather changes to overcast, the rigid plan fails.

* **Strategy:** The **Receding Horizon** (every 15m) re-optimization is the countermeasure. By updating the forecast inputs with the latest "current state" (actual SoC, actual Temp) every 15 minutes, the system self-corrects. If solar generation drops, the new optimization path will immediately trigger a grid charge or load shed to compensate.6

### **9.2 The Rebound Effect in Thermal Control**

A common risk in optimizing thermal mass is the "Rebound Effect." If the optimization turns off the heating for 4 hours to avoid high prices, the internal temperature drops. When heating resumes, the heat pump may run at maximum power (low efficiency/high COP penalty) to recover.

* **Mitigation:** The objective function should include a **load smoothing** term or a penalty for high power ramp-rates ($\Delta P_{HP}$). Alternatively, implementing **soft constraints** on temperature allows the solver to let the temperature drift slightly below the setpoint (e.g., $19.5^\circ$C instead of $20.0^\circ$C) rather than surging power to maintain a rigid line.1

## ---

**10\. Conclusion and Roadmap**

To calculate optimal energy usage for the specified residential scenario, a static calculation is insufficient. The solution requires a dynamic, feedback-driven control loop. The recommended roadmap for the user is:

1. **Data Aggregation:** Centralize all sensor data (Temp, Grid, Solar, Battery) into Home Assistant.  
2. **System Identification:** Use the modestpy library to estimate the $R$ and $C$ parameters of the building envelope using 2 weeks of historical sensor data. This transforms the "insulation factor" into a dynamic thermal model.  
3. **Efficiency Modeling:** Implement a polynomial COP curve for the heat pump (using hplib), explicitly accounting for the identified refrigerant (R32/R410A) and flow temperatures.  
4. **Optimization Engine:** Deploy **EMHASS**. Configure it with:  
   * **Method:** MPC (Model Predictive Control).  
   * **Solver:** HiGHS or CBC.  
   * **Horizon:** Dynamic, handling the 13:00 Nord Pool price release via a "Day-Ahead" global plan and "Intra-Day" local tracking.  
5. **Thermal Storage:** Explicitly model the Underfloor Heating as a flexible asset, allowing the solver to "overheat" the floor slab during cheap hours (Thermal Arbitrage).

By implementing this architecture, the user moves beyond simple automation to a true **Energy Management System**, capable of reducing operational costs by 15-30% while maintaining superior thermal comfort through predictive action.

#### **Citerade verk**

1. Energy Management Using Deep Learning-Based Model Predictive Control (MPC), hämtad januari 16, 2026, [https://www.youtube.com/watch?v=LRUy2MYjpus](https://www.youtube.com/watch?v=LRUy2MYjpus)  
2. EMHASS \- David Hernández Webpage \- Google Sites, hämtad januari 16, 2026, [https://sites.google.com/site/davidusb/projects/emhass](https://sites.google.com/site/davidusb/projects/emhass)  
3. An EMS based on Linear Programming — emhass 0.15.3 documentation, hämtad januari 16, 2026, [https://emhass.readthedocs.io/en/latest/lpems.html](https://emhass.readthedocs.io/en/latest/lpems.html)  
4. An MPC-Based Energy Management System for Multiple Residential Microgrids, hämtad januari 16, 2026, [https://people.kth.se/\~kallej/papers/building\_case15.pdf](https://people.kth.se/~kallej/papers/building_case15.pdf)  
5. emhass/docs/config.md at master \- GitHub, hämtad januari 16, 2026, [https://github.com/davidusb-geek/emhass/blob/master/docs/config.md](https://github.com/davidusb-geek/emhass/blob/master/docs/config.md)  
6. EMHASS: An Energy Management for Home Assistant \- Page 62 \- Custom Integrations, hämtad januari 16, 2026, [https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=62](https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=62)  
7. The forecast module — emhass 0.15.4 documentation, hämtad januari 16, 2026, [https://emhass.readthedocs.io/en/latest/forecasts.html](https://emhass.readthedocs.io/en/latest/forecasts.html)  
8. Examples of using MPC for energy management \- Pagina personal de Ramon Costa, hämtad januari 16, 2026, [https://ramon-costa.staff.upc.edu/Talks/20250724\_NUAA\_talk\_3.pdf](https://ramon-costa.staff.upc.edu/Talks/20250724_NUAA_talk_3.pdf)  
9. EMHASS: An Energy Management for Home Assistant \- Page 39 \- Custom Integrations, hämtad januari 16, 2026, [https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=39](https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=39)  
10. Modelling and Simulation of Underfloor Heating System Supplied from Heat Pump \- Sheffield Hallam University Research Archive, hämtad januari 16, 2026, [https://shura.shu.ac.uk/25017/1/1570260071%20Final%20Revised.pdf](https://shura.shu.ac.uk/25017/1/1570260071%20Final%20Revised.pdf)  
11. EMHASS: An Energy Management for Home Assistant \- Page 137 \- Custom Integrations, hämtad januari 16, 2026, [https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=137](https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=137)  
12. EMHASS: An Energy Management for Home Assistant \- Page 83 \- Custom Integrations, hämtad januari 16, 2026, [https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=83](https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=83)  
13. EMHASS: An Energy Management for Home Assistant \- Page 177 \- Custom Integrations, hämtad januari 16, 2026, [https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=177](https://community.home-assistant.io/t/emhass-an-energy-management-for-home-assistant/338126?page=177)  
14. Simplified Building Thermal Model Development and Parameters Evaluation Using a Stochastic Approach \- MDPI, hämtad januari 16, 2026, [https://www.mdpi.com/1996-1073/13/11/2899](https://www.mdpi.com/1996-1073/13/11/2899)  
15. Building Thermal-Network Models: A Comparative Analysis, Recommendations, and Perspectives \- MDPI, hämtad januari 16, 2026, [https://www.mdpi.com/1996-1073/15/4/1328](https://www.mdpi.com/1996-1073/15/4/1328)  
16. 1 Grey-Box Modeling and Application for Building Energy Simulations \- A Critical Review \- OSTI.GOV, hämtad januari 16, 2026, [https://www.osti.gov/servlets/purl/1808373](https://www.osti.gov/servlets/purl/1808373)  
17. INVESTIGATION OF RC MODELS FOR TEMPERATURE PREDICTION IN RESIDENTIAL ROOMS \- TU Delft, hämtad januari 16, 2026, [https://filelist.tudelft.nl/Websections/Urban%20Energy/Projects/Report%201-Task%204.2%20INVESTIGATION%20OF%20RC%20MODELS%20.pdf](https://filelist.tudelft.nl/Websections/Urban%20Energy/Projects/Report%201-Task%204.2%20INVESTIGATION%20OF%20RC%20MODELS%20.pdf)  
18. ModestPy: An Open-Source Python Tool for Parameter Estimation in Functional Mock-up Units \- LiU Electronic Press, hämtad januari 16, 2026, [https://ep.liu.se/en/conference-article.aspx?series=\&issue=154\&Article\_No=13](https://ep.liu.se/en/conference-article.aspx?series&issue=154&Article_No=13)  
19. ModestPy: Open-source Python Toolfor Parameter Estimation in FMUs \- Semantic Scholar, hämtad januari 16, 2026, [https://www.semanticscholar.org/paper/ModestPy%3A-Open-source-Python-Toolfor-Parameter-in-Arendt-Jradi/13058418016c7ce83d8f262df1910802e7a5c0ac](https://www.semanticscholar.org/paper/ModestPy%3A-Open-source-Python-Toolfor-Parameter-in-Arendt-Jradi/13058418016c7ce83d8f262df1910802e7a5c0ac)  
20. Parameters estimation and least squares minimization \- Python and Gekko \- Stack Overflow, hämtad januari 16, 2026, [https://stackoverflow.com/questions/65628949/parameters-estimation-and-least-squares-minimization-python-and-gekko](https://stackoverflow.com/questions/65628949/parameters-estimation-and-least-squares-minimization-python-and-gekko)  
21. New Python Library: hplib \- heat pump library \- Google Groups, hämtad januari 16, 2026, [https://groups.google.com/g/openmod-initiative/c/-Cb54qwJ5CE](https://groups.google.com/g/openmod-initiative/c/-Cb54qwJ5CE)  
22. Heat pump — mosaik 3.3.0 documentation, hämtad januari 16, 2026, [https://mosaik.readthedocs.io/en/3.3.1/ecosystem/components/mosaik-heatpump/models/heatpump.html](https://mosaik.readthedocs.io/en/3.3.1/ecosystem/components/mosaik-heatpump/models/heatpump.html)  
23. ACRONYMS AC Air conditioner COP Coefficient of performance \[W/W\] DB Dry bulb temperature GWP Global warming potential GHG Greenh \- OSTI.GOV, hämtad januari 16, 2026, [https://www.osti.gov/servlets/purl/1879970](https://www.osti.gov/servlets/purl/1879970)  
24. Performance Measurement of R32 Vapor Injection Heat Pump System \- Purdue e-Pubs, hämtad januari 16, 2026, [https://docs.lib.purdue.edu/cgi/viewcontent.cgi?article=2260\&context=iracc](https://docs.lib.purdue.edu/cgi/viewcontent.cgi?article=2260&context=iracc)  
25. Empirical Study of the Effect of Thermal Loading on the Heating Efficiency of Variable-Speed Air Source Heat Pumps \- Publications, hämtad januari 16, 2026, [https://docs.nrel.gov/docs/fy23osti/85081.pdf](https://docs.nrel.gov/docs/fy23osti/85081.pdf)  
26. Polynomial equation of COP approximation with supply temperature as an... \- ResearchGate, hämtad januari 16, 2026, [https://www.researchgate.net/figure/Polynomial-equation-of-COP-approximation-with-supply-temperature-as-an-argument\_tbl3\_362393904](https://www.researchgate.net/figure/Polynomial-equation-of-COP-approximation-with-supply-temperature-as-an-argument_tbl3_362393904)  
27. Energy flexibility quantification of the building's thermal mass for radiator and floor heating systems \- IRIS Re.Public@polimi.it, hämtad januari 16, 2026, [https://re.public.polimi.it/retrieve/9c38975e-385b-425c-908b-4a33935f6a12/1-s2.0-S0360544225015452-main%20%281%29.pdf](https://re.public.polimi.it/retrieve/9c38975e-385b-425c-908b-4a33935f6a12/1-s2.0-S0360544225015452-main%20%281%29.pdf)  
28. Modeling energy flexibility of low energy buildings utilizing thermal mass \- DTU Inside, hämtad januari 16, 2026, [https://backend.orbit.dtu.dk/ws/files/128686116/1406\_2.pdf](https://backend.orbit.dtu.dk/ws/files/128686116/1406_2.pdf)  
29. Model Predictive Control for Efficient Management of Energy Resources in Smart Buildings, hämtad januari 16, 2026, [https://www.mdpi.com/1996-1073/14/18/5592](https://www.mdpi.com/1996-1073/14/18/5592)  
30. Rule-based energy management strategies for PEMFC-based micro-CHP systems: A comparative analysis \- PMC \- NIH, hämtad januari 16, 2026, [https://pmc.ncbi.nlm.nih.gov/articles/PMC11459011/](https://pmc.ncbi.nlm.nih.gov/articles/PMC11459011/)  
31. emhass \- PyPI, hämtad januari 16, 2026, [https://pypi.org/project/emhass/0.3.36/](https://pypi.org/project/emhass/0.3.36/)  
32. SMART HOME ENERGY OPTIMIZATION SYSTEM \- Thermal Science, hämtad januari 16, 2026, [https://thermalscience.rs/pdfs/papers-2024/TSCI2406071S.pdf](https://thermalscience.rs/pdfs/papers-2024/TSCI2406071S.pdf)  
33. Machine Learning Approach to Predict Building Thermal Load Considering Feature Variable Dimensions: An Office Building Case Study \- MDPI, hämtad januari 16, 2026, [https://www.mdpi.com/2075-5309/13/2/312](https://www.mdpi.com/2075-5309/13/2/312)  
34. Introduction — emhass 0.15.4 documentation, hämtad januari 16, 2026, [https://emhass.readthedocs.io/en/latest/intro.html](https://emhass.readthedocs.io/en/latest/intro.html)  
35. davidusb-geek/emhass: emhass: Energy Management for Home Assistant, is a Python module designed to optimize your home energy interfacing with Home Assistant. \- GitHub, hämtad januari 16, 2026, [https://github.com/davidusb-geek/emhass](https://github.com/davidusb-geek/emhass)  
36. Use Case: Thermal Model · davidusb-geek emhass · Discussion \#340 \- GitHub, hämtad januari 16, 2026, [https://github.com/davidusb-geek/emhass/discussions/340](https://github.com/davidusb-geek/emhass/discussions/340)  
37. siku2/hass-emhass: emhass: Energy Management for Home Assistant, is a Python module designed to optimize your home energy interfacing with Home Assistant. \- GitHub, hämtad januari 16, 2026, [https://github.com/siku2/hass-emhass](https://github.com/siku2/hass-emhass)  
38. Example configurations — emhass 0.15.3 documentation, hämtad januari 16, 2026, [https://emhass.readthedocs.io/en/latest/study\_case.html](https://emhass.readthedocs.io/en/latest/study_case.html)  
39. Configuration file — emhass 0.15.4 documentation, hämtad januari 16, 2026, [https://emhass.readthedocs.io/en/latest/config.html](https://emhass.readthedocs.io/en/latest/config.html)  
40. Publication: A Comparison of Model Predictive Control and Reinforcement Learning Methods for Building Energy Storage Management \- Thesis Central, hämtad januari 16, 2026, [https://theses-dissertations.princeton.edu/entities/publication/2a3a4067-d8e5-4158-9849-8adfaadd20e1](https://theses-dissertations.princeton.edu/entities/publication/2a3a4067-d8e5-4158-9849-8adfaadd20e1)  
41. (PDF) Comparative Field Deployment of Reinforcement Learning and Model Predictive Control for Residential HVAC \- ResearchGate, hämtad januari 16, 2026, [https://www.researchgate.net/publication/396143036\_Comparative\_Field\_Deployment\_of\_Reinforcement\_Learning\_and\_Model\_Predictive\_Control\_for\_Residential\_HVAC](https://www.researchgate.net/publication/396143036_Comparative_Field_Deployment_of_Reinforcement_Learning_and_Model_Predictive_Control_for_Residential_HVAC)  
42. Reinforcement Learning vs. Model Predictive Control, Which one is more doable \- Reddit, hämtad januari 16, 2026, [https://www.reddit.com/r/ControlTheory/comments/1j7voro/reinforcement\_learning\_vs\_model\_predictive/](https://www.reddit.com/r/ControlTheory/comments/1j7voro/reinforcement_learning_vs_model_predictive/)  
43. A Learning-Based Model Predictive Control Strategy for Home Energy Management Systems \- IEEE Xplore, hämtad januari 16, 2026, [https://ieeexplore.ieee.org/document/10371281/](https://ieeexplore.ieee.org/document/10371281/)  
44. Analysis and Application of Model Predictive Control in Energy System \- YouTube, hämtad januari 16, 2026, [https://www.youtube.com/watch?v=l8ekLCIAiaM](https://www.youtube.com/watch?v=l8ekLCIAiaM)  
45. Potential of building thermal mass for energy flexibility in residential buildings: a sensitivity analysis \- IBPSA Publications, hämtad januari 16, 2026, [https://publications.ibpsa.org/proceedings/esim/2018/papers/esim2018\_1-3-A-3.pdf](https://publications.ibpsa.org/proceedings/esim/2018/papers/esim2018_1-3-A-3.pdf)