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

| Sinyal | GPIO | Neden bu pin |
|---|---:|---|
| `SCK` | 47 | Strapping/USB/UART0/JTAG rolü yok, RTC yetenekli değil — yani susturma yedeği olarak zaten değersiz |
| `MOSI` | 41 | `MTDI` |
| `CS` | 39 | `MTCK`, ve silikon bu pini zayıf dahili pull-up ile açıyor: panel, ROM ve bootloader boyunca seçilmemiş kalıyor. Yine de harici 10 kΩ pull-up konuyor, çünkü doğrulanmamış bir dahili değere güvenmek gerekmesin |
| `DC` | 40 | `MTDO` |
| `RST` | 18 | Silikon açılışta **yüksek** sürüyor. Aktif-düşük reset için bu güvenli seviye, ve GPIO18'i susturma görevinden men eden kusurun işe yaradığı tek yer |
| `BL` | 42 | `MTMS`. LEDC timer 1; `hk_ui` timer 0 ve 0-2 kanallarını tutuyor |

`SPI3_HOST` seçildi: SPI2'nin IO_MUX pinleri `GPIO9-14` ve altısının beşi zaten dolu, yani SPI2'nin hız avantajına buradan ulaşılamıyor. SPI3'ün IO_MUX pini yok, dolayısıyla kaybedilen bir şey de yok.

### Bedeli: pad-JTAG tümüyle gidiyor

`GPIO39-42` = `MTCK`/`MTDO`/`MTDI`/`MTMS`. Harici JTAG probuyla hata ayıklama kalmıyor. `GPIO19/20` üzerindeki USB Serial/JTAG duruyor ve zaten bu yapının ikincil konsolu — kaybedilen prob, hata ayıklayıcı değil.

Bu bedel kabul ediliyor, ama **`GPIO14-17` bilerek boş bırakılıyor**. Serbest pinler arasında hem RTC yetenekli hem de strapping/USB/UART0/JTAG rolü taşımayan tek dörtlü onlar, yani susturma hatlarının gerçek yedek havuzu. ADR-0011 yedek olarak `GPIO40/42`'yi adlandırmıştı; ikisi de o ADR'nin kendi ölçütlerinden ikisini karşılamıyor (JTAG rolü var, RTC yetenekli değiller). Bu ADR yedek havuzu **güçlendiriyor**, zayıflatmıyor.

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
- Arka ışık doğrudan GPIO'dan değil, 2N7002 üzerinden. Bazı modül varyantları `BLK`'yı doğrudan LED anoduna, bazıları bir sürücü girişine bağlıyor; FET ikisini de karşılıyor. Kapıdaki 10 kΩ pull-down, firmware çalışmadan önce arka ışığın sönük kalmasını sağlayan mekanizmadır.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| `GPIO14-17`'yi ekrana vermek | Susturma hatlarının tek gerçek yedek havuzu. Bir güvenlik yedeğini bir kolaylığa çevirmek. |
| SPI2 kullanmak | IO_MUX pinlerinin beşi dolu; hız avantajı zaten ulaşılamaz, ve SPI2 ileride o pinlere erişebilen bir cihaz için boşta kalsın. |
| LVGL | Birkaç ekran, bir animasyon ve bir marquee için bütün bir grafik yığını. Doğrudan çizim yeter; gerekirse sonra eklenir, tersi zordur. |
| Ekranı ayrı bir durum makinesiyle sürmek | Göstergenin takılabileceği durum tam olarak budur. |

## Doğrulama

Hiçbiri yapılmadı; ekran henüz hiçbir karta bağlanmadı.

- Panelin açılması, `240×240` tam kare ve alt-pencere çizimi.
- Kare süresi ve bunun ses görevine etkisi (`G3`).
- Arka ışığın firmware çalışmadan önce sönük olduğu.
- Panel + arka ışık akımı, ve 3V3 rayındaki gürültünün `BATT_SENSE` okumasına etkisi.
- `CS`'in ROM ve bootloader boyunca gerçekten boşta kaldığı.
