---
name: merzarkabul-hardware-safety
description: Plan, review or implement Merzarkabul Airplay Speakers power-input, amplifier, driver, enclosure or EMI work. Use for schematics, wiring, BOM compatibility, bring-up and physical test plans. Do not declare physical gates passed without recorded measurements.
---

# Merzarkabul Airplay Speakers hardware safety

Read `AGENTS.md`, `docs/01-planning/risk-register.md`, the relevant hardware plan and accepted ADRs.

Route work through gates:

- Unknown drivers: G0 before power.
- Amplifier: current-limited supply and dummy-load G1 before drivers.
- Power input: the adapter's no-load output is measured before it is ever connected and must read below 25.5 V: 24 V sits 2 V under the amplifier's 26 V maximum, so an adapter that rises off-load is refused, not derated (ADR-0020). Barrel-jack polarity (centre-positive) is verified with a meter before first power-up; the 24 V / 2.9 A adapter, both 5 V bucks (one for the ESP32-S3, one for the PCM5102A), the four amplifiers, brownout and power-on/off pop are checked in G1 on the dummy load, never on drivers.
- Limiter budget: the limiter ceiling is derived from the 2.9 A adapter budget with all four amplifiers driven and verified by VIN sag in G1: eight BTL channels can draw far more than 70 W, and a sagging VIN resets the ESP32-S3 mid-song (ADR-0020).
- Tweeter: verified HPF, limiter and mute sequence before low-level G2.
- Staging: the first energised path is one amplifier, one woofer and one tweeter -- on the dummy load first, then on real drivers after G0-G2 pass on that pair. The other three amplifiers are wired to drivers only after that.

Separate measured value, datasheet limit, retailer claim and engineering assumption. Require a test report path for every accepted gate. Stop and mark BLOCKED when a missing measurement makes energizing unsafe.
