---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-08-31
tags: [adr, esp32, board, flash, ota]
---

# ADR-0010: Kanonik ESP32-S3 kartı N16R8

## Bağlam

Depoda iki kart varyantı paralel yaşıyordu. BOM ana tablosu, pin planı, şema çizimleri ve OTA manifest örneği `N8R8` (8 MB flash); fiyatlandırılan Türkiye listesi ve KiCad paftası ise `N16R8` (16 MB flash) diyordu. Flash boyutu OTA bölüm bütçesini doğrudan belirlediği için bu fark masum bir yazım tutarsızlığı değildir.

## Karar

Kanonik prototip kartı **ESP32-S3, 16 MB flash + 8 MB PSRAM (`N16R8` sınıfı)** olacaktır. Tüm belgeler, şema, KiCad paftası ve OTA manifesti tek bu değeri kullanır.

OTA donanım kimliği: `hw_revision = "prototype-n16r8"`, release asset adı `harman-kardom-esp32s3-n16r8-vX.Y.Z.bin`.

## Gerekçe

- [[ADR-0008-github-releases-ota|ADR-0008]] eşit boyutlu iki uygulama slotu (`ota_0` / `ota_1`), ayrı `factory_calibration` ve `nvs` bölümleri gerektirir.
- AirPlay yığını, DSP ve TLS birlikte 8 MB flashta çift slot bütçesini daraltır; 16 MB bu riski erken aşamada ortadan kaldırır.
- Fiyatlandırılmış ve stokta görünen aday zaten N16R8'dir; ek maliyet dört cihaz için sınırlıdır.
- PSRAM her iki varyantta da 8 MB olduğundan ses buffer bütçesi kart seçiminden etkilenmez.

## Sonuçlar ve açık koşullar

- Kart **`accepted`, pin ataması hâlâ `candidate`**: [[../02-hardware/circuit-and-wiring-plan#3.1 Aday ESP32-S3 pin planı|aday GPIO tablosu]] satın alınan kartın şeması ve boot testi görülmeden `accepted` yapılmaz.
- N8R8 ikincil yedek olarak kalır. Yalnız partition bütçesi ölçülüp 8 MB'a sığdığı kanıtlanırsa ve yeni bir ADR ile açılırsa kullanılabilir.
- PCB anten / IPEX anten seçimi bu ADR kapsamında değildir; kasa kararıyla birlikte ayrıca belgelenir.
- Firmware iş listesindeki partition CSV ve size budget görevi 16 MB varsayımıyla açılır.
