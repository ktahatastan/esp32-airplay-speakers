---
status: partial
owner: hardware-engineer
reviewers: [firmware-engineer]
updated: 2026-08-31
tags: [esp32, pins, board]
---

# ESP32-S3 kart ve pin seçimi

## Kart: kilitlendi

Kanonik kart **ESP32-S3, 16 MB flash + 8 MB PSRAM (`N16R8` sınıfı)**. Karar ve gerekçe: [[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]].

Gerekçenin özeti: [[../07-decisions/ADR-0008-github-releases-ota|ADR-0008]] eşit boyutlu `ota_0` / `ota_1` slotları ile ayrı `factory_calibration` ve `nvs` bölümleri istiyor. AirPlay yığını, DSP ve TLS birlikte 8 MB flashta bu bütçeyi daraltır.

N8R8 yalnız ikincil yedektir; partition bütçesi ölçülüp 8 MB'a sığdığı kanıtlanır ve yeni bir ADR açılırsa değerlendirilir.

## Pin ataması: hâlâ aday

Aday GPIO tablosu kanonik olarak [[circuit-and-wiring-plan#3.1 Aday ESP32-S3 pin planı|devre ve bağlantı planında]] tutulur. Burada tekrarlanmaz; iki yerde ayrı liste tutulursa sapar.

Tablonun `accepted` olabilmesi için gerekenler:

| Koşul | Kanıt |
|---|---|
| Satın alınan kartın şeması elde | Üretici/satıcı şeması veya kart baskı yazısı fotoğrafı |
| I2S üçlüsü boot'u bozmuyor | Kart üzerinde boot + I2S saat ölçümü (TP11-TP13) |
| Buton pini strapping değil | Boot testi ve datasheet karşılaştırması |
| RGB PWM ses tabanına girmiyor | G3 dip gürültü ölçümü |
| USB/UART recovery erişilebilir | Fiziksel kurtarma prosedürü denendi |

## Kaçınılan pinler

Boot/strapping: `GPIO0`, `GPIO3`, `GPIO45`, `GPIO46`. Native USB: `GPIO19`, `GPIO20`. Kart üzerinde dahili flash/PSRAM'e ayrılan pinler kart şeması görülmeden kullanılmaz.

## İlgili belgeler

- [[circuit-and-wiring-plan|Devre ve bağlantı planı]]
- [[../03-firmware/ota-and-release-plan|OTA ve partition bütçesi]]
- [[../05-procurement/bom|BOM]]
