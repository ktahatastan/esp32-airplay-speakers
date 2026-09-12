---
status: active
owner: qa-engineer
updated: 2026-08-31
tags: [testing, gates]
---

# Test stratejisi ve kabul kapıları

Her kayıt; seri no, firmware/PCB sürümü, ölçüm cihazı, ortam, önkoşul, adımlar, ham veri yolu, pass/fail ve sorumlu içerir.

| Gate | Kapsam | Geçiş koşulu |
|---|---|---|
| G0 | Sürücü | DC/empedans/polarite verisi kayıtlı |
| G1 | Amfi/dummy-load | Jak polaritesi, güç, clipping, DC offset, termal, besleme dip/brownout ve açılış-kapanış pop güvenli |
| G2 | Sürücü bring-up | HPF/crossover/limiter doğrulandı |
| G6 | OTA/recovery | İmza/manifest, enerji kesintisi, ilk-boot sağlık kontrolü, rollback ve USB/UART recovery geçti |
| G7 | Dört cihaz | 2+ saat drift/jitter ve yeniden bağlanma geçti |
| G8 | Dayanıklılık | 24 saat soak, kapalı kabinde sürekli sıcaklık ve mekanik güvenlik geçti |

G0-G2 geçmeden dört üniteye çoğaltma yoktur. Manuel dinleme/ürün kabulü kullanıcıya aittir; agent ölçüm ve otomatik test kanıtını raporlar.

Elektriksel bring-up sırasında kullanılacak test noktaları, beklenen gerilim/dalga şekilleri, BTL çıkış ölçüm yöntemi ve osiloskop kanal planı [[../02-hardware/circuit-and-wiring-plan#7. Test noktaları ve osiloskop planı|devre ve bağlantı planında]] tanımlıdır.

G6 senaryoları ve release kabul sözleşmesi [[../03-firmware/ota-and-release-plan#G6 kabul matrisi|OTA ve sürüm yönetimi planında]] tanımlıdır.

## G1 besleme ve amfi zorunlu ölçümleri

[[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]] amfiyi 19 V DC adaptörden doğrudan besler; araya koruma entegresi girmez. Bu yüzden aşağıdakiler G1'de dummy-load üzerinde ayrı ayrı kaydedilir. Hiçbiri "adaptör öyle yazıyor" ile geçilemez.

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| Jak polaritesi | İlk enerjilendirmeden önce adaptör fişinde DMM | Merkez pozitif ölçüldü ve `VIN` girişiyle eşleşiyor; ters polarite koruması (seri Schottky veya ideal diyot) yerinde |
| Adaptör gerilimi | Yüksüz ve tam yükte DMM | 19 V ± satıcı toleransı; `CONFIG_HK_SUPPLY_MV` ile uyumlu, 8-26 V amfi aralığında |
| Besleme dip / brownout | Tam yükte bas darbesi, osiloskopla `VIN` ve 5 V ray | Amfi ve ESP32-S3 reset yemiyor; 5 V ray düşmüyor |
| Açılış pop | Adaptör takılırken çıkışta osiloskop | Mute sıralayıcısı çıkışı susturuyor; dummy-load üzerinde darbe yok |
| Kapanış pop | Çalarken adaptör çekilirken çıkışta osiloskop | Amfi susturulmadan önce çıkışa DC darbesi gitmiyor |
| Amfi ısınması | Tam yükte dummy-load üzerinde sıcaklık kaydı | Modül sıcaklığı üretici sınırının altında; termal kaçış yok |

## G8 kabin ölçümleri

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| 24 saat soak | Kapalı kabinde sürekli çalma | Kesinti, reset veya ses kaybı yok |
| Kapalı kabin sıcaklığı | Soak boyunca amfi ve buck üzerinde kayıt | Üretici sınırlarının altında ve kararlı |
| Mekanik güvenlik | Görsel ve elle kontrol | Gevşeyen bağlantı, sürtünen kablo veya titreşim kaynaklı hasar yok |
| Besleme kaybında kapanış | Çalarken adaptör çekilir | Pop yok, ayarlar korunmuş, yeniden takınca temiz açılış (PRD-007) |

## Belge bütünlüğü kapısı

Her birleşme öncesi `python3 scripts/check_docs.py` çalıştırılır. Bu betik wikilink hedeflerini, frontmatter alanlarını, ADR durum sözlüğünü ve terim sapmalarını denetler. Fiziksel kapıların yerine geçmez; yalnız projenin kendi kaydının tutarlı olduğunu kanıtlar.
