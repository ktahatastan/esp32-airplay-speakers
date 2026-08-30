---
status: complete
owner: orchestrator
updated: 2026-08-30
tags: [development-log, hardware, wiring]
---

# 2026-08-30 — Devre ve bağlantı planı

## Amaç

Tek Harman Kardom hoparlör için güç, batarya/BMS, I2S/DAC, bi-amp sürücüler, buton/LED, konnektör ve fiziksel yerleşim bağlantılarını çizmek.

## Değişiklikler

- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı şemaları]] oluşturuldu.
- Common-port ve separate-port BMS farkı işaretlendi.
- ESP32-S3 için yalnız prototip adayı GPIO tablosu eklendi.
- PCM5102A 3-wire I2S ve XH-A232 BTL bağlantıları açıklandı.
- Tweeter koruması `C_SAFE` ölçüm bekleyen zorunlu bileşen olarak gösterildi.
- USB/harici 5 V geri besleme riskine servis jumperı eklendi.
- TP0-TP27 test noktaları, beklenen gerilim/dalga biçimleri ve ölçüm referansları eklendi.
- Class-D BTL çıkışları için diferansiyel prob veya iki kanal `CH1-CH2` yöntemi tanımlandı.
- Besleme ripple, I2S saatleri, DAC seviyesi, dummy-load ve power-sequence ölçüm planı eklendi.
- Batarya, BMS, güç, ESP32-S3, PCM5102A, XH-A232, sürücüler, kullanıcı kontrolleri ve TP0-TP27 noktalarını tek paftada gösteren SVG/PNG görsel şema eklendi.
- EasyEDA/KiCad benzeri koordinat çerçevesi, devre sembolleri, net ve parça referansları ile antet kullanan ikinci mühendislik paftası görünümü eklendi.
- Belge donanım ve ana Obsidian MOC sayfalarına bağlandı.

## Kararlar

Kabul edilmiş güç ve V1 şarj politikası değiştirilmedi. Kesin BMS, GPIO, sigorta, kablo ve tweeter kondansatörü açık karar olarak bırakıldı.

## Doğrulama

Tüm Obsidian wikilink hedefleri, Mermaid çit çiftleri, YAML frontmatter, satır sonu boşlukları ve `git diff --check` doğrulandı. SVG XML olarak ayrıştırıldı; Chrome ile 1800×1200 PNG render alınıp görsel olarak incelendi. Yerel Mermaid CLI bulunmadığından Mermaid otomatik render testi yapılmadı. Fiziksel bağlantı veya elektriksel gate testi yapılmadı.

## Sonraki adım

G0 kapsamında dört woofer ve dört tweeter ölçülmeli; XH-A232 ile PCM5102A modüllerinin ön/arka yüksek çözünürlüklü fotoğrafları kaydedilmelidir.
