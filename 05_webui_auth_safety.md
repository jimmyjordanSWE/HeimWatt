# Design Study: WebUI, Authentication & Safety

> **Status**: In Review  
> **Created**: 2026-01-20  
> **Updated**: 2026-01-21

## Executive Summary

HeimWatt is an **appliance** — a headless box installed once, managed entirely via WebUI. This document covers user authentication, external service authentication, plugin management, and safety constraints.

---

## Deployment Model

```
┌────────────────────────────────────────────────────────────────┐
│                  HomeNetwork                                   │
│                                                                │
│   ┌──────────────┐           ┌─────────────────────────────┐  │
│   │   Browser    │◄─────────►│   HeimWatt Box              │  │
│   │   (Phone/PC) │   HTTP    │   (Basement/DIN Rail)       │  │
│   └──────────────┘           │                             │  │
│                              │   - Core (C binary)         │  │
│                              │   - WebUI (React bundle)    │  │
│                              │   - Plugins (forked procs)  │  │
│                              │   - Database                │  │
│                              └─────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

**Key property**: No SSH/terminal access required. All management via WebUI.

---

## Question 1: WebUI Architecture

### Decision: Hybrid (Core serves API, WebUI as static bundle)

```
Core
├── HTTP Server
│   └── /api/* endpoints (JSON)
├── Static file serving (React bundle)
└── WebSocket for real-time updates

WebUI (React)
└── Calls /api/* for all data
└── No direct plugin communication
```

**Rationale**:
- Core controls all truth (plugin states, device status)
- Plugins can't hide from UI — Core exposes all via `/api/plugins`
- Single binary deployment (Core embeds WebUI assets)

---

## Question 2: User Authentication

Users authenticate to HeimWatt itself (not external services).

### First-Run Setup

```
┌────────────────────────────────────────────────────────────────┐
│ Welcome to HeimWatt                                            │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│   Create your admin password:                                  │
│                                                                │
│   Password:         [________________________]                 │
│   Confirm:          [________________________]                 │
│                                                                │
│   🔒 This password encrypts your external service credentials. │
│      If you forget it, you'll need to reconnect all services.  │
│                                                                │
│                                        [Create Account]        │
└────────────────────────────────────────────────────────────────┘
```

### Session Auth

```
POST /api/auth/login { "password": "..." }
→ Set-Cookie: hw_session=<JWT>

All subsequent requests:
Cookie: hw_session=<JWT>
```

**JWT Properties**:
- Expires in 30 days (long-lived — appliance should "just work")
- Refresh happens automatically on activity
- Password change invalidates all sessions

---

## Question 2b: External Service Authentication

How HeimWatt authenticates TO external services (Nibe, MELCloud, etc.).

> **See [03_sdk_plugin_taxonomy.md](03_sdk_plugin_taxonomy.md#credential-management)** for credential types schema.

### Credential Types Summary

| Type | Use Case | Flow |
|------|----------|------|
| `oauth` | HeimWatt-registered OAuth | Click → Provider login → Done |
| `oauth_user_provided` | Community plugins | Enter client_id → Provider login → Done |
| `password` | Legacy APIs | Enter credentials in form |
| `api_key` | Simple APIs | Paste key |

### Token Refresh

Core handles OAuth token refresh transparently. Plugins just request credentials — always get fresh tokens.

If refresh fails (user revoked, token expired), WebUI shows "Reconnect" button.

### Credential Encryption

All credentials encrypted using password-derived key:

```
User Password + Salt → Argon2id → 256-bit Key → AES-256-GCM
```

**Password change**: Re-encrypts all credentials with new key. Services stay connected.

**Password reset**: Credentials lost. User must reconnect all services.

---

## Plugin Store & Installation

All plugin management via WebUI. No manual file copying.

### Plugin Repository

HeimWatt hosts plugin catalog:

```
plugins.heimwatt.com
├── /catalog.json
├── /plugins/
│   ├── com.heimwatt.nibe/
│   │   ├── manifest.json
│   │   ├── plugin-linux-amd64.tar.gz
│   │   ├── plugin-linux-arm64.tar.gz
│   │   └── signature
│   └── com.community.growatt/
│       └── signature
```

### Plugin Store UI

```
┌────────────────────────────────────────────────────────────────┐
│ Plugin Store                                        [Search]   │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Official Plugins                                              │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐            │
│  │ 🔥 Nibe     │  │ ⚡ Tibber   │  │ 🌡️ SMHI    │            │
│  │ Heat Pump   │  │ Electricity │  │ Weather    │            │
│  │ ✓ Official  │  │ ✓ Official  │  │ ✓ Official │            │
│  │ [Install]   │  │ [Install]   │  │ [Install]  │            │
│  └─────────────┘  └─────────────┘  └─────────────┘            │
│                                                                │
│  Community Plugins                                             │
│  ┌─────────────┐  ┌─────────────┐                             │
│  │ 🔋 Growatt  │  │ 🏠 Custom   │                             │
│  │ Battery     │  │ Upload Own  │                             │
│  │ Community   │  │             │                             │
│  │ [Install]   │  │ [Upload]    │                             │
│  └─────────────┘  └─────────────┘                             │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Installation Flow

**Step 1: Download & Validate**

```
┌────────────────────────────────────────────────────────────────┐
│ Installing: Nibe Heat Pump                                     │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ✓ Downloaded from repository                                 │
│  ✓ Signature verified (Official HeimWatt plugin)              │
│  ✓ Manifest validated                                         │
│                                                                │
│  ⚠️ This plugin can control devices:                          │
│     • Heat Pump                                                │
│     • Hot Water Tank                                           │
│                                                                │
│  [Cancel]                               [Allow & Continue]     │
└────────────────────────────────────────────────────────────────┘
```

**Step 2: Connect Service**

For OAuth providers:
```
┌────────────────────────────────────────────────────────────────┐
│ Connect: Nibe Uplink                                           │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Sign in with your Nibe account to connect your heat pump.    │
│                                                                │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │               [Connect with Nibe]                        │ │
│  └──────────────────────────────────────────────────────────┘ │
│                                                                │
│  HeimWatt will be able to:                                    │
│  • Read your heat pump status                                 │
│  • Control temperature setpoints                              │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

For password-based services:
```
┌────────────────────────────────────────────────────────────────┐
│ Connect: MELCloud                                              │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Email:    [________________________]                          │
│  Password: [________________________]                          │
│                                                                │
│  🔒 Credentials encrypted and stored locally.                 │
│                                                                │
│  [Cancel]                                       [Connect]      │
└────────────────────────────────────────────────────────────────┘
```

**Step 3: Device Discovery**

After connection, plugin reports discovered devices:

```
┌────────────────────────────────────────────────────────────────┐
│ ✓ Nibe Heat Pump Connected                                     │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Discovered devices:                                           │
│                                                                │
│  ✓ Heat Pump "NIBE F2120-12"                                  │
│  ✓ Hot Water Tank "NIBE EMMY"                                 │
│                                                                │
│                                              [Done]            │
└────────────────────────────────────────────────────────────────┘
```

### Devices Dashboard

After installation, users see devices (not plugins):

```
┌────────────────────────────────────────────────────────────────┐
│ My Devices                                                     │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  🌡️ Heat Pump                        22°C → 21°C │ COP: 4.2   │
│     └── via Nibe Uplink ✓                                     │
│                                                                │
│  🔋 Battery                          78% │ +2.3 kW (charging) │
│     └── via MELCloud ✓                                        │
│                                                                │
│  ⚡ Electricity                      0.42 SEK/kWh             │
│     └── via Tibber ✓                                          │
│                                                                │
│  🌤️ Weather                          Stockholm, 8°C, Cloudy   │
│     └── via SMHI (public API)                                 │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Core API Endpoints

```
GET  /api/store/catalog        → List available plugins
POST /api/plugins/install      → Download and install plugin
GET  /api/plugins              → List installed plugins
POST /api/plugins/:id/uninstall → Remove plugin

POST /api/oauth/start/:provider → Start OAuth flow
GET  /api/oauth/callback        → Handle OAuth redirect
POST /api/credentials           → Save password/api_key
GET  /api/credentials/status    → List connected services

GET  /api/devices               → List discovered devices
```

---

## Question 3: Safety

### Defense in Depth

Multi-layer protection for device actuation:

| Layer | Enforcer | What It Does |
|-------|----------|--------------|
| 1 | User | Sets comfort constraints ("18-22°C") |
| 2 | Solver | Hard constraints: min cycle times, max power |
| 3 | SDK | Device-type defaults, plugin overrides |
| 4 | Plugin | Clamp values, refuse invalid commands |
| 5 | Hardware | Built-in protection (compressor, BMS, GFCI) |

### Solver-Level Constraints

Built into optimization — solver cannot produce schedules that violate:

```python
# Solver constraints (pseudo-code)
for device in devices:
    # Cannot turn on within min_cycle_s of last off
    constraint: on_time[t] - off_time[t-1] >= device.min_cycle_s
    
    # Cannot exceed max cycles per hour
    constraint: sum(cycles[t:t+3600]) <= device.max_cycles_per_hour
```

These are global, apply to all devices of that type.

### SDK Device-Type Defaults

SDK knows category defaults:

| Device Type | Min Cycle | Max Cycles/Hour |
|-------------|-----------|-----------------|
| `battery` | 5 min | 4 |
| `heat_pump` | 10 min | 3 |
| `ev_charger` | 1 min | 10 |
| `water_heater` | 15 min | 2 |

### Plugin Safety Override

Plugins can specify stricter (never looser) limits:

```json
{
  "safety": {
    "min_cycle_s": 900,
    "max_cycles_per_hour": 2,
    "max_setpoint_c": 55
  }
}
```

### Runtime Enforcement

SDK rejects commands that violate constraints:

```
[WARN] Command rejected: Battery cycled 2 min ago (min: 5 min)
```

Logged and visible in WebUI audit trail.

### Audit Trail

All actuations logged:

```
2026-01-21 14:32:15 │ Heat Pump │ Setpoint 22→21°C │ Source: Solver │ ✓ Executed
2026-01-21 14:33:02 │ Battery   │ Discharge 3kW    │ Source: Solver │ ✓ Executed
2026-01-21 14:34:45 │ Battery   │ Charge 2kW       │ Source: Solver │ ✗ Rejected (min cycle)
```

### Failsafe Philosophy

> "Don't trust the solver to be safe. Don't trust the plugin to be safe. Trust hardware, but verify."

**Verification**: Plugin reports `actual` values. Core compares to `scheduled`. Alarm if divergence exceeds threshold.

---

## Cross-Document References

| Document | Relationship |
|----------|-------------|
| [03_sdk_plugin_taxonomy.md](03_sdk_plugin_taxonomy.md) | Plugin capabilities, credential API, manifest schema |
| [04_concurrency_strategy.md](04_concurrency_strategy.md) | Non-blocking API handling |

---

## Implementation Priority

1. **First-run setup wizard** — password creation, WebUI bootstrap
2. **Plugin Store API** — catalog, download, install
3. **Credential storage** — Argon2id derivation, AES encryption
4. **OAuth flow** — redirect handling, token storage
5. **Device discovery** — plugin reports devices, UI displays
6. **Safety enforcement** — SDK constraints, audit logging
