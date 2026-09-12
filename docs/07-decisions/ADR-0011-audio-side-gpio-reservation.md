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

Pin tablosuna iki GPIO eklenir ve `HK_PIN_FORBIDDEN_MASK` tamamlanır.

| Sinyal | GPIO | Ne | Durum |
|---|---|---|---|
| `AMP_MUTE` | 21 | dört TPA3110'un `SD` girişi paralel, aktif düşük | **rezervasyon** |
| `DAC_XSMT` | 13 | PCM5102A `XSMT`, aktif düşük | **rezervasyon** |

Mevcut yedi atama (GPIO4-10) **değişmez**. Hepsi geçerli ve yeniden kesmek üretilmiş şemaları ve prob noktası haritasını bedelsiz geçersiz kılardı.

Kart kararı ([[ADR-0010-esp32-s3-n16r8-board|ADR-0010]]) değişmez; o ADR zaten GPIO atamasını `candidate` olarak bırakmıştı. Bu ADR o adayı genişletir, kartı değiştirmez. Atama, satın alınan kartın kendi şeması ve bir boot testi olmadan `accepted` sayılmaz.

## Gerekçe

### Neden şimdi

Hiçbir şey lehimlenmedi ve donanım gelmedi. Bir pin eklemek bugün bedava; harness kurulduktan sonra pahalı. Bekleyip sonra ihtiyaç duymak, kabloyu değil kabloyu taşımayı gerektirir.

### Susturma hattı ESP32 tarafından tutulmaz

Bu ADR'nin en önemli maddesi ve tek güvenlik maddesi.

Bu parçadaki her aday GPIO, reset'ten çıkarken **yüksek empedanslıdır** ve çıkış sürücüsü kapalıdır; ROM, ikinci aşama bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce milisaniye. O pencerede amfi, girişindeki her şeyi üretmekte serbesttir.

Bu yüzden güvenli durum **harici bir direnç** ile tutulur, GPIO ile değil: dört amfinin her birinin `SD` pad'inde kendi 10 kΩ pull-down'ı (amfinin yanında, kablonun ucunda değil) ve `XSMT`'de bir 10 kΩ pull-down — beş direnç. Parçanın tipik 45 kΩ dahili pull'una karşı 10 kΩ, dörtten fazla kat baskındır.

Tek GPIO dört `SD` girişini paralel sürer. Dört adet 10 kΩ pull-down paralelde 2,5 kΩ'dur; GPIO'nun HIGH sürerken gördüğü yük budur ve sürücü kapasitesinin içindedir. Bu bir aritmetiktir; `G1` HIGH seviyesini dört amfi bağlıyken ölçer.

Yazılımın işi susturmayı **bırakmaktır**, yaratmak değil. Bu firmware hiç çalışmazsa hoparlörler sessiz kalır.

Şu pinler susturma görevinden **adıyla** dışlanır:

- **GPIO18, 19, 20** — silikon bunları açılışta HIGH sürer.
- **GPIO0, 39, 43, 44** — zayıf dahili pull-up ile açılırlar.

Aktif-düşük bir susturma hattı bunların herhangi birinde olsaydı, yazılım var olmadan önce amfi serbest kalırdı — empedansı hâlâ açık `G0` engeli olan sürücülere.

GPIO21 seçildi çünkü bu modülde reset'te veya sonrasında dahili pull'u olmayan, açılış glitch tablosunda yer almayan, strapping/USB/UART0/JTAG rolü bulunmayan ve `gpio_hold_en()` derin uykuda çalışsın diye RTC yetenekli tek pin. İkinci ve üçüncü tercihler GPIO40 (MTDO) ve GPIO42 (MTMS).

### `AMP_MUTE` bir rezervasyon, bağlantı değil

XH-A232 kartlarının erişilebilir bir `SD` pad'i olup olmadığı [[../02-hardware/circuit-and-wiring-plan|kablolama planında]] hâlâ açık bir karar. Yani bu pin hiçbir şeye bağlanmayabilir. Dört kart aynı revizyon olmalıdır; `SD` dalı ya dördünde ya hiçbirinde takılıdır.

Yine de ayrılıyor. Ayrılmasının maliyeti başka kimsenin istemediği bir pin; ihtiyacın harness lehimlendikten sonra keşfedilmesinin maliyeti harness.

### Yasak maske eksikti — ve tek koruma oydu

`HK_PIN_FORBIDDEN_MASK` yalnız altı pini koruyordu (strapping ve native USB). Eksik olanlar: GPIO26-32 (SPI flash, 26'da PSRAM CS1), **GPIO33-37 (oktal PSRAM DQ4-DQ7 ve DQS)**, GPIO43-44 (UART0) ve S3 die'ında var olmayan GPIO22-25.

Bunun önemi, ESP-IDF'in yakalayacağını varsaymanın kolay olmasında. **Bu yapılandırmada yakalamıyor:**

`esp_mspi_pin_reserve()` (`spi_flash/flash_ops.c:160-177`) DQS ve D4-D7 girdilerini, **flash** quad olduğunda atlar. N16R8 quad flash + **oktal** PSRAM'dir. Yani PSRAM'in aktif sürdüğü beş pin hiç rezerve edilmez. Rezervasyonun olduğu yerde bile bir şey kurtarmaz: `gpio_config()` rezerve maskesine hiç bakmaz, LEDC yalnızca uyarı basıp sinyali yine de bağlar.

Yani `hk_pins.h`'deki derleme-zamanı denetimi gerçekten tek korumadır ve deliği tam da pahalı hatanın olduğu yerdeydi. `#define HK_PIN_X 35` her denetimden geçer ve yalnız açılışta başarısız olurdu.

Aynı isimli modülün farklı sonek taşıyan sürümünde (quad PSRAM, R2) bu beş pin serbesttir. Tuzak tam olarak budur.

## Eklenmeyenler ve neden

- **Hoparlör çıkışında DC sezme** — doğru cevap DC'yi hiç üretmemek: HPF, susturma sıralaması ve limiter. Bir sezme devresi, önlenmesi gereken bir arızayı ölçmek için pin ve karmaşıklık harcar.

## Sonuçlar

- `hk_pins.h` 9 GPIO tanımlar; `HK_PIN_COUNT` 9'dur.
- Derleme-zamanı denetimleri artık 18 rezerve pini ve var olmayan dört GPIO'yu reddeder. Dokuz negatif durumla sınandı.
- Üretilmiş KiCad şeması **depodan kaldırıldı**: üreteç bu değişiklikle güncellendi ama bu makinede KiCad sembol kütüphaneleri kurulu olmadığı için çıktı yeniden üretilemedi ve eldeki dosya artık yanlış pin atamasını gösteriyordu. `scripts/check_generated_kicad.py` bundan sonra üreteç ile çıktının ayrışmasını CI'da yakalar.
- SVG şeması yeniden üretildi ve dokuz pini gösteriyor.
- Beş harici pull-down direnci (dört `SD`, bir `XSMT`) BOM'a ve kablolama planına girmelidir; bunlar opsiyonel değil, susturma mekanizmasının kendisidir.
- Atama hâlâ `candidate`. Satın alınan kartın şeması ve bir boot testi gerekir.
