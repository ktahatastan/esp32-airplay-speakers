---
title: Satıcılar ve ürün adayları
tags:
  - procurement
  - suppliers
owner: procurement-researcher
status: living-document
updated: 2026-08-31
---

# Satıcılar ve ürün adayları

Bu liste bir onaylı tedarikçi listesi değil, sipariş öncesi kısa listedir. Satıcı beyanı ile üretici veri sayfası çelişirse üretici verisi esas alınır. 2026-08-30 tarihli fiyatlı sepet için [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesine]] bakın; anlık değerler kalıcı karar olarak kullanılmamalıdır.

## Kısa liste

| Grup | Öncelikli aday | Alternatif | Sipariş öncesi soru |
|---|---|---|---|
| **ESP32-S3 N16R8 — kanonik** ([[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]]) | [TLS Robotik: ESP32-S3 N16R8](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/) | Orijinal Espressif **N16R8** dağıtıcı stoğu | Kart gerçekten WROOM-1-N16R8 mi; pin dizilimi ve kart şeması sağlanabiliyor mu? |
| ESP32-S3 N8R8 — **yalnız yedek, sipariş edilmez** | [Mouser Türkiye: ESP32-S3-DevKitC-1U-N8R8](https://www.mouser.com.tr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8R8) | [Robo90: ESP32-S3-DevKitC-1-N8R8](https://www.robo90.com/esp32-s3-devkitc-1-n8r8-gelistirme-karti-orjinal) | ADR-0010 kartı N16R8'e kilitledi. Bu satır yalnız partition bütçesi 8 MB'a sığdığı kanıtlanır ve yeni bir ADR açılırsa değerlendirilir. |
| PCM5102A DAC | [Aletler: PCM5102A DAC modülü](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) | Türkiye stok bulunamazsa aynı PCB pin dizilimli ürün araştırılacak | PCB üzerindeki `SCK/BCK/LCK/DIN`, `XSMT`, `FLT`, `DEMP` bağlantıları ve line-out topolojisi nedir? |
| 5 V buck — 2 adet (A: ESP32-S3, B: PCM5102A) | [Robotistan: MP1584EN 3 A](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | Daha düşük ripple'lı otomotiv sınıfı buck araştırılacak | 24 V giriş / 5 V gerçek yükte sıcaklık ve ripple değeri; DAC hattında hışırtı? İki ayrı buck, çünkü paylaşılan buck DAC'a hışırtı verdi (ADR-0020). |
| Durum LED'i | [Robotistan: 5 mm RGB LED modülü](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) | [Robotistan: WS2812 modülü](https://www.robotistan.com/ws2812-rgb-led-modulu) | Ortak anot/katot ve GPIO sürüşü; WS2812 seçilirse 3.3 V veri seviyesi güvenilir mi? |
| Fonksiyon butonu | [Robotistan: KY-004](https://www.robotistan.com/ky-004-buton-modulu) | Kasa için panel tipi NO buton | Elektriksel ömür, panel derinliği ve titreşim dayanımı? |
| 24 V / 2,9 A DC adaptör | Aday yok | — | Yüksüz çıkış < 25,5 V mı, tolerans nedir; 2,9 A sürekli; 5,5 × 2,1 mm merkez pozitif uç; çıkış PE'ye bağlı mı, izole mi? |
| DC giriş soketi | [Direnc.net: DC-005 5.5 x 2.1](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) | Seçilen adaptöre uygun panel tipi 5.5 x 2.1 | 24 V ve 2,9 A sürekli akımda belgeli kontak değeri; merkez polaritesi? |
| 1/4 W dirençler | [Robotistan: 1/4 W direnç kategorisi](https://www.robotistan.com/14w-direnc) | [Direnc.net: 600'lü metal film set](https://www.direnc.net/1/4w-metal-film-direnc-paketi-600-adet) | 10 kΩ, 330 Ω ve 680 Ω değerleri; RGB modülünde seri direnç var mı? |
| 100 nF seramik | [Robotistan: seramik kondansatörler](https://www.robotistan.com/seramik-kondansator-1) | Aynı değerde X7R, en az 25 V | Paket adedi ve dielektrik bilgisi? |
| `C_A` bulk kondansatörü, 35 V sınıfı | Aday yok; 35 V sınıfı 105 °C düşük-ESR seri aranacak | [Direnc.net: 1000 µF / 25 V](https://www.direnc.net/1000uf25v) yalnız 25 V sınıfı örnek olarak görüldü; 24 V rayda **kullanılmaz** | ESR, ripple akımı ve 105 °C ömür sınıfı nedir? Gerilim sınıfı adaptörün yüksüz çıkışının üstünde olmalı. |
| Tweeter deney kondansatörü | [Direnc.net: 2,2 µF / 400 V kutupsuz polyester](https://www.direnc.net/22uf-400v-damla-tipi-polyester-kondansator-225mm) | [Direnc.net: kutupsuz µF kategorisi](https://www.direnc.net/kutupsuz-uf-kondansatorler) | Kapasitans toleransı ölçülebiliyor mu; aynı kabindeki dört `C_SAFE` aynı seri/lot mu? Değer G2'den önce dondurulmaz. |
| Header ve jumper | [Robotistan: 2,54 mm header](https://www.robotistan.com/header) | [Direnc.net: jumper cap](https://www.direnc.net/jumpers) | Pin aralığı 2,54 mm mi; jumper temas direnci/prototip uygunluğu? |
| Vidalı klemens | [Robotistan: KF128V 5,08 mm klemens](https://www.robotistan.com/klemens-1) | Kasa kararından sonra kilitli JST/Micro-Fit sınıfı | Akım, kablo kesiti, titreşim ve kutuplama gereksinimini karşılıyor mu? |

## Üretici ve birincil teknik kaynaklar

- [Espressif ESP32-S3 veri sayfası](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf): Wi-Fi, BLE, I2S ve bellek/çevrebirim yetenekleri.
- [Espressif ESP32-S3-WROOM-1 veri sayfası](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): `N8R8` ve anten varyantları.
- [Texas Instruments PCM5102A veri sayfası](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf): 32-bit/384 kHz PCM arayüzü, 2.1 Vrms çıkış ve 112 dB SNR sınıfı teknik sınırlar.
- [Texas Instruments TPA3110D2 veri sayfası](https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf): 8-26 V besleme; 16 V'ta 8 ohm yüke 15 W/kanal değeri %10 THD+N koşulundadır.
- [Monolithic Power MP1584 veri sayfası](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584/document_id/204/): buck regülatörün gerçek sınırları; modül kalitesi ayrıca ölçülmelidir.

## Tedarik ilkeleri

- Dört amfi kartı aynı revizyondur; aynı kabinde kazanç farkı duyulur ve `SD` dalı ya dördünde ya hiçbirinde takılır. Revizyon değişirse BOM revizyonu ve test tekrarı gerekir.
- `30 W`, `40 A`, `3 A` gibi pazarlama başlıkları tek başına kabul kriteri değildir; çalışma koşulu ve ısıl performans aranır.
- Stokta görünen ürün satın alma anında yeniden kontrol edilir; toplu siparişten önce bir örnek doğrulanır.

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[research-log|Araştırma günlüğü]]
