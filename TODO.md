# Harman Kardom iş listesi

Kilitli kararlar `AGENTS.md` içindedir ve yalnız supersede eden ADR ile değişir. Firmware aşama sırası [[docs/03-firmware/firmware-plan|firmware planındadır]].

## P0 - Tasarımı kilitleyen ölçümler

- [ ] Dört woofer'ın DC dirençlerini ayrı ayrı ölç.
- [ ] Dört tweeter'ın DC dirençlerini ayrı ayrı ölç.
- [ ] Sürücü etiketlerini ve bağlantı uçlarını fotoğraflandır.
- [ ] Hedefi kesinleştir: dört bağımsız mono kutu mu, iki stereo çift mi?
- [ ] İstenen normal ses seviyesinde çalışma süresi hedefini belirle.
- [x] Aday AirPlay yığınının AirPlay 1/2 ve multiroom yeteneklerini kaynak kodu/lisansla doğrula.
- [ ] Dört hedefin birlikte seçilebildiği en küçük ağ ses prototipini kur.
- [x] İki saatlik drift/jitter ölçüm düzenini ve G7 sayısal kabul eşiklerini ADR-0007'de kilitle.

## P1 - Tek hoparlör güç prototipi

- [ ] 4S1P test paketi için dört eşlenmiş hücre seç.
- [ ] Gerçek balanslı, en az 10 A sürekli 4S BMS seçimini doğrula.
- [ ] Paketi sigorta, NTC ve uygun izolasyonla punta kaynaklı hazırlat.
- [ ] PD tetikleyicinin 20 V profilini boşta ve 2 A yükte doğrula.
- [ ] XL4015'i batarya bağlı değilken 16,80 V / 2,00 A'e kalibre et; elektronik yükle doğrula.
- [ ] XL4015 şarj sonlandırma davranışını ölç; sonlandırma yoksa ADR-0009 alternatiflerinden birini seç.
- [ ] Type-C kablosunun 5 A / e-marker kimliğini USB-C test cihazıyla doğrula.
- [ ] XH-A232'yi 12 V, 14,8 V ve 16,8 V'ta dummy-load ile test et.
- [ ] MP1584'ü 5,10 V'a ayarla; ESP32 Wi-Fi akım sıçramalarında brownout testi yap.
- [ ] PCM5102A ve amfi girişinde buck kaynaklı gürültüyü ölç.
- [ ] INA226 ile bekleme, normal müzik ve yüksek ses güç tüketimini kaydet.

## P2 - Ses koruması ve çalışma süresi

- [ ] Ölçülen sürücü empedansına göre güvenli amfi gerilimini onayla.
- [ ] Woofer HPF, aktif crossover ve tweeter limiter başlangıç değerlerini belirle.
- [ ] Tam dolu ve düşük bataryada clipping/limiter davranışını doğrula.
- [ ] 4S1P gerçek çalışma süresini ölç.
- [ ] Sonuca göre nihai paketi 4S1P veya 4S2P olarak seç.

## P3 - Şarj ve power-path

- [ ] Sürüm 1'de şarj sırasında amfiyi donanımsal olarak kapat.
- [ ] Şarj akımı, hücre sıcaklığı, balans ve şarj sonlandırmayı doğrula.
- [ ] BMS balans akımını/eşiğini satıcıdan yazılı al; beş çevrimde hücre sapmasını ölç.
- [ ] XL4015 ters polarite riskine karşı kutup etiketleme ve bağlantı sırası prosedürünü yaz.
- [ ] Şarjdayken çalma gereksinimi için BQ24610 hazır kart/modül araştırmasını tamamla.
- [ ] Hazır çözüm uygun değilse BQ24610 veya BQ25792 tabanlı özel güç PCB'si tasarla.
- [ ] Adaptör-batarya geçişinde pop, reset ve ses kesintisi testi yap.

## P4 - Dört hoparlöre çoğaltma

- [ ] İlk prototip kabulünden sonra dört hoparlörlük toplam BOM'u kesinleştir.
- [ ] Dört batarya paketini aynı hücre ve BMS ile üret.
- [ ] Her cihaz için ayrı sigorta, sıcaklık sensörü ve seri numarası kullan.
- [ ] Dört cihazda AirPlay senkronu ve batarya telemetrisini birlikte test et.
- [ ] Kabin içi batarya bölmesini akustik hacimden ayır ve dışa havalandır.

## P5 - Buton, LED ve provisioning

- [x] Proje, AirPlay, BLE, SoftAP, mDNS ve QR yüzeylerinde Harman Kardom adlandırmasını uygula. (Captive portal yüzeyi yok; aşağıya bakın.)
- [x] Kanonik ESP32-S3 kartını seç (ADR-0010: N16R8).
- [ ] Satın alınan kartın şemasıyla aday GPIO tablosunu doğrula ve boot testinden geçir.
- [x] Tek buton için kısa basış, 5 sn ağ sıfırlama ve 12 sn kullanıcı fabrika sıfırlama durum makinesini geliştir.
- [ ] 12 sn fabrika sıfırlamayı **donanımda** doğrula; 2026-09-05'te basış olay üretmedi (köprü teması).
- [x] Fabrika sürücü koruma/limiter kalibrasyonunu kullanıcı resetinden ayrı NVS alanında tut (ayrı partition; gerçek NVS ile test edildi).
- [x] RGB LED durum sürücüsünü audio task'tan bağımsız düşük öncelikli görev olarak geliştir.
- [ ] **Captive portal yok.** `wifi_prov_scheme_softap` sayfa sunmuyor; PRD-004'ün uygulamasız kurulum gereksinimi karşılanmıyor. Portalı yaz ya da ADR-0005'i revize et.
- [x] ESP-IDF Unified Provisioning BLE transport ve Security 2 / benzersiz PoP ekle.
- [ ] SRP6a kullanıcı adına karar ver: `harmankardom` mu, ekosistem varsayılanı `wifiprov` mu? ADR gerekir.
- [x] Cihaz başına provisioning ve Wi-Fi QR kodu üret (`tools/provision_credentials.py`).
- [x] Provisioning tamamlanınca BLE belleğinin serbest bırakıldığını doğrula (`BTDM memory released`, geliştirme kartı).
- [x] iOS'ta Espressif BLE Provisioning uygulamasıyla kurulum testi yap — uçtan uca çalıştı.
- [ ] Android'de BLE kurulum testi yap; hiç denenmedi.
- [ ] Uygulamasız iOS/Android akışını test et — **önce portalın var olması gerekiyor**. Espressif'in SoftAP uygulaması ayrıca AES-GCM katmanında düşüyor (`mbedtls_gcm_auth_decrypt : -18`); ESP-IDF'in kendi istemcisi aynı cihaza bağlanıyor, yani kusur o uygulamada.
- [ ] Özel mobil uygulama kararı verilirse iOS AccessorySetupKit ve Android Companion Device Manager prototipi hazırla.
- [ ] Provisioning timeout, tekrar deneme, parola gizliliği ve NVS encryption testlerini yap.
- [ ] Ayrı 24 V DC / 5 A fiziksel güç anahtarını BMS sonrası yük hattına ekle.
- [ ] Hoparlör kapalıyken şarj; açılırken/kapanırken pop ve ESP32 reset testlerini yap.

## P6 - Firmware aşamaları

Ayrıntı, önkoşul ve kabul ölçütleri: [[docs/03-firmware/firmware-plan|firmware planı]].

- [x] `F0` iskelet: ESP-IDF `v5.5.1` kilidi, partition CSV, boyut/partition doğrulayıcısı, host testi, PR CI.
- [ ] `F0` kalan: satın alınan kartla `idf.py flash monitor` ile açılış raporunu doğrula ve GPIO tablosunu `accepted` yap.
- [x] `F1` araştırma yarısı: yığın seçildi, derlendi, lisans incelendi, ADR-0007 kabul edildi.
- [x] `F1` ölçüm yarısı, tek kartlık kısmı: yığın vendor edildi, karta yüklendi, bir Apple cihazı bağlandı, PTP kilitlendi, ses duyuldu.
- [ ] `F1` kalan: dört hedefin birlikte seçilebildiğini ve **akış sırasındaki** kaynak kullanımını ölç — dört kart ister.
- [ ] `F2` I2S/DAC/bi-amp ses yolu bring-up (G1 sonrası).
- [ ] `F3` HPF, crossover ve limiter zinciri (G0 kapandıktan sonra).
- [x] `F4` Wi-Fi, mDNS ve BLE/SoftAP Unified Provisioning — geliştirme kartında uçtan uca. Portal kısmı hariç (yukarıya bakın).
- [x] `F5` buton durum makinesi ve RGB LED animatörü — 12 sn senaryosu ve LED'in ses zamanlamasına etkisi hariç.
- [ ] `F6` NVS ayrımı, güç telemetrisi ve güvenli kapanış (G4 sonrası).
- [ ] `F7` imzalı A/B OTA, release hattı ve recovery (G6).
- [ ] `F8` dört cihaz senkronu ve soak (G7, G8).

## P6b - Firmware güvenliği ve kurtarma

- [x] `factory_cal` ile `user_settings` NVS şemasını ve migration testlerini yaz.
- [ ] Cihaz başına benzersiz PoP/QR üretim, seri eşleme ve güvenli yedekleme prosedürünü tanımla.
- [x] ESP-IDF sürümünü kilitle (`v5.5.1`); `esp_ghota` spike'ı tamamlandı ve aday reddedildi (ADR-0008).
- [x] `otadata`, `ota_0`, `ota_1` ve kalibrasyon/NVS alanlarını içeren partition CSV ve size budget oluştur (iki kart için ayrı tablo, CI'da denetleniyor).
- [x] SemVer `v*.*.*` tag ile test/build/sign/checksum/GitHub Release üreten GitHub Actions hattını kur. **Hiç sürüm yayımlanmadı.**
- [x] Release manifest target/donanım/sürüm/hash doğrulamasını ve stable update state machine'ini geliştir (`hk_manifest`, `hk_ota`; üretici/doğrulayıcı CI'da çapraz denetimli).
- [x] Idle audio, batarya, NTC ve Wi-Fi koşullarına bağlı OTA erteleme kapılarını uygula (`hk_gate`, `hk_sched`). Batarya eşiği hâlâ G3/G4'ten gelecek.
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
- [ ] KiCad kurulu bir makinede `generate_harman_kardom.py --validate` ile ERC/PDF doğrulamasını tamamla.
