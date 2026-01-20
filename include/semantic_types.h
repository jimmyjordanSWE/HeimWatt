#ifndef HEIMWATT_SEMANTIC_TYPES_H
#define HEIMWATT_SEMANTIC_TYPES_H

// X-Macro for Semantic Types
// Format: X(ENUM_SUFFIX, ID_STRING, UNIT_STRING, DESC_STRING)
#define HEIMWATT_SEMANTIC_TYPES(X)                                                                 \
    /* Atmosphere (Weather) */                                                                     \
    X(ATMOSPHERE_TEMPERATURE, "atmosphere.temperature", "celsius", "Ambient air temperature (2m)") \
    X(ATMOSPHERE_TEMPERATURE_SURFACE, "atmosphere.temperature.surface", "celsius",                 \
      "Temperature at ground level")                                                               \
    X(ATMOSPHERE_TEMPERATURE_850HPA, "atmosphere.temperature.850hpa", "celsius",                   \
      "Temperature at ~1.5km altitude")                                                            \
    X(ATMOSPHERE_APPARENT_TEMPERATURE, "atmosphere.apparent_temperature", "celsius",               \
      "Perceived temperature")                                                                     \
    X(ATMOSPHERE_DEW_POINT, "atmosphere.dew_point", "celsius", "Dew point temperature")            \
    X(ATMOSPHERE_HUMIDITY, "atmosphere.humidity", "percent", "Relative humidity")                  \
    X(ATMOSPHERE_PRESSURE, "atmosphere.pressure", "hpa", "Atmospheric pressure at surface")        \
    X(ATMOSPHERE_PRESSURE_SEALEVEL, "atmosphere.pressure.sealevel", "hpa",                         \
      "Pressure reduced to sea level")                                                             \
    X(ATMOSPHERE_WIND_SPEED, "atmosphere.wind_speed", "m/s", "Wind speed at 10m")                  \
    X(ATMOSPHERE_WIND_SPEED_80M, "atmosphere.wind_speed.80m", "m/s", "Wind speed at 80m")          \
    X(ATMOSPHERE_WIND_SPEED_120M, "atmosphere.wind_speed.120m", "m/s", "Wind speed at 120m")       \
    X(ATMOSPHERE_WIND_DIRECTION, "atmosphere.wind_direction", "degrees",                           \
      "Wind direction (0=N, 90=E)")                                                                \
    X(ATMOSPHERE_WIND_GUST, "atmosphere.wind_gust", "m/s", "Maximum wind gust speed")              \
    X(ATMOSPHERE_PRECIPITATION, "atmosphere.precipitation", "mm", "Total precipitation amount")    \
    X(ATMOSPHERE_PRECIPITATION_PROB, "atmosphere.precipitation_probability", "percent",            \
      "Probability of precipitation")                                                              \
    X(ATMOSPHERE_RAIN, "atmosphere.rain", "mm", "Rain amount")                                     \
    X(ATMOSPHERE_SNOWFALL, "atmosphere.snowfall", "cm", "Snowfall amount")                         \
    X(ATMOSPHERE_SNOW_DEPTH, "atmosphere.snow_depth", "m", "Total snow depth on ground")           \
    X(ATMOSPHERE_SNOW_WATER_EQUIV, "atmosphere.snow_water_equivalent", "mm",                       \
      "Water content of snow pack")                                                                \
    X(ATMOSPHERE_FREEZING_LEVEL, "atmosphere.freezing_level_height", "m",                          \
      "Altitude of 0 deg C isotherm")                                                              \
    X(ATMOSPHERE_VISIBILITY, "atmosphere.visibility", "m", "Horizontal visibility")                \
    X(ATMOSPHERE_CLOUD_COVER, "atmosphere.cloud_cover", "percent", "Total cloud cover")            \
    X(ATMOSPHERE_CLOUD_COVER_LOW, "atmosphere.cloud_cover.low", "percent",                         \
      "Low altitude cloud cover")                                                                  \
    X(ATMOSPHERE_CLOUD_COVER_MID, "atmosphere.cloud_cover.mid", "percent",                         \
      "Mid altitude cloud cover")                                                                  \
    X(ATMOSPHERE_CLOUD_COVER_HIGH, "atmosphere.cloud_cover.high", "percent",                       \
      "High altitude cloud cover")                                                                 \
    X(ATMOSPHERE_SUNSHINE_DURATION, "atmosphere.sunshine_duration", "s",                           \
      "Duration of bright sunshine")                                                               \
    X(ATMOSPHERE_EVAPOTRANSPIRATION, "atmosphere.evapotranspiration", "mm",                        \
      "Reference evapotranspiration (ET0)")                                                        \
    X(ATMOSPHERE_CAPE, "atmosphere.cape", "J/kg", "Convective Available Potential Energy")         \
    X(ATMOSPHERE_LIFTED_INDEX, "atmosphere.lifted_index", "kelvin", "Stability index")             \
    X(ATMOSPHERE_WEATHER_CODE, "atmosphere.weather_code", "wmo_code", "WMO weather code")          \
    X(ATMOSPHERE_IS_DAY, "atmosphere.is_day", "boolean", "1 if day, 0 if night")                   \
    /* Solar */                                                                                    \
    X(SOLAR_IRRADIANCE, "solar.irradiance", "W/m2", "Global Horizontal Irradiance (GHI)")          \
    X(SOLAR_GHI, "solar.ghi", "W/m2", "Global Horizontal Irradiance")                              \
    X(SOLAR_DNI, "solar.dni", "W/m2", "Direct Normal Irradiance")                                  \
    X(SOLAR_DHI, "solar.dhi", "W/m2", "Diffuse Horizontal Irradiance")                             \
    X(SOLAR_GTI, "solar.gti", "W/m2", "Global Tilted Irradiance")                                  \
    X(SOLAR_AZIMUTH, "solar.azimuth", "degrees", "Solar azimuth angle")                            \
    X(SOLAR_ELEVATION, "solar.elevation", "degrees", "Solar elevation angle")                      \
    X(SOLAR_ZENITH, "solar.zenith", "degrees", "Solar zenith angle")                               \
    X(SOLAR_UV_INDEX, "solar.uv_index", "index", "UV Index strength")                              \
    X(SOLAR_UV_RADIANCE, "solar.uv_radiance", "W/m2", "UV irradiance")                             \
    X(SOLAR_ALBEDO, "solar.albedo", "ratio", "Ground reflectivity (0-1)")                          \
    X(SOLAR_CLOUD_OPACITY, "solar.cloud_opacity", "percent", "Cloud opacity")                      \
    X(SOLAR_SOILING_LOSS, "solar.soiling_loss", "percent", "PV loss due to dirt/snow")             \
    /* Energy */                                                                                   \
    X(ENERGY_PRICE_SPOT, "energy.price.spot", "currency/kWh", "Day-ahead spot price")              \
    X(ENERGY_PRICE_INTRADAY, "energy.price.intraday", "currency/kWh", "Intraday market price")     \
    X(ENERGY_PRICE_TOTAL, "energy.price.total", "currency/kWh", "Spot + tax + fees")               \
    X(ENERGY_PRICE_TAX, "energy.price.tax", "currency/kWh", "Energy tax component")                \
    X(ENERGY_PRICE_GRID_FEE, "energy.price.grid_fee", "currency/kWh", "Grid transmission fee")     \
    X(ENERGY_DEMAND, "energy.demand", "W", "Total grid demand")                                    \
    X(ENERGY_GENERATION, "energy.generation", "W", "Total grid generation")                        \
    X(ENERGY_GENERATION_NUCLEAR, "energy.generation.nuclear", "W", "Nuclear generation")           \
    X(ENERGY_GENERATION_WIND, "energy.generation.wind", "W", "Wind generation")                    \
    X(ENERGY_GENERATION_SOLAR, "energy.generation.solar", "W", "Solar generation")                 \
    X(ENERGY_GENERATION_HYDRO, "energy.generation.hydro", "W", "Hydro generation")                 \
    X(ENERGY_FLOW, "energy.flow", "W", "Net exchange flow")                                        \
    X(ENERGY_FREQUENCY, "energy.frequency", "Hz", "Grid frequency")                                \
    X(ENERGY_CARBON_INTENSITY, "energy.carbon_intensity", "gCO2/kWh", "Carbon intensity")          \
    /* Storage */                                                                                  \
    X(STORAGE_SOC, "storage.soc", "percent", "State of Charge")                                    \
    X(STORAGE_ENERGY, "storage.energy", "kWh", "Stored energy")                                    \
    X(STORAGE_CAPACITY, "storage.capacity", "kWh", "Total capacity")                               \
    X(STORAGE_CAPACITY_USABLE, "storage.capacity.usable", "kWh", "Usable capacity")                \
    X(STORAGE_SOH, "storage.soh", "percent", "State of Health")                                    \
    X(STORAGE_POWER, "storage.power", "W", "Net power flow")                                       \
    X(STORAGE_POWER_CHARGE, "storage.power.charge", "W", "Charging component")                     \
    X(STORAGE_POWER_DISCHARGE, "storage.power.discharge", "W", "Discharging component")            \
    X(STORAGE_VOLTAGE, "storage.voltage", "V", "Battery voltage")                                  \
    X(STORAGE_CURRENT, "storage.current", "A", "Battery current")                                  \
    X(STORAGE_TEMPERATURE, "storage.temperature", "celsius", "Battery temperature")                \
    X(STORAGE_CYCLES, "storage.cycles", "count", "Charge cycle count")                             \
    X(STORAGE_STATUS, "storage.status", "enum", "Status enum")                                     \
    X(STORAGE_GRID_CONNECTION, "storage.grid_connection", "enum", "Grid connection state")         \
    /* Vehicle */                                                                                  \
    X(VEHICLE_SOC, "vehicle.soc", "percent", "Battery level")                                      \
    X(VEHICLE_RANGE, "vehicle.range", "km", "Driving range")                                       \
    X(VEHICLE_ODOMETER, "vehicle.odometer", "km", "Total distance")                                \
    X(VEHICLE_SPEED, "vehicle.speed", "km/h", "Vehicle speed")                                     \
    X(VEHICLE_LOC_LAT, "vehicle.location.lat", "degrees", "Latitude")                              \
    X(VEHICLE_LOC_LON, "vehicle.location.lon", "degrees", "Longitude")                             \
    X(VEHICLE_LOC_HEADING, "vehicle.location.heading", "degrees", "Heading")                       \
    X(VEHICLE_INSIDE_TEMP, "vehicle.inside_temp", "celsius", "Cabin temperature")                  \
    X(VEHICLE_OUTSIDE_TEMP, "vehicle.outside_temp", "celsius", "Exterior temperature")             \
    X(VEHICLE_IS_LOCKED, "vehicle.is_locked", "boolean", "Lock status")                            \
    X(VEHICLE_SENTRY_MODE, "vehicle.sentry_mode", "boolean", "Sentry mode")                        \
    X(VEHICLE_TIRE_PRESS_FL, "vehicle.tire_pressure.fl", "bar", "Front Left pressure")             \
    X(VEHICLE_TIRE_PRESS_FR, "vehicle.tire_pressure.fr", "bar", "Front Right pressure")            \
    X(VEHICLE_TIRE_PRESS_RL, "vehicle.tire_pressure.rl", "bar", "Rear Left pressure")              \
    X(VEHICLE_TIRE_PRESS_RR, "vehicle.tire_pressure.rr", "bar", "Rear Right pressure")             \
    X(VEHICLE_CHARGING_STATE, "vehicle.charging.state", "enum", "Charging state")                  \
    X(VEHICLE_CHARGING_POWER, "vehicle.charging.power", "W", "Charging power")                     \
    X(VEHICLE_CHARGING_CURRENT, "vehicle.charging.current", "A", "Charging current")               \
    X(VEHICLE_CHARGING_VOLTAGE, "vehicle.charging.voltage", "V", "Charging voltage")               \
    X(VEHICLE_CHARGING_PHASES, "vehicle.charging.phases", "count", "Active phases")                \
    X(VEHICLE_CHARGING_LIMIT_SOC, "vehicle.charging.limit_soc", "percent", "Charge limit")         \
    X(VEHICLE_CHARGING_TIME_REM, "vehicle.charging.time_remaining", "s", "Time to complete")       \
    /* Marine */                                                                                   \
    X(MARINE_WAVE_HEIGHT, "marine.wave_height", "m", "Significant wave height")                    \
    X(MARINE_WAVE_DIRECTION, "marine.wave_direction", "degrees", "Wave direction")                 \
    X(MARINE_WAVE_PERIOD, "marine.wave_period", "s", "Wave period")                                \
    X(MARINE_SWELL_HEIGHT, "marine.swell_height", "m", "Swell height")                             \
    X(MARINE_SWELL_DIRECTION, "marine.swell_direction", "degrees", "Swell direction")              \
    X(MARINE_SWELL_PERIOD, "marine.swell_period", "s", "Swell period")                             \
    X(MARINE_WATER_TEMP, "marine.water_temp", "celsius", "Sea surface temperature")                \
    X(MARINE_CURRENT_SPEED, "marine.current_speed", "m/s", "Current speed")                        \
    X(MARINE_CURRENT_DIRECTION, "marine.current_direction", "degrees", "Current direction")        \
    X(MARINE_TIDE_LEVEL, "marine.tide_level", "m", "Sea level relative to mean")                   \
    /* Air Quality */                                                                              \
    X(AIR_AQI, "air.aqi", "index", "Air Quality Index")                                            \
    X(AIR_PM2_5, "air.pm2_5", "ug/m3", "PM2.5")                                                    \
    X(AIR_PM10, "air.pm10", "ug/m3", "PM10")                                                       \
    X(AIR_NO2, "air.no2", "ug/m3", "NO2")                                                          \
    X(AIR_SO2, "air.so2", "ug/m3", "SO2")                                                          \
    X(AIR_O3, "air.o3", "ug/m3", "O3")                                                             \
    X(AIR_CO, "air.co", "ug/m3", "CO")                                                             \
    X(AIR_POLLEN_BIRCH, "air.pollen.birch", "grain/m3", "Birch pollen")                            \
    X(AIR_POLLEN_GRASS, "air.pollen.grass", "grain/m3", "Grass pollen")                            \
    X(AIR_POLLEN_OLIVE, "air.pollen.olive", "grain/m3", "Olive pollen")                            \
    X(AIR_POLLEN_RAGWEED, "air.pollen.ragweed", "grain/m3", "Ragweed pollen")                      \
    /* Soil */                                                                                     \
    X(SOIL_TEMPERATURE, "soil.temperature", "celsius", "Surface soil temperature")                 \
    X(SOIL_TEMPERATURE_10CM, "soil.temperature.10cm", "celsius", "Soil temp at 10cm")              \
    X(SOIL_MOISTURE, "soil.moisture", "m3/m3", "Volumetric moisture")                              \
    X(SOIL_MOISTURE_10CM, "soil.moisture.10cm", "m3/m3", "Moisture at 10cm")

// Semantic Type Enum
typedef enum
{
    SEM_UNKNOWN = 0,
#define X(suffix, id, unit, desc) SEM_##suffix,
    HEIMWATT_SEMANTIC_TYPES(X)
#undef X
        SEM_TYPE_COUNT
} semantic_type;

// Metadata Structure
typedef struct
{
    semantic_type type;
    const char *id;
    const char *enum_name;
    const char *unit;
    const char *description;
} semantic_meta;

// Metadata Lookup function (implemented in .c)
const semantic_meta *semantic_get_meta(semantic_type type);

// Helper: Get enum from ID string (O(N) or specialized lookup)
semantic_type semantic_from_string(const char *id);

#endif  // HEIMWATT_SEMANTIC_TYPES_H
