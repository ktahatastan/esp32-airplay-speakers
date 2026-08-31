---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-08-31
tags: [adr, hardware, gpio, audio, safety]
---

# ADR-0011: Ses tarafı GPIO rezervasyonu ve yasak pin maskesinin tamamlanması

## Karar

Pin tablosuna dört GPIO eklenir ve `HK_PIN_FORBIDDEN_MASK` tamamlanır.

| Sinyal | GPIO | Ne | Durum |
|---|---|---|---|
| `AMP_MUTE` | 21 | TPA3110 `SD`, aktif düşük | **rezervasyon** |
| `DAC_XSMT` | 13 | PCM5102A `XSMT`, aktif düşük | **rezervasyon** |
| `BATT_SENSE` | 1 | ADC1_CH0, 4S paket bölücü üzerinden | **rezervasyon** |
| `NTC_SENSE` | 2 | ADC1_CH1, hücre termistörü | **rezervasyon** |

Mevcut dokuz atama (GPIO4-12) **değişmez**. Hepsi geçerli ve yeniden kesmek üretilmiş şemaları ve TP11-TP25 prob haritasını bedelsiz geçersiz kılardı.

Kart kararı ([[ADR-0010-esp32-s3-n16r8-board|ADR-0010]]) değişmez; o ADR zaten GPIO atamasını `candidate` olarak bırakmıştı. Bu ADR o adayı genişletir, kartı değiştirmez. Atama, satın alınan kartın kendi şeması ve bir boot testi olmadan `accepted` sayılmaz.

## Gerekçe

### Neden şimdi

Hiçbir şey lehimlenmedi ve donanım gelmedi. Bir pin eklemek bugün bedava; harness kurulduktan sonra pahalı. Bekleyip sonra ihtiyaç duymak, kabloyu değil kabloyu taşımayı gerektirir — analog bir kanal söz konusuysa bölücüyü de.

### Susturma hattı ESP32 tarafından tutulmaz

Bu ADR'nin en önemli maddesi ve tek güvenlik maddesi.

Bu parçadaki her aday GPIO, reset'ten çıkarken **yüksek empedanslıdır** ve çıkış sürücüsü kapalıdır; ROM, ikinci aşama bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce milisaniye. O pencerede amfi, girişindeki her şeyi üretmekte serbesttir.

Bu yüzden güvenli durum **harici bir direnç** ile tutulur, GPIO ile değil: `SD` ve `XSMT` netlerinin her birine 10 kΩ pull-down. Parçanın tipik 45 kΩ dahili pull'una karşı 10 kΩ, dörtten fazla kat baskındır.

Yazılımın işi susturmayı **bırakmaktır**, yaratmak değil. Bu firmware hiç çalışmazsa hoparlörler sessiz kalır.

Şu pinler susturma görevinden **adıyla** dışlanır:

- **GPIO18, 19, 20** — silikon bunları açılışta HIGH sürer.
- **GPIO0, 39, 43, 44** — zayıf dahili pull-up ile açılırlar.

Aktif-düşük bir susturma hattı bunların herhangi birinde olsaydı, yazılım var olmadan önce amfi serbest kalırdı — empedansı hâlâ açık `G0` engeli olan sürücülere.

GPIO21 seçildi çünkü bu modülde reset'te veya sonrasında dahili pull'u olmayan, açılış glitch tablosunda yer almayan, strapping/USB/UART0/JTAG rolü bulunmayan ve `gpio_hold_en()` derin uykuda çalışsın diye RTC yetenekli tek pin. İkinci ve üçüncü tercihler GPIO40 (MTDO) ve GPIO42 (MTMS).

### `AMP_MUTE` bir rezervasyon, bağlantı değil

XH-A232 kartının erişilebilir bir `SD` pad'i olup olmadığı [[../02-hardware/circuit-and-wiring-plan|kablolama planında]] hâlâ açık bir karar. Yani bu pin hiçbir şeye bağlanmayabilir.

Yine de ayrılıyor. Ayrılmasının maliyeti başka kimsenin istemediği bir pin; ihtiyacın harness lehimlendikten sonra keşfedilmesinin maliyeti harness.

### ADC1'in son iki kanalı

ESP32-S3'te ADC1 tam olarak GPIO1-10'dur. Bu tasarım GPIO4-10'u zaten I2S, buton ve LED'e harcadı. GPIO3 ADC1_CH2'dir ama strapping pinidir. Geriye **GPIO1 ve GPIO2** kalır ve tasarımın tam olarak iki analog ölçüme ihtiyacı vardır.

Son ikisi oldukları için boş bırakılmıyor, şimdi sahipleniliyor: ileride bir sinyal birini alsaydı, bu ancak biri batarya ölçümü eklemeye çalışıp koyacak yer bulamadığında fark edilirdi.

ADC2 (GPIO11-20) eşdeğer bir yedek değil: bu parçada yalnız ADC1 sürekli/DMA denetleyicisini destekler (`SOC_ADC_DIG_SUPPORTED_UNIT`).

Bölücü oranı, NTC ağı ve her eşik `G3`/`G4` ölçümlerinden gelir. Bu ADR hiçbir kalibre değer ima etmez.

### Yasak maske eksikti — ve tek koruma oydu

`HK_PIN_FORBIDDEN_MASK` yalnız altı pini koruyordu (strapping ve native USB). Eksik olanlar: GPIO26-32 (SPI flash, 26'da PSRAM CS1), **GPIO33-37 (oktal PSRAM DQ4-DQ7 ve DQS)**, GPIO43-44 (UART0) ve S3 die'ında var olmayan GPIO22-25.

Bunun önemi, ESP-IDF'in yakalayacağını varsaymanın kolay olmasında. **Bu yapılandırmada yakalamıyor:**

`esp_mspi_pin_reserve()` (`spi_flash/flash_ops.c:160-177`) DQS ve D4-D7 girdilerini, **flash** quad olduğunda atlar. N16R8 quad flash + **oktal** PSRAM'dir. Yani PSRAM'in aktif sürdüğü beş pin hiç rezerve edilmez. Rezervasyonun olduğu yerde bile bir şey kurtarmaz: `gpio_config()` rezerve maskesine hiç bakmaz, LEDC yalnızca uyarı basıp sinyali yine de bağlar.

Yani `hk_pins.h`'deki derleme-zamanı denetimi gerçekten tek korumadır ve deliği tam da pahalı hatanın olduğu yerdeydi. `#define HK_PIN_X 35` her denetimden geçer ve yalnız açılışta başarısız olurdu.

Aynı isimli modülün farklı sonek taşıyan sürümünde (quad PSRAM, R2) bu beş pin serbesttir. Tuzak tam olarak budur.

## Eklenmeyenler ve neden

- **Şarj algılama** — [[ADR-0009-usb-c-pd-charge-chain|ADR-0009]] şarj katını ana anahtarın batarya tarafına koyuyor ve "ESP32 telemetrisinin düşük taraf yük anahtarıyla şarj katını kesmesi"ni `G4`'ün henüz seçmediği üç seçenekten biri olarak bırakıyor. Yani sezilecek tanımlı bir elektriksel sinyal henüz yok. Bir pin ayırmak, bileşen içinde sessizce bir mimari karar vermek olurdu ki `AGENTS.md` bunu ADR'siz yasaklıyor. [[ADR-0004-v1-charge-policy|ADR-0004]] kilidi o zamana kadar prosedürel kalır.
- **Hoparlör çıkışında DC sezme** — doğru cevap DC'yi hiç üretmemek: HPF, susturma sıralaması ve limiter. Bir sezme devresi, önlenmesi gereken bir arızayı ölçmek için pin ve karmaşıklık harcar.

## Sonuçlar

- `hk_pins.h` 13 GPIO tanımlar; `HK_PIN_COUNT` 13'tür.
- Derleme-zamanı denetimleri artık 18 rezerve pini ve var olmayan dört GPIO'yu reddeder. Dokuz negatif durumla sınandı.
- Üretilmiş KiCad şeması **depodan kaldırıldı**: üreteç bu değişiklikle güncellendi ama bu makinede KiCad sembol kütüphaneleri kurulu olmadığı için çıktı yeniden üretilemedi ve eldeki dosya artık yanlış pin atamasını gösteriyordu. `scripts/check_generated_kicad.py` bundan sonra üreteç ile çıktının ayrışmasını CI'da yakalar.
- SVG şeması yeniden üretildi ve on üç pini gösteriyor.
- Harici pull-down dirençleri BOM'a ve kablolama planına girmelidir; bunlar opsiyonel değil, susturma mekanizmasının kendisidir.
- Atama hâlâ `candidate`. Satın alınan kartın şeması ve bir boot testi gerekir.
