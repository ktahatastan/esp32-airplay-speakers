---
status: active
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [firmware, plan, roadmap, esp32]
---

# Firmware planı ve aşamalandırma

Bu belge firmware'in **ne olduğunu** ve **hangi sırayla yapıldığını** birlikte tanımlar. Aşama sırası keyfi değildir: her aşama kendinden öncekinin kanıtına ve gerektiğinde bir donanım kapısına bağlıdır.

`F0` iskeleti `firmware/` altında kuruldu ve ESP-IDF `v5.5.1` ile derleniyor. `F0`'ın kalan işi, satın alınan kartta açılış raporunu doğrulayıp GPIO tablosunu `accepted` yapmaktır. Kurulum ve doğrulama komutları `firmware/README.md` dosyasındadır.

## Kilitli girdiler

| Girdi | Değer | Kaynak |
|---|---|---|
| Kart | ESP32-S3, 16 MB flash + 8 MB PSRAM | [[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]] |
| Ses topolojisi | Mono program, bi-amp: sol yol woofer, sağ yol tweeter | [[../07-decisions/ADR-0002-biamp-signal-chain\|ADR-0002]] |
| Provisioning | SoftAP/captive portal ve BLE, **sırayla**; transport girişten türetilir | [[../07-decisions/ADR-0005-dual-provisioning\|ADR-0005]] |
| Dağıtım | SemVer tag -> GitHub Releases -> imzalı A/B OTA | [[../07-decisions/ADR-0008-github-releases-ota\|ADR-0008]] |
| Şarj davranışı | V1'de şarj sırasında amfi kapalı | [[../07-decisions/ADR-0004-v1-charge-policy\|ADR-0004]] |
| AirPlay yığını | `rbouteiller/airplay-esp32`, sabit commit'e vendor | [[../07-decisions/ADR-0007-airplay-stack\|ADR-0007]] |

`F1` spike'ının araştırma yarısı tamamlandı ve `ADR-0007` kabul edildi; ölçüm yarısı donanım bekliyor. Yığının lisansı **ticari olmayan** kullanımla sınırlıdır ve bu tüm projeyi bağlar.

## Modüller

| Modül | Sorumluluk |
|---|---|
| `audio` | AirPlay alıcı, saat/buffer yönetimi, I2S sürücü, DSP zinciri, limiter |
| `network` | Wi-Fi istemci, mDNS, SoftAP captive portal, BLE provisioning |
| `ui` | Buton durum makinesi, RGB LED animatörü |
| `storage` | `factory_cal` ve `user_settings` NVS ayrımı, şema migrasyonu |
| `power` | Paket gerilimi, NTC, düşük gerilim politikası, güvenli kapanış |
| `update` | Sürüm kontrolü, manifest doğrulama, A/B OTA, sağlık kontrolü, rollback |
| `diagnostics` | Parolasız, kişisel veri içermeyen log ve sayaçlar |

## Görev ve çekirdek yerleşimi

Başlangıç hedefi; kesin yerleşim `F2` ölçümüyle kararlaştırılır ve burada güncellenir.

| Görev | Öncelik | Çekirdek hedefi | Kural |
|---|---|---|---|
| I2S besleme / DSP | En yüksek | Ağdan ayrı çekirdek | Bloklanmaz, heap ayırmaz, log basmaz |
| AirPlay alıcı / paket işleme | Yüksek | Ağ çekirdeği | Ses görevine yalnız kilitsiz kuyrukla dokunur |
| Wi-Fi / mDNS / portal | Orta | Ağ çekirdeği | |
| `power` telemetrisi | Düşük | Serbest | Periyodik, yavaş |
| `ui` LED animatörü | En düşük | Serbest | Ses zamanlamasına etkisi G3'te ölçülür |

Ses yolunda dinamik bellek ayırma, dosya sistemi erişimi ve TLS işi yasaktır.

---

## Aşamalar

Her aşama: **önkoşul -> çıktı -> kabul ölçütü**. Kabul ölçütü ölçülebilir değilse aşama başlatılmaz.

### F0 — İskelet ve araç zinciri

- **Önkoşul:** yok. Donanım gerekmez.
- **Çıktı:** `firmware/` ESP-IDF projesi, sabitlenmiş IDF sürümü (`v5.5.1`), `sdkconfig.defaults`, partition CSV, host birim testi hedefi, `PROJECT_VER` üretimi, partition/boyut doğrulayıcısı ve PR CI iş akışı.
- **Kabul ölçütü:**
  - Temiz checkout'ta `idf.py -C firmware build` geçiyor ve tekrarlanabilir.
  - IDF sürümü tek yerde sabit; sürüm `firmware/README.md` ve CI'da aynı değerde yazılı.
  - 16 MB flash için `nvs`, `nvs_keys`, `factory_cal`, `otadata`, `ota_0`, `ota_1` bölümleri tanımlı; `tools/check_partitions.py` çakışma, hizalama, eşit slot boyutu, zorunlu bölümler ve serbest alan payını denetliyor.
  - GPIO ataması derleme zamanında zorlanıyor: aynı pini iki işlev paylaşamaz, strapping (`GPIO0/3/45/46`) ve native USB (`GPIO19/20`) pinleri kullanılamaz.
  - Host birim testleri ESP-IDF olmadan çalışıyor ve pin tablosunu devre planındaki tabloyla işlev işlev karşılaştırıyor.
  - PR CI: belge bütünlüğü + host testleri + partition kapısı + üretilen çizimlerin güncelliği + firmware build + boyut kapısı.
- **Gate:** yok. `G6`'nın ön koşuludur.

### F1 — AirPlay yığını fizibilite spike'ı

- **Önkoşul:** F0. Bir adet ESP32-S3 kartı. Hoparlör gerekmez.
- **Çıktı:** aday yığınların kaynak kodu/lisans incelemesi, en küçük çalışan alıcı, dört sahte cihazın Apple istemcisinde birlikte görünüp görünmediğinin kanıtı, kaynak tüketimi ölçümü, `ADR-0007` doldurulmuş hâli.
- **Kabul ölçütü:**
  - [x] Desteklenen AirPlay sürümü birincil kaynakla belgelendi — kaynakta HomeKit SRP-6a, FairPlay `/fp-setup`, IEEE-1588 PTP ve AirPlay 2'ye özgü RTSP metotları.
  - [x] Flash/DIRAM bütçesine sığdığı **derlenerek** gösterildi: ESP32-S3 + ESP-IDF v5.5.1, 1.460.192 baytlık imaj (bir OTA slotunun %20,3'ü), 136.495 bayt statik DIRAM.
  - [x] Lisans hedef kullanımla uyumlu; ticari olmayan sınırı kabul edildi ve kaydedildi.
  - [x] `ADR-0007` `accepted` oldu ve `G7` sayısal eşikleri kilitlendi.
  - [ ] Dört hedefin Apple cihazında **birlikte seçilebildiği** gösterilmedi — donanım gerekir.
  - [ ] Çalışma zamanı heap/PSRAM ve CPU yükü ölçülmedi — donanım gerekir.
- **Gate:** `G7`'nin ön koşulu. **`G7` başarısız olursa** yığının `CONFIG_AIRPLAY_FORCE_V1` yoluyla AirPlay 1'e geri çekilme seçeneği vardır; senkron çoklu-oda düşer ve PRD-002 yeniden müzakere edilir.

> [!note] F1 kısmen tamamlandı
> Araştırma ve derleme yarısı bitti; ölçüm yarısı donanım bekliyor. Yığın seçimi bu yüzden `accepted`, senkron iddiası değil.

> [!warning] Bu aşama projenin en büyük teknik riskidir
> Multiroom senkron kanıtlanamazsa dört senkron hoparlör hedefi düşer. Bu nedenle F1, pahalı donanım işinden **önce** yapılır.

### F2 — Ses yolu bring-up

- **Önkoşul:** F1 kabul. Donanım tarafında `G1` (amfi + dummy-load) geçmiş olmalı.
- **Çıktı:** I2S sürücü, PCM5102A 3-wire yapılandırma, mono programın iki DSP yoluna ayrılması, test sinyali üreteci, boot/mute sıralaması.
- **Kabul ölçütü:**
  - TP11/TP12/TP13'te beklenen I2S saatleri osiloskopla doğrulandı.
  - TP14/TP15'te iki kanal bağımsız sürülebiliyor; kanal eşlemesi (sol=woofer, sağ=tweeter) kanıtlandı.
  - Dummy-load üzerinde 1 kHz `-40 dBFS` ve `-20 dBFS` temiz.
  - Açılış/kapanışta DAC ve amfi pop davranışı kaydedildi.
  - Ses görevi hiçbir koşulda underrun vermiyor (sayaç sıfır).
- **Gate:** `G1` girdi, `G3` katkı.

### F3 — DSP koruma zinciri

- **Önkoşul:** F2. Donanım tarafında `G0` (sürücü empedansı) **kapanmış** olmalı.
- **Çıktı:** woofer HPF, aktif crossover, kanal gain/delay, RMS ve tepe limiter, clipping davranışı, `factory_cal` profil formatı.
- **Kabul ölçütü:**
  - Filtre katsayıları ölçülmüş sürücü empedansından türetildi; tahmin yok.
  - Tweeter yolu HPF'i ölçümle doğrulandı; `C_SAFE` değeri G2 raporundan geldi.
  - Limiter tam dolu (16,8 V) ve düşük (12,0 V) bataryada ayrı ayrı doğrulandı.
  - Kullanıcı reseti koruma profilini silmiyor (otomatik test).
  - DSP zinciri ses görevinde deterministik süre içinde bitiyor.
- **Gate:** `G2` zorunlu.

> [!danger] G0 kapanmadan F3'e başlanmaz
> Ölçülmemiş empedansla türetilen bir crossover veya limiter, tweeter'ı kalıcı olarak bozabilir.

### F4 — Ağ ve provisioning

- **Önkoşul:** F0. F2'den bağımsız çalışabilir.
- **Çıktı:** Wi-Fi istemci ve yeniden bağlanma, mDNS adı, SoftAP + captive portal, BLE Unified Provisioning (Security 2 / SRP6a, cihaz başına PoP), QR üretimi, provisioning zaman aşımı, BLE belleğinin serbest bırakılması.
- **Kabul ölçütü:**
  - [x] [[../controls-and-provisioning-plan#Harman Kardom ürün kimliği\|Kimlik tablosundaki]] tüm yüzey adları doğru üretiliyor (`hk_identity`, host testli).
  - [x] Provisioning politikası saf mantık olarak yazıldı ve test edildi (`hk_provision`): ilk açılışta zaman aşımı yok, butonla açılan pencere 10 dakikada kapanır, bağlantı denemesi boyunca radyolar açık kalır, başarıdan sonra ikisi de kapanır ve BLE serbest bırakılabilir.
  - [x] Wi-Fi istasyon, yeniden bağlanma, mDNS ve SoftAP provisioning sürücü katmanı yazıldı; ESP-IDF v5.5.1 ile derleniyor.
  - [x] BLE transport'u NimBLE ile etkinleştirildi. ADR-0005 seçenek C: transport girişten türetilir — kimlik bilgisi yoksa SoftAP, yapılandırılmış cihazda butonla BLE.
  - [ ] iOS ve Android'de hem uygulamalı BLE hem uygulamasız captive portal akışı — donanım gerekir.
  - [ ] Provisioning sonrası BLE heap'inin geri kazanıldığı ölçülmedi — donanım gerekir.
  - [x] Wi-Fi parolası ve PoP'un loglarda görünmediği otomatik taramayla denetleniyor (`tools/check_no_credential_logs.py`, CI'da).
  - [x] Cihaz başına salt/verifier üreten üretim aracı yazıldı (`tools/provision_credentials.py`), ESP-IDF'in kendi SRP6a uygulamasını kullanıyor. Firmware kimlik bilgisi yoksa provisioning'i **açmayı reddediyor**, zayıf bir güvenlik moduna düşmüyor.
  - [x] Cihaz başına QR yükü üretiliyor; biçim ESP-IDF'in `wifi_prov_print_qr()` çıktısıyla aynı.
- **Gate:** `PRD-004`. `G6` girdi.

### F5 — Kullanıcı arayüzü

- **Önkoşul:** F4 (provisioning tetikleyicisi için).
- **Çıktı:** buton durum makinesi (kısa / 5 sn / 12 sn), 50 ms debounce, açılışta kurtarma modu, RGB LED animatörü ve durum tablosu.
- **Kabul ölçütü:**
  - [x] Üç eşik ayrı ayrı doğrulandı; 12 sn işlemi **yalnız buton bırakılınca** onaylanıyor (`hk_button`, host testli).
  - [x] Yanlışlıkla kısa dokunma kayıtlı Wi-Fi'yi silmiyor. Kısa basış ile ağ sıfırlama arasındaki aralık bilerek ölüdür: orada bırakmak hiçbir şey yapmaz.
  - [x] LED durum önceliği yazıldı ve test edildi (`hk_led`): hata > OTA > buton geri bildirimi > düşük batarya > etkinlik.
  - [x] Buton GPIO ve RGB PWM sürücüsü yazıldı; ayrı düşük öncelikli görevde çalışıyor, ses görevinden bağımsız.
  - [ ] Kullanıcı resetinin `factory_cal`'a dokunmadığı NVS testiyle gösterilmedi (`F6` deposunu bekliyor).
  - [ ] LED PWM'inin I2S zamanlamasına ve analog dip gürültüsüne etkisi ölçülmedi — donanım gerekir.
- **Gate:** `PRD-005`, `G3` katkı.

### F6 — Depolama ve güç telemetrisi

- **Önkoşul:** F4. Donanım tarafında `G4` (batarya/BMS/şarj) geçmiş olmalı.
- **Çıktı:** `factory_cal` / `user_settings` NVS şeması ve migrasyon, paket gerilimi ve NTC okuma, düşük gerilim politikası, güvenli kapanış sıralaması, şarj durumunda amfi kilidi.
- **Kabul ölçütü:**
  - [x] Şema kararları saf mantık olarak yazıldı ve test edildi (`hk_schema`): eksik, eski, **yeni** (rollback) ve bozuk durumların her biri iki depo için ayrı ayrı çözülüyor.
  - [x] Bozuk veya eksik depoda cihaz açılıyor; hiçbir depo hatası ölümcül değil.
  - [x] Kullanıcı reseti `factory_cal`'a ulaşamıyor. Garanti yapısal: ayrı partition, salt-okunur açılış ve `tools/check_storage_isolation.py` ile CI'da denetleniyor (PRD-008).
  - [x] Kalibrasyon yoksa veya okunamıyorsa ses **izinli değil**; uydurma bir varsayılan profil yazılmıyor.
  - [ ] `OTA_MIN_PACK_MV` eşiği G3/G4 ölçümünden türetilecek — donanım gerekir.
  - [ ] Paket gerilimi ve NTC okuma, düşük gerilimde kontrollü kapanış — donanım gerekir.
  - [ ] Şarj algılandığında amfi kilidi (ADR-0004) — amfi kontrolü `F2`, algılama donanım gerektirir.
- **Gate:** `G4` girdi, `G8` katkı.

### F7 — OTA ve release hattı

- **Önkoşul:** F0, F6. Tüm ayrıntı: [[ota-and-release-plan\|OTA ve sürüm yönetimi planı]].
- **Çıktı:** manifest ayrıştırıcı ve donanım eşleme, HTTPS indirme, güncelleme kapıları durum makinesi, ilk-boot sağlık kontrolü, rollback, `esp_ghota` spike sonucu, imzalama ve GitHub Actions release hattı, canary/stable kanal politikası, USB/UART recovery prosedürü.
- **Kabul ölçütü:** [[ota-and-release-plan#G6 kabul matrisi\|G6 kabul matrisinin]] on iki satırının tamamı test raporlu.
- **Gate:** `G6` zorunlu.
- **Durum (2026-08-31):** yazma yarısı bitti, ölçme yarısı donanım bekliyor.
  - [x] `esp_ghota` spike'ı yapıldı ve aday **reddedildi** (ADR-0008). Yerine `esp_https_ota` üstünde kendi istemcimiz.
  - [x] `hk_manifest` — manifest tek bayt indirilmeden yargılanıyor.
  - [x] `hk_gate` — kalibrasyon yoksa güncelleme başlamıyor.
  - [x] `hk_ota` — inen görüntünün tanımlayıcısı manifest ile karşılaştırılıyor. ESP-IDF `project_name`'i hiç karşılaştırmadığı için bu kontrol yoksa başka bir projenin S3 görüntüsü kurulurdu.
  - [x] `hk_ota_client` — HTTPS/JSON/`esp_https_ota` katmanı. Derleniyor; **çalıştırılmadı**.
  - [x] `sdkconfig.release` imzalama profili ve iki işli `release.yml` hattı. İmzalama adımı tek kullanımlık anahtarla uçtan uca denendi.
  - [x] `make_manifest.py` — manifest imzalı ikiliden üretiliyor, alan adları aygıt yazılımıyla CI'da çapraz denetleniyor.
  - [x] ISRG Root YR sertifikası pakete eklendi ve üretilen pakette doğrulandı.
  - [ ] İlk-boot sağlık kontrolü ve `esp_ota_mark_app_valid_cancel_rollback()`.
  - [ ] Güncelleme zamanlayıcısı ve LED entegrasyonu.
  - [ ] USB/UART recovery prosedürü.
  - [ ] `G6` matrisinin on iki satırı — **donanım gerekir**, hiçbiri çalıştırılmadı.

### F8 — Çoklu cihaz ve dayanıklılık

- **Önkoşul:** F1'den F7'ye kadar tümü. Donanım tarafında `G5` geçmiş dört ünite.
- **Çıktı:** dört cihazda senkron ölçümü, drift/jitter kaydı, yeniden bağlanma ve tek cihaz kapanması senaryoları, 24 saat soak, düşük batarya kapanışı, cihazlar arası kalibrasyon toleransı.
- **Kabul ölçütü:** `G7` sayısal eşikleri (ADR-0007 ile kesinleşecek) ve `G8` soak testi geçti.
- **Gate:** `G7`, `G8`.

---

## Aşama-kapı matrisi

| Aşama | Donanım önkoşulu | Kapatmaya katkı |
|---|---|---|
| F0 | — | G6 hazırlığı |
| F1 | — | G7 hazırlığı, ADR-0007 |
| F2 | G1 | G3 |
| F3 | G0, G2 | G2 |
| F4 | — | PRD-004 |
| F5 | — | PRD-005, G3 |
| F6 | G4 | G8 |
| F7 | — | G6 |
| F8 | G5 (dört ünite) | G7, G8 |

Paralel çalıştırılabilir: `F4` ve `F5` ile `F2`/`F3`. Aynı anda tek yazma sahibi kuralı korunur; `audio` ve `network` modülleri farklı çalışanlara verilebilir.

## Önerilen depo yerleşimi

```text
firmware/
  CMakeLists.txt       proje tanımı; PROJECT_VER version.txt'ten gelir      [F0 · var]
  sdkconfig.defaults   kart, partition, PSRAM ve rollback ayarları          [F0 · var]
  partitions.csv       16 MB yerleşimi: çift OTA slotu + ayrı kalibrasyon   [F0 · var]
  version.txt          katı SemVer                                          [F0 · var]
  main/                app_main; F0'da yalnız açılış raporu                  [F0 · var]
  components/
    hk_pins/           GPIO ataması; kısıtları derleyici zorlar             [F0 · var]
    hk_identity/       MAC'ten türetilen tüm yüzey adları                   [F0 · var]
    hk_version/        SemVer ayrıştırma ve OTA güncelleme kararı           [F0 · var]
    hk_audio/          I2S, DSP, limiter                                    [F2-F3]
    hk_airplay/        rbouteiller/airplay-esp32 sarmalayıcısı              [F1 · vendor]
    hk_provision/      provisioning politikası (saf, testli)               [F4 · var]
    hk_network/        Wi-Fi, mDNS, provisioning transport                  [F4 · var]
    hk_button/         buton durum makinesi (saf, testli)                  [F5 · var]
    hk_led/            LED durum önceliği ve deseni (saf, testli)          [F5 · var]
    hk_ui/             buton GPIO ve RGB PWM sürücüsü                       [F5 · var]
    hk_schema/         hangi durumda ne yapılacağı (saf, testli)            [F6 · var]
    hk_storage/        iki NVS deposu ve aralarındaki duvar                 [F6 · var]
    hk_power/          telemetri, kapanış politikası                        [F6]
    hk_manifest/       yayımlanan sürüm bu cihaza ait mi (saf, testli)      [F7 · var]
    hk_gate/           güncelleme şimdi başlayabilir mi (saf, testli)       [F7 · var]
    hk_ota/            inen görüntü manifest ile uyuşuyor mu + istemci      [F7 · var]
  test/                host tarafı birim testleri                           [F0 · var]
  tools/               partition ve boyut doğrulaması                       [F0 · var]
```

Her bileşen kendi başlık dosyasında açık bir arayüz sunar; modüller birbirinin iç durumuna erişmez.

ESP-IDF bağımlılığı olmayan bileşenler bilerek saf C yazılır. Test edilebilirliğin kaynağı budur: mantık, hiçbir sürücüye enerji vermek güvenli olmadan yıllar önce bir dizüstünde doğrulanabilir.

Kurulum, derleme ve doğrulama komutları için `firmware/README.md`.

## Tamamlanma tanımı

Bir firmware görevi ancak şunlar varsa biter: kod, host testi veya ölçüm kanıtı, güncellenmiş belge, gerekliyse ADR, `python3 scripts/check_docs.py` çıktısı ve açık risk listesi. Fiziksel ölçüm gerektiren kabul, operatör kaydı olmadan `PASS` yapılamaz.

## İlgili belgeler

- [[ota-and-release-plan\|OTA ve sürüm yönetimi]]
- [[security-and-recovery\|Güvenlik ve recovery]]
- [[../controls-and-provisioning-plan\|Kontroller ve provisioning]]
- [[../01-architecture/audio-network-feasibility\|AirPlay fizibilitesi]]
- [[../02-hardware/board-and-pin-selection\|Kart ve pin seçimi]]
- [[../06-testing/test-strategy\|Test kapıları]]
