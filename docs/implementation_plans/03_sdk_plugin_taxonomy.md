# Design Study: SDK Plugin Taxonomy

> **Status**: In Review  
> **Created**: 2026-01-20  
> **Updated**: 2026-01-21

## Executive Summary

This document defines the plugin model for HeimWatt. **Capability-based architecture** Plugins declare what they can do, what devices they provide, and what credentials they need. All management happens via WebUI.

---

## Plugin Taxonomy

### Capabilities

Plugins declare capabilities in their manifest:

| Capability | Description | Example |
|------------|-------------|---------|
| `report` | Push semantic data to Core | Weather plugin |
| `query` | Pull semantic data from Core | Solver |
| `actuate` | Control external devices | Heat pump wrapper |
| `constrain` | Provide optimization constraints | Battery limits |
| `sense` | Real-time sensor stream | Temperature probe |

Plugins can have multiple capabilities. Core enforces capability checks — a plugin without `actuate` cannot call device control APIs.

### Device-Centric Model

Plugins declare **devices**, not services. The user cares about their battery, not MELCloud:

```json
{
  "id": "com.heimwatt.melcloud",
  "name": "MELCloud Integration",
  "capabilities": ["report", "query", "actuate"],
  "devices": [
    {
      "type": "heat_pump",
      "name": "Heat Pump",
      "provides": ["hvac.power.actual", "hvac.cop.actual"],
      "consumes": ["schedule.heat_pump.power"]
    },
    {
      "type": "battery",
      "name": "Home Battery",
      "provides": ["storage.soc", "storage.power"],
      "consumes": ["schedule.battery.power"]
    }
  ]
}
```

**WebUI shows devices, not plugins**:
```
My Devices
├── 🔋 Home Battery
│   └── via MELCloud ✓ Connected
├── 🌡️ Heat Pump
│   └── via MELCloud ✓ Connected
└── ⚡ Electricity Price
    └── via Tibber ✓ Connected
```

---

## Credential Management

### Credential Types

| Type | Use Case | UX |
|------|----------|-----|
| `oauth` | HeimWatt-registered OAuth (Nibe, Tesla) | Click Connect → login on provider → done |
| `oauth_user_provided` | Community plugins, niche providers | User registers own app, enters client_id/secret |
| `password` | Legacy APIs (MELCloud) | Enter username/password in WebUI |
| `api_key` | Simple APIs (Nordpool) | Paste API key in WebUI |

### Manifest Schema

```json
{
  "credentials": {
    "type": "oauth",
    "display_name": "Nibe Uplink"
  }
}
```

For `oauth_user_provided` (community plugins):
```json
{
  "credentials": {
    "type": "oauth_user_provided",
    "provider_name": "Growatt Solar",
    "registration_url": "https://openapi.growatt.com/register",
    "instructions": "Register an app at Growatt developer portal",
    "auth_url": "https://openapi.growatt.com/oauth/authorize",
    "token_url": "https://openapi.growatt.com/oauth/token",
    "scopes": ["read", "write"]
  }
}
```

For `password`:
```json
{
  "credentials": {
    "type": "password",
    "required": ["username", "password"],
    "display_name": "MELCloud Account"
  }
}
```

### Token Refresh

Core handles token refresh transparently. Plugin just calls:

```c
char *token = NULL;
if (sdk_credential_get(ctx, "access_token", &token) == 0) {
    // Token is always fresh — Core refreshed if needed
    make_api_request(token);
    sdk_credential_destroy(&token);  // Zero and free
}
```

If refresh fails (user revoked, token expired), `sdk_credential_get()` returns error. Plugin reports "disconnected" status.

### Encryption

Credentials encrypted at rest using password-derived key:

```
User Password + Salt → Argon2id → 256-bit Key → AES-256-GCM
```

**Trade-off**: Password reset = must re-connect external services (acceptable).

---

## Rate Limiting & Safety

Multi-layer protection for device actuation:

### Layer 1: Solver Constraints (Global)

Built into optimization — solver cannot produce schedules that violate:
- Minimum cycle time (e.g., 5 minutes between on/off)
- Maximum cycles per hour

### Layer 2: Device Type Defaults (SDK)

SDK knows device category defaults:

| Device Type | Min Cycle | Max Cycles/Hour |
|-------------|-----------|-----------------|
| `battery` | 5 min | 4 |
| `heat_pump` | 10 min | 3 |
| `ev_charger` | 1 min | 10 |

### Layer 3: Plugin Override (Tighten Only)

Plugin can specify stricter limits in manifest:

```json
{
  "safety": {
    "min_cycle_s": 900,
    "max_cycles_per_hour": 2
  }
}
```

Cannot loosen SDK defaults.

### Layer 4: Runtime Enforcement

SDK rejects commands that violate limits:
```
[WARN] Cycle rejected: Battery last cycled 2 min ago (min: 5 min)
```

Logged and visible in WebUI.

---

## Plugin Installation

> **See [05_webui_auth_safety.md](05_webui_auth_safety.md#plugin-store--installation)** for full WebUI flows.

### Flow Summary

1. User browses Plugin Store in WebUI
2. Click "Install" → Core downloads from repository
3. Signature verified (Official HeimWatt or Community)
4. Manifest validated
5. If plugin has `actuate` capability → user approval required
6. Credential entry form shown
7. Plugin loaded and running

### Validation

Manifest validated at load time. Invalid manifests rejected with clear error:

```
❌ Plugin "Broken Plugin" failed to load
   Error: Invalid capability 'reprot' (did you mean 'report'?)
```

---

## SDK API Summary

### Capabilities

```c
// Report semantic data to Core
int sdk_report_value(plugin_ctx *ctx, const char *type_name, double value, int64_t ts);

// Query semantic data from Core
int sdk_query_latest(plugin_ctx *ctx, semantic_type type, sdk_data_point *out);

// Device control (requires 'actuate' capability)
int sdk_device_setpoint(plugin_ctx *ctx, const char *device_id, double value);
```

### Credentials

```c
// Get credential (Core handles refresh transparently)
int sdk_credential_get(plugin_ctx *ctx, const char *key, char **value);

// Zero and free credential
void sdk_credential_destroy(char **value);
```

### Scheduling

```c
int sdk_register_ticker(plugin_ctx *ctx, sdk_tick_handler handler);
int sdk_register_cron(plugin_ctx *ctx, const char *expression, sdk_tick_handler handler);
int sdk_register_fd(plugin_ctx *ctx, int fd, sdk_io_handler handler);
```

---

## Decisions Summary

| Topic | Decision |
|-------|----------|
| Plugin abstraction | **Capabilities**: report, query, actuate, constrain, sense |
| Device model | **Plugins declare devices**, not services |
| `actuate` approval | **Required** — user must approve at install |
| Credential types | **Four types**: oauth, oauth_user_provided, password, api_key |
| Credential storage | **Password-derived key** (Argon2id → AES-256-GCM) |
| Token refresh | **Core handles transparently** |
| Rate limiting | **Multi-layer**: Solver → SDK defaults → Plugin override → Runtime |
| Validation | **At load time**, fail fast with clear errors |

---

## Cross-Document References

| Document | Relationship |
|----------|-------------|
| [05_webui_auth_safety.md](05_webui_auth_safety.md) | Plugin Store, WebUI flows, credential entry |
| [04_concurrency_strategy.md](04_concurrency_strategy.md) | Non-blocking IPC for credential requests |
| [semantic_types.md](../reference/semantic_types.md) | Valid types for `provides`/`consumes` |

---

## Implementation Priority

1. **Manifest schema update** — add capabilities, devices, credentials
2. **SDK credential API** — `sdk_credential_get/destroy`
3. **Core capability enforcement** — reject unauthorized calls
4. **Solver constraints** — min cycle times built into optimization
5. **Plugin Store API** — repository browsing, download, install
