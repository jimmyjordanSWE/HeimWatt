# Design Study: WebUI, Authentication & Safety

> **Status**: Draft  
> **Date**: 2026-01-20

---

## Question 1: WebUI as Plugin vs Core?

### Option A: WebUI in Core (Current)

```
Core
├── HTTP Server
├── Static file serving (React bundle)
├── REST API endpoints
└── WebSocket for real-time
```

**Pros**:
- Core controls all communication
- No "ghost" plugins — everything reports through SDK
- Simpler deployment
- Core can enforce WebUI always shows all plugins

**Cons**:
- Core gets larger
- Harder to customize UI without rebuilding Core

### Option B: WebUI as Plugin

```
Core ←IPC→ WebUI Plugin ←HTTP→ Browser
```

**Pros**:
- Core stays minimal (pure broker)
- UI can be replaced/upgraded independently

**Cons**:
- Extra IPC hop for every request
- UI could lie about plugin states
- "Official" vs "Custom" UI complexity

### Recommendation: **Hybrid**

```
Core
├── Minimal HTTP (REST API only)
└── /api/* endpoints

External (Docker)
└── WebUI Container (Nginx + React)
    └── Proxies /api/* to Core
```

**Rationale**:
- Core serves **data API only** (JSON endpoints)
- UI is a **static bundle** served by Nginx or embedded
- No UI logic in Core, but Core controls truth
- Plugins can't hide — Core exposes all via `/api/plugins`

---

## Question 2: Authentication

### Threat Model

| Threat | Impact | Likelihood |
|--------|--------|------------|
| Unauthorized local access | Device control | Medium (shared network) |
| Remote attack | Full system control | Low (if properly firewalled) |
| Credential theft | API access | Medium |

### Minimum Viable Auth

For local-first system:

1. **First-Run Setup Wizard**
   - Generate random admin password
   - Display on terminal (or write to `/var/run/heimwatt/password`)
   - User saves it

2. **Session-Based Auth**
   ```
   POST /api/auth/login { "password": "..." }
   → Set-Cookie: hw_session=<JWT>
   
   All subsequent requests:
   Cookie: hw_session=<JWT>
   ```

3. **JWT with Short Expiry**
   - Expires in 24h
   - Refresh token for long-lived sessions

### Optional: mTLS for API

For users who want to expose HeimWatt remotely:

```
Client ←mTLS→ Reverse Proxy ←HTTP→ Core
```

Core stays simple; proxy handles TLS certificates.

---

## Question 3: Safety

### Defense in Depth

```
┌─────────────────────────────────────┐
│ Layer 1: User Constraints           │
│   "Living room: 18-22°C"            │
├─────────────────────────────────────┤
│ Layer 2: Solver Constraints         │
│   From device definitions           │
│   (cycle protection, power limits)  │
├─────────────────────────────────────┤
│ Layer 3: Device Plugin Validation   │
│   Clamp values to safe range        │
│   Refuse impossible commands        │
├─────────────────────────────────────┤
│ Layer 4: Hardware Failsafe          │
│   Device's own thermostat/limits    │
│   (built into heat pump, battery)   │
└─────────────────────────────────────┘
```

### Specific Safety Rules

| Device Type | HeimWatt Enforces | Hardware Enforces |
|-------------|-------------------|-------------------|
| Heat Pump | Cycle timing, max setpoint | Compressor protection, defrost |
| Battery | SoC limits, charge rate | Cell voltage, temperature cutoff |
| EV Charger | Max power, schedule | GFCI, voltage sensing |
| Water Heater | Max temp (60°C) | Mechanical thermostat |

### Implementation

1. **Device Definition includes safety limits**:
   ```json
   {
     "safety": {
       "max_setpoint_c": 55,
       "min_setpoint_c": 15,
       "cycle_min_on_s": 600,
       "cycle_min_off_s": 300
     }
   }
   ```

2. **Plugin SDK enforces**:
   ```c
   int sdk_device_setpoint(plugin_ctx *ctx, double value) {
       if (value > ctx->device_def->safety.max_setpoint) {
           sdk_log(ctx, SDK_LOG_WARN, "Clamping setpoint %.1f → %.1f",
                   value, ctx->device_def->safety.max_setpoint);
           value = ctx->device_def->safety.max_setpoint;
       }
       // ... send to device
   }
   ```

3. **Core logs all actuations** for audit trail

### Failsafe Philosophy

> "Don't trust the solver to be safe. Don't trust the plugin to be safe. 
> Trust the device hardware, but verify."

**Verify**: Plugin reports `actual` values; Core compares to `scheduled`. Alarm if divergence exceeds threshold.
