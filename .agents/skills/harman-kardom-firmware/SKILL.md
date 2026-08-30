---
name: harman-kardom-firmware
description: Design, implement or review Harman Kardom ESP32-S3 firmware for AirPlay audio, I2S/DSP, BLE or SoftAP provisioning, buttons, LEDs, NVS, power telemetry, OTA and recovery. Use for firmware tasks with explicit file ownership; do not assume AirPlay 2 multiroom support.
---

# Harman Kardom firmware

Read `AGENTS.md`, firmware docs, controls/provisioning plan and relevant ADRs.

- Prove protocol/library capabilities from source and a test; keep ADR-0007 open until G7 passes.
- Keep audio/I2S paths non-blocking and isolated from portal, LED and logging work.
- Split `factory_calibration` from `user_settings`; user reset must not remove safety limits.
- Never log Wi-Fi credentials, PoP, keys or QR secrets.
- Provisioning is time-limited after physical activation and shuts down BLE/SoftAP after success.
- OTA checks battery/external power, uses rollback/recovery and survives interrupted update testing.
- Button/LED behavior follows `docs/controls-and-provisioning-plan.md`.
- Add automated tests for state machines and storage migrations, then map integration evidence to G6/G7/G8.

Return changed files, test output, timing/memory impact and unresolved hardware dependencies.
