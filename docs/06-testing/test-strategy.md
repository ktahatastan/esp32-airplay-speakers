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
| G1 | Amfi/dummy-load | Jak polaritesi, yüksüz adaptör gerilimi, akım bütçesi, güç, clipping, DC offset, termal, besleme dip/brownout ve açılış-kapanış pop güvenli |
| G2 | Sürücü bring-up | HPF/crossover/limiter doğrulandı |
| G6 | OTA/recovery | İmza/manifest, enerji kesintisi, ilk-boot sağlık kontrolü, rollback ve USB/UART recovery geçti |
| G8 | Dayanıklılık | 24 saat soak, kapalı kabinde sürekli sıcaklık ve mekanik güvenlik geçti |

Enerji verilen ilk yol tek amfi, tek woofer ve tek tweeter'dır: önce dummy-load üzerinde, sonra o çift için `G0`-`G2` geçince gerçek sürücülerde. Öteki üç amfi sürücülere ancak ondan sonra bağlanır; sekiz sürücü birden hiçbir zaman ilk enerjilendirmede değildir. Manuel dinleme/ürün kabulü kullanıcıya aittir; agent ölçüm ve otomatik test kanıtını raporlar.

Elektriksel bring-up sırasında kullanılacak test noktaları, beklenen gerilim/dalga şekilleri, BTL çıkış ölçüm yöntemi ve osiloskop kanal planı [[../02-hardware/circuit-and-wiring-plan#7. Test noktaları ve osiloskop planı|devre ve bağlantı planında]] tanımlıdır.

G6 senaryoları ve release kabul sözleşmesi [[../03-firmware/ota-and-release-plan#G6 kabul matrisi|OTA ve sürüm yönetimi planında]] tanımlıdır.

## G1 besleme ve amfi zorunlu ölçümleri

[[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]] dört amfiyi 24 V / 2,9 A DC adaptörden doğrudan besler; araya koruma entegresi girmez. Bu yüzden aşağıdakiler G1'de dummy-load üzerinde ayrı ayrı kaydedilir. Hiçbiri "adaptör öyle yazıyor" ile geçilemez.

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| Jak polaritesi | İlk enerjilendirmeden önce adaptör fişinde DMM | Merkez pozitif ölçüldü ve `VIN` girişiyle eşleşiyor; ters polarite koruması (seri Schottky veya ideal diyot) yerinde |
| Adaptör yüksüz gerilimi | Bağlamadan önce, yüksüz DMM | < 25,5 V. 24 V, amfinin 26 V maksimumunun 2 V altındadır; pay bu ölçümle doğrulanır, ölçülmeden adaptör bağlanmaz |
| Adaptör gerilimi | Tam yükte DMM | 24 V ± satıcı toleransı; `CONFIG_HK_SUPPLY_MV=24000` ile uyumlu, 8-26 V amfi aralığında |
| Limiter tavanı / akım bütçesi | Dört amfi birden sürülürken `VIN`'de ampermetre + osiloskop | Tavan 2,9 A adaptör bütçesinden türetilmiş; sekiz kanal birlikte 70 W'ı aşmıyor, `VIN` çökmüyor, ESP32-S3 reset yemiyor |
| Aşamalı enerjilendirme | Tek amfi + tek woofer/tweeter çifti, önce dummy-load | Bu çift `G0`-`G2`'yi geçmeden ikinci amfi sürücüye bağlanmıyor |
| Besleme dip / brownout | Tam yükte bas darbesi, osiloskopla `VIN` ve iki 5 V ray (buck A ESP32-S3, buck B DAC) | Amfiler ve ESP32-S3 reset yemiyor; iki 5 V ray da düşmüyor |
| Açılış pop | Adaptör takılırken çıkışta osiloskop | Mute sıralayıcısı çıkışı susturuyor; dummy-load üzerinde darbe yok |
| Kapanış pop | Çalarken adaptör çekilirken çıkışta osiloskop | Amfinin kendi kapanış geçişi kaydedildi; DAC susturuluyken çıkışa giden darbe amfinindir ve kabul edilebilir seviyede (kayıt olmadan karar yok) |
| Amfi ısınması | Tam yükte dummy-load üzerinde sıcaklık kaydı | Dört modülün sıcaklığı üretici sınırının altında; termal kaçış yok |
| Giriş yükü | Dört amfi girişi paralelken DAC çıkışında osiloskop | Dört ~10 kΩ giriş paralelde ~2,5 kΩ; PCM5102A tam ölçekte bozulmasız sürüyor (aritmetik burada ölçüme dönüşür) |

## G8 kabin ölçümleri

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| 24 saat soak | Kapalı kabinde sürekli çalma | Kesinti, reset veya ses kaybı yok |
| Kapalı kabin sıcaklığı | Soak boyunca dört amfi ve iki buck üzerinde kayıt | Üretici sınırlarının altında ve kararlı |
| Mekanik güvenlik | Görsel ve elle kontrol | Gevşeyen bağlantı, sürtünen kablo veya titreşim kaynaklı hasar yok |
| Besleme kaybında kapanış | Çalarken adaptör çekilir | Pop yok, ayarlar korunmuş, yeniden takınca temiz açılış (PRD-007) |

## Belge bütünlüğü kapısı

Her birleşme öncesi `python3 scripts/check_docs.py` çalıştırılır. Bu betik wikilink hedeflerini, frontmatter alanlarını, ADR durum sözlüğünü ve terim sapmalarını denetler. Fiziksel kapıların yerine geçmez; yalnız projenin kendi kaydının tutarlı olduğunu kanıtlar.
