---
status: draft
owner: orchestrator
updated: 2026-09-13
tags: [requirements]
---

# Gereksinimler

| Kimlik | Gereksinim | Kabul kanıtı |
|---|---|---|
| PRD-001 | Cihaz ağda MAC türevi benzersiz son ekiyle (`Merzarkabul XXXX`) görünmeli; ad, hangi ağa takılırsa takılsın çakışmamalı. Kurulum yüzeyleri (BLE yayını ve açık kurulum ağı) aynı son eki `PROV_Merzarkabul-XXXX` olarak taşır (ADR-0023); AirPlay ve mDNS adları değişmez | mDNS/AirPlay keşif kaydı |
| PRD-003 | Dört woofer ve dört tweeter'ın her biri kendi korumalı amfi kanalından sürülmeli; woofer bandı DAC `LOUT`, tweeter bandı DAC `ROUT` (ADR-0002) | Şema, crossover ve limiter testi |
| PRD-004 | Wi-Fi kurulumu **PIN girmeden**, BLE veya captive portal ile yapılabilmeli: BLE'de protocomm Security 1 ve sahiplik kanıtı yok, kurulum ağı açık, stok Espressif uygulamaları cihazı QR'sız ve önek ayarı değişmeden listeler; QR isteğe bağlı ve sır içermez (ADR-0023). Kabul evde kurulum içindir; ortak alan, ofis ya da konuk ağı kabulün dışındadır (risk kaydı) | iOS/Android test raporu — henüz yok. 2026-09-05'in BLE sonucu Security 2 ile alındı ve bu gereksinimin kanıtı değildir; portal hiçbir telefonda açılmadı |
| PRD-005 | 5 sn ağ reseti, 12 sn kullanıcı reseti çalışmalı | Durum makinesi testi |
| PRD-007 | Besleme kesilirken veya brownout'ta pop üretmeden güvenli kapanmalı | G8 raporu |
| PRD-008 | Kullanıcı reseti fabrika DSP kalibrasyonunu silmemeli | NVS testi |
| PRD-009 | Bir Apple cihazı alıcıyı bulmalı, eşleşmeli ve ses akışı gönderebilmeli (AirPlay 2, ADR-0007) | Geliştirme kartında 2026-09-05: iPhone oturumu kuruldu, `ptp_clock: LOCKED`, ses duyuldu ([[../06-testing/devkit-bring-up|bring-up kaydı]]); ürün kartında 2026-09-08: ürünün kendi yolu (PCM5102A → XH-A232) çaldı ([[../06-testing/bench-measurement-order|tezgâh kaydı]]). Dinleme kaydıdır; ses yolunun ölçümü G1 |

PRD-002 ve PRD-006 numaraları boştur; bir numara bir kez verilir ve bir daha verilmez.

Bir agent tek başına gereksinim silemez; değişiklik kabul edilmiş ADR gerektirir.
