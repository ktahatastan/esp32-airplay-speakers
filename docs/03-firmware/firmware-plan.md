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
- `update`: SemVer/GitHub Releases tabanlı imzalı A/B OTA, sağlık kontrolü, rollback, canary/stable dağıtım ve recovery; ayrıntı [[ota-and-release-plan]].
- `diagnostics`: parolasız, kişisel veri içermeyen loglar.

Audio görevleri LED/web görevlerinden yüksek öncelikte ve bloklanmayan yolda çalışır. Task/core yerleşimi ölçümle belirlenir.
