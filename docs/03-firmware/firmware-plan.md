---
status: active
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-13
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
| Provisioning | SoftAP + captive portal ve BLE **aynı anda**; yöneticiyi BLE sürer, erişim noktasını `hk_network` kaldırır. PIN yok: BLE'de protocomm Security 1 ve sahiplik kanıtı yok, kurulum ağı açık, kurulum adları `PROV_Merzarkabul-XXXX`; daha güçlü bir mod yalnız aşan ADR ile döner | [[../07-decisions/ADR-0016-simultaneous-dual-transport\|ADR-0016]], [[../07-decisions/ADR-0023-pinless-provisioning\|ADR-0023]] |
| Dağıtım | SemVer tag -> GitHub Releases -> imzalı A/B OTA | [[../07-decisions/ADR-0008-github-releases-ota\|ADR-0008]] |
| Besleme | 24 V / 2,9 A DC adaptör, barrel jak; `CONFIG_HK_SUPPLY_MV` varsayılan 24000 (8-26 V), tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV` 12000 | [[../07-decisions/ADR-0020-dc-adapter-power\|ADR-0020]] |
| AirPlay yığını | `rbouteiller/airplay-esp32`, `38027441ff43`'e vendor edildi | [[../07-decisions/ADR-0007-airplay-stack\|ADR-0007]], [[../07-decisions/ADR-0013-airplay-integration-shape\|ADR-0013]] |
| Çıkış arka ucu | DSP zinciri (`hk_airplay/output/hk_airplay_output_i2s.c`) ürün ve release imajında; vendor edilen düz geçiş yalnız geliştirme kartında ve tezgâh istisnasında, release onu reddeder; S/PDIF yalnız geliştirme kartında | [[../07-decisions/ADR-0022-dsp-product-output-backend\|ADR-0022]] |

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
- **Çıktı:** I2S sürücü, PCM5102A 3-wire yapılandırma, mono programın iki DSP yoluna ayrılması (yazıldı: `hk_dsp`, host'ta testli — ölçüm yarısı aşağıda açık), test sinyali üreteci, boot/mute sıralaması.
- **Kabul ölçütü:**
  - I2S test noktalarında beklenen saatler osiloskopla doğrulandı.
  - DAC çıkış test noktalarında iki kanal bağımsız sürülebiliyor; kanal eşlemesi (sol=woofer, sağ=tweeter) kanıtlandı.
  - Dummy-load üzerinde 1 kHz `-40 dBFS` ve `-20 dBFS` temiz.
  - Açılış/kapanışta DAC ve amfi pop davranışı kaydedildi.
  - Ses görevi hiçbir koşulda underrun vermiyor (sayaç sıfır).
- **Gate:** `G1` girdi.

### F3 — DSP koruma zinciri

- **Önkoşul:** F2. Kodun kendisi için donanım önkoşulu yok: zincir üründe derlidir ve profil yokken susar. **Sayılar** için önkoşul `G0`'dır — hiçbir köşe ya da tavan `G0` (sürücü empedansı) kapanmadan `factory_cal`'a **kalibrasyon olarak** yazılmaz; o güne kadar dinleme yolu tezgâh yapısı ya da onun `factory_cal`'a yazılmış, kaynağı `provisional` olan kopyasıdır (aşağıdaki gece durumu), ve sürücüde dinleme kademeli ve düşük seviyededir (`AGENTS.md`).
- **Çıktı:** woofer HPF, aktif crossover, kanal gain/delay/polarite, besleme bütçesi katı ve tepe limiter, clipping davranışı, `factory_cal` profil formatı. Sürücü başına termal (RMS) limiter bu aşamanın açık maddesidir: eşiği ve zaman sabiti `G2`'nin sürücü ölçümünü bekler.
- **Durum (2026-09-08):** zincir çalışıyor, sayılar bekliyor. `hk_dsp` (mono toplam, kullanıcı EQ'su, subsonic yüksek-geçiren, LR4 ayrımı, dal başına kazanç ve limiter), `hk_biquad` (LR4, DF2T), `hk_limiter` (attack'sız tepe limiter) ve `hk_profile` (profilin kendisi, doğrulaması, zincire dönüşmesi) host'ta testli; tezgâh profili yer tutucu köşelerle (55 Hz subsonic, 3500 Hz crossover — 2026-09-12 kaba taramasından sonra 2800'den yükseltildi) tezgâh yapısına derleniyor ve her açılışta ölçülmediğini söylüyor; zincirin ürün kartında çaldığının tek kaydı `86f629c` commit mesajıdır, `docs/06-testing/` altında tezgâh kaydı yoktur. Hiçbirinde ölçülmüş sürücü değeri yok; `G0` kapandığında yapılacak iş bir struct doldurmaktır. Zincir bitmiş değildir: bilinen boşluklar sonraya bırakıldı. Ayrıntı: [[../04-acoustics/measurement-and-dsp-plan#Profil: biçim yazıldı, sayılar bekliyor|ölçüm ve DSP planı]].
- **Durum (2026-09-12):** zincir artık **ürünün çıkış arka ucudur** ([[../07-decisions/ADR-0022-dsp-product-output-backend|ADR-0022]]): ürün ve release imajı DSP arka ucunu derler, vendor edilen düz geçiş yalnız geliştirme kartında ve tezgâh istisnasında seçilebilir. Aynı gün kodda kapananlar: subsonic yüksek-geçiren dördüncü derece (Butterworth, köşede −3 dB, 24 dB/oktav); limiter release/hold dal başına; hizalama gecikmesi (tek dalda, en çok 64 örnek) ve tweeter polaritesi alanları; besleme bütçesi katı (`supply_budget_sq`, `supply_window_ms` — iki dalın toplamı üzerinde ortalama, tepe limiter'lardan önce ortak kazanç); profilin geçerliliği açılışta `hk_main` tarafından `hk_profile_load()` ile yargılanıyor ve `hk_storage` kararı ses izni için istiyor; ses görevinde blok süresi ve besleme dedektörü telemetrisi (`dsp block max ... us mean ... us of ... us` satırı; üründe 60 s'de bir, tezgâh yapısında 10 s'de bir). Şema 2'dir. **Her sayı hâlâ yer tutucudur** ve profil yokken ürün susar; tweeter DC direnci kayıtta 3,5 Ω, firmware'in tezgâh profili 3,7 Ω taşıyordu, kayıt kazandı ve operatörün teyidi bekleniyor. Amfinin kazanç strap'i okunmadı (`C3`, [[../06-testing/bench-measurement-order|tezgâh sırası]]); okunmadan sürücüde 24 V dinleme yok. F3 açık kalır: kapatan şey `G0`/`G1`/`G2` sayılarıdır, kod değil.
- **Durum (2026-09-12, gece):** profil yazma aracı var — `firmware/tools/write_profile.py`. Kayıttaki sayıları `factory_cal`'ın `profile` blob'una (şema 2, 116 bayt) çevirir, firmware'in reddedeceğini `hk_profile_valid()` ile aynı adla önceden reddeder, blob'u kartın kendi `factory_cal.csv`'sine ekleyip imajı yeniden üretir ve flaş komutunu basıp durur: kimlik bilgilerini yeniden üretmez, flaşlamaz — flaş sahibinin işidir. Sayıların **tek** yeri `docs/assets/measurements/drivers/profile-2026-09-12-provisional.json`: her alan kaynağını (`measured` / `derived` / `placeholder`, hangi kayıt) yanında taşır, `source` dizesi `provisional`dır ve açılış raporunun `calibration … from '…'` satırında öyle görünür; derlenmiş tezgâh profili `factory_cal` boşken yedek olarak kalır ve dosyayla aynı sayıları taşır. Bu, yukarıdaki "hiçbir köşe ya da tavan `G0` kapanmadan `factory_cal`'a yazılmaz" cümlesini **daraltır**, kaldırmaz: yasak, yer tutucuyu kalibrasyon diye yazmaktır; kaynağı `provisional` olan ve kademeli çiftin dinleme testi için sahibin eliyle flaşlanan profil o yasağın içinde değildir, ikisini ayıran `source` dizesidir. Yazılmış profil ürünün ses yolunu **açar** (`present:ok` → `PERMITTED`), o yüzden yalnız tek amfi + bir çift tezgâhtayken yazılır. Sonraki oturumun oynatma testi [[../06-testing/bench-measurement-order#E — Oynatma testi (ürün yolu, sonraki oturum)|tezgâh sırası §E]]'de; hiçbir kapıyı açmaz. Ayrıntı: [[../04-acoustics/measurement-and-dsp-plan#Profili yazmak — değerler dosyası, araç, flaş|ölçüm ve DSP planı]].
- **Kabul ölçütü:**
  - Filtre katsayıları ölçülmüş sürücü empedansından türetildi; tahmin yok.
  - Tweeter yolu HPF'i ölçümle doğrulandı; `C_SAFE` değeri G2 raporundan geldi.
  - Tepe tavanları ölçüldükleri besleme gerilimiyle saklanır ve yapılandırılan beslemeye (`CONFIG_HK_SUPPLY_MV`, varsayılan 24 V) yalnız **aşağı** ölçeklenir: `hk_profile_ceiling_at()` çarpanı `min(1, referans / besleme)`'dir, referansın altındaki bir besleme tavanı yükseltmez. Aşağı ölçekleme dijital tavanı yarıya indirir; **sürücüdeki voltun garantisi değildir**, çünkü TPA3110D2 sabit kazançlıdır — çıkış, ray kırpana kadar kazanç × giriştir (SLOS528F, Tablo 3), besleme ile ölçeklenmez. Tezgâh profili 12 V'ta (`HK_BENCH_REFERENCE_SUPPLY_MV`) dinlendi: tezgâh 12 V rayını kırpmadıysa yarıya inen tavan sürücüdeki voltu da yarıya indirir; kırptıysa 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye tezgâhın duyduğundan fazlasını verebilir (aynı kaydırıcı konumunda iki katına kadar) — hangisinin geçerli olduğunu `C3` (amfinin kazanç strap'i) söyler. `G2` tavanı kullanılacağı beslemede kaydeder ve `C3` ondan önce okunur.
  - 2,9 A adaptör bütçesi tepe tavanlarında değil, kendi alanında taşınır: `supply_budget_sq` ve `supply_window_ms`, `G1` S7'de dört amfi birlikte **4 Ω sınıfı** dummy-load'a sürülürken adaptörün 2,9 A noktasında ölçülür (`VIN` çökmesi ve dedektörün log'daki en yüksek ortalama-karesi), ölçeklenmeden saklanır ve `VIN` çökmesiyle sınanır (`G1` satırı). Tepe tavanları `G2`'nin sürücü koruma sayılarıdır, bütçenin altında kalır ve onu taşımaz. Ölçüm hâlâ gerekli; tezgâh yer tutucusu (1,0) bir tam ölçekli dalın ortalama-karesidir — doğrulayıcı sınırının (2,0) yarısı — ve tezgâh kazançlarıyla (0,25 / 0,18, en çok ≈ 0,095) ancak kullanıcı EQ'su yükseltirse devreye girer; açılışta bunu söyler.
  - Kullanıcı reseti koruma profilini silmiyor (otomatik test).
  - DSP zinciri ses görevinde deterministik süre içinde bitiyor. Kanıt: ürün kartında en az 30 dakikalık bir akış boyunca ses görevinin bastığı `dsp block max ... us mean ... us of ... us` satırı (üründe 60 s'de bir, tezgâh yapısında 10 s'de bir) — `max`, blok süresi 7981 µs'nin (log satırındaki `of ... us`; ölçülen süre 352 karelik bloğa normalize edilir, kaynak hızından bağımsız) altında ve alıcının underrun sayacı sıfır (`under=0`); operatör kaydı `docs/06-testing/` altına girer ([[../06-testing/test-strategy|test stratejisi]]). Aracı kodda; sayı yok.
- **Gate:** `G2` zorunlu.

> [!danger] G0 kapanmadan hiçbir köşe veya tavan kalibrasyon olarak yazılmaz
> Ölçülmemiş empedansla türetilen bir crossover veya limiter, tweeter'ı kalıcı olarak bozabilir. Kural kodun var olmasını değil sayının ölçüm diye yazılmasını yasaklar: zincir üründe derlidir ve `factory_cal`'da geçerli bir profil yokken ürün **susar** (ADR-0022); dinleme yolu tezgâh yapısı ya da kaynağı `provisional` olan, yalnız kademeli çift tezgâhtayken sahibin flaşladığı profildir, sürücüde dinleme kademeli ve düşük seviyededir, ve amfi kazancı (`C3`) okunmadan 24 V'ta sürücüde dinleme yoktur.

### F4 — Ağ ve provisioning

- **Önkoşul:** F0. F2'den bağımsız çalışabilir.
- **Çıktı:** Wi-Fi istemci ve yeniden bağlanma, mDNS adı, açık SoftAP + captive portal, BLE Unified Provisioning (protocomm Security 1, sahiplik kanıtı yok — [[../07-decisions/ADR-0023-pinless-provisioning|ADR-0023]]), sır içermeyen etiket/QR üretimi, provisioning zaman aşımı, BLE belleğinin serbest bırakılması.
- **Kabul ölçütü:**
  - [x] [[../controls-and-provisioning-plan#Merzarkabul Airplay Speakers ürün kimliği\|Kimlik tablosundaki]] tüm yüzey adları doğru üretiliyor (`hk_identity`, host testli). 2026-09-13'ten beri BLE yayını ve kurulum ağı tek adı paylaşır: `PROV_Merzarkabul-XXXX` (ADR-0023); AirPlay ve mDNS adları değişmedi.
  - [x] Provisioning politikası saf mantık olarak yazıldı ve test edildi (`hk_provision`): ilk açılışta zaman aşımı yok, butonla açılan pencere 10 dakikada kapanır, bağlantı denemesi boyunca radyolar açık kalır, başarıdan sonra ikisi de kapanır ve BLE serbest bırakılabilir.
  - [x] Wi-Fi istasyon, yeniden bağlanma, mDNS ve SoftAP provisioning sürücü katmanı yazıldı; ESP-IDF v5.5.1 ile derleniyor.
  - [x] BLE transport'u NimBLE ile etkinleştirildi. O gün ADR-0005 seçenek C'ydi (transport girişten türetilir); 2026-09-08'den beri iki taşıma birlikte açılıyor (ADR-0016).
  - [x] Provisioning politikası artık gerçekten işletiliyor. Önceki hâlinde `hk_prov_handle` her yerde `now_ms = 0` ile çağrılıyor, `HK_PROV_EV_TICK` hiç gönderilmiyor ve `hk_prov_radios()` hiç okunmuyordu: on dakikalık sınırlı pencere hiçbir zaman dolamazdı. Ana döngü saniyede bir tick veriyor ve pencere kapandığında `hk_network_close_provisioning()` çağrılıyor.
  - [ ] Politikanın bağlantı olayları hâlâ beslenmiyor: `hk_main` politikaya yalnız tick, buton kısa basış, ağ sıfırlama ve fabrika sıfırlama verir; `CONNECT_OK` / `CONNECT_FAIL` / `CREDENTIALS` olaylarını hiçbir çağıran üretmez. Üç başarısız katılımın açtığı geri dönüş penceresi bu yüzden host'ta testli ama cihazda **erişilemez** (ADR-0023 kaydı, risk kaydı). Bağlanacak ya da silinecek; ayrı bir karar.
  - [x] iOS'ta **uygulamalı BLE** akışı uçtan uca çalıştı: QR'lı kurulum, kimlik bilgisi teslimi, katılma ve `provisioning succeeded` (2026-09-05, geliştirme kartı) — **Security 2 ile**. ADR-0023'ün Security 1 / `no_pop` yolu bu sonucun kapsamı dışındadır.
  - [x] Uygulamasız yol **yazıldı** (ADR-0015, `hk_portal`): captive DNS her adı cihaza çözüyor, portal sayfası düz bir form ve aldığı bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye veriyor. Form ayrıştırıcısı saf C ve host'ta testli. Kurulum ağı o gün WPA2'ydi; 2026-09-13'ten beri **açık** ve sayfa bunu bir cümleyle söylüyor (ADR-0023).
  - [ ] Uygulamasız yol **ölçülmedi**: hiçbir telefon bu sayfayı açmadı — ne WPA2'li, ne açık hâliyle. PRD-004 bu ölçüm olmadan kapanmaz.
  - [ ] Android'de hiçbir akış denenmedi.
  - [ ] PIN'siz yol hiçbir kartta denenmedi: `esp_prov.py --transport ble --verbose` ile `proto-ver`'de `sec_ver 1` ve `cap [no_pop, wifi_scan]`; iki stok Espressif uygulamasının `PROV_Merzarkabul-XXXX`'i QR'sız ve önek ayarı değişmeden listelemesi; kilitsiz kurulum ağı ve sayfadaki açık-ağ cümlesi. Operatör kaydı olmadan `PASS` yok.
  - [x] Provisioning kapandığında BLE belleğinin geri verildiği ölçüldü: `BTDM memory released`, ardından `provisioning closed and its memory released`.
  - [x] Wi-Fi parolasının loglarda görünmediği otomatik taramayla denetleniyor (`tools/check_no_credential_logs.py`, CI'da).
  - [x] Cihaz başına Security 2 kimlik bilgisi üreten araç 2026-08-31'de yazıldı ve firmware kimlik bilgisi yoksa provisioning'i açmayı reddediyordu. **2026-09-13'te ADR-0023 ile geri alındı:** firmware kurulum için kimlik bilgisi istemez ve reddetmez; `tools/provision_credentials.py` adını korur (CI ve belgeler ona bağlı) ama artık sır üretmez — cihaz klasörüne yalnız şema satırlı `factory_cal.csv`, iki QR yükü ve etiket yazar. Daha önce kimlik bilgisi yazılmış kartlar yeniden flaşlanmaz; eski satırlar okunmaz.
  - [x] Cihaz başına QR yükü üretiliyor ve sır taşımıyor: `{"ver":"v1","name":"PROV_Merzarkabul-XXXX","transport":"ble"|"softap","security":1}` — ESP-IDF'in `wifi_prov_print_qr()` çıktısının kanıtsız biçimi artı açık `security` alanı. QR isteğe bağlıdır; `PROV_` öneki sayesinde cihaz listeden de seçilir.
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
- **Çıktı:** 24 saat soak, Wi-Fi kopması ve yeniden bağlanma, besleme kaybında kapanış kaydı: amfinin kendi geçişi kaydedilir ve kararlaştırılmış bir seviyeye göre yargılanır — amfi susturması olmadığı için firmware "pop yok" vaat edemez ([[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]]; adaylar [[../02-hardware/circuit-and-wiring-plan#3.4 Kanal ve sürücü kuralları|kablolama planı §3.4]]).
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
    hk_audio/          susturma sırası, DSP zinciri, profil, limiter (saf, testli) [F2-F3 · var]
    hk_airplay/        rbouteiller/airplay-esp32 sarmalayıcısı; S/PDIF (geliştirme kartı) ve düz geçiş (yalnız geliştirme kartı / tezgâh istisnası) vendor/audio/ altında [F1 · vendor]
      output/          DSP zinciri, ürünün çıkış arka ucu (`hk_airplay_output_i2s.c`, vendor `audio_output.c`'nin gölgesi) [F1-F3 · var]
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
