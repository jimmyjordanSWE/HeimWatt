# Suggested APIs for Missing Providers

This document lists recommended APIs to cover the missing providers identified in `missing_providers.log`.

## 1. Atmosphere, Solar, Marine, Air, Soil
**Recommended Provider:** [Open-Meteo](https://open-meteo.com)

Open-Meteo is an excellent "all-in-one" solution that covers the vast majority of the missing semantic types in the atmosphere, solar, marine, air, and soil categories. It aggregates data from national weather services (NOAA, DWD, MeteoFrance, etc.) and provides it via a clean JSON API.

### Covered Categories:
- **Atmosphere**: Temperature, pressure, wind (speed/dir/gusts), precipitation, visibility, cloud cover, etc.
- **Solar**: GHI, DNI, DHI, UV Index, etc.
- **Marine**: Wave height, direction, period, swell, sea temperature.
- **Air Quality**: AQI (US/Europe), PM2.5, PM10, NO2, SO2, O3, CO, Pollen (Birch, Grass, etc.).
- **Soil**: Soil temperature (0cm, 10cm, etc.), soil moisture.

### Rate Limits (Free Tier):
- **10,000 API calls per day**
- **5,000 API calls per hour**
- **600 API calls per minute**
- No API key required for non-commercial use.

### Implementation Strategy:
Feature extraction can be combined into single API calls per location to save bandwidth and stay within rate limits.

**Example Endpoint (Weather + Solar + Soil):**
```
GET https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m,relative_humidity_2m,dew_point_2m,apparent_temperature,precipitation_probability,precipitation,rain,showers,snowfall,snow_depth,pressure_msl,surface_pressure,cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high,visibility,evapotranspiration,et0_fao_evapotranspiration,vapor_pressure_deficit,wind_speed_10m,wind_speed_80m,wind_speed_120m,wind_direction_10m,wind_direction_80m,wind_direction_120m,wind_gusts_10m,temperature_80m,temperature_120m,temperature_180m,soil_temperature_0cm,soil_temperature_6cm,soil_temperature_18cm,soil_temperature_54cm,soil_moisture_0_to_1cm,soil_moisture_1_to_3cm,soil_moisture_3_to_9cm,soil_moisture_9_to_27cm,soil_moisture_27_to_81cm&daily=weather_code,sunrise,sunset,daylight_duration,sunshine_duration,uv_index_max,uv_index_clear_sky_max&timezone=auto
```

**Air Quality Endpoint**:
```
GET https://air-quality-api.open-meteo.com/v1/air-quality?latitude=52.52&longitude=13.41&hourly=pm10,pm2_5,carbon_monoxide,nitrogen_dioxide,sulphur_dioxide,ozone,aerosol_optical_depth,dust,uv_index,uv_index_clear_sky,ammonia,alder_pollen,birch_pollen,grass_pollen,mugwort_pollen,olive_pollen,ragweed_pollen
```

**Marine Endpoint**:
```
GET https://marine-api.open-meteo.com/v1/marine?latitude=54.544587&longitude=10.227487&hourly=wave_height,wave_direction,wave_period,wind_wave_height,wind_wave_direction,wind_wave_period,swell_wave_height,swell_wave_direction,swell_wave_period
```

---

## 2. Energy (Grid)
**Recommended Provider:** [ENTSO-E Transparency Platform](https://transparency.entsoe.eu/) (Europe)

For energy grid data (prices, generation mix, load, cross-border flows), the ENTSO-E Transparency Platform is the standard for European data. It is free but requires registration.

### Covered Categories:
- **Energy Price**: Spot prices (Day-ahead), intraday.
- **Energy Generation**: Generation by fuel type (Nuclear, Wind, Solar, Hydro, etc.).
- **Energy Demand/Load**: Total grid load.
- **Energy Flow**: Cross-border physical flows.

### Rate Limits:
- **400 requests per minute** per user (IP + Token based).
- **Free of charge**.

### Implementation Strategy:
1. Register an account on [transparency.entsoe.eu](https://transparency.entsoe.eu/).
2. Request API access via email (instructions on their site).
3. Use their REST API which returns XML.

**Example Request (Day Ahead Prices):**
```
GET https://web-api.tp.entsoe.eu/api?securityToken=YOUR_TOKEN&documentType=A44&in_Domain=10YDE-RWENET---I&out_Domain=10YDE-RWENET---I&periodStart=202301010000&periodEnd=202301012300
```
*Note: You will need to parse XML responses.*

**Alternative:**
If you are strictly in the Nordics, **Nord Pool** has an API, but it is often paid or restricted for non-customers. ENTSO-E is generally the best free source for this data.

---

## 3. Vehicle (EV data)
**Recommended Provider:** [Smartcar](https://smartcar.com/) (Aggregator) or Manufacturer Direct (e.g., Tesla Fleet API)

For generic vehicle support, **Smartcar** is a developer-friendly API that abstracts many manufacturers (Tesla, Ford, VW, etc.).

### Covered Categories:
- **Vehicle**: SoC, Range, Odometer, Location, Tire Pressure, Charging Status, Lock Status.

### Rate Limits & Cost:
- **Smartcar Free Tier**: 1 Vehicle, limited history. Good for personal use/dev.
- **Tesla Fleet API**: Now requires paid developer account for third-party access (official path). Unofficial APIs exist but are unstable.

### Implementation Strategy:
- **Smartcar**: Use their SDKs or REST API. Requires OAuth2 flow to link the user's car account.
- **Tesla**: If building specifically for Tesla, use the Tesla Fleet API (requires registration and possibly payment).

---

## 4. Storage (Home Battery)
**Recommended Approach:** Local Integration (Modbus TCP / MQTT)

Most home battery systems (Victron, Huawei, SMA, Fronius) provide local interfaces which are faster and more reliable than cloud APIs.

### Covered Categories:
- **Storage**: SoC, Power (Charge/Discharge), Voltage, Current, SOH.

### Implementation Strategy:
- **Modbus TCP**: Many inverters expose Modbus TCP on the local network. This is the preferred method for real-time control (filling `storage.power`, `storage.soc`, etc.).
- Checks if the device supports **SunSpec Alliance** models for standardized register mapping.

---

## 5. Local Network Plugins (Home Automation)
**Recommended Approach:** Zigbee2MQTT / Local APIs (IKEA, Shelly)

For specialized local control of lights, sensors, and smart plugs, leveraging existing local infrastructure is best.

### A. IKEA Home Smart (Tradfri/Dirigera)
- **Tradfri Ecosystem**:
  - Uses Zigbee for device communication.
  - The **Tradfri Gateway** uses **CoAP (Constrained Application Protocol)** with DTLS for security. It is fully local.
  - The newer **Dirigera Hub** exposes a **RESTful WebSocket API** (local).
- **Strategy**: Implementing a native CoAP client (libcoap) or REST client to talk to the hub. Alternatively, replace the hub with a Zigbee Stick (see below).

### B. Zigbee (Generic)
**Recommended:** Interface with **Zigbee2MQTT** or similar bridges via MQTT.
- Instead of writing a custom Zigbee stack, run **Zigbee2MQTT** which bridges Zigbee devices to an MQTT broker.
- **HeimWatt Implementation**: Write a "Generic MQTT" plugin that subscribes to topics like `zigbee2mqtt/device_id`.
- **Benefits**: Supports thousands of devices (IKEA, Philips Hue, Xiaomi, etc.) without vendor lock-in.

### C. Shelly (Energy Monitoring & Control)
- **Features**: Highly popular for energy monitoring (Shelly EM, Plug S).
- **API**: Best-in-class local API.
  - **Gen 1**: Simple REST API (`http://device-ip/status`).
  - **Gen 2/3**: RPC over HTTP, WebSocket, or MQTT.
- **Strategy**: Polling the local HTTP endpoint is simplest for a plugin. MQTT is better for real-time events.

### D. Matter / Thread
- The emerging standard.
- **Strategy**: Currently complex to implement a full Matter controller from scratch. recommended to bridge via Home Assistant or Apple/Google hubs until libraries mature.

### E. Modbus TCP & Inverters
- Already mentioned in Storage/Solar, but critical for "heavy" local appliances (Heat pumps, EV Chargers).
