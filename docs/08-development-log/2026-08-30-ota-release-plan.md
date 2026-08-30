---
status: complete
owner: firmware-engineer
updated: 2026-08-30
tags: [development-log, firmware, ota, release]
---

# 2026-08-30 — OTA ve release planı

## Amaç

GitHub Releases tabanlı otomatik firmware güncelleme, sürümleme, güvenli A/B OTA, rollback ve dört cihazlık dağıtım planını proje kaydına eklemek.

## Değişiklikler

- [[../03-firmware/ota-and-release-plan|OTA ve sürüm yönetimi planı]] oluşturuldu.
- [[../07-decisions/ADR-0008-github-releases-ota|ADR-0008]] ile otomatik GitHub Releases dağıtımı kabul edildi.
- `esp_ghota` uygulama adayı; yerel `ESP HTTPS OTA` fallback olarak kaydedildi.
- SemVer/tag CI hattı, release asset sözleşmesi, imzalama, canary/stable dağıtım ve G6 matrisi tanımlandı.
- Firmware MOC, güvenlik/recovery, iş kırılımı ve test stratejisi güncellendi.

## Doğrulama

Kaynaklar birincil ESP-IDF, GitHub ve `esp_ghota` deposuyla karşılaştırıldı. Obsidian wikilink hedefleri, frontmatter ve `git diff --check` doğrulandı. Firmware iskeleti bulunmadığından build veya fiziksel OTA testi yapılmadı.

## Sonraki adım

Firmware framework sürümünü kilitleyip `esp_ghota` uyumluluk spike'ı, partition size budget ve GitHub Actions PR build hattını uygulamak.
