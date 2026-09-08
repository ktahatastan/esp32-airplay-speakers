---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer, verifier]
updated: 2026-09-08
tags: [adr, display, gpio, ui, spi]
---

# ADR-0017: Yuvarlak GC9A01 ekran eklenir

## Bağlam

Sahibi bir **GC9A01 240×240 yuvarlak IPS** modül aldı ve ürüne girmesini istiyor: eşleşmede karekod, cihaz kimliği ve kurulum PIN'i; eşleştikten sonra şarj durumu ve AirPlay'den gelen çalma bilgisi.

Bugüne kadar cihazın tek göstergesi [[ADR-0011-audio-side-gpio-reservation|ADR-0011]]'in ayırdığı üç PWM kanalıydı, ve o LED henüz hiçbir karta lehimlenmedi. Yani cihaz durumunu bugün yalnız seri konsoldan söylüyor.

Ekran, kullanıcının **PIN'i ve karekodu nereden okuyacağı** sorusunu da çözüyor. [[ADR-0015-softap-captive-portal|ADR-0015]] kurulum ağını WPA2 yapmıştı ve bunun bedeli kullanıcının bir parola girmesiydi; parola ekranda yazınca o bedel kalkıyor, güvenlik özelliği duruyor.

## Karar

Ekran ürüne girer, SPI3 üzerinden sürülür, ve **ikinci bir doğruluk kaynağı değildir.**

### Pin ataması

Elimizdeki modül **yedi pinli**: `RST · CS · DC · SDA · SCL · GND · VCC`. Tablo o sırayla yazıldı, ki elindeki parçaya karşı okunabilsin.

| Modül ucu | GPIO | Neden bu pin |
|---|---:|---|
| `RST` | 18 | Silikon açılışta **yüksek** sürüyor. Aktif-düşük reset için bu güvenli seviye, ve GPIO18'i susturma görevinden men eden kusurun işe yaradığı tek yer |
| `CS` | 39 | `MTCK`, ve silikon bu pini zayıf dahili pull-up ile açıyor: panel, ROM ve bootloader boyunca seçilmemiş kalıyor |
| `DC` | 40 | `MTDO` |
| `SDA` | 41 | `MTDI`. Tek yön; `SDO` bağlanmıyor |
| `SCL` | 47 | Strapping/USB/UART0/JTAG rolü yok, RTC yetenekli değil — yani susturma yedeği olarak zaten değersiz |
| `GND`, `VCC` | — | Ayrı LDO, aşağıya bakın |

**Arka ışık ucu yok.** Yedi pinli varyantta panel `VCC` ile birlikte yanıyor. Üç sonucu var ve üçü de kabul ediliyor: firmware çalışmadan önce ekranı karartmanın yolu yok, parlaklık ayarı yok, ve panel akımı sürekli. Bunun karşılığında `GPIO42` serbest kaldı.

`SPI3_HOST` seçildi: SPI2'nin IO_MUX pinleri `GPIO9-14` ve altısının beşi zaten dolu, yani SPI2'nin hız avantajına buradan ulaşılamıyor. SPI3'ün IO_MUX pini yok, dolayısıyla kaybedilen bir şey de yok.

### Bedeli: pad-JTAG tümüyle gidiyor

`GPIO39`/`GPIO40`/`GPIO41` = `MTCK`/`MTDO`/`MTDI`. JTAG dördünün dördünü birden istediği için üçünü almak yeterli: harici probla hata ayıklama kalmıyor. `GPIO19/20` üzerindeki USB Serial/JTAG duruyor ve zaten bu yapının ikincil konsolu — kaybedilen prob, hata ayıklayıcı değil.

Bu bedel kabul ediliyor, ama **`GPIO14-17` bilerek boş bırakılıyor**. Serbest pinler arasında hem RTC yetenekli hem de strapping/USB/UART0/JTAG rolü taşımayan tek dörtlü onlar, yani susturma hatlarının gerçek yedek havuzu. ADR-0011 yedek olarak `GPIO40/42`'yi adlandırmıştı; ikisi de o ADR'nin kendi ölçütlerinden ikisini karşılamıyor (JTAG rolü var, RTC yetenekli değiller). Bu ADR yedek havuzu **güçlendiriyor**, zayıflatmıyor.

### Tezgâh kablolaması ürün kablolaması değil

Elde seri direnç paketi yok, o yüzden tezgâhta sinyaller **doğrudan** ESP'ye bağlanıyor ve `CS` pull-up'ı takılmıyor. İkisi de üründe geri geliyor ve sebepleri şunlar:

- **4 × 33 Ω seri, ESP ucunda.** Uçan kablolarda kenar hızını yavaşlatıyor, ve bu kablolar analog ses yolunun yanından geçiyor. Tezgâhta ses yolu bağlı olmadığı için ölçülebilir bir zararı yok.
- **`CS` üzerinde 10 kΩ pull-up.** `GPIO39`'un zayıf dahili pull-up'ı açılış penceresini kapatıyor, ama o değer doğrulanmadı. Tezgâhta bir yanlış `CS` iddiası yalnız panele çöp gösterir; üründe buna güvenmek kalıcı bir çözüm değil.

### Ekranın kendi durumu yoktur

Ekran, LED'i çözen aynı fonksiyondan besleniyor (`hk_led_resolve`). Sebep estetik değil: komut dizisiyle sürülen bir gösterge, tanımlanmamış bir geçişte eski ekranda kalır — ve bunun en olası yeri ağ sıfırlamasından sonrasıdır. Türetilmiş bir gösterge orada kalamaz, çünkü gösterecek ayrı bir durumu yoktur.

Aynı sebeple ekran ve LED birbiriyle çelişemez: ikisi de aynı çözümün çıktısı.

### Ses yolundan uzak durur

Ekran görevi `hk_ui`'nin önceliğinde ya da altında, ses görevinden ayrı çekirdekte. [[ADR-0007-airplay-stack|ADR-0007]] odalar arası senkrona `≤1 ms` veriyor ve bir SPI aktarımı o bütçeyi harcayamaz. `esp_lcd_panel_draw_bitmap()` kuyruğa atıp dönüyor, yani kaynak tampon aktarım bitene kadar yaşamak zorunda: dönüşümlü iki şerit tamponu, dahili DMA'lı RAM'de.

## Sonuçlar

- `hk_pins.h` altı pin daha taşıyor; `HK_PIN_COUNT` 13'ten 19'a çıktı ve derleme zamanı denetimleri (çakışma, yasak maske) bunlara da uygulanıyor.
- Güç bütçesine bölünmemiş bir yük giriyor: panel + arka ışık için **≤50 mA**, ve beslemesi ESP modülünün kendi 3V3'ü değil, MP1584 rayından ayrı bir LDO. Sebep `BATT_SENSE`/`NTC_SENSE`'in referans aldığı rayı panelin anahtarlama yükünden uzak tutmak; `G3` gürültü kapısı bunu ölçecek.
- Modül **5 V toleranslı değil.** Üstünde LDO olan varyantlarda bile mantık 3,3 V.
- `SDO` bağlanmıyor: 4 telli SPI'da geri okunan bir şey yok.
- Arka ışık sürekli yanıyor. Sekiz pinli bir varyanta geçilirse `BLK` bir FET üzerinden sürülür ve kapıdaki pull-down, firmware çalışmadan önce ekranı karartan mekanizma olur; bugün öyle bir mekanizma yok.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| `GPIO14-17`'yi ekrana vermek | Susturma hatlarının tek gerçek yedek havuzu. Bir güvenlik yedeğini bir kolaylığa çevirmek. |
| SPI2 kullanmak | IO_MUX pinlerinin beşi dolu; hız avantajı zaten ulaşılamaz, ve SPI2 ileride o pinlere erişebilen bir cihaz için boşta kalsın. |
| LVGL | Birkaç ekran, bir animasyon ve bir marquee için bütün bir grafik yığını. Doğrudan çizim yeter; gerekirse sonra eklenir, tersi zordur. |
| Sekiz pinli varyantı beklemek | Elde olan yedi pinli. Arka ışık kontrolü olmaması ölçülebilir bir eksik ama bring-up'ı durdurmuyor; gerekirse varyant değişir ve `GPIO42` zaten boş duruyor. |
| Ekranı ayrı bir durum makinesiyle sürmek | Göstergenin takılabileceği durum tam olarak budur. |

## Doğrulama

Hiçbiri yapılmadı; ekran henüz hiçbir karta bağlanmadı.

- Panelin açılması, `240×240` tam kare ve alt-pencere çizimi.
- Kare süresi ve bunun ses görevine etkisi (`G3`).
- Panel + arka ışık akımının ne olduğu, ve 3V3 rayındaki gürültünün `BATT_SENSE` okumasına etkisi.
- Seri direnç olmadan kenar hızının analog yola ne yaptığı — tezgâhta ses yolu bağlanınca.
- `CS`'in ROM ve bootloader boyunca gerçekten boşta kaldığı.
