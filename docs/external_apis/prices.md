# Spot Price API Reference
**Service**: Elpriset Just Nu
**Status**: [Primary Price Source]

## 1. Endpoints
Base URL: `https://www.elprisetjustnu.se/api/v1`

### Get Daily Prices (Hourly)
Returns hourly spot prices for a specific region and day.
`GET /prices/{year}/{month}-{day}_{zone}.json`

**Parameters**:
- `year`: 2026
- `month`: 01-12
- `day`: 01-31
- `zone`: SE1 (North), SE2 (North-Central), SE3 (Stockholm), SE4 (Malmö)

**Example**:
`https://www.elprisetjustnu.se/api/v1/prices/2026/01-13_SE3.json`

## 2. Response Structure (JSON Array)
```json
[
  {
    "SEK_per_kWh": 1.23,
    "EUR_per_kWh": 0.11,
    "EXR": 11.2, // Exchange rate
    "time_start": "2026-01-13T00:00:00+01:00",
    "time_end": "2026-01-13T01:00:00+01:00"
  },
  ...
]
```

## 3. Parameter Mapping (for LEOP)
| API Field | Type | LEOP Field | Notes |
|-----------|------|------------|-------|
| `SEK_per_kWh` | float | `price_sek` | Source currency |
| `time_start` | ISO8601 | `timestamp` | Start of the hour |

## 4. Usage Notes
- **Update Time**: Next day's prices are usually released around 13:00 CET.
- **Failover**: If 404, prices are not yet available.
- **Future Changes**: From Oct 1, 2025, it may return 15-min intervals (96 entries). LEOP should handle variable array lengths.

---

# [Backup] ENTSO-E Transparency Platform
**Status**: [Secondary/Backup] - Requires API Token.
**Access**:
1. Register at [transparency.entsoe.eu](https://transparency.entsoe.eu/).
2. Email `transparency@entsoe.eu` with subject "Restful API access".
3. Wait ~3 days for token in User Settings.

**Endpoint**:
`GET /api?securityToken={TOKEN}&documentType=A44&in_Domain=10Y1001A1001A46L&out_Domain=10Y1001A1001A46L&periodStart={START}&periodEnd={END}`
*Complexity*: High (XML response). Use Elpriset Just Nu for simplicity unless EU-wide data is needed.
