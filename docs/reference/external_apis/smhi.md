# SMHI Open Data API Reference
**Service**: Meteorological Forecasts (PMP3g)
**Status**: [Primary Weather Source]

## 1. Endpoints
Base URL: `https://opendata-download-metfcst.smhi.se`

### Get Point Forecast
Returns a 10-day forecast for a specific coordinate.
`GET /api/category/pmp3g/version/2/geotype/point/lon/{longitude}/lat/{latitude}/data.json`

**Parameters**:
- `longitude`: Decimal degrees (e.g., 18.06)
- `latitude`: Decimal degrees (e.g., 59.33)
- **Note**: Coordinates are truncated to a grid (approx 2.5km resolution).

## 2. Response Structure (JSON)
```json
{
  "approvedTime": "2024-01-01T12:00:00Z",
  "referenceTime": "2024-01-01T12:00:00Z",
  "geometry": { "type": "Point", "coordinates": [[18.06, 59.33]] },
  "timeSeries": [
    {
      "validTime": "2024-01-01T13:00:00Z", // ISO 8601
      "parameters": [
        { "name": "t", "levelType": "hl", "level": 2, "unit": "Cel", "values": [15.4] },
        { "name": "Wsymb2", "levelType": "hl", "level": 2, "unit": "category", "values": [3] }
      ]
    },
    ...
  ]
}
```

## 3. Parameter Mapping (Relevant for LEOP)
| SMHI Param | Description | Unit | LEOP Field | Notes |
|------------|-------------|------|------------|-------|
| `t` | Temperature | C | `temp_c` | - |
| `r` | Relative Humidity | % | `humidity_pct`| - |
| `ws` | Wind Speed | m/s | `wind_speed_ms`| - |
| `tcc_mean` | Total Cloud Cover | octas (0-8)| `cloud_cover_pct` | Convert `val/8.0 * 100` |
| `Wsymb2` | Weather Symbol | int 1-27 | - | [Symbol Table](https://opendata.smhi.se/apidocs/metfcst/parameters.html#parameter-wsymb2) |
| `pcat` | Precip Category | int 0-6 | - | 0=No, 1=Snow, 2=Snow/Rain, 3=Rain... |
| `pmean` | Mean Precip | mm/h | `precip_mmh` | - |

## 4. Usage Notes
- **Rate Limit**: Not strictly defined but "be reasonable". Cache responses!
- **Refresh Rate**: Forecasts updated roughly every hour.
- **Timezone**: Times are UTC (`Z`).
- **PMP3g**: This is the current standard model.
