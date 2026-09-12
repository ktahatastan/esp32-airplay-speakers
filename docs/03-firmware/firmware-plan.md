---
status: active
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-08
tags: [firmware, plan, roadmap, esp32]
---

# Firmware planı ve aşamalandırma

Bu belge firmware'in **ne olduğunu** ve **hangi sırayla yapıldığını** birlikte tanımlar. Aşama sırası keyfi değildir: her aşama kendinden öncekinin kanıtına ve gerektiğinde bir donanım kapısına bağlıdır.

`F0` iskeleti `firmware/` altında kuruldu ve ESP-IDF `v5.5.1` ile derleniyor. 2026-09-05'te imaj geliştirme kartında ([[../06-testing/devkit-bring-up|kayıt]]), 2026-09-08'de **ürün kartında** çalıştı ([[../06-testing/product-board-bring-up|kayıt]]): oktal PSRAM açıldı, 16 MB bölüm tablosu yüklendi, kimlik MAC'ten türedi, kalibrasyon yokken ses izinli olmadı.

`F0`'ın kalan işi bu yüzden **ikiye ayrıldı** ve yarısı hâlâ açık: açılış raporu doğrulandı, ama GPIO tablosu `candidate` kaldı. Açılan bir kart pin tablosunu kanıtlamaz — firmware o pinleri sürmedi, ve pinlerin ucunda ne olduğu kartın özelliğidir, imajın değil. [[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]] `accepted` için satın alınan kartın **kendi şemasını** istiyor. Kurulum ve doğrulama komutları `firmware/README.md` dosyasındadır.

## Kilitli girdiler

| Girdi | Değer | Kaynak |
|---|---|---|
| Kart | ESP32-S3, 16 MB flash + 8 MB PSRAM | [[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]] |
| Ses topolojisi | Mono program, bi-amp: DAC sol kanalı woofer bandı, sağ kanalı tweeter bandı; dört özdeş XH-A232'ye hat seviyesinde paralel | [[../07-decisions/ADR-0002-biamp-signal-chain\|ADR-0002]] |
| Kabin | Tek kabin, pasif radyatörlü; 4 woofer + 4 tweeter, tek program, ağda tek cihaz | [[../07-decisions/ADR-0021-single-cabinet\|ADR-0021]] |
| Provisioning | SoftAP/captive portal ve BLE, **sırayla**; transport girişten türetilir | [[../07-decisions/ADR-0005-dual-provisioning\|ADR-0005]] |
| Dağıtım | SemVer tag -> GitHub Releases -> imzalı A/B OTA | [[../07-decisions/ADR-0008-github-releases-ota\|ADR-0008]] |
| Besleme | 24 V / 2,9 A DC adaptör, barrel jak; `CONFIG_HK_SUPPLY_MV` varsayılan 24000 (8-26 V), tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV` 12000 | [[../07-decisions/ADR-0020-dc-adapter-power\|ADR-0020]] |
| AirPlay yığını | `rbouteiller/airplay-esp32`, `38027441ff43`'e vendor edildi | [[../07-decisions/ADR-0007-airplay-stack\|ADR-0007]], [[../07-decisions/ADR-0013-airplay-integration-shape\|ADR-0013]] |

`F1` spike'ının araştırma yarısı tamamlandı, `ADR-0007` kabul edildi ve yığın tek kartta gerçekten çalıştı; ölçüm yarısından geriye akış sırasındaki kaynak ölçümü kaldı. Yığının lisansı **ticari olmayan** kullanımla sınırlıdır ve bu tüm projeyi bağlar.

## Modüller

| Modül | Sorumluluk |
|---|---|
| `audio` | AirPlay alıcı, saat/buffer yönetimi, I2S sürücü, DSP zinciri, limiter |
| `network` | Wi-Fi istemci, mDNS, SoftAP captive portal, BLE provisioning |
| `ui` | Buton durum makinesi, RGB LED animatörü |
| `storage` | `factory_cal` ve `user_settings` NVS ayrımı, şema migrasyonu |
| `update` | Sürüm kontrolü, manifest doğrulama, A/B OTA, sağlık kontrolü, rollback |
| `diagnostics` | Parolasız, kişisel veri içermeyen log ve sayaçlar |

## Görev ve çekirdek yerleşimi

Başlangıç hedefi; kesin yerleşim `F2` ölçümüyle kararlaştırılır ve burada güncellenir.

| Görev | Öncelik | Çekirdek hedefi | Kural |
|---|---|---|---|
| I2S besleme / DSP | En yüksek | Ağdan ayrı çekirdek | Bloklanmaz, heap ayırmaz, log basmaz |
| AirPlay alıcı / paket işleme | Yüksek | Ağ çekirdeği | Ses görevine yalnız kilitsiz kuyrukla dokunur |
| Wi-Fi / mDNS / portal | Orta | Ağ çekirdeği | |
| `ui` LED animatörü | En düşük | Serbest | Ses zamanlamasına etkisi G1'de ölçülür |

Ses yolunda dinamik bellek ayırma, dosya sistemi erişimi ve TLS işi yasaktır.

---

## Aşamalar

Her aşama: **önkoşul -> çıktı -> kabul ölçütü**. Kabul ölçütü ölçülebilir değilse aşama başlatılmaz.

### F0 — İskelet, araç zinciri ve depolama

- **Önkoşul:** yok. Donanım gerekmez.
- **Çıktı:** `firmware/` ESP-IDF projesi, sabitlenmiş IDF sürümü (`v5.5.1`), `sdkconfig.defaults`, partition CSV, `factory_cal` / `user_settings` NVS şeması ve migrasyon, host birim testi hedefi, `PROJECT_VER` üretimi, partition/boyut doğrulayıcısı ve PR CI iş akışı.
- **Kabul ölçütü:**
  - Temiz checkout'ta `idf.py -C firmware build` geçiyor ve tekrarlanabilir.
  - IDF sürümü tek yerde sabit; sürüm `firmware/README.md` ve CI'da aynı değerde yazılı.
  - 16 MB flash için `nvs`, `nvs_keys`, `factory_cal`, `otadata`, `ota_0`, `ota_1` bölümleri tanımlı; `tools/check_partitions.py` çakışma, hizalama, eşit slot boyutu, zorunlu bölümler ve serbest alan payını denetliyor.
  - GPIO ataması derleme zamanında zorlanıyor: aynı pini iki işlev paylaşamaz, strapping (`GPIO0/3/45/46`) ve native USB (`GPIO19/20`) pinleri kullanılamaz.
  - Host birim testleri ESP-IDF olmadan çalışıyor ve pin tablosunu devre planındaki tabloyla işlev işlev karşılaştırıyor.
  - PR CI: belge bütünlüğü + host testleri + partition kapısı + üretilen çizimlerin güncelliği + firmware build + boyut kapısı.
  - [x] Şema kararları saf mantık olarak yazıldı ve test edildi (`hk_schema`): eksik, eski, **yeni** (rollback) ve bozuk durumların her biri iki depo için ayrı ayrı çözülüyor.
  - [x] Bozuk veya eksik depoda cihaz açılıyor; hiçbir depo hatası ölümcül değil.
  - [x] Kullanıcı reseti `factory_cal`'a ulaşamıyor. Garanti yapısal: ayrı partition, salt-okunur açılış ve `tools/check_storage_isolation.py` ile CI'da denetleniyor (PRD-008).
  - [x] Kalibrasyon yoksa veya okunamıyorsa ses **izinli değil**; uydurma bir varsayılan profil yazılmıyor.
- **Gate:** yok. `G6`'nın ön koşuludur; depolama tarafı `PRD-008`'i kapatır.

### F1 — AirPlay yığını fizibilite spike'ı

- **Önkoşul:** F0. Bir adet ESP32-S3 kartı. Hoparlör gerekmez.
- **Çıktı:** aday yığınların kaynak kodu/lisans incelemesi, en küçük çalışan alıcı, bir Apple cihazının alıcıyı bulup bağlanıp akıtabildiğinin kanıtı, kaynak tüketimi ölçümü, `ADR-0007` doldurulmuş hâli.
- **Kabul ölçütü:**
  - [x] Desteklenen AirPlay sürümü birincil kaynakla belgelendi — kaynakta HomeKit SRP-6a, FairPlay `/fp-setup`, IEEE-1588 PTP ve AirPlay 2'ye özgü RTSP metotları.
  - [x] Flash/DIRAM bütçesine sığdığı **derlenerek** gösterildi: ESP32-S3 + ESP-IDF v5.5.1, 1.460.192 baytlık imaj (bir OTA slotunun %20,3'ü), 136.495 bayt statik DIRAM.
  - [x] Lisans hedef kullanımla uyumlu; ticari olmayan sınırı kabul edildi ve kaydedildi.
  - [x] `ADR-0007` `accepted` oldu.
  - [x] Yığın vendor edildi ve **depodan derlenen imaj** bir Apple cihazıyla gerçek bir oturum taşıdı: mDNS ve RTSP dışarıdan doğrulandı, `ptp_clock: LOCKED`, ses duyuldu (2026-09-05, tek kart).
  - [ ] Çalışma zamanı heap/PSRAM ve CPU yükü **akış sırasında** ölçülmedi. Ağa katıldıktan sonraki boş bellek ölçüldü (237.843 B dahili, 2.094.848 B PSRAM); alıcının akarken ne tükettiği ayrıştırılmadı.
- **Gate:** yok; `F2`'nin ön koşulu. AirPlay 2 eşleşmesi bir iOS güncellemesiyle kırılırsa yığının `CONFIG_AIRPLAY_FORCE_V1` yoluyla AirPlay 1'e geri çekilme seçeneği vardır.

> [!note] F1 kısmen tamamlandı
> Araştırma ve derleme yarısı bitti. Ölçüm yarısı 2026-09-05'te tek kartta ilerledi: alıcı çalıştı, bir Apple cihazı bağlandı, PTP kilitlendi. Geriye kalan, akış sırasındaki heap/PSRAM/CPU ölçümüdür. Yığın seçimi bu yüzden `accepted`.

> [!warning] Bu aşama projenin en büyük teknik riskidir
> Telefonun bulup bağlanıp akıtabildiği bir alıcı olmadan ürün yoktur. Bu nedenle F1, pahalı donanım işinden **önce** yapılır.

### F2 — Ses yolu bring-up

- **Önkoşul:** F1 kabul. Donanım tarafında `G1` (amfi + dummy-load) geçmiş olmalı.
- **Not (2026-09-05):** geliştirme kartında bir **tezgâh** çıkışı var — vendor edilen yığının S/PDIF çıkışı `GPIO6`'dan, üç pasif parçayla operatörün kendi DAC'ına. Bu F2 değildir ve hiçbir kapıya dokunmaz: o gün ürünün I2S/PCM5102A yolu susturuluydu ve dönüşümü başka bir cihaz yapıyordu.
- **Not (2026-09-08):** ürün kartında zincir ilk kez çaldı — AirPlay → I²S → PCM5102A → XH-A232 → ses, mono ve temiz ([[../06-testing/bench-measurement-order#KAPANDI — ses zinciri uçtan uca çalışıyor (2026-09-08)|tezgâh kaydı]]). Bu bir dinleme kaydıdır; aşağıdaki ölçütlerin osiloskop ve dummy-load yarısı açık.
- **Çıktı:** I2S sürücü, PCM5102A 3-wire yapılandırma, mono programın iki DSP yoluna ayrılması, test sinyali üreteci, boot/mute sıralaması.
- **Kabul ölçütü:**
  - I2S test noktalarında beklenen saatler osiloskopla doğrulandı.
  - DAC çıkış test noktalarında iki kanal bağımsız sürülebiliyor; kanal eşlemesi (sol=woofer, sağ=tweeter) kanıtlandı.
  - Dummy-load üzerinde 1 kHz `-40 dBFS` ve `-20 dBFS` temiz.
  - Açılış/kapanışta DAC ve amfi pop davranışı kaydedildi.
  - Ses görevi hiçbir koşulda underrun vermiyor (sayaç sıfır).
- **Gate:** `G1` girdi.

### F3 — DSP koruma zinciri

- **Önkoşul:** F2. Donanım tarafında `G0` (sürücü empedansı) **kapanmış** olmalı.
- **Çıktı:** woofer HPF, aktif crossover, kanal gain/delay, RMS ve tepe limiter, clipping davranışı, `factory_cal` profil formatı.
- **Durum (2026-09-08):** zincir çalışıyor, sayılar bekliyor. `hk_dsp` (mono toplam, kullanıcı EQ'su, subsonic yüksek-geçiren, LR4 ayrımı, dal başına kazanç ve limiter), `hk_biquad` (LR4, DF2T), `hk_limiter` (attack'sız tepe limiter) ve `hk_profile` (profilin kendisi, doğrulaması, zincire dönüşmesi) host'ta testli; tezgâh profili yer tutucu köşelerle (55 Hz subsonic, 2800 Hz crossover) tezgâh yapısına derleniyor ve her açılışta ölçülmediğini söylüyor; zincirin ürün kartında çaldığının tek kaydı `86f629c` commit mesajıdır, `docs/06-testing/` altında tezgâh kaydı yoktur. Hiçbirinde ölçülmüş sürücü değeri yok; `G0` kapandığında yapılacak iş bir struct doldurmaktır. Zincir bitmiş değildir: bilinen boşluklar sonraya bırakıldı. Ayrıntı: [[../04-acoustics/measurement-and-dsp-plan#Profil: biçim yazıldı, sayılar bekliyor|ölçüm ve DSP planı]].
- **Kabul ölçütü:**
  - Filtre katsayıları ölçülmüş sürücü empedansından türetildi; tahmin yok.
  - Tweeter yolu HPF'i ölçümle doğrulandı; `C_SAFE` değeri G2 raporundan geldi.
  - Tavan ölçüldüğü besleme gerilimiyle saklanır ve yapılandırılan beslemeye (`CONFIG_HK_SUPPLY_MV`, varsayılan 24 V) ölçeklenir. Firmware tarafı hazır: `hk_profile_ceiling_at()` bunu yapıyor ve iki gerilimde inşa edilen zincirde yalnız tavanlar değişiyor. Profil tezgâhta 12 V referansta (`HK_BENCH_REFERENCE_SUPPLY_MV`) ölçülür ve 24 V'a ölçeklenir. Limiter 24 V adaptör beslemesinde dummy-load üzerinde doğrulanacak; tavan, dört amfi birden sürülürken 2,9 A adaptör bütçesinden türetilir ve `VIN` çökmesiyle sınanır (`G1` satırı). Ölçüm hâlâ gerekli.
  - Kullanıcı reseti koruma profilini silmiyor (otomatik test).
  - DSP zinciri ses görevinde deterministik süre içinde bitiyor.
- **Gate:** `G2` zorunlu.

> [!danger] G0 kapanmadan F3'e başlanmaz
> Ölçülmemiş empedansla türetilen bir crossover veya limiter, tweeter'ı kalıcı olarak bozabilir.

### F4 — Ağ ve provisioning

- **Önkoşul:** F0. F2'den bağımsız çalışabilir.
- **Çıktı:** Wi-Fi istemci ve yeniden bağlanma, mDNS adı, SoftAP + captive portal, BLE Unified Provisioning (Security 2 / SRP6a, cihaz başına PoP), QR üretimi, provisioning zaman aşımı, BLE belleğinin serbest bırakılması.
- **Kabul ölçütü:**
  - [x] [[../controls-and-provisioning-plan#Merzarkabul Airplay Speakers ürün kimliği\|Kimlik tablosundaki]] tüm yüzey adları doğru üretiliyor (`hk_identity`, host testli).
  - [x] Provisioning politikası saf mantık olarak yazıldı ve test edildi (`hk_provision`): ilk açılışta zaman aşımı yok, butonla açılan pencere 10 dakikada kapanır, bağlantı denemesi boyunca radyolar açık kalır, başarıdan sonra ikisi de kapanır ve BLE serbest bırakılabilir.
  - [x] Wi-Fi istasyon, yeniden bağlanma, mDNS ve SoftAP provisioning sürücü katmanı yazıldı; ESP-IDF v5.5.1 ile derleniyor.
  - [x] BLE transport'u NimBLE ile etkinleştirildi. ADR-0005 seçenek C: transport girişten türetilir — kimlik bilgisi yoksa SoftAP, yapılandırılmış cihazda butonla BLE.
  - [x] Provisioning politikası artık gerçekten işletiliyor. Önceki hâlinde `hk_prov_handle` her yerde `now_ms = 0` ile çağrılıyor, `HK_PROV_EV_TICK` hiç gönderilmiyor ve `hk_prov_radios()` hiç okunmuyordu: on dakikalık sınırlı pencere hiçbir zaman dolamazdı. Ana döngü saniyede bir tick veriyor ve pencere kapandığında `hk_network_close_provisioning()` çağrılıyor.
  - [x] iOS'ta **uygulamalı BLE** akışı uçtan uca çalıştı: QR'lı kurulum, kimlik bilgisi teslimi, katılma ve `provisioning succeeded` (2026-09-05, geliştirme kartı).
  - [x] Uygulamasız yol **yazıldı** (ADR-0015, `hk_portal`): kurulum ağı WPA2, captive DNS her adı cihaza çözüyor, portal sayfası düz bir form ve aldığı bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye veriyor. Form ayrıştırıcısı saf C ve host'ta testli.
  - [ ] Uygulamasız yol **ölçülmedi**: hiçbir telefon bu sayfayı açmadı. PRD-004 bu ölçüm olmadan kapanmaz.
  - [ ] Android'de hiçbir akış denenmedi.
  - [x] Provisioning kapandığında BLE belleğinin geri verildiği ölçüldü: `BTDM memory released`, ardından `provisioning closed and its memory released`.
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
  - [x] LED durum önceliği yazıldı ve test edildi (`hk_led`): hata > OTA > buton geri bildirimi > etkinlik.
  - [x] Buton GPIO ve RGB PWM sürücüsü yazıldı; ayrı düşük öncelikli görevde çalışıyor, ses görevinden bağımsız.
  - [x] Kullanıcı resetinin `factory_cal`'a dokunmadığı **gerçek NVS ile gösterildi**: `firmware/test/nvs_host/`, ESP-IDF'in `linux` hedefinde, `partitions.csv`'den üretilmiş gerçek bölüm tablosu ve gerçek `hk_storage.c` ile. 113 kontrol; 21 ardışık reset ve bir tam `nvs_flash_erase()` sonrası kalibrasyon duruyor ve ses hâlâ izinli. Kart gerekmiyor.
  - [x] LED durum alanlarının sahipliği ayrıldı. Önceki `hk_ui_set_status()` tüm yapıyı değiştiriyordu ve her çağıran alanların yalnız bir kısmını dolduruyordu; OTA göstergesi sıradaki Wi-Fi olayında sessizce sönerdi.
  - [x] Kısa basış ve 5 sn basış donanımda doğrulandı: kısa basış provisioning açıyor ve **çökme yok** (2026-09-03 oturumunun yığın taşması burada yeniden üretilmedi), 5 sn kayıtlı ağı unutuyor ve açık pencereyi bozmuyor.
  - [ ] 12 sn fabrika sıfırlama donanımda **doğrulanmadı**: köprü teması kesilip debounce sayacı sıfırlandı, olay üretilmedi. Tekrar denenecek.
  - [ ] LED PWM'inin I2S zamanlamasına ve analog dip gürültüsüne etkisi ölçülmedi — ürün donanımı gerekir.
- **Gate:** `PRD-005`.

### F7 — OTA ve release hattı

- **Önkoşul:** F0. Tüm ayrıntı: [[ota-and-release-plan\|OTA ve sürüm yönetimi planı]].
- **Çıktı:** manifest ayrıştırıcı ve donanım eşleme, HTTPS indirme, güncelleme kapıları durum makinesi, ilk-boot sağlık kontrolü, rollback, `esp_ghota` spike sonucu, imzalama ve GitHub Actions release hattı, canary/stable kanal politikası, USB/UART recovery prosedürü.
- **Kabul ölçütü:** [[ota-and-release-plan#G6 kabul matrisi\|G6 kabul matrisinin]] on satırının tamamı test raporlu.
- **Gate:** `G6` zorunlu.
- **Durum (2026-08-31):** yazma yarısı bitti, ölçme yarısı donanım bekliyor.
  - [x] `esp_ghota` spike'ı yapıldı ve aday **reddedildi** (ADR-0008). Yerine `esp_https_ota` üstünde kendi istemcimiz.
  - [x] `hk_manifest` — manifest tek bayt indirilmeden yargılanıyor.
  - [x] `hk_gate` — ses çalarken, Wi-Fi yokken veya başka bir güncelleme sürerken güncelleme başlamıyor; durum verilmezse reddediyor.
  - [x] `hk_ota` — inen görüntünün tanımlayıcısı manifest ile karşılaştırılıyor. ESP-IDF `project_name`'i hiç karşılaştırmadığı için bu kontrol yoksa başka bir projenin S3 görüntüsü kurulurdu.
  - [x] `hk_ota_client` — HTTPS/JSON/`esp_https_ota` katmanı. Derleniyor; **çalıştırılmadı**.
  - [x] `sdkconfig.release` imzalama profili ve iki işli `release.yml` hattı. İmzalama adımı tek kullanımlık anahtarla uçtan uca denendi.
  - [x] `make_manifest.py` — manifest imzalı ikiliden üretiliyor, alan adları aygıt yazılımıyla CI'da çapraz denetleniyor.
  - [x] ISRG Root YR sertifikası pakete eklendi ve üretilen pakette doğrulandı.
  - [x] İlk-boot sağlık kontrolü politikası (`hk_health`) — planın dört ölçütü, enjekte edilen girdilerle host'ta test edildi. Çağrının kendisi ve alt sistem raporlamaları açık.
  - [x] `esp_ota_mark_app_valid_cancel_rollback()` çağrısı ve alt sistem raporlayıcıları (`hk_health_monitor`). `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` aynı değişiklikte açıldı; yayın hattı eşleşmeyi denetliyor. Monitor **yalnız imaj `PENDING_VERIFY`** iken iş yapar, yani USB'den yazılan bir yapı bu yolun dışındadır ve ilk kez `G6`'da devreye girer.
  - [x] Güncelleme döngüsü ve LED entegrasyonu bağlandı. Sürüm kaynağı seçilmedi: private depo tokensiz erişilemiyor (ADR-0008 §7, risk kaydı).
  - [x] USB/UART kurtarma prosedürü ve betiği ([[usb-recovery]]). Donanımda çalıştırılmadı.
  - [ ] `G6` matrisinin on satırı — **donanım gerekir**, hiçbiri çalıştırılmadı.

### F8 — Dayanıklılık

- **Önkoşul:** F1'den F7'ye kadar tümü. Donanım tarafında `G0`-`G2` geçmiş kabin.
- **Çıktı:** 24 saat soak, Wi-Fi kopması ve yeniden bağlanma, besleme kaybında pop'suz kapanış.
- **Kabul ölçütü:** `G8` soak testi geçti.
- **Gate:** `G8`.

---

## Aşama-kapı matrisi

| Aşama | Donanım önkoşulu | Kapatmaya katkı |
|---|---|---|
| F0 | — | G6 hazırlığı, PRD-008 |
| F1 | — | ADR-0007 |
| F2 | G1 | — |
| F3 | G0, G2 | G2 |
| F4 | — | PRD-004 |
| F5 | — | PRD-005 |
| F7 | — | G6 |
| F8 | G0-G2 | G8 |

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
    hk_schema/         hangi durumda ne yapılacağı (saf, testli)            [F0 · var]
    hk_storage/        iki NVS deposu ve aralarındaki duvar                 [F0 · var]
    hk_manifest/       yayımlanan sürüm bu cihaza ait mi (saf, testli)      [F7 · var]
    hk_gate/           güncelleme şimdi başlayabilir mi (saf, testli)       [F7 · var]
    hk_ota/            inen görüntü manifest ile uyuşuyor mu + istemci      [F7 · var]
    hk_health/         ilk açılış imajı onaylanmalı mı (saf, testli)        [F7 · var]
    hk_audio/          susturma sırası, limiter, biquad/LR4 (saf, testli)   [F2-F3 · var]
    hk_sched/          güncelleme zamanlaması ve backoff (saf, testli)      [F7 · var]
    hk_settings/       kullanıcı ayarı tablosu, varsayılan ve aralık        [F0 · var]
  test/                host tarafı birim testleri                           [F0 · var]
  tools/               partition ve boyut doğrulaması                       [F0 · var]
```

Her bileşen kendi başlık dosyasında açık bir arayüz sunar; modüller birbirinin iç durumuna erişmez.

Politika modülleri açılışta gerçekten **çalıştırılıyor**: `hk_main.c` içindeki `report_policies()` her birini cihazın o an bildiği duruma karşı işletip sonucu basıyor. Bu süs değil — arkasında sürücüsü olmayan bir politika, çalışmayanla ayırt edilemez. Bugün açılış günlüğünün söylemesi gereken şey, sesin izinli **olmadığı**, imajın onaylanamadığı ve hiçbir ayarın depodan gelmediği; çünkü bunların hiçbiri henüz doğru değil.

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
