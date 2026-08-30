---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-30
tags: [adr, ota, github-releases, security]
---

# ADR-0008: Otomatik OTA ve GitHub Releases dağıtımı

## Karar

Harman Kardom firmware sürümleri SemVer Git etiketleriyle oluşturulacak ve GitHub Actions tarafından test/build sonrası GitHub Release asset'i olarak yayımlanacaktır. Cihazlar uygun güç, sıcaklık, ağ ve idle-audio koşullarında daha yeni stable sürümü HTTPS üzerinden otomatik olarak alacaktır.

OTA; çift uygulama slotu, imzalı image, ilk-açılış sağlık kontrolü, rollback ve USB/UART recovery içerir. Dört cihazın tamamına doğrudan aynı anda dağıtım yapılmaz; bir canary cihazdan sonra stable terfisi uygulanır.

`esp_ghota` ilk istemci adayıdır fakat ESP-IDF/AirPlay uyumluluk spike'ı geçmeden zorunlu bağımlılık değildir. Başarısız olursa ESP-IDF `ESP HTTPS OTA` ve proje manifest istemcisi kullanılır.

## Gerekçe

- Kullanıcı müdahalesi olmadan güvenilir sürüm dağıtımı.
- Release binary, manifest, checksum ve release notes için tek izlenebilir kaynak.
- Enerji kesintisi veya bozuk sürümde dört hoparlörü aynı anda kullanılmaz hale getirmeme.
- Firmware signing anahtarını cihaz veya kaynak kod içinde dağıtmama.

## Sonuçlar

- G6, enerji kesintisi ve rollback testleriyle zorunlu kapıdır.
- OTA partition boyutu firmware özellik bütçesini etkiler.
- Bootloader/partition table V1'de uzaktan güncellenmez.
- Ayrıntılı plan: [[../03-firmware/ota-and-release-plan]].

