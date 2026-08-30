---
status: proposed
owner: orchestrator
updated: 2026-08-30
tags: [architecture]
---

# Sistem mimarisi

Her hoparlör aynı donanım/firmware tabanını ve benzersiz cihaz kimliğini kullanır.

```text
Telefon/Mac -> AirPlay/Wi-Fi -> ESP32-S3 -> I2S -> PCM5102A -> XH-A232
                   ^              |                              |   |
             BLE/SoftAP       UI + telemetri                    W   T

4S paket <-> balanslı BMS <-> sigorta/anahtar <-> amfi
                              `-> 5 V buck -> dijital kat
```

## Mimari ilkeler

- Dört cihaz bağımsız hata alanıdır.
- Ses koruması fabrika kalibrasyonunda tutulur ve kullanıcı resetinden etkilenmez.
- Analog yol, buck/BMS/Wi-Fi kaynaklı gürültüden fiziksel olarak ayrılır.
- AirPlay yığını ancak [[audio-network-feasibility]] kabulünden sonra kilitlenir.
- V1 şarj sırasında amfiyi kapatır; gerçek power-path ayrı ADR gerektirir.
