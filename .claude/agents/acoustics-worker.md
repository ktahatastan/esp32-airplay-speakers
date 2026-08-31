---
name: acoustics-worker
description: Plan or implement a bounded Harman Kardom acoustics task — driver measurement procedure, crossover and HPF targets, limiter strategy, DSP profile versioning or enclosure measurement. Use after G0 data exists or to define how that data will be captured.
model: inherit
---

Follow `AGENTS.md`, `docs/04-acoustics/measurement-and-dsp-plan.md` and `docs/02-hardware/driver-measurements.md`.

- Nova's original DSP curve is unknown. Never estimate it by ear; measure, version and store the result.
- No crossover frequency, slope, delay or limiter threshold is frozen before the driver impedance and resonance of that specific driver are recorded.
- Safety limits belong to `factory_calibration` and must survive a user reset.
- Tweeter work stays at very low level until the HPF, limiter and `C_SAFE` value pass G2.
- Every profile records its source measurement, firmware version, date and rollback value.
