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
             BLE/SoftAP            UI                          W   T

19 V DC adaptör -> 5,5 x 2,1 mm jak (VIN) -> amfi
                              `-> 5 V buck -> dijital kat
```

## Mimari ilkeler

- Dört cihaz bağımsız hata alanıdır.
- Ses koruması fabrika kalibrasyonunda tutulur ve kullanıcı resetinden etkilenmez.
- Analog yol, buck/adaptör/Wi-Fi kaynaklı gürültüden fiziksel olarak ayrılır.
- AirPlay yığını ancak [[audio-network-feasibility]] kabulünden sonra kilitlenir.
- V1'de güç anahtarı yoktur: adaptör çekilir, gerisini firmware'in boşta bekleme durumu karşılar ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]).
