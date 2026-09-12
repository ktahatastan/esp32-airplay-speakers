# Merzarkabul Airplay Speakers iş listesi

Kilitli kararlar `AGENTS.md` içindedir ve yalnız supersede eden ADR ile değişir. Firmware aşama sırası [[docs/03-firmware/firmware-plan|firmware planındadır]].

## P0 - Tasarımı kilitleyen ölçümler

- [x] Dört woofer'ın DC dirençlerini ayrı ayrı ölç.
- [x] Dört tweeter'ın DC dirençlerini ayrı ayrı ölç.
- [ ] Sürücü etiketlerini ve bağlantı uçlarını fotoğraflandır.
- [x] Aday AirPlay yığınının AirPlay 1/2 yeteneğini ve lisansını kaynak kodundan doğrula.

## P1 - Güç prototipi

- [ ] 24 V adaptörün boşta çıkışını bağlamadan önce ölç (25,5 V'un altında olmalı); 2,9 A yükte gerilimini ölç; jak polaritesini (merkez artı) ilk güç vermeden önce ölçerek doğrula.
- [ ] `VIN` üzerindeki seri Schottky/ideal diyot adayına karar ver: gerilim düşümünü ve ısısını ölç (2,9 A'da yaklaşık 1 W veya üstü beklenir), reddedilirse 0 Ω köprüle (ADR-0020).
- [ ] Bir XH-A232'yi 12 V ve 24 V'ta dummy-load ile test et — D kademesi **4 Ω sınıfı** (ya da G0'ın ölçtüğü `Z_min`) yükte, 24 V'ta: termal ve koruma davranışı, çünkü veri sayfasının mutlak azami asgari BTL yükü 15 V üstünde 4,8 Ω ve amfi bu noktada karakterize edilmemiş (risk kaydı, `Kritik`). Sonra dördünü birlikte 24 V'ta 4 Ω sınıfı yüke sürerken adaptörün 2,9 A noktasını bul: `VIN` çökmesi, DSP dedektör okuması → `supply_budget_sq` / `supply_window_ms`.
- [ ] İki MP1584'ü (A: ESP32-S3, B: DAC) 5,10 V'a ayarla; ESP32 Wi-Fi akım sıçramalarında brownout testi yap.
- [ ] PCM5102A'da (kendi buck'ı B ile) ve dört amfi girişinde buck kaynaklı gürültüyü ölç.
- [ ] Dört amfi girişinin paralel yükünü DAC çıkışında ölç (kazanç ayarına bağlı: veri sayfası 36 dB'de ~9 kΩ, 20 dB'de ~60 kΩ giriş verir, yani dördü paralel ~2,25-15 kΩ; eski "~2,5 kΩ" yalnız 36 dB'de tutar); seviye düşümü ve bozulma kaydı G1'e girer.
- [ ] Açılış ve kapanışta hoparlör çıkışında amfinin kendi geçişini kaydet (`VIN`, 5 V raylar, `XSMT`, bir amfi çıkışı tek kayıtta); "pop yok" vaadi yok, kararlaştırılmış bir seviyeye göre yargılanır; çekmede amfilerin DAC'tan önce düşüp düşmediği yazılır. `VIN` tarafı önlem adayları kablolama planı §3.4'te; karar bu kayıttan sonra, ADR ile.
- [ ] XH-A232 kazanç strap'ini (`GAIN0`/`GAIN1`) dört kartta oku (`C3`), profilin `amp_gain_db` alanına yaz; **24 V'ta sürücüde dinlemeden önce** — amfi sabit kazançlı, tezgâh seviyesi 12 V'ta rayla sınırlıydı.

## P2 - Ses koruması

- [x] **G0 kapısındaki deliği kapat:** ses izni artık `factory_cal` içindeki `profile` blob'unun varlığını istiyor, şema sürümünü değil.
- [x] Profilin **geçerliliğini** de denetle: `hk_main` açılışta blob'u `hk_profile_load()` ile — arka ucun kullandığı aynı yargıçla — yargılıyor, `hk_storage` ses izni için kararı istiyor; reddedilen profil adıyla loglanır ve ses susturulu kalır (2026-09-12, ADR-0022).
- [ ] Tezgâh istisnasını kaldır: `G0`/`G2` profil ürettiğinde `sdkconfig.bench` gereksiz kalmalı.
- [ ] Ölçülen sürücü empedansına göre güvenli amfi seviyesini onayla; 24 V'ta XH-A232 4 Ω sınıfı sürücülere önemli güç verir, tavanı limiter belirler. Önce `C3` (amfi kazancı) ve G1'in 24 V / 4 Ω sınıfı kaydı: veri sayfası 15 V üstünde 4,8 Ω asgari yük verir, amfi bu noktada karakterize edilmemiş (risk kaydı, `Kritik`; karar ADR-0020 sahibinin).
- [ ] Woofer HPF, aktif crossover ve tweeter limiter başlangıç değerlerini belirle. **Firmware tarafı hazır:** `hk_profile` (şema 2) profilin biçimini, doğrulamasını ve zincire dönüşmesini taşıyor; kalan iş ölçülen sayıları doldurmak — G0: iki `Re`, subsonic köşe (`Fb`), crossover köşe (≥ 2 × tweeter `Fs`); G2: dal kazançları, tavanlar ve kaydedildikleri besleme, dal başına release/hold, tek dalda hizalama gecikmesi, tweeter polaritesi.
- [ ] Tepe tavanı ölçeklemesi artık tek yönlü (`min(1, referans/besleme)`) ve 24 V'ta dijital tavanı yarıya indirir; tezgâh 12 V rayını kırpmadıysa bu sürücüdeki voltu da yarıya indirir, kırptıysa 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye tezgâhın duyduğundan fazlasını verebilir — hangisi olduğunu `C3` söyler (TPA3110D2 sabit kazançlı). G2'de tavanları kullanılacakları beslemede (`reference_supply_mv` = 24000) kaydet; ölçekleme modelinin kendisi `C3` sayısıyla birlikte ADR maddesi (risk kaydı).
- [ ] Besleme bütçesi katının iki sayısını (`supply_budget_sq`, `supply_window_ms`) G1 S7'de dört amfi birlikte **4 Ω sınıfı** yüke sürülürken adaptörün 2,9 A noktasında ölç ve profile ölçeklenmeden yaz (ADR-0020, ADR-0022). Tezgâh yer tutucusu (1,0 / 100 ms) bir tam ölçekli dalın ortalama-karesi, doğrulayıcı sınırının (2,0) yarısı; tezgâh kazançlarıyla ancak kullanıcı EQ'su yükseltirse devreye girer.
- [ ] Tweeter `Re`'sini teyit et: kayıt 3,5 Ω, firmware'in tezgâh profili 2026-09-12'ye kadar 3,7 taşıyordu; kayıt kazandı, operatör prob direnciyle birlikte yeniden okur.
- [ ] Tezgâhta tweeter'ın önünde bugün bir `C_SAFE` takılı mı, hangi değer? Firmware'in notu 7 µF parçanın arızalı çıktığını ve takılı olmadığını söylüyor; 10 µF'in takıldığına dair kayıt yok (sürücü ölçüm planı, operatör sorusu).
- [ ] 2026-09-08'de hangi yapı çaldı (commit, `SDKCONFIG_DEFAULTS` parçaları, açılış raporunun arka uç satırı)? Kayıt ikiye ayrılıyor; yalnız operatör söyleyebilir (tezgâh ölçüm sırası, açık soru).
- [ ] Ürün kartında DSP arka ucuyla ≥ 30 dk akış: `dsp block max ... us mean ... us of ... us` satırları (üründe 60 s'de bir, tezgâh yapısında 10 s'de bir; max < 7981 µs, ölçüm 352 karelik bloğa normalize) ve underrun sayacı; sonuç `docs/06-testing/` altına (F3 kabul ölçütü, test stratejisi "Firmware ölçümleri").
- [ ] Sürücü başına termal (RMS) limiter — G2'nin güç dayanımı / termal zaman sabiti ölçümünü bekler; dedektör mekanizması `hk_supply_limiter`'da hazır, dal başına örneklenir.
- [ ] 24 bit I2S çıkış + dither ve AirPlay ses seviyesi çarpımının float alana taşınması — kalite işi, koruma değil; gölgenin I2S yapılandırmasını ve vendor ses rampasını değiştirdiği için ayrı, gölge incelemesi yeniden kaydedilen bir değişiklik.
- [ ] Çalışma zamanında EQ değişikliği: bir ayar yazıcısı (portal/alıcı) var olduğunda `hk_dsp_set_eq` için çalma görevine posta kutusu; bugün EQ satırları çalma başında okunur ve açılış raporunda listelenir.

## P4 - Kabin

- [ ] Dört amfi ve iki buck ısısını kapalı kabinde ölç; havalandırmayı buna göre belirle (G8).
- [ ] Sürücü yerleşimini ve pasif radyatör akordunu G0 (`Fs`, `Vas`) sonrasında belirle; woofer'ların ayrı hacim alıp almayacağı da o zaman kararlaşır ([[docs/04-acoustics/cabinet-plan|kabin planı]]).
- [ ] İlk prototip kabulünden sonra BOM'u kesinleştir.

## P5 - Buton, LED ve provisioning

- [x] Proje, AirPlay, BLE, SoftAP, mDNS ve QR yüzeylerinde Merzarkabul adlandırmasını uygula.
- [x] Kanonik ESP32-S3 kartını seç (ADR-0010: N16R8).
- [x] Satın alınan kartı boot testinden geçir (2026-09-08, ürün kartı bring-up kaydı).
- [ ] Aday GPIO tablosunu satın alınan kartın şemasıyla doğrula — `accepted` için gereken bu.
- [x] Tek buton için kısa basış, 5 sn ağ sıfırlama ve 12 sn kullanıcı fabrika sıfırlama durum makinesini geliştir.
- [ ] 12 sn fabrika sıfırlamayı **donanımda** doğrula; 2026-09-05'te basış olay üretmedi (köprü teması).
- [x] Fabrika sürücü koruma/limiter kalibrasyonunu kullanıcı resetinden ayrı NVS alanında tut (ayrı partition; gerçek NVS ile test edildi).
- [x] RGB LED durum sürücüsünü audio task'tan bağımsız düşük öncelikli görev olarak geliştir.
- [x] Captive portalı yaz (ADR-0015: WPA2 kurulum ağı + `hk_portal`).
- [ ] Portalı donanımda dene: iOS ve Android kurulum sayfasını kendiliğinden açıyor mu, ağ listesi doluyor mu, kimlik bilgisiyle katılıyor mu?
- [x] İki taşımayı aynı anda aç (ADR-0016). Ürün kartında ölçüldü: tek pencerede BLE advertise + SoftAP + portal, 148.007 B dahili boş.
- [ ] Her iki taşımayla ayrı ayrı kurulumu tamamla, diğeri ayaktayken.
- [ ] Pencere kapandıktan sonra dahili belleğin geri geldiğini ölç (`FREE_BTDM`).
- [ ] Pencere kapanıp yeniden açıldığında yalnız SoftAP sunulduğunu ve logun BLE iddia etmediğini doğrula.
- [ ] WPA2 kurulum ağının BLE + Security 2 yolunu bozmadığını donanımda doğrula.
- [x] ESP-IDF Unified Provisioning BLE transport ve Security 2 / benzersiz PoP ekle.
- [x] SRP6a kullanıcı adına karar ver — ADR-0014: `wifiprov`, çünkü ad sır değil ve özel bir ad yalnız QR'sız yolu kırıyor.
- [ ] Verifier değiştiği için QR'lı BLE kurulumunu donanımda **tekrar** doğrula.
- [ ] QR'sız kurulumu (listeden seç + parolayı yaz) ilk kez dene; ADR-0014 bu yolu denenebilir hâle getirdi.
- [x] Provisioning ve Wi-Fi QR kodu üret (`tools/provision_credentials.py`).
- [x] Provisioning tamamlanınca BLE belleğinin serbest bırakıldığını doğrula (`BTDM memory released`, geliştirme kartı).
- [x] iOS'ta Espressif BLE Provisioning uygulamasıyla kurulum testi yap — uçtan uca çalıştı.
- [ ] Android'de BLE kurulum testi yap; hiç denenmedi.
- [ ] Uygulamasız iOS/Android akışını test et — **önce portalın var olması gerekiyor**. Espressif'in SoftAP uygulaması ayrıca AES-GCM katmanında düşüyor (`mbedtls_gcm_auth_decrypt : -18`); ESP-IDF'in kendi istemcisi aynı cihaza bağlanıyor, yani kusur o uygulamada.
- [ ] Özel mobil uygulama kararı verilirse iOS AccessorySetupKit ve Android Companion Device Manager prototipi hazırla.
- [ ] Provisioning timeout, tekrar deneme, parola gizliliği ve NVS encryption testlerini yap.
- [ ] Açılırken/kapanırken pop ve ESP32 reset testlerini yap.

## P6 - Firmware aşamaları

Ayrıntı, önkoşul ve kabul ölçütleri: [[docs/03-firmware/firmware-plan|firmware planı]].

- [x] `F0` iskelet: ESP-IDF `v5.5.1` kilidi, partition CSV, boyut/partition doğrulayıcısı, host testi, PR CI.
- [x] `F0` kalan, birinci yarı: ürün kartına yazıldı ve açılış raporu doğrulandı (2026-09-08).
- [ ] `F0` kalan, ikinci yarı: GPIO tablosunu satın alınan kartın **şemasıyla** karşılaştır ve `accepted` yap. Açılış testi tek başına yetmiyor (ADR-0011).
- [x] `F1` araştırma yarısı: yığın seçildi, derlendi, lisans incelendi, ADR-0007 kabul edildi.
- [x] `F1` ölçüm yarısı: yığın vendor edildi, karta yüklendi, bir Apple cihazı bağlandı, PTP kilitlendi, ses duyuldu.
- [ ] `F1` kalan: **akış sırasındaki** kaynak kullanımını (PSRAM, CPU) ölç.
- [ ] `F2` I2S/DAC/bi-amp ses yolu bring-up (G1 sonrası).
- [ ] `F3` HPF, crossover ve limiter zinciri: ürün çıkış arka ucu (ADR-0022); dördüncü derece subsonic, dal başına limiter zamanlaması, delay/polarite alanları ve besleme bütçesi katı kodda (şema 2), profil açılışta yargılanıyor, blok süresi telemetrisi var; sayılar G0/G1/G2 — her biri yer tutucu, profil yokken ürün susar.
- [x] `F4` Wi-Fi, mDNS ve BLE/SoftAP Unified Provisioning — geliştirme kartında uçtan uca. Portal kısmı hariç (yukarıya bakın).
- [x] `F5` buton durum makinesi ve RGB LED animatörü — 12 sn senaryosu ve LED'in ses zamanlamasına etkisi hariç.
- [ ] `F7` imzalı A/B OTA, release hattı ve recovery (G6).
- [ ] `F8` soak (G8).

## P6b - Firmware güvenliği ve kurtarma

- [x] `factory_cal` ile `user_settings` NVS şemasını ve migration testlerini yaz.
- [ ] Benzersiz PoP/QR üretimi, seri eşleme ve güvenli yedekleme prosedürünü tanımla.
- [x] ESP-IDF sürümünü kilitle (`v5.5.1`); `esp_ghota` spike'ı tamamlandı ve aday reddedildi (ADR-0008).
- [x] `otadata`, `ota_0`, `ota_1` ve kalibrasyon/NVS alanlarını içeren partition CSV ve size budget oluştur (iki kart için ayrı tablo, CI'da denetleniyor).
- [x] SemVer `v*.*.*` tag ile test/build/sign/checksum/GitHub Release üreten GitHub Actions hattını kur. **Hiç sürüm yayımlanmadı.**
- [x] Release manifest target/donanım/sürüm/hash doğrulamasını ve stable update state machine'ini geliştir (`hk_manifest`, `hk_ota`; üretici/doğrulayıcı CI'da çapraz denetimli).
- [x] Ses çalarken, Wi-Fi yokken ve eşzamanlı güncellemede OTA erteleme kapılarını uygula (`hk_gate`, `hk_sched`).
- [ ] İlk-boot health check, A/B rollback, canary/stable dağıtım ve güç kesintisi G6 testlerini tamamla.
- [x] Wi-Fi parolası/PoP/anahtarların loglarda görünmediğini otomatik taramayla doğrula (`tools/check_no_credential_logs.py`, CI'da).
- [x] USB/UART recovery ve boot prosedürünü saha servis dokümanına ekle ([[docs/03-firmware/usb-recovery|usb-recovery]]). Donanımda çalıştırılmadı.

## P7 - Agentic süreç ve proje hafızası

- [ ] Her geliştirme görevine sahibi, dosya kapsamı, başarı ölçütü ve gate ata.
- [ ] Mimari/güvenlik davranışı değiştiğinde ADR aç veya mevcut ADR'yi supersede et.
- [ ] Material değişikliklerde geliştirme günlüğü ve test kanıtını güncelle.
- [ ] Agent sahipliği değişiminde handoff kaydı oluştur.
- [x] Birleşme öncesi doğrulamayı tekrarlanabilir hâle getir (`scripts/check_docs.py`).
- [ ] Her birleşme öncesi `python3 scripts/check_docs.py` ve `git diff --check` çalıştır.
- [ ] Geliştirme kartında açıklanmayan ~14 s'lik ilk birleşme düşüşünü ürün kartında tekrar incele; sebep kodunu kaydet.
- [ ] KiCad kurulu bir makinede `generate_merzarkabul.py --validate` ile ERC/PDF doğrulamasını tamamla ve paftayı üret.
