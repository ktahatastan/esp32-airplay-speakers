---
status: active
owner: orchestrator
updated: 2026-08-30
tags: [roadmap, planning]
---

# Yol haritası

| Aşama | Çıktı | Çıkış kapısı |
|---|---|---|
| 0. Keşif | Sürücü ölçümü, stack/lisans araştırması | G0 + AirPlay fizibilite kararı |
| 1. Masa prototipi | Dummy-load, 24 V adaptör (boşta < 25,5 V), iki 5 V buck, DAC, bir amfi; sonra dört amfi tam yükte `VIN` bütçesi | G1 |
| 2. Korumalı ses | HPF/crossover/limiter — önce tek woofer ve tek tweeter | G2 |
| 3. Kabin | Termal (dört amfi ve iki buck kapalı kabinde), EMI, mekanik, sürücü yerleşimi ve pasif radyatör akordu (G0 sonrası), akustik | G8'in kapalı kabin satırları |
| 4. Firmware | Provisioning, reset, LED, OTA | G6 |
| 5. Dayanıklılık | 24 saat soak | G8 |
| 6. Final | Üretim BOM kilidi | Tüm açık ADR'ler kabul |
