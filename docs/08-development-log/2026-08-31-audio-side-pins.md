---
title: Ses tarafı GPIO'ları eklendi ve yasak pin maskesindeki delik kapatıldı
status: partial
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-08-31
tags: [development-log, hardware, gpio, safety, f2, f3, f6]
---

# 2026-08-31 — Ses tarafı pinleri

## Neden şimdi

Hiçbir şey lehimlenmedi. Bir pin eklemek bugün bedava, harness kurulduktan sonra pahalı — analog bir kanalda ise kabloyu değil bölücüyü taşımak demek.

## Beklemediğim bulgu: yasak maskede delik vardı

`HK_PIN_FORBIDDEN_MASK` yalnız altı pini koruyordu. Eksik olanlar SPI flash (`GPIO26-32`), **oktal PSRAM** (`GPIO33-37`), UART0 konsolu (`GPIO43/44`) ve S3 die'ında hiç var olmayan `GPIO22-25`.

Bunun önemsiz görünmesinin sebebi, ESP-IDF'in yakalayacağını varsaymak. **Bu yapılandırmada yakalamıyor** ve kaynağı okuyup doğruladım:

```c
/* spi_flash/flash_ops.c:166 */
if (!bootloader_flash_is_octal_mode_enabled()
    && i >= ESP_MSPI_IO_DQS && i <= ESP_MSPI_IO_D7) {
    continue;
}
```

Soru **flash** oktal mı diye soruyor. N16R8 quad flash + **oktal PSRAM**. Yani PSRAM'in aktif sürdüğü beş pin rezervasyondan atlanıyor. Üstelik rezervasyonun olduğu yerde de bir şey değişmiyor: `gpio_config()` rezerve maskesine hiç bakmaz, LEDC yalnız uyarı basıp sinyali yine de bağlar.

Yani `hk_pins.h`'deki derleme-zamanı denetimi gerçekten **tek** koruma ve deliği tam da pahalı hatanın olduğu yerdeydi. `#define HK_PIN_X 35` her denetimden geçer, yalnız açılışta ölürdü.

Aynı modülün quad-PSRAM sürümünde (R2) o beş pin serbest. Tuzak tam olarak bu: aynı isim, farklı sonek, farklı cevap.

Dokuz negatif durumla sınadım — oktal PSRAM DQ6/DQS, flash MOSI, PSRAM CS1, UART0 TX, var olmayan GPIO24, strapping, native USB ve çakışma. Dokuzu da reddedildi.

## Eklenen dört pin

| Sinyal | GPIO | Not |
|---|---|---|
| `AMP_MUTE` | 21 | TPA3110 `SD`, aktif düşük |
| `DAC_XSMT` | 13 | PCM5102A `XSMT`, aktif düşük |
| `BATT_SENSE` | 1 | ADC1_CH0 |
| `NTC_SENSE` | 2 | ADC1_CH1 |

Mevcut dokuzu (GPIO4-12) değiştirmedim: hepsi geçerli ve yeniden kesmek üretilmiş şemaları ve prob haritasını bedelsiz geçersiz kılardı.

## Asıl güvenlik maddesi: susturmayı GPIO tutmaz

Bu parçadaki her aday GPIO reset'ten **yüksek empedanslı** çıkar ve ROM, bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce milisaniye. O pencerede amfi girişindeki her şeyi üretmekte serbesttir.

Bu yüzden güvenli durumu **harici 10 kΩ pull-down** tutuyor, GPIO değil. Parçanın tipik 45 kΩ dahili pull'una karşı dörtten fazla kat baskın. Dirençler BOM'a `R_MUTE` olarak girdi ve "opsiyonel değil" diye işaretlendi.

Yazılımın işi susturmayı **bırakmak**. Bu firmware hiç çalışmazsa hoparlörler sessiz kalır.

`GPIO18/19/20` susturma görevinden adıyla dışlandı — silikon bunları açılışta HIGH sürüyor. `GPIO0/39/43/44` de dışlandı, zayıf dahili pull-up ile açılıyorlar. Herhangi biri aktif-düşük bir susturma hattında olsaydı, yazılım var olmadan amfiyi serbest bırakırdı; empedansı hâlâ açık `G0` engeli olan sürücülere. Bu, statik denetimlerin yakalayamayacağı bir şey — 18 ve 39 tamamen geçerli atamalar, sadece burada yanlış — o yüzden teste bağlandı.

## ADC1'in son iki kanalı

ADC1 bu parçada tam olarak GPIO1-10 ve tasarım GPIO4-10'u zaten harcamıştı. GPIO3 ADC1_CH2 ama strapping. Geriye **GPIO1 ve GPIO2** kalıyordu; ihtiyaç tam olarak iki analog ölçüm.

Son ikisi oldukları için sahiplendim. İleride bir sinyal birini alsaydı, bu ancak biri batarya ölçümü eklemeye çalışıp yer bulamadığında fark edilirdi.

## Eklemediklerim

- **Şarj algılama** — ADR-0009 topolojiyi hâlâ açık bırakıyor; sezilecek tanımlı bir sinyal yok. Pin ayırmak, bileşen içinde sessizce mimari karar vermek olurdu.
- **DC sezme** — doğru cevap DC'yi hiç üretmemek. Sezme devresi, önlenmesi gereken bir arızayı ölçmek için pin harcar.

## Üç pin tablosu artık uyuşuyor

Denetim, `hk_pins.h`, SVG ve KiCad üretecinin birbirinden farklı olduğunu bulmuştu: KiCad U5'te 13 pin, SVG'de 10, başlıkta 9. `hk_pins.h`'in kendi eşitlik bozma kuralı ("belge ve çizim kazanır") tanımsızdı, çünkü iki çizim birbiriyle çelişiyordu. Üçünü de 13'e eşitledim ve programatik olarak karşılaştırdım: eksik yok, fazla yok.

## KiCad şeması depodan kaldırıldı

Üreteci güncelledim ama bu makinede KiCad sembol kütüphaneleri kurulu olmadığı için çıktıyı yeniden üretemedim. Elde kalan `.kicad_sch` artık **yanlış** pin atamasını gösteriyordu.

Yanlış bir şemayı tutmak hiç tutmamaktan kötü: bayat olduğu anlaşılamayan bir çizimden birisi lehim yapar. Üreteç kaynak, `.kicad_sch` türev — türevi sildim.

`scripts/check_generated_kicad.py` bundan sonra üretecin hash'ini çıktının yanında tutuyor ve ayrışmayı CI'da yakalıyor. Yani bu bir daha sessizce olamaz.

## Yan bulgu

`check_docs.py` yeni açtığım venv içindeki 18 paket README'sini de taramaya başlamıştı. `SKIP_DIRS`'e `.venv`, `venv`, `site-packages` ve `__pycache__` eklendi.

## Doğrulama

103 belge / 0 hata · 10697 ana makine kontrolü / 0 hata · dokuz negatif pin durumu reddedildi · üç pin tablosu programatik olarak eşit · SVG yeniden üretildi ve dört yeni pini gösteriyor · firmware uyarısız derleniyor.

## Açık kalanlar

- Pin ataması hâlâ `candidate`: satın alınan kartın şeması ve boot testi gerekiyor.
- XH-A232'de erişilebilir `SD` pad'i olup olmadığı hâlâ açık; `AMP_MUTE` bir rezervasyon.
- KiCad şeması, KiCad kurulunca iki komutla geri gelir.
