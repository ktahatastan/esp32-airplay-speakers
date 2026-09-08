---
status: active
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-08
tags: [testing, product-board, bring-up, firmware, evidence]
---

# Ürün kartı bring-up kaydı

Bu, deponun firmware'inin **ürün kartında** ilk kez çalıştığı kayıt. Kart, [[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]]'un kilitlediği `N16R8`'dir — [[devkit-bring-up|geliştirme kartı]] değil.

> Buradaki hiçbir satır bir fiziksel kapı (`G0`-`G8`) açmaz. Karta sürücü, amfi, DAC ve batarya bağlı değildi; ölçülen tek şey işlemci, bellek ve bölüm yerleşimidir.

## Kart kimliği

Etiketten değil çipten, `esptool` ile, 2026-09-08:

```text
ESP32-S3 (QFN56) revision v0.2
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_3v3)
Crystal 40MHz
MAC: 68:ee:8f:4b:93:2c
Manufacturer: 68  Device: 4018
Detected flash size: 16MB
Flash type set in eFuse: quad (4 data lines), voltage 3.3V
```

Bu, ADR-0010'un istediği karttır ve ürün profilinin dört ayarı bununla eşleşir: `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, `CONFIG_SPIRAM_MODE_OCT` @80 MHz, `partitions.csv`, ve ürün OTA donanım sürümü.

## Doğru porta ulaşmak: ilk port yanlış porttu

Kart takıldığında görünen ilk seri port `0x303A:0x4001` kimliğindeydi. Bu **ROM'un indirme portu değildir**; çalışan bir uygulamanın TinyUSB CDC portudur. `esptool` oraya bağlanamaz ve hata mesajı ("No serial data received") sebebini söylemez.

Üç yeniden başlatma yolu denendi ve üçü de sökmedi: `--before default_reset`, `--before usb_reset`, ve 1200-baud dokunuşu. Karttaki uygulama host'tan yeniden başlatmayı uygulamıyor.

Çözüm kartın **ikinci** USB soketiydi: arkasında bir WCH CH343 UART köprüsü var (`0x1A86:0x55D3`), ve esptool onu DTR/RTS ile kendisi sıfırlıyor — düğmeye basmaya gerek kalmadı.

> Kayda geçme sebebi: bu, "kart bozuk" diye yorumlanmaya çok müsait bir belirti. Port görünüyor, cihaz cevap vermiyor. Sonraki kartta ilk bakılacak şey USB PID'idir: `0x4001` uygulamanın portu, `0x1001` çipin kendi USB-Serial/JTAG'i, `0x1A86:*` ise köprü.

## Baud sınırı

`read_flash` 921600 ve 460800 baud'da `Invalid head of packet` ile düştü (0xAE ve 0xCC). 230400 baud'da sorunsuz. Yazma da 230400'de yapıldı. Bu köprü/kablo ikilisinde yüksek baud güvenilir değil; bir sonraki oturum bunu varsaymasın.

## Karta gelmeden önce üzerinde ne vardı

Kart boş değildi: RGB LED'i döndüren bir üretici demosu vardı (seri portta `50%G / 50%B / 50%R`). Silmeden önce **tüm flash yedeklendi**:

```text
flash-full-16MB-68ee8f4b932c.bin   16.777.216 bayt
sha256 d73b6dd5a15e469a30629798664fb6e9e5c0ee30ef2679d75858d97a72b6fb15
```

Yedek depo dışında tutuluyor. Yedekten çözümlenen doluluk:

| Bölge | Durum |
|---|---|
| `0x000000` - `0x160000` | demo imajı (1408 KB) |
| `0x200000`, `0x570000` | birer 64 KB blok |
| geri kalan | silinmiş (`0xFF`) |

Bizim tablomuzun `factory_cal` ofsetinde (`0x13000`) veri vardı, ama o **demo kodudur, kalibrasyon değildir**: bu kart hiç `G0`'dan geçmedi. Yani kaybedilecek ölçüm yoktu.

## Silme: tam değil, cerrahi

[[../03-firmware/usb-recovery|USB kurtarma notunun]] ilkesi "kurtarma cerrahi olmalı"dır, çünkü tam silme `factory_cal`'ı da götürür ve oradaki ölçümleri hiçbir yazılım yeniden üretemez. Bu kartta kaybedilecek kalibrasyon olmadığı **yedekten doğrulandığı hâlde** yine cerrahi gidildi:

```text
esptool erase_region 0x9000 0x17000     # nvs, otadata, phy_init, nvs_keys, factory_cal
```

Sebep ilkeyi korumak değil sadece: ilk açılış raporunun **belirsiz olmaması**. Yabancı baytların üzerine yazılan bir depo, "boş mu, bozuk mu" ayrımını rapordan okunamaz hâle getirirdi.

## Yazma

```text
idf.py -C firmware -p /dev/cu.usbmodem5CE60456821 -b 230400 flash
```

Dört bölge, dördünde de `Hash of data verified`:

| Ofset | İmaj | Boyut | sha256 (ilk 16) |
|---|---|---:|---|
| `0x0` | `bootloader.bin` | 22.496 | `d19e2ad8f9349438` |
| `0x8000` | `partition-table.bin` | 3.072 | `9e7f93481767aa09` |
| `0xf000` | `ota_data_initial.bin` | 8.192 | `7d2c7ac4888bfd75` |
| `0x20000` | `harman-kardom.bin` | 1.324.368 | `d45eb850f8592cfd` |

Depo durumu: `cd222d4`, çalışma ağacı temiz.

## Açılış raporu

```text
I (43) boot.esp32s3: SPI Flash Size : 16MB
I (61) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (93) boot:  5 ota_0            OTA app          00 10 00020000 006e0000
I (100) boot:  6 ota_1            OTA app          00 11 00700000 006e0000
I (106) boot:  7 storage          Unknown data     01 82 00de0000 00220000
I (353) octal_psram: vendor id    : 0x0d (AP)
I (353) octal_psram: density      : 0x03 (64 Mbit)
I (396) esp_psram: Found 8MB PSRAM device
I (400) esp_psram: Speed: 80MHz
I (829) esp_psram: SPI SRAM memory test OK
I (898) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (905) spi_flash: detected chip: boya
I (907) spi_flash: flash io: qio
I (955) hk_health: image state 2: nothing to confirm
I (979) hk: Harman Kardom
I (980) hk: firmware    0.1.0
I (980) hk: idf         v5.5.1
I (987) hk: slot        ota_0 at 0x00020000, 7208960 bytes
I (992) hk: board       prototype-n16r8
I (995) hk: chip        2 core(s), revision 2
I (999) hk: flash       16 MB detected
I (1003) hk: psram       8 MB
I (1006) hk: free        289667 B internal (largest block 196608 B), 8386156 B psram
I (1013) hk: device id   932C
I (1016) hk: airplay     Harman Kardom 932C
I (1020) hk: ble         HarmanKardom-932C
I (1023) hk: softap      HarmanKardom-Setup-932C
I (1028) hk: mdns        harman-kardom-932c.local
I (1077) hk: storage     user=use calibration=fail_safe
I (1110) hk: power       UNKNOWN (no calibrated limits, no ADC driver)
I (1116) hk: audio       NOT permitted
I (1120) hk: output      SILENT (i2s=0 dac=0 amp=0)
I (1124) hk: first boot  WAIT (INCOMPLETE)
E (1135) hk: audio is NOT permitted: this device has no trustworthy driver protection profile. No default profile is invented (G0/G2).
```

## Ne PASS oldu

| Ölçüm | Beklenen | Sonuç |
|---|---|---|
| Oktal PSRAM açılıyor ve bellek testini geçiyor | `octal_psram:` + 8 MB | **PASS** — vendor AP, 64 Mbit, 8192K heap'e eklendi |
| PSRAM 80 MHz | `Speed: 80MHz` | **PASS** |
| 16 MB flash algılanıyor | `SPI Flash Size : 16MB` | **PASS** |
| Ürün bölüm tablosu yükleniyor | `ota_0` `0x20000`+`0x6e0000` | **PASS** — geliştirme tablosunun `0x2e0000`'ı değil |
| Kart kendi varyantını bildiriyor | `prototype-n16r8` | **PASS** |
| Slot boyutu ürün slotu | 7.208.960 B | **PASS** |
| Kimlik MAC'ten türüyor | `932C` | **PASS** — MAC son iki baytı `93:2c` ile birebir |
| Dört yüzey adı doğru üretiliyor | AirPlay/BLE/SoftAP/mDNS | **PASS** |
| Kalibrasyon yokken `fail_safe`'e düşüyor | `calibration=fail_safe` | **PASS** |
| Kalibrasyon yokken ses **izinli değil** | `audio NOT permitted` | **PASS** |
| Çıkış hatları susturulu | `i2s=0 dac=0 amp=0` | **PASS** |
| Kimlik bilgisi yokken provisioning **açılmıyor** | ret, düşürme yok | **PASS** |
| Kurulum ağı anahtarı yokluğu bildiriliyor (ADR-0015) | uyarı satırı | **PASS** |
| Tekrarlanabilirlik | iki açılış aynı | **PASS** — `free 289667 B` dâhil her satır birebir, panik yok |

Boş bellek, geliştirme kartına göre beklendiği gibi büyük: **8.386.156 B PSRAM** (geliştirme kartında 2.094.848 B). Dahili boş 289.667 B, en büyük blok 196.608 B.

## Ne PASS olmadı, ve neden

- **GPIO tablosu hâlâ `candidate`.** Açılış raporu tabloyu basıyor ve kendini `candidate` diye adlandırıyor. [[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]] tabloyu `accepted` yapmak için açılış testini yeterli saymıyor: **satın alınan kartın kendi şeması** gerekiyor. Kart açıldı diye pinler doğru demek değil — firmware o pinleri henüz sürmedi, ve kartın üzerinde o pinlere bağlı ne olduğu bilinmiyor.
- **Wi-Fi başlamadı** ve bu doğru davranış: `factory_cal` yok, provisioning kimlik bilgisi yok, firmware zayıf moda düşmeyi reddediyor (`network did not start: ESP_ERR_NOT_FOUND`). Ağ yolu bu kartta henüz hiç sınanmadı.
- **`hk_portal` çalıştırılmadı.** Kurulum ağı ancak kimlik bilgileri yazıldıktan sonra açılır.
- **Hiçbir fiziksel kapı açılmadı.** Kartta sürücü, amfi, DAC, batarya yok.

## Küçük bir tutarsızlık

`hk_health` `image state 2` bildiriyor (`ESP_OTA_IMG_VALID`), `-1`/`UNDEFINED` değil — çünkü `ota_data_initial.bin` açıkça yazıldı. Sonuç aynı ve doğru: onaylanacak bir şey yok, rollback yolu yalnız `PENDING_VERIFY`'da iş yapar. Kayda geçiyor çünkü "ilk açılışta state `-1` olmalı" beklentisi yanlıştır ve bir sonraki oturumu yanlış yere baktırabilir.

---

## Kimlik bilgileri yazıldı, ve portal ilk kez çalıştırıldı

`provision_credentials.py --device 932C --image` ile üretildi (çıktı depo dışında, dizin `700`), `0x13000`'a yazıldı, `Hash of data verified`. İmaj 53.248 bayt = `0xd000`, ilk NVS sayfası ACTIVE, beş anahtarın beşi de yerinde: `cal`, `schema`, `prov_salt`, `prov_verif`, `ap_pass`.

Üretici tarafında bir kusur bulundu ve düzeltildi: yalnız `label.txt` `600` yapılıyordu, oysa **`ap_pass.bin` parolayı düz metin taşıyor** ve `qr.txt` de aynı parolayı `pop` alanında taşıyor; ikisi de `644`'tü. Üçü de artık `600`. `prov_salt`/`prov_verif` bilerek açık: salt telde açık gider, verifier parolaya çevrilemez.

### İlk deneme: açılış döngüsü

Kimlik bilgileri yazıldıktan sonra cihaz **açılış döngüsüne** girdi — 25 saniyede 16 açılış, her biri tam olarak `hk_portal: setup page open` satırından sonra. Deterministik `LoadProhibited`, her seferinde birebir aynı backtrace:

```text
httpd_find_uri_handler          IDF/components/esp_http_server/src/httpd_uri.c:89
httpd_register_uri_handler      IDF/components/esp_http_server/src/httpd_uri.c:139
protocomm_httpd_add_endpoint    IDF/components/protocomm/.../protocomm_httpd.c:206
protocomm_set_security          IDF/components/protocomm/src/common/protocomm.c:290
wifi_prov_mgr_start_provisioning IDF/components/wifi_provisioning/src/manager.c:1715
start_provisioning              firmware/components/hk_network/hk_network.c:414
```

Sebep, ADR-0015'i uygularken yaptığım bir hataydı ve iki katmanlıydı:

1. `wifi_prov_scheme_softap_set_httpd_handle()`'ın parametresi "Handle to HTTPD server instance" diye belgeli, ama protocomm onu **handle'a işaretçi** olarak kullanıyor: `protocomm_httpd.c:205-206` `httpd_handle_t *server = pc_httpd->priv;` ardından `httpd_register_uri_handler(*server, ...)`. Handle'ın **değerini** geçmek, protocomm'a o sayıyı adres sanıp oradan bir sunucu yapısı okutur.
2. Daha derini ömür: handle `start_provisioning()`'in **yığın çerçevesinde** duruyordu. `&server` geçilseydi bile protocomm, pencere açık kaldığı sürece ölü bir yığın adresini tutacaktı.

Düzeltme ikisini birden kapatıyor: `hk_portal` kendi statiğinin adresini veren bir fonksiyon sunuyor (`hk_portal_server_slot()`), yani hem doğru dolaylılık hem sahibine bağlı ömür. Teardown güvenli: protocomm işaretçiyi yalnız kendi ayırdığında `free` ediyor (`ext_handle_provided` yanlışken), bu yol her zaman o bayrağı doğru yapıyor.

Bu kusur host testleriyle yakalanamazdı: iki ESP-IDF bileşeni arasındaki bir sözleşme, ve yalnız gerçek silikonda görünüyor.

### Düzeltmeden sonra ölçülen

```text
hk_store: calibration store: match -> use
hk: storage     user=use calibration=use
hk_net: provisioning credentials loaded: salt 16 B, verifier 384 B
hk_net: setup network key loaded: 12 B
hk_portal: setup page open at http://192.168.4.1/
wifi:mode : sta + softAP
esp_netif_lwip: DHCP server started on interface WIFI_AP_DEF with IP: 192.168.4.1
wifi_prov_mgr: Provisioning started with service name : HarmanKardom-Setup-932C
hk_net: provisioning open over softap
```

| Ölçüm | Sonuç |
|---|---|
| Kalibrasyon deposu okunuyor | **PASS** — `calibration=use` |
| SRP6a salt/verifier yükleniyor | **PASS** — 16 B / 384 B |
| ADR-0015'in `ap_pass`'i yükleniyor | **PASS** — 12 B, ilk kez bir cihazda |
| Portal ayağa kalkıyor | **PASS** — `192.168.4.1` |
| SoftAP + DHCP açılıyor | **PASS** |
| Provisioning doğru adla açılıyor | **PASS** — `HarmanKardom-Setup-932C` |
| Açılış döngüsü / panik | **PASS** — 1 açılış, 0 panik |
| Ses hâlâ izinsiz | **PASS** — kalibrasyon var ama sürücü profili yok |

**Dışarıdan doğrulanmadı:** Mac'in komşu ağ taraması 17 ağ döndürdüğü hâlde `HarmanKardom-Setup-932C`'yi göstermedi. Bu, ağın yayında olmadığı anlamına gelmez — macOS'un komşu listesi önbelleklidir ve seri porta her dokunuşum kartı sıfırlayıp AP'yi indirip kaldırıyor. Kesin cevap telefondan gelecek: ağ listesinde görünüyor mu, ve **kilit simgesi var mı** (ADR-0015'in WPA2 iddiası).

## Kart üzerindeki durum LED'i

Kullanıcı kartta **yeşil** yanan bir LED gördü. O renk bizim değildi: `HK_DEVKIT_STATUS_LED` ayarı `HK_BOARD_DEVKIT_N8R2`'ye bağlıydı, yani ürün profilinde ayna derlenmiyordu ve açılış logunda `mirrored on gpio` satırı hiç geçmiyordu. Adreslenebilir bir LED son yazılan rengi tutar; sildiğimiz demo firmware'i `50%G / 50%B / 50%R` döngüsü yapıyordu.

Bu, aynanın kartla değil **harici RGB LED'in bağlı olup olmadığıyla** ilgili olduğunu gösterdi: ADR-0011'in göstergesi henüz hiçbir karta lehimlenmedi, yani ürün kartı da tıpkı geliştirme kartı gibi durumunu yalnız seri konsoldan gösteriyordu. Ayar `HK_ONBOARD_STATUS_LED` olarak yeniden adlandırıldı ve kart bağımlılığı kaldırıldı.

Ayna hâlâ ikinci bir gösterge değil: renk aynı render geçişinden geliyor. Pin çakışması artık yorumla değil `_Static_assert` ile engelleniyor — `HK_PIN_MASK` ile kesişen bir GPIO derleme hatası.

Ölçülen: `hk_ui: on-board status LED mirrored on gpio48`, panik yok. **Rengin gözle doğrulanması operatöre ait** ve cihaz o an provisioning'de olduğu için beklenen renk mavi nefestir, yeşil değil.

### Sarı yanıp sönen LED, ve altındaki gerçek kusur

Ayna açıldıktan sonra operatör LED'i **sarı yanıp sönerken** gördü. Durum tablosuna göre bu `HK_LED_CONNECTING` ("Wi-Fi'ye katılıyor"); cihaz ise provisioning'deydi ve orada olması gereken mavi nefestir.

Kök neden LED'de değildi. `WIFI_EVENT_STA_START` korumasızdı:

```c
case WIFI_EVENT_STA_START:
    s_status.connecting = true;
    publish_status();
    esp_wifi_connect();     /* kurulum penceresi radyonun sahibiyken de */
```

SoftAP şeması Wi-Fi'yi APSTA'ya alıyor, bu istasyonu başlatıyor ve olay tam da radyoyu bilerek sahiplenen pencerenin içinde düşüyor. İki sonucu var:

1. **Aynı hatanın tekrarı.** 2026-09-05'te `STA_DISCONNECTED` dalı için düzeltilen şeyin ta kendisi: yöneticinin ilk işi taramaktır ve `esp_wifi_scan_start` devam eden bir bağlanma varken reddedilir. O düzeltme yalnız kopma dalını korumuş, başlama dalını değil — ve geliştirme kartında BLE yolu kullanıldığı için (BLE `WIFI_MODE_STA`, APSTA geçişi yok) hiç görünmemişti.
2. **Göstergenin yalan söylemesi.** Kayıtlı ağ yokken `esp_wifi_connect()` anında hata döner, ve **başlamamış bir çağrının ardından `STA_DISCONNECTED` olayı doğmaz** — yani `connecting` pencere boyunca açık kalır. LED'in sürekli sarı olmasının sebebi buydu.

Düzeltme iki dalı da kapatıyor: pencere radyonun sahibiyken istasyon bağlanmıyor, ve pencere dışında `esp_wifi_connect()` hata dönerse durum orada temizleniyor — çünkü başlamamış bir çağrı için başka temizleyecek bir şey yok.

Ölçülen: `hk_net: station started while setup owns the radio; not joining`, ve LED **mavi nefes** (operatör doğruladı). Bu, `hk_led` önceliğinin doğru olduğunu da gösteriyor: `connecting` gerçekten doğruyken provisioning'i bastırması bilinçli bir tercih, ve sorun sıralamada değil, `connecting`'in yanlışlıkla doğru olmasındaydı.

### Dışarıdan doğrulanan

Operatör kurulum ağını telefonunun Wi-Fi listesinde **gördü**. Mac'in `system_profiler` komşu listesi onu göstermemişti; o liste önbelleklidir ve seri porta her dokunuş kartı sıfırlayıp AP'yi indirip kaldırıyordu. Cihazın kendi logu ile dış gözlem bu kez uyuştu.

**Hâlâ doğrulanmadı:** ağın WPA2 olduğu (kilit simgesi), portalın kendiliğinden açıldığı, ve kurulumun uçtan uca tamamlandığı.

## Ekran: kare bütçesi ölçümü (kart 056C, 2026-09-08)

Kartın kendi raporladığı sayılar. Ölçüm ilk kareden **sonra** alınıyor: SPI, PSRAM
kaynaklı bir aktarımın DMA tamponlarını ilk gönderimde ayırdığı için, öncesinde
alınan sayı yanlış soruyu cevaplıyor.

| aşama | render | aktarım | galaksi | kare | 42 ms'yi aşan |
|---|---:|---:|---:|---:|---|
| ilk hâli | 112.356 µs | 9.500 µs | 3.029 µs | ~62 ms | hepsi |
| vignette geçişi kaldırıldı | 52.861 µs | 9.465 µs | 3.060 µs | ~52 ms | — |
| iris satır aralığı + 240 MHz | **5.604 µs** | 9.494 µs | 2.912 µs | **22 ms** | **720 karede 0** |

Ham kayıt:

```text
I (8322) hk_lcd: first frame sent: render 5604 us, transfer 9494 us, sky 2912 us;
                 internal free 151663 B (largest block 77824 B)
I (13331) hk_lcd: 120 frames, 0 over 42 ms; last 22 ms (sky 2904 us)
I (38551) hk_lcd: 720 frames, 0 over 42 ms; last 23 ms (sky 2926 us)
```

Panel aktarımı 80 MHz'de 9,5 ms ve kısaltılamaz — 240×240×16 bit = 115.200 bayt.
Kısalması gereken render'dı.

### Dahili RAM

| durum | boş | en büyük blok |
|---|---:|---:|
| ekran eklenmeden önce, iki taşıma açık | 148.007 B | — |
| ekran eklendikten sonra (kuyruk derinliği 10) | **6.291 B** | 2.176 B |
| kuyruk derinliği 2 | 81.783 B | 31.744 B |
| tam arayüz, 240 MHz | 151.663 B | 77.824 B |

### Karekod: çözücüyle doğrulama

İkinci bir kodlayıcıyla modül karşılaştırması yapıldı ve **tutmadı** — aynı sürüm ve
boyut, farklı maske. Maske seçimi standardın serbest bıraktığı bir arama olduğu için
bu yanlış testtir. Doğru test çözmektir; OpenCV `QRCodeDetector` ile:

```text
qr0: DECODED OK  'HELLO'
qr1: DECODED OK  'WIFI:T:WPA;S:HarmanKardom-Setup-932C;P:...;;'
qr2: DECODED OK  '{"ver":"v1","name":"HarmanKardom-932C","username":"wifiprov",...'
qr3: DECODED OK  'WIFI:T:WPA;S:HarmanKardom-Setup-932C;P:4719;;'
4/4 decoded correctly
```

### Hâlâ açık

- **Yön.** Panelin aynalanıp aynalanmadığı operatör tarafından okunmadı. Açılıştaki
  yön kartı (`ÜST/SOL/SAĞ/ALT` + asimetrik bir `F`) bunu X ve Y için ayrı ayrı çözer;
  merkezi artı işareti çözemez, çünkü her çevirme altında aynı görünür.
- Ekranın yazı ve simge okunabilirliği yalnız host PNG'lerinde bakıldı; 240 piksellik
  yuvarlak camda kol mesafesinden hiçbir şey doğrulanmadı.
