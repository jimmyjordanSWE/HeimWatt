# HeimWatt Hardware Deployment Options

> **Status**: Draft (2026-01-14)
> **Scope**: Hardware recommendations for various deployment scenarios.

---

## 1. The "Budget / Home Lab" Option
**Target**: Enthusiasts, Developers, DIY.
**Budget**: €30 - €60.

The best value for money comes from repurposed enterprise thin clients. They offer industrial build quality for the price of a Raspberry Pi power supply.

### Recommended Hardware
*   **Device**: **Dell Wyse 3040** (Used/Refurbished).
    *   **CPU**: Intel Atom x5 (Quad Core).
    *   **RAM**: 2GB.
    *   **Storage**: 8GB / 16GB eMMC (Soldered, industrial durability).
    *   **Power**: 5V (Micro-USB or Barrel). < 5W power draw.
*   **Connectivity**:
    *   Ethernet (Built-in).
    *   4x USB ports (Add Wi-Fi or Zigbee dongles easily).
*   **Add-ons**: USB-to-RS485 dongle (~€5) for Modbus connectivity.

### Why this wins
*   **Durability**: No SD cards to fail. eMMC storage lasts 10+ years.
*   **Price**: €30-50 on eBay/Tradera (fraction of a new RPi setup).
*   **Size**: Extremely compact, fanless.

---

## 2. The "Commercial Product" Option (Mid-Volume)
**Target**: Startup selling a "HeimWatt Hub" with decent margins.
**Budget**: ~€75 BOM (Bill of Materials).
**Retail Price**: €199 - €249.

For selling a finished product without building custom circuit boards, repackaging high-quality OEM "Router" boards is the optimal strategy.

### Recommended Hardware
*   **Device**: **NanoPi R2S Plus** (Hardware Revision with eMMC).
    *   **Storage**: 32GB eMMC.
    *   **Case**: Official metal case (Yellow/Black or custom color) acts as heatsink. Heavy, premium feel.
    *   **Networking**: **Dual Gigabit Ethernet** (WAN/LAN isolation).
*   **Connectivity**: USB port for RS485/Zigbee.
*   **Strategy**:
    1.  Buy bulk units from FriendlyElec.
    2.  Flash HeimWatt OS to eMMC.
    3.  Apply "HeimWatt" branding sticker.
    4.  Bundle with high-quality DIN-rail mount and PSU.

---

## 3. The "Industrial / Heavy Use" Option (Gold Standard)
**Target**: High-end installations, Whole-home control (Battery + Solar + Heating), Electricians.
**Budget**: €300 - €500.

For mission-critical control where the device sits in the fuse box (Elcentral) and controls thousands of euros of equipment.

### Recommended Hardware
*   **Device**: **Kunbus Revolution Pi (RevPi Connect 4)**.
    *   **Core**: Raspberry Pi Compute Module 4 (CM4).
    *   **Form Factor**: Industrial DIN-rail housing.
    *   **Power**: 24V DC (Standard cabinet voltage).
*   **Key Features**:
    *   **Native RS485**: No USB dongles allowed. Built-in industrial terminals.
    *   **2x Ethernet**: Strict separation of Internet and Local Control network.
    *   **Hardware Watchdog**: Auto-reboot if system freezes.

---

## 4. The "Mass Market" Option (IKEA Style)
**Target**: Average consumers, "plug and play", Data-only initially.
**Budget**: ~€5 - €10 BOM.
**Retail Price**: €50 - €90.

To reach the mass market, remove the computer entirely from the user's home and rely on the Cloud + a simple reader.

### Recommended Hardware
*   **Device**: **HeimWatt Link** (Custom ESP32 Dongle).
*   **Interface**: **HAN Port / P1 Port** (RJ12 or RJ45 on smart meter).
*   **Power**: Parasitic power from the meter itself (No cables).
*   **Function**:
    1.  Read P1 serial data.
    2.  Send to HeimWatt Cloud via Wi-Fi.
*   **User Experience**: "Plug it in, open the App."

### Deployment Strategy
Start with the **Mass Market Dongle** to acquire users (Data layer). Upsell the **Commercial Hub (Option 2)** for users who want local control, battery management, and offline reliability.
