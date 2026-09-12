---
status: active
owner: qa-engineer
updated: 2026-09-12
tags: [testing, gates]
---

# Test stratejisi ve kabul kapıları

Her kayıt; seri no, firmware/PCB sürümü, ölçüm cihazı, ortam, önkoşul, adımlar, ham veri yolu, pass/fail ve sorumlu içerir.

| Gate | Kapsam | Geçiş koşulu |
|---|---|---|
| G0 | Sürücü | DC/empedans/polarite verisi kayıtlı |
| G1 | Amfi/dummy-load | Jak polaritesi, yüksüz adaptör gerilimi, akım bütçesi (4 Ω sınıfı yükte), güç, clipping, DC offset, termal ve koruma davranışı 24 V'ta 4 Ω sınıfı yüke, amfi kazanç strap'i okundu (`C3`), besleme dip/brownout ve açılış-kapanış geçişi kaydedildi |
| G2 | Sürücü bring-up | HPF/crossover/limiter, dal başına release/hold, hizalama gecikmesi ve polarite ölçümle doğrulandı; tavanlar kullanılacakları beslemede kaydedildi |
| G6 | OTA/recovery | İmza/manifest, enerji kesintisi, ilk-boot sağlık kontrolü, rollback ve USB/UART recovery geçti |
| G8 | Dayanıklılık | 24 saat soak, kapalı kabinde sürekli sıcaklık ve mekanik güvenlik geçti |

Enerji verilen ilk yol tek amfi, tek woofer ve tek tweeter'dır: önce dummy-load üzerinde, sonra o çift için `G0`-`G2` geçince gerçek sürücülerde. Öteki üç amfi sürücülere ancak ondan sonra bağlanır; sekiz sürücü birden hiçbir zaman ilk enerjilendirmede değildir. Manuel dinleme/ürün kabulü kullanıcıya aittir; agent ölçüm ve otomatik test kanıtını raporlar.

Elektriksel bring-up sırasında kullanılacak test noktaları, beklenen gerilim/dalga şekilleri, BTL çıkış ölçüm yöntemi ve osiloskop kanal planı [[../02-hardware/circuit-and-wiring-plan#7. Test noktaları ve osiloskop planı|devre ve bağlantı planında]] tanımlıdır.

G6 senaryoları ve release kabul sözleşmesi [[../03-firmware/ota-and-release-plan#G6 kabul matrisi|OTA ve sürüm yönetimi planında]] tanımlıdır.

## G1 besleme ve amfi zorunlu ölçümleri

[[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]] dört amfiyi 24 V / 2,9 A DC adaptörden doğrudan besler; araya koruma entegresi girmez. Bu yüzden aşağıdakiler G1'de dummy-load üzerinde ayrı ayrı kaydedilir. Hiçbiri "adaptör öyle yazıyor" ile geçilemez.

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| Jak polaritesi | İlk enerjilendirmeden önce adaptör fişinde DMM | Merkez pozitif ölçüldü ve `VIN` girişiyle eşleşiyor; ters polarite koruması (seri Schottky veya ideal diyot) yerinde. **Adaptör fişi ölçüldü 2026-09-12: iç +, dış −**; kabin jakı tarafı jak takılınca ölçülür |
| Adaptör yüksüz gerilimi | Bağlamadan önce, yüksüz DMM | < 25,5 V. 24 V, amfinin 26 V maksimumunun 2 V altındadır; pay bu ölçümle doğrulanır, ölçülmeden adaptör bağlanmaz. **Ölçüldü 2026-09-12: 24,49 V** ([[bench-measurement-order#Adaptör yüksüz gerilimi — ölçüldü (2026-09-12)|kayıt]]) |
| Adaptör gerilimi | Tam yükte DMM | 24 V ± satıcı toleransı; `CONFIG_HK_SUPPLY_MV=24000` ile uyumlu, 8-26 V amfi aralığında |
| Besleme bütçesi / akım bütçesi | Dört amfi birden **4 Ω sınıfı** (ya da `G0`'ın ölçtüğü `Z_min`) non-inductive dummy-load'a sürülürken `VIN`'de ampermetre + osiloskop; amfi kazanç ayarı (`C3`) kayıtta; DSP'nin besleme dedektörü log satırı okunur | Adaptörün 2,9 A noktası bulundu ve kaydedildi: dedektörün en yüksek ortalama-karesi `supply_budget_sq`, `VIN` çöküşünün zaman sabiti `supply_window_ms` olarak, **ölçeklenmeden** profile yazılır; o noktada `VIN` çökmüyor, ESP32-S3 reset yemiyor. 8 Ω'da alınmış bir sonuç geçmez: gerçek akımın yaklaşık yarısıdır |
| Aşamalı enerjilendirme | Tek amfi + tek woofer/tweeter çifti, önce dummy-load | Bu çift `G0`-`G2`'yi geçmeden ikinci amfi sürücüye bağlanmıyor |
| Besleme dip / brownout | Tam yükte bas darbesi, osiloskopla `VIN` ve iki 5 V ray (buck A ESP32-S3, buck B DAC) | Amfiler ve ESP32-S3 reset yemiyor; iki 5 V ray da düşmüyor |
| Açılış pop | Adaptör takılırken çıkışta osiloskop; `TP33` ile birlikte | Amfinin kendi açılış geçişi kaydedildi; DAC `R6` ile susturuluyken çıkışa giden darbe amfinindir ve kararlaştırılmış bir seviyeye göre yargılanır. Firmware amfi susturması olmadan "darbe yok" vaat edemez (ADR-0011); kayıt olmadan karar yok |
| Kapanış pop | Çalarken adaptör çekilirken çıkışta osiloskop | Amfinin kendi kapanış geçişi kaydedildi; DAC susturuluyken çıkışa giden darbe amfinindir ve kabul edilebilir seviyede (kayıt olmadan karar yok) |
| Amfi ısınması | Tam yükte dummy-load üzerinde sıcaklık kaydı | Dört modülün sıcaklığı üretici sınırının altında; termal kaçış yok |
| Giriş yükü | Dört amfi girişi paralelken DAC çıkışında osiloskop | Paralel yük kazanç ayarına bağlıdır (SLOS528F Tablo 2; `C3` okuması kayıtta): 36 dB'de ~2,25 kΩ (en kötü 1,8 kΩ), 20 dB'de ~15 kΩ; PCM5102A'nın asgari yükü 1 kΩ. Ölçülen yük `C3` ile birlikte kaydedilir; DAC tam ölçekte bozulmasız sürüyor (aritmetik burada ölçüme dönüşür) |

## G8 kabin ölçümleri

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| 24 saat soak | Kapalı kabinde sürekli çalma | Kesinti, reset veya ses kaybı yok |
| Kapalı kabin sıcaklığı | Soak boyunca dört amfi ve iki buck üzerinde kayıt | Üretici sınırlarının altında ve kararlı |
| Mekanik güvenlik | Görsel ve elle kontrol | Gevşeyen bağlantı, sürtünen kablo veya titreşim kaynaklı hasar yok |
| Besleme kaybında kapanış | Çalarken adaptör çekilir; `VIN`, 5 V raylar ve bir amfi çıkışı tek kayıtta | Kapanış geçişi kaydedildi ve kararlaştırılmış seviyenin altında (amfi susturması yok, "pop yok" vaadi yok); amfilerin DAC'tan önce düştüğü sıra görüldü ya da görülmediği yazıldı; ayarlar korunmuş; yeniden takınca temiz açılış (PRD-007) |

## Firmware ölçümleri — kart gerekir, sürücü gerekmez

Bunlar bir kapıyı tek başına kapatmaz; `F3`'ün kabul ölçütlerine kanıt sağlar ([[../03-firmware/firmware-plan|firmware planı]]). Sayı operatörün kaydıdır, kodun değil.

| Ölçüm | Yöntem | Geçiş koşulu |
|---|---|---|
| DSP görev süresi | Ürün kartında, DSP arka ucuyla, en az 30 dakikalık gerçek bir AirPlay akışı; ses görevinin bastığı `dsp block max ... us mean ... us of ... us` satırları (üründe 60 s'de bir, tezgâh yapısında 10 s'de bir; log'da `dsp block max` ile aranır) ve alıcının underrun sayacı (`under=`) toplanır; EQ'nun üç bandı açıkken tekrarlanır | `max` her satırda blok süresi 7981 µs'nin (log satırındaki `of ... us`; ölçülen süre 352 karelik bloğa normalize edilir, yani 48 kHz kaynağın kısa blokları da aynı sınıra karşı okunur) altında; `under=0`; ortalama ve en kötü değer, satırın bastığı ortalama blok uzunluğu ve blok başına biquad sayısıyla birlikte (`hk_dsp_biquads_per_frame`: düz 6, üç EQ bandıyla 9) kayda girer |
| Besleme dedektörü boşta | Aynı akışta, aynı satırlar | Dedektörün aralık başına en yüksek ortalama-karesi ve en düşük kazancı kayda girer; tezgâh yer tutucusuyla (bütçe 1,0) kazanç EQ yükseltmedikçe 1,0'da kalmalı — kalmıyorsa profil ya da EQ beklenen değil |
| Açılış raporu | Ürün yapısı, profil yazılmamış kart | Rapor `profile absent` ve sesin izinli olmadığını yazıyor; alıcı I2S'i saatlemiyor. Profil yazılmış ama reddedilmişse rapor kararın adını yazıyor (`present:<verdict>`) ve ses susturulu kalıyor |

## Belge bütünlüğü kapısı

Her birleşme öncesi `python3 scripts/check_docs.py` çalıştırılır. Bu betik wikilink hedeflerini, frontmatter alanlarını, ADR durum sözlüğünü ve terim sapmalarını denetler. Fiziksel kapıların yerine geçmez; yalnız projenin kendi kaydının tutarlı olduğunu kanıtlar.
