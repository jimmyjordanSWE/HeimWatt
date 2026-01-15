# Semantic Types Reference

> **Purpose**: Complete vocabulary of known data types for the HeimWatt platform.

---

## Format

```
<domain>.<measurement>[.<qualifier>]
```

**Rules**:
1. **Domain-first**: Group by where the measurement comes from
2. **Raw data only**: Never include calculated/derived values
3. **Canonical units**: Each measurement type has ONE canonical unit
4. **Snake_case**: All lowercase with underscores

---

## Atmosphere (Weather)

| Type | Unit | Description |
|------|------|-------------|
| `atmosphere.temperature` | °C | Ambient air temperature at 2m |
| `atmosphere.temperature.surface` | °C | Temperature at ground level |
| `atmosphere.apparent_temperature` | °C | Perceived temperature |
| `atmosphere.dew_point` | °C | Temperature at saturation |
| `atmosphere.humidity` | % | Relative humidity |
| `atmosphere.pressure` | hPa | Atmospheric pressure at surface |
| `atmosphere.pressure.sealevel` | hPa | Pressure reduced to sea level |
| `atmosphere.wind_speed` | m/s | Wind speed at 10m |
| `atmosphere.wind_speed.80m` | m/s | Wind speed at 80m (turbine height) |
| `atmosphere.wind_direction` | deg | Wind direction (0=N, 90=E) |
| `atmosphere.wind_gust` | m/s | Maximum wind gust speed |
| `atmosphere.precipitation` | mm | Total precipitation amount |
| `atmosphere.precipitation_probability` | % | Probability of precipitation |
| `atmosphere.rain` | mm | Rain amount |
| `atmosphere.snowfall` | cm | Snowfall amount |
| `atmosphere.visibility` | m | Horizontal visibility |
| `atmosphere.cloud_cover` | % | Total cloud cover |
| `atmosphere.cloud_cover.low` | % | Low altitude cloud cover |
| `atmosphere.cloud_cover.mid` | % | Mid altitude cloud cover |
| `atmosphere.cloud_cover.high` | % | High altitude cloud cover |
| `atmosphere.sunshine_duration` | s | Duration of bright sunshine |
| `atmosphere.evapotranspiration` | mm | Reference ET0 |
| `atmosphere.cape` | J/kg | Convective Available Potential Energy |
| `atmosphere.weather_code` | enum | WMO weather code |
| `atmosphere.is_day` | bool | 1 if daytime, 0 if nighttime |

---

## Solar (Irradiance & PV)

| Type | Unit | Description |
|------|------|-------------|
| `solar.ghi` | W/m² | Global Horizontal Irradiance |
| `solar.dni` | W/m² | Direct Normal Irradiance |
| `solar.dhi` | W/m² | Diffuse Horizontal Irradiance |
| `solar.gti` | W/m² | Global Tilted Irradiance (POA) |
| `solar.azimuth` | deg | Solar azimuth angle |
| `solar.elevation` | deg | Solar elevation angle |
| `solar.uv_index` | index | UV Index strength |
| `solar.albedo` | 0-1 | Ground reflectivity |
| `solar.cloud_opacity` | % | Cloud opacity to solar radiation |

---

## Energy (Grid & Markets)

| Type | Unit | Description |
|------|------|-------------|
| `energy.price.spot` | currency/kWh | Day-ahead spot price |
| `energy.price.intraday` | currency/kWh | Intraday market price |
| `energy.price.total` | currency/kWh | Spot + tax + fees |
| `energy.price.tax` | currency/kWh | Energy tax component |
| `energy.price.grid_fee` | currency/kWh | Grid transmission fee |
| `energy.demand` | W | Total grid load |
| `energy.generation` | W | Total grid generation |
| `energy.generation.nuclear` | W | Nuclear generation |
| `energy.generation.wind` | W | Wind generation |
| `energy.generation.solar` | W | Solar generation |
| `energy.generation.hydro` | W | Hydro generation |
| `energy.flow` | W | Net exchange flow |
| `energy.frequency` | Hz | Grid frequency |
| `energy.carbon_intensity` | gCO2/kWh | Carbon intensity |

---

## Storage (Battery)

| Type | Unit | Description |
|------|------|-------------|
| `storage.soc` | % | State of Charge |
| `storage.energy` | kWh | Current stored energy |
| `storage.capacity` | kWh | Total capacity |
| `storage.capacity.usable` | kWh | Usable capacity |
| `storage.soh` | % | State of Health |
| `storage.power` | W | Net power (+ charge, - discharge) |
| `storage.power.charge` | W | Charging power |
| `storage.power.discharge` | W | Discharging power |
| `storage.voltage` | V | Battery terminal voltage |
| `storage.current` | A | Battery current flow |
| `storage.temperature` | °C | Internal battery temperature |
| `storage.cycles` | count | Total charge cycles |
| `storage.status` | enum | Standby, Charging, Discharging |
| `storage.grid_connection` | enum | Connected, Islanded |

---

## Vehicle (EV)

| Type | Unit | Description |
|------|------|-------------|
| `vehicle.soc` | % | Battery level |
| `vehicle.range` | km | Estimated driving range |
| `vehicle.odometer` | km | Total distance traveled |
| `vehicle.speed` | km/h | Current vehicle speed |
| `vehicle.location.lat` | deg | GPS Latitude |
| `vehicle.location.lon` | deg | GPS Longitude |
| `vehicle.inside_temp` | °C | Cabin temperature |
| `vehicle.outside_temp` | °C | Exterior temperature |
| `vehicle.is_locked` | bool | Door lock status |
| `vehicle.charging.state` | enum | Disconnected, Plugged, Charging, Complete |
| `vehicle.charging.power` | W | Charging power |
| `vehicle.charging.current` | A | Charging current |
| `vehicle.charging.voltage` | V | Charging voltage |
| `vehicle.charging.limit_soc` | % | Charge limit set point |

---

## Air Quality

| Type | Unit | Description |
|------|------|-------------|
| `air.aqi` | index | Air Quality Index |
| `air.pm2_5` | µg/m³ | PM2.5 Particulate Matter |
| `air.pm10` | µg/m³ | PM10 Particulate Matter |
| `air.no2` | µg/m³ | Nitrogen Dioxide |
| `air.so2` | µg/m³ | Sulfur Dioxide |
| `air.o3` | µg/m³ | Ozone |
| `air.co` | µg/m³ | Carbon Monoxide |

---

## Soil

| Type | Unit | Description |
|------|------|-------------|
| `soil.temperature` | °C | Surface soil temperature |
| `soil.temperature.10cm` | °C | Soil temperature at 10cm |
| `soil.moisture` | m³/m³ | Volumetric soil moisture |

---

## Marine

| Type | Unit | Description |
|------|------|-------------|
| `marine.wave_height` | m | Significant wave height |
| `marine.wave_direction` | deg | Mean wave direction |
| `marine.wave_period` | s | Mean wave period |
| `marine.water_temp` | °C | Sea surface temperature |
| `marine.tide_level` | m | Sea level relative to mean |

---

## API Sources

| Domain | APIs |
|--------|------|
| Atmosphere | Open-Meteo, SMHI, Yr.no, OpenWeather |
| Solar | PVGIS, Solcast, Open-Meteo, NASA POWER |
| Energy | Elpriset, ENTSO-E, Tibber, Awattar |
| Storage | Tesla, Sonnen, SolarEdge, Fronius |
| Vehicle | Tesla, VW, BMW, Smartcar |
| Air Quality | OpenAQ, AQICN, IQAir |

For detailed API documentation, see [external_apis/](external_apis/).
