---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-09-12
tags: [adr, hardware, gpio, audio, safety]
---

# ADR-0011: Ses tarafı GPIO rezervasyonu ve yasak pin maskesinin tamamlanması

## Karar

Pin tablosuna bir GPIO eklenir ve `HK_PIN_FORBIDDEN_MASK` tamamlanır.

| Sinyal | GPIO | Ne | Durum |
|---|---|---|---|
| `DAC_XSMT` | 13 | PCM5102A `XSMT`, aktif düşük | **rezervasyon** |

Amfi için bir susturma hattı **yoktur** ve ayrılmaz: XH-A232 kartında güç girişi, ses girişi ve hoparlör çıkışları dışında hiçbir bağlantı yoktur (sahibin kart üzerindeki tespiti, 2026-09-12). TPA3110'un `SD` bacağı kart üzerinde dışarı çıkarılmamıştır. Zincirdeki tek susturma DAC'ın `XSMT`'sidir; dört amfi `VIN` geldiği andan itibaren canlıdır.

Mevcut yedi atama (GPIO4-10) **değişmez**. Hepsi geçerli ve yeniden kesmek üretilmiş şemaları ve prob noktası haritasını bedelsiz geçersiz kılardı.

Kart kararı ([[ADR-0010-esp32-s3-n16r8-board|ADR-0010]]) değişmez; o ADR zaten GPIO atamasını `candidate` olarak bırakmıştı. Bu ADR o adayı genişletir, kartı değiştirmez. Atama, satın alınan kartın kendi şeması ve bir boot testi olmadan `accepted` sayılmaz.

## Gerekçe

### Neden şimdi

Hiçbir şey lehimlenmedi ve donanım gelmedi. Bir pin eklemek bugün bedava; harness kurulduktan sonra pahalı. Bekleyip sonra ihtiyaç duymak, kabloyu değil kabloyu taşımayı gerektirir.

### Susturma hattı ESP32 tarafından tutulmaz

Bu ADR'nin en önemli maddesi ve tek güvenlik maddesi.

Bu parçadaki her aday GPIO, reset'ten çıkarken **yüksek empedanslıdır** ve çıkış sürücüsü kapalıdır; ROM, ikinci aşama bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce milisaniye. O pencerede dört amfi zaten canlıdır ve DAC'ın verdiği her şeyi üretmekte serbesttir.

Bu yüzden güvenli durum **harici bir direnç** ile tutulur, GPIO ile değil: `XSMT`'de, DAC modülünün yanında (kablonun ucunda değil) bir 10 kΩ pull-down, `R6`. Parçanın tipik 45 kΩ dahili pull'una karşı 10 kΩ, dörtten fazla kat baskındır. Bu tek direnç, amfilerin kendi susturması olmadığı için, sekiz sürücüyü firmware var olmadan önce sessiz tutan tek şeydir.

Yazılımın işi susturmayı **bırakmaktır**, yaratmak değil. Bu firmware hiç çalışmazsa DAC susturulu, hoparlörler sessiz kalır.

Şu pinler susturma görevinden **adıyla** dışlanır:

- **GPIO18, 19, 20** — silikon bunları açılışta HIGH sürer.
- **GPIO0, 39, 43, 44** — zayıf dahili pull-up ile açılırlar.

Aktif-düşük bir susturma hattı bunların herhangi birinde olsaydı, yazılım var olmadan önce DAC canlı amfilere açılırdı — empedansı hâlâ açık `G0` engeli olan sürücülere.

`GPIO14-17` bilerek boş bırakılır: serbest pinler arasında hem RTC yetenekli hem de strapping/USB/UART0/JTAG rolü olmayan tek dörtlü onlardır, yani `XSMT` ataması kart elde iken taşınmak zorunda kalırsa yedek havuzudur.

### Amfide susturma girişi yoktur

XH-A232 kartında güç girişi, ses girişi ve hoparlör çıkışları dışında bağlantı yoktur; TPA3110'un `SD` bacağı kart üzerinde erişilebilir değildir. Bu yüzden firmware kontrollü bir amfi susturması yoktur ve bunun için GPIO ayrılmaz. Sonuçları açıkça yazılır:

- Susturma sıralaması iki hat sürer: I²S saati ve `XSMT`. Açarken önce saat, saat oturunca DAC; kapatırken önce DAC susturulur ve saat DAC'ın yumuşak susturma rampası bitene kadar tutulur. Amfi için bir adım yoktur.
- TPA3110'un kendi açılış/kapanış geçişi firmware'in erişemediği bir şeydir. Adaptör takılırken ve çekilirken dört amfinin ürettiği pop `G1`'de, DAC susturuluyken, olduğu gibi kaydedilir. Duyulur bir pop `VIN` tarafında bir donanım önlemi isterse o ayrı bir ADR'dir.
- Dört kart aynı revizyon olmak zorundadır; aynı kabinde kazanç farkı duyulur.

### Yasak maske eksikti — ve tek koruma oydu

`HK_PIN_FORBIDDEN_MASK` yalnız altı pini koruyordu (strapping ve native USB). Eksik olanlar: GPIO26-32 (SPI flash, 26'da PSRAM CS1), **GPIO33-37 (oktal PSRAM DQ4-DQ7 ve DQS)**, GPIO43-44 (UART0) ve S3 die'ında var olmayan GPIO22-25.

Bunun önemi, ESP-IDF'in yakalayacağını varsaymanın kolay olmasında. **Bu yapılandırmada yakalamıyor:**

`esp_mspi_pin_reserve()` (`spi_flash/flash_ops.c:160-177`) DQS ve D4-D7 girdilerini, **flash** quad olduğunda atlar. N16R8 quad flash + **oktal** PSRAM'dir. Yani PSRAM'in aktif sürdüğü beş pin hiç rezerve edilmez. Rezervasyonun olduğu yerde bile bir şey kurtarmaz: `gpio_config()` rezerve maskesine hiç bakmaz, LEDC yalnızca uyarı basıp sinyali yine de bağlar.

Yani `hk_pins.h`'deki derleme-zamanı denetimi gerçekten tek korumadır ve deliği tam da pahalı hatanın olduğu yerdeydi. `#define HK_PIN_X 35` her denetimden geçer ve yalnız açılışta başarısız olurdu.

Aynı isimli modülün farklı sonek taşıyan sürümünde (quad PSRAM, R2) bu beş pin serbesttir. Tuzak tam olarak budur.

## Eklenmeyenler ve neden

- **Hoparlör çıkışında DC sezme** — doğru cevap DC'yi hiç üretmemek: HPF, susturma sıralaması ve limiter. Bir sezme devresi, önlenmesi gereken bir arızayı ölçmek için pin ve karmaşıklık harcar.

## Sonuçlar

- `hk_pins.h` 8 GPIO tanımlar; `HK_PIN_COUNT` 8'dir.
- Derleme-zamanı denetimleri artık 18 rezerve pini ve var olmayan dört GPIO'yu reddeder. Dokuz negatif durumla sınandı.
- Üretilmiş KiCad şeması **depodan kaldırıldı**: üreteç bu değişiklikle güncellendi ama bu makinede KiCad sembol kütüphaneleri kurulu olmadığı için çıktı yeniden üretilemedi ve eldeki dosya artık yanlış pin atamasını gösteriyordu. `scripts/check_generated_kicad.py` bundan sonra üreteç ile çıktının ayrışmasını CI'da yakalar.
- SVG şeması yeniden üretildi ve sekiz pini gösteriyor.
- Bir harici pull-down direnci (`R6`, `XSMT`) BOM'a ve kablolama planına girmelidir; opsiyonel değil, susturma mekanizmasının kendisidir. Amfi tarafında pull-down yoktur, çünkü tutulacak bir pad yoktur.
- Atama hâlâ `candidate`. Satın alınan kartın şeması ve bir boot testi gerekir.
