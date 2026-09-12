---
status: active
owner: orchestrator
updated: 2026-08-30
tags: [project, scope]
---

# Vizyon ve kapsam

## Vizyon

Harman Kardon Nova sürücülerini yeniden kullanarak tek kabinde sekiz sürücü taşıyan, AirPlay 2 ile çalan ve güvenli biçimde kurulabilen bir aktif hoparlör üretmek.

## Kapsam içi

- Tek kabin: dört woofer + dört tweeter, dört özdeş amfi (sekiz BTL kanal), tek mono program (ADR-0021).
- AirPlay 2 üzerinden oynatma: bir telefon cihazı bulur, eşleşir ve akış gönderir (PRD-009).
- ESP32-S3, I2S DAC, dört özdeş iki kanallı Class-D amfi ve DSP sürücü koruması (ADR-0002).
- 24 V / 2,9 A DC adaptörle besleme; ESP32-S3 ve DAC için iki ayrı 5 V buck (ADR-0020).
- Nova'nın kendi pasif radyatörleriyle akortlu kapalı kabin; sürücü yerleşimi ve akort G0 sonrası.
- BLE ve SoftAP/captive portal provisioning.
- Tek fonksiyon butonu, RGB LED.
- OTA, kurtarma, test kanıtı ve servis dokümantasyonu.

## Şimdilik kapsam dışı

- HomeKit/MFi/Fast Pair kimliği taklit etmek.
- Ölçüm olmadan Nova'nın orijinal DSP eğrisini tahmin etmek.
- Stereo çalma (ADR-0021).
- Çoklu cihaz senkronu; ürün ağda tek cihazdır (ADR-0021).
- G0-G2 kapıları geçmeden ilk amfi-woofer-tweeter çiftinin ötesine sinyal vermek.
