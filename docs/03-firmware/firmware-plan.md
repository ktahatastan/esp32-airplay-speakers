---
status: active
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [firmware, plan, roadmap, esp32]
---

# Firmware planı ve aşamalandırma

Bu belge firmware'in **ne olduğunu** ve **hangi sırayla yapıldığını** birlikte tanımlar. Aşama sırası keyfi değildir: her aşama kendinden öncekinin kanıtına ve gerektiğinde bir donanım kapısına bağlıdır.

Depoda henüz firmware kaynağı yoktur. `F0` ilk yazılacak iştir.

## Kilitli girdiler

| Girdi | Değer | Kaynak |
|---|---|---|
| Kart | ESP32-S3, 16 MB flash + 8 MB PSRAM | [[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]] |
| Ses topolojisi | Mono program, bi-amp: sol yol woofer, sağ yol tweeter | [[../07-decisions/ADR-0002-biamp-signal-chain\|ADR-0002]] |
| Provisioning | SoftAP/captive portal **ve** BLE Unified Provisioning | [[../07-decisions/ADR-0005-dual-provisioning\|ADR-0005]] |
| Dağıtım | SemVer tag -> GitHub Releases -> imzalı A/B OTA | [[../07-decisions/ADR-0008-github-releases-ota\|ADR-0008]] |
| Şarj davranışı | V1'de şarj sırasında amfi kapalı | [[../07-decisions/ADR-0004-v1-charge-policy\|ADR-0004]] |
| AirPlay yığını | **Seçilmedi** | [[../07-decisions/ADR-0007-airplay-stack\|ADR-0007]] açık |

`ADR-0007` açık olduğu için `F1` bir fizibilite spike'ıdır ve sonucu tüm ses mimarisini etkileyebilir. `F1` kapanmadan `F2` sonrasına kaynak ayrılmaz.

## Modüller

| Modül | Sorumluluk |
|---|---|
| `audio` | AirPlay alıcı, saat/buffer yönetimi, I2S sürücü, DSP zinciri, limiter |
| `network` | Wi-Fi istemci, mDNS, SoftAP captive portal, BLE provisioning |
| `ui` | Buton durum makinesi, RGB LED animatörü |
| `storage` | `factory_calibration` ve `user_settings` NVS ayrımı, şema migrasyonu |
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
- **Çıktı:** `firmware/` ESP-IDF projesi, sabitlenmiş IDF sürümü, `sdkconfig.defaults`, partition CSV taslağı, host birim testi hedefi, `PROJECT_VER` üretimi, PR CI iş akışı.
- **Kabul ölçütü:**
  - Temiz checkout'ta `idf.py build` geçiyor ve tekrarlanabilir.
  - IDF sürümü tek yerde sabit; sürüm dosyada yazılı.
  - 16 MB flash için `nvs`, `nvs_keys`, `factory_calibration`, `otadata`, `ota_0`, `ota_1` bölümleri tanımlı ve boyut bütçesi tabloda.
  - PR CI: format + statik analiz + host testleri + build + boyut kapısı.
- **Gate:** yok. `G6`'nın ön koşuludur.

### F1 — AirPlay yığını fizibilite spike'ı

- **Önkoşul:** F0. Bir adet ESP32-S3 kartı. Hoparlör gerekmez.
- **Çıktı:** aday yığınların kaynak kodu/lisans incelemesi, en küçük çalışan alıcı, dört sahte cihazın Apple istemcisinde birlikte görünüp görünmediğinin kanıtı, kaynak tüketimi ölçümü, `ADR-0007` doldurulmuş hâli.
- **Kabul ölçütü:**
  - Desteklenen AirPlay sürümü birincil kaynakla (kaynak kodu/lisans) belgelendi.
  - Dört hedefin Apple cihazında **birlikte seçilebildiği** kayıtla gösterildi veya gösterilemediği açıkça yazıldı.
  - Heap/PSRAM kullanımı ve CPU yükü ölçüldü; 16 MB flash + 8 MB PSRAM bütçesine sığıyor.
  - Lisans hedef kullanımla uyumlu.
  - `ADR-0007` `accepted` veya `rejected` oldu.
- **Gate:** `G7`'nin ön koşulu. **Bu aşama başarısız olursa ürün gereksinimi PRD-002 yeniden müzakere edilir.**

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
- **Çıktı:** woofer HPF, aktif crossover, kanal gain/delay, RMS ve tepe limiter, clipping davranışı, `factory_calibration` profil formatı.
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
  - [[../controls-and-provisioning-plan#Harman Kardom ürün kimliği\|kimlik tablosundaki]] tüm yüzey adları doğru üretiliyor.
  - iOS ve Android'de hem uygulamalı BLE hem uygulamasız captive portal akışı test raporuyla geçti.
  - Provisioning sonrası BLE kapandı ve heap geri kazanıldı (ölçüldü).
  - Wi-Fi parolası ve PoP hiçbir log, portal yanıtı veya crash dump'ta görünmüyor (otomatik tarama).
  - Cihaz başına PoP benzersiz; ortak fabrika parolası yok.
- **Gate:** `PRD-004`. `G6` girdi.

### F5 — Kullanıcı arayüzü

- **Önkoşul:** F4 (provisioning tetikleyicisi için).
- **Çıktı:** buton durum makinesi (kısa / 5 sn / 12 sn), 50 ms debounce, açılışta kurtarma modu, RGB LED animatörü ve durum tablosu.
- **Kabul ölçütü:**
  - Üç eşik ayrı ayrı doğrulandı; 12 sn işlemi **yalnız buton bırakılınca** onaylanıyor.
  - Yanlışlıkla kısa dokunma kayıtlı Wi-Fi'yi silmiyor.
  - Kullanıcı reseti `factory_calibration`'a dokunmuyor (PRD-008 testi).
  - LED PWM'inin I2S zamanlamasına ve analog dip gürültüsüne etkisi ölçüldü.
- **Gate:** `PRD-005`, `G3` katkı.

### F6 — Depolama ve güç telemetrisi

- **Önkoşul:** F4. Donanım tarafında `G4` (batarya/BMS/şarj) geçmiş olmalı.
- **Çıktı:** `factory_calibration` / `user_settings` NVS şeması ve migrasyon, paket gerilimi ve NTC okuma, düşük gerilim politikası, güvenli kapanış sıralaması, şarj durumunda amfi kilidi.
- **Kabul ölçütü:**
  - Şema migrasyonu ileri ve geri testli; bozuk NVS'te cihaz açılıyor.
  - `OTA_MIN_PACK_MV` eşiği G3/G4 ölçümünden türetildi; ölçüm yoksa OTA başlamıyor.
  - Düşük gerilimde kontrollü kapanış: önce mute, sonra amfi, sonra sistem.
  - Şarj algılandığında amfi ADR-0004 uyarınca kapalı.
  - Batarya yüzdesi yalnız açık devre geriliminden hesaplanmıyor; yöntem belgelendi.
- **Gate:** `G4` girdi, `G8` katkı.

### F7 — OTA ve release hattı

- **Önkoşul:** F0, F6. Tüm ayrıntı: [[ota-and-release-plan\|OTA ve sürüm yönetimi planı]].
- **Çıktı:** manifest ayrıştırıcı ve donanım eşleme, HTTPS indirme, güncelleme kapıları durum makinesi, ilk-boot sağlık kontrolü, rollback, `esp_ghota` spike sonucu, imzalama ve GitHub Actions release hattı, canary/stable kanal politikası, USB/UART recovery prosedürü.
- **Kabul ölçütü:** [[ota-and-release-plan#G6 kabul matrisi\|G6 kabul matrisinin]] on iki satırının tamamı test raporlu.
- **Gate:** `G6` zorunlu.

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
  CMakeLists.txt
  sdkconfig.defaults
  partitions.csv
  version.txt
  main/
  components/
    hk_audio/          I2S, DSP, limiter
    hk_airplay/        seçilen yığının sarmalayıcısı
    hk_network/        Wi-Fi, mDNS, portal, BLE provisioning
    hk_ui/             buton, LED
    hk_storage/        NVS şeması ve migrasyon
    hk_power/          telemetri, kapanış politikası
    hk_update/         manifest, OTA, sağlık kontrolü
  test/                host tarafı birim testleri
```

Her bileşen kendi başlık dosyasında açık bir arayüz sunar; modüller birbirinin iç durumuna erişmez.

## Tamamlanma tanımı

Bir firmware görevi ancak şunlar varsa biter: kod, host testi veya ölçüm kanıtı, güncellenmiş belge, gerekliyse ADR, `python3 scripts/check_docs.py` çıktısı ve açık risk listesi. Fiziksel ölçüm gerektiren kabul, operatör kaydı olmadan `PASS` yapılamaz.

## İlgili belgeler

- [[ota-and-release-plan\|OTA ve sürüm yönetimi]]
- [[security-and-recovery\|Güvenlik ve recovery]]
- [[../controls-and-provisioning-plan\|Kontroller ve provisioning]]
- [[../01-architecture/audio-network-feasibility\|AirPlay fizibilitesi]]
- [[../02-hardware/board-and-pin-selection\|Kart ve pin seçimi]]
- [[../06-testing/test-strategy\|Test kapıları]]
