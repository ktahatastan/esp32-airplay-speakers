---
status: draft
owner: firmware-engineer
updated: 2026-08-30
tags: [firmware, plan]
---

# Firmware planı

- `audio`: AirPlay, saat/buffer, I2S, DSP, limiter.
- `network`: Wi-Fi, mDNS, BLE/SoftAP provisioning.
- `ui`: buton state machine ve LED animator.
- `storage`: `factory_calibration` / `user_settings` ayrımı.
- `power`: batarya/NTC/gerilim ve güvenli kapanış.
- `update`: imzalı/A-B OTA, rollback ve recovery.
- `diagnostics`: parolasız, kişisel veri içermeyen loglar.

Audio görevleri LED/web görevlerinden yüksek öncelikte ve bloklanmayan yolda çalışır. Task/core yerleşimi ölçümle belirlenir.
