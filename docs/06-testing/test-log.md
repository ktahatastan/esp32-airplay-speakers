---
status: active
owner: qa-engineer
updated: 2026-09-12
tags: [testing, log]
---

# Test kayıt dizini

Yeni rapor için [[../templates/test-report|test raporu şablonunu]] kullanın.

| Tarih | Gate | Cihaz | Sonuç | Rapor |
|---|---|---|---|---|
| 2026-09-05 | yok (kapı değil) | N8R2 geliştirme kartı | PASS | [[devkit-bring-up|Geliştirme kartı bring-up kaydı]] |
| 2026-09-08 | yok (kapı değil) | N16R8 **ürün kartı** | PASS | [[product-board-bring-up|Ürün kartı bring-up kaydı]] |
| 2026-09-12 | yok (sahip gözlemi, kurulum kaydedilmedi) | Ürün kartı tezgâhı, tek amfi | GÖZLEM — ölçüm değil | [[bench-measurement-order#Sahip gözlemi — paylaşılan buck DAC'a hışırtı bindirdi (2026-09-12)|Paylaşılan buck DAC'a hışırtı bindirdi]] |
| 2026-09-12 | G1 satırları (adaptör yüksüz gerilimi, polarite) | 24 V adaptör, fiş boşta | PASS — 24,49 V < 25,5 V; merkez pozitif |
| 2026-09-12 | G0 (kısmi) | Woofer + tweeter, DAC taraması, ikinci N16R8 (`056C`) | KAYIT — woofer `Fs` ≈ 50 Hz (kaba, ±%13); tweeter 1–3 kHz düz, tepe çözülemedi; ince tarama açık | [[../02-hardware/driver-measurements#11. Sonuç|sürücü ölçüm kaydı §11]] | [[bench-measurement-order#Adaptör yüksüz gerilimi — ölçüldü (2026-09-12)|Adaptör yüksüz gerilimi]] |
