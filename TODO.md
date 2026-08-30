# Harman Kardom iş listesi

## P0 - Tasarımı kilitleyen ölçümler

- [ ] Dört woofer'ın DC dirençlerini ayrı ayrı ölç.
- [ ] Dört tweeter'ın DC dirençlerini ayrı ayrı ölç.
- [ ] Sürücü etiketlerini ve bağlantı uçlarını fotoğraflandır.
- [ ] Hedefi kesinleştir: dört bağımsız mono kutu mu, iki stereo çift mi?
- [ ] İstenen normal ses seviyesinde çalışma süresi hedefini belirle.
- [ ] Aday AirPlay yığınının AirPlay 1/2 ve multiroom yeteneklerini kaynak kodu/lisansla doğrula.
- [ ] Dört hedefin birlikte seçilebildiği en küçük ağ ses prototipini kur.
- [ ] İki saatlik drift/jitter ölçüm düzenini ve G7 sayısal kabul eşiklerini ADR-0007'de kilitle.

## P1 - Tek hoparlör güç prototipi

- [ ] 4S1P test paketi için dört eşlenmiş hücre seç.
- [ ] Gerçek balanslı, en az 10 A sürekli 4S BMS seçimini doğrula.
- [ ] Paketi sigorta, NTC ve uygun izolasyonla punta kaynaklı hazırlat.
- [ ] 16,8 V CC/CV şarj cihazını boşta ve yükte doğrula.
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

- [ ] Proje, AirPlay, BLE, SoftAP, mDNS, captive portal ve QR yüzeylerinde Harman Kardom adlandırmasını uygula.
- [ ] Kesin ESP32-S3 kartını seç ve I2S/strapping pinleriyle çakışmayan buton + RGB GPIO'larını ata.
- [ ] Tek buton için kısa basış, 5 sn ağ sıfırlama ve 12 sn kullanıcı fabrika sıfırlama durum makinesini geliştir.
- [ ] Fabrika sürücü koruma/limiter kalibrasyonunu kullanıcı resetinden ayrı NVS alanında tut.
- [ ] RGB LED durum sürücüsünü audio task'tan bağımsız düşük öncelikli görev olarak geliştir.
- [ ] İlk açılış ve reset sonrası mevcut SoftAP captive portal akışını doğrula.
- [ ] ESP-IDF Unified Provisioning BLE transport ve Security 2 / benzersiz PoP ekle.
- [ ] Cihaz başına provisioning ve Wi-Fi QR kodu üret.
- [ ] Provisioning tamamlanınca BLE belleğinin serbest bırakıldığını doğrula.
- [ ] iOS ve Android Espressif Provisioning uygulamalarıyla BLE kurulum testi yap.
- [ ] Uygulamasız iOS/Android SoftAP + captive portal davranışını test et.
- [ ] Özel mobil uygulama kararı verilirse iOS AccessorySetupKit ve Android Companion Device Manager prototipi hazırla.
- [ ] Provisioning timeout, tekrar deneme, parola gizliliği ve NVS encryption testlerini yap.
- [ ] Ayrı 24 V DC / 5 A fiziksel güç anahtarını BMS sonrası yük hattına ekle.
- [ ] Hoparlör kapalıyken şarj; açılırken/kapanırken pop ve ESP32 reset testlerini yap.

## P6 - Firmware güvenliği ve kurtarma

- [ ] `factory_calibration` ile `user_settings` NVS şemasını ve migration testlerini yaz.
- [ ] Cihaz başına benzersiz PoP/QR üretim, seri eşleme ve güvenli yedekleme prosedürünü tanımla.
- [ ] ESP-IDF sürümünü kilitle; `esp_ghota` uyumluluk/kaynak kullanımı spike'ını tamamla.
- [ ] `otadata`, `ota_0`, `ota_1` ve kalibrasyon/NVS alanlarını içeren partition CSV ve size budget oluştur.
- [ ] SemVer `v*.*.*` tag ile test/build/sign/checksum/GitHub Release üreten GitHub Actions hattını kur.
- [ ] Release manifest target/donanım/sürüm/hash doğrulamasını ve stable update state machine'ini geliştir.
- [ ] Idle audio, batarya, NTC ve Wi-Fi koşullarına bağlı OTA erteleme kapılarını uygula.
- [ ] İlk-boot health check, A/B rollback, canary/stable dağıtım ve güç kesintisi G6 testlerini tamamla.
- [ ] Wi-Fi parolası/PoP/anahtarların loglarda görünmediğini otomatik taramayla doğrula.
- [ ] USB/UART recovery ve boot prosedürünü saha servis dokümanına ekle.

## P7 - Agentic süreç ve proje hafızası

- [ ] Her geliştirme görevine sahibi, dosya kapsamı, başarı ölçütü ve gate ata.
- [ ] Mimari/güvenlik davranışı değiştiğinde ADR aç veya mevcut ADR'yi supersede et.
- [ ] Material değişikliklerde geliştirme günlüğü ve test kanıtını güncelle.
- [ ] Agent sahipliği değişiminde handoff kaydı oluştur.
- [ ] Birleşme öncesi Obsidian linkleri, JSON/TOML/YAML, skill ve `git diff --check` doğrulamalarını çalıştır.
