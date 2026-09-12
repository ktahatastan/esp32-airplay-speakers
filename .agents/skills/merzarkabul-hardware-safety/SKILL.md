---
name: merzarkabul-hardware-safety
description: Plan, review or implement Merzarkabul Airplay Speakers power-input, amplifier, driver, enclosure or EMI work. Use for schematics, wiring, BOM compatibility, bring-up and physical test plans. Do not declare physical gates passed without recorded measurements.
---

# Merzarkabul Airplay Speakers hardware safety

Read `AGENTS.md`, `docs/01-planning/risk-register.md`, the relevant hardware plan and accepted ADRs.

Route work through gates:

- Unknown drivers: G0 before power.
- Amplifier: current-limited supply and dummy-load G1 before drivers.
- Power input: barrel-jack polarity verified with a meter before first power-up; the 19 V adapter, the 5 V buck, brownout and power-on/off pop are checked in G1 on the dummy load, never on drivers.
- Tweeter: verified HPF, limiter and mute sequence before low-level G2.
- Do not replicate to four units until G0-G2 pass.

Separate measured value, datasheet limit, retailer claim and engineering assumption. Require a test report path for every accepted gate. Stop and mark BLOCKED when a missing measurement makes energizing unsafe.
