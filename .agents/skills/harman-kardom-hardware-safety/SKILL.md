---
name: harman-kardom-hardware-safety
description: Plan, review or implement Harman Kardom battery, charger, power, amplifier, driver, enclosure, EMI or thermal work. Use for schematics, wiring, BOM compatibility, bring-up and physical test plans. Do not declare physical gates passed without recorded measurements.
---

# Harman Kardom hardware safety

Read `AGENTS.md`, `docs/01-planning/risk-register.md`, the relevant hardware plan and accepted ADRs.

Route work through gates:

- Unknown drivers: G0 before power.
- Amplifier: current-limited supply and dummy-load G1 before drivers.
- Tweeter: verified HPF, limiter and mute sequence before low-level G2.
- Power/EMI: full 4S voltage range, brownout, noise and pop G3.
- Battery: matched cells, spot weld, balanced BMS, fuse, NTC and supervised external G4.
- Enclosure: thermal and mechanical G5 before scaling.

Separate measured value, datasheet limit, retailer claim and engineering assumption. Require a test report path for every accepted gate. Stop and mark BLOCKED when a missing measurement makes energizing unsafe.
