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
