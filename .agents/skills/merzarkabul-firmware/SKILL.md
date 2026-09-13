---
name: merzarkabul-firmware
description: Design, implement or review Merzarkabul Airplay Speakers ESP32-S3 firmware for AirPlay audio, I2S/DSP, BLE or SoftAP provisioning, buttons, LEDs, NVS, OTA and recovery. Use for firmware tasks with explicit file ownership; do not describe the DSP chain as finished.
---

# Merzarkabul Airplay Speakers firmware

Read `AGENTS.md`, firmware docs, controls/provisioning plan and relevant ADRs.

- Prove protocol/library capabilities from source and a test; ADR-0007 is the accepted stack choice, and AirPlay 2 is claimed only as far as the record shows it: a phone finding, pairing with and streaming to the receiver (PRD-009).
- Keep audio/I2S paths non-blocking and isolated from portal, LED and logging work.
- Split `factory_cal` from `user_settings`; user reset must not remove safety limits.
- Never log Wi-Fi credentials or keys. Setup has no proof of possession and the QR holds no secret (ADR-0023); the home Wi-Fi password is the only secret setup handles.
- Provisioning is protocomm Security 1 without proof of possession on BLE and SoftAP, the setup network is open and the setup names carry the `PROV_` prefix (ADR-0023). A stronger mode, a setup-network key or a credential gate returns only through a superseding ADR, never as a fix.
- Provisioning is time-limited after physical activation and shuts down BLE/SoftAP after success. The first-boot window stays open until setup completes.
- OTA uses rollback/recovery and survives an interrupted update (supply cut mid-write).
- Button/LED behavior follows `docs/controls-and-provisioning-plan.md`.
- Add automated tests for state machines and storage migrations, then map integration evidence to G6 and G8.

Return changed files, test output, timing/memory impact and unresolved hardware dependencies.
