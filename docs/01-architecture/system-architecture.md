---
status: proposed
owner: orchestrator
updated: 2026-08-30
tags: [architecture]
---

# Sistem mimarisi

Tek kabin, ağda tek cihaz: bir ESP32-S3, bir PCM5102A, dört özdeş XH-A232, dört woofer + dört tweeter, tek mono program; kimlik MAC'ten türer ([[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]).

```text
Telefon/Mac -> AirPlay/Wi-Fi -> ESP32-S3 -> I2S -> PCM5102A -+- LOUT -> 4x XH-A232 L girişi (paralel) -> 4x woofer
                   ^              |                            `- ROUT -> 4x XH-A232 R girişi (paralel) -> 4x tweeter (C_SAFE)
             BLE/SoftAP            UI

24 V / 2,9 A DC adaptör -> 5,5 x 2,1 mm jak (VIN) -> 4x amfi
                                             |-> buck A 5 V -> ESP32-S3
                                             `-> buck B 5 V -> PCM5102A
```

DAC'ın sol kanalı woofer bandını, sağ kanalı tweeter bandını taşır; dört amfinin girişleri hat seviyesinde paraleldir ve her amfinin bir çıkışı bir woofer, öbürü o tweeter'ın kendi `C_SAFE`'i üzerinden bir tweeter sürer ([[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]]).

## Mimari ilkeler

- Zincirde tek susturma vardır ve DAC'tadır: `XSMT`, `GPIO13` ve harici pull-down `R6`. XH-A232'de susturma girişi yoktur; dört amfi `VIN` geldiği andan itibaren canlıdır ([[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]]).
- Ses koruması fabrika kalibrasyonunda tutulur ve kullanıcı resetinden etkilenmez.
- Analog yol, buck'lar/adaptör/Wi-Fi kaynaklı gürültüden fiziksel olarak ayrılır. DAC'ın kendi buck'ı vardır, çünkü ESP32-S3 ile paylaşılan bir buck DAC'a tezgâhta duyulur bir hışırtı bindirdi ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]).
- AirPlay yığını ancak [[audio-network-feasibility]] kabulünden sonra kilitlenir.
- V1'de güç anahtarı yoktur: adaptör çekilir, gerisini firmware'in boşta bekleme durumu karşılar (ADR-0020).
