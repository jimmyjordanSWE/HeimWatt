# Open-Meteo API Reference
**Service**: Weather Forecast & Solar
**Status**: [Primary Solar Source] / [Backup Weather Source]

## 1. Endpoints
Base URL: `https://api.open-meteo.com/v1`

### Get Forecast (Weather + Solar)
Returns mixed weather and solar parameters.
`GET /forecast`

**Query Parameters**:
- `latitude`: e.g. 59.33
- `longitude`: e.g. 18.06
- `hourly`: Comma-separated list of values (see below).
- `timezone`: `Europe/Berlin` (or `auto`)

**Key Parameters for LEOP**:
- `temperature_2m`
- `direct_normal_irradiance` (DNI) - Critical for solar calc
- `diffuse_radiation`
- `cloud_cover`
- `wind_speed_10m`

**Example URL**:
`https://api.open-meteo.com/v1/forecast?latitude=59.33&longitude=18.06&hourly=temperature_2m,cloud_cover,direct_normal_irradiance,diffuse_radiation,wind_speed_10m&timezone=auto`

## 2. Response Structure (JSON)
Open-Meteo uses a "Columnar" JSON structure (arrays of values), not an array of objects.

```json
{
  "latitude": 59.33,
  "longitude": 18.06,
  "utc_offset_seconds": 3600,
  "hourly": {
    "time": ["2024-01-01T00:00", "2024-01-01T01:00", ...],
    "temperature_2m": [1.2, 1.1, ...],
    "direct_normal_irradiance": [0.0, 0.0, ...], // Watts/m²
    "diffuse_radiation": [0.0, 0.0, ...],       // Watts/m²
    "cloud_cover": [100, 90, ...]               // %
  }
}
```

## 3. Parameter Mapping
| Open-Meteo Param | Unit | LEOP Field | Notes |
|------------------|------|------------|-------|
| `temperature_2m` | C | `temp_c` | - |
| `cloud_cover` | % | `cloud_cover_pct` | - |
| `direct_normal_irradiance` | W/m² | `solar_dni` | Direct sun (perp. to rays) |
| `diffuse_radiation` | W/m² | `solar_diffuse` | Scattered sun |
| `wind_speed_10m` | km/h | `wind_speed_ms` | **Warning**: Convert km/h / 3.6 |

## 4. Usage Notes
- **Free Tier**: No API key required for non-commercial.
- **Solar Calculation**: Total Irradiance = DNI * cos(zenith) + Diffuse (simplified).
- **Time**: Response `time` array matches index-for-index with value arrays.
