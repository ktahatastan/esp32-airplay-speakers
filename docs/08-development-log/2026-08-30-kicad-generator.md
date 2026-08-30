---
status: complete
owner: hardware-engineer
updated: 2026-08-30
tags: [development-log, hardware, kicad]
---

# 2026-08-30 — KiCad şema üreticisi

## Yapılan iş

- A3 tek sayfalık Harman Kardom modül şeması için tekrar çalıştırılabilir Python üreticisi eklendi.
- İlk eski `.sch` yükseltme yaklaşımı KiCad 10 CLI tarafından dönüştürülmediği için, yerel `.kicad_sch` üreten `kicad-sch-api` yaklaşımına geçildi.
- KiCad proje/şema kaynakları Git'e alındı; PDF, ERC raporu, kullanıcı durumu ve yedek çıktıları hariç tutuldu.
- G0-G4 blokajları, BTL prob güvenliği, `C_SAFE`, şarj sırasında çalma yasağı ve doğrulanmamış modül pin sıraları paftaya işlendi.

## Doğrulama

- Python üretimi: başarılı.
- KiCad CLI 10.0.6 PDF dışa aktarma: başarılı.
- ERC: `0 error / 11 warning`; uyarılar bilerek tekil bırakılan `SCK_GND`, `FMT_LOW`, `XSMT_HIGH`, `NC_DAC`, `AMP_SD_TBD`, I2C, USB ve reserved pinler.
- PDF raster önizleme ile A3 yerleşim kontrolü: başarılı; bölüm başlıklarının sol sayfa sınırı düzeltilerek yeniden üretildi.

## Açık riskler

- Modül konektörleri mantıksal arayüzdür; satın alınan kart revizyonları görülmeden fiziksel kablolama onaylanmaz.
- `C_SAFE`, sürücü empedansları, BMS balans davranışı ve güç anahtarı DC kesme değeri hâlâ blokajdır.

İlgili belge: [[../02-hardware/kicad-schematic|KiCad şeması ve üretim scripti]].
