---
title: Satıcılar ve ürün adayları
tags:
  - procurement
  - suppliers
status: living-document
updated: 2026-08-30
---

# Satıcılar ve ürün adayları

Bu liste bir onaylı tedarikçi listesi değil, sipariş öncesi kısa listedir. Satıcı beyanı ile üretici veri sayfası çelişirse üretici verisi esas alınır. 2026-08-30 tarihli fiyatlı sepet için [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesine]] bakın; anlık değerler kalıcı karar olarak kullanılmamalıdır.

## Kısa liste

| Grup | Öncelikli aday | Alternatif | Sipariş öncesi soru |
|---|---|---|---|
| ESP32-S3 N16R8 | [TLS Robotik: ESP32-S3 N16R8](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/) | Orijinal Espressif N8R8/N16R8 dağıtıcı stoğu | Kart gerçekten WROOM-1-N16R8 mi; pin dizilimi ve kart şeması sağlanabiliyor mu? |
| ESP32-S3 N8R8 | [Mouser Türkiye: ESP32-S3-DevKitC-1U-N8R8](https://www.mouser.com.tr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8R8) | [Robo90: ESP32-S3-DevKitC-1-N8R8](https://www.robo90.com/esp32-s3-devkitc-1-n8r8-gelistirme-karti-orjinal) | Kart gerçekten N8R8 mi? `1U` ise IPEX anten dahil mi, ayrıca anten alınmalı mı? |
| PCM5102A DAC | [Aletler: PCM5102A DAC modülü](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) | Türkiye stok bulunamazsa aynı PCB pin dizilimli ürün araştırılacak | PCB üzerindeki `SCK/BCK/LCK/DIN`, `XSMT`, `FLT`, `DEMP` bağlantıları ve line-out topolojisi nedir? |
| 5 V buck | [Robotistan: MP1584EN 3 A](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | Daha düşük ripple'lı otomotiv sınıfı buck araştırılacak | 14.8 V giriş / 5 V gerçek yükte sıcaklık ve ripple değeri? |
| Li-ion hücre | [Pilpaketi: Aspilsan INR18650A28](https://www.pilpaketi.com/aspilsan-pil) | Molicel P28A yalnız ikincil alternatif | 16/32 adet aynı üretim partisi sağlanabiliyor mu; kapasite/iç direnç eşleme ve punta paket hizmeti var mı? |
| 4S balanced BMS | [Pilburada: HXYP 4S 12 A](https://www.pilburada.com/hxyp-4s-148v-12a-li-ion-bms-devre-85187) | [Elektropil: OLT 4S 40 A balanslı](https://www.elektropil.com/olt-4s-40a-168v-balansli-lityum-batarya-bms--mor-pcb--m031125) | Gerçek sürekli akım, balans başlangıç voltajı/akımı, MOSFET tipi, NTC desteği ve bağlantı sırası? |
| USB-C PD tetikleyici | [Meltis: 5-20 V PD/QC/AFC seçici](https://www.meltisteknoloji.com/pd-qc-afc-hizli-sarj-adaptorunden-voltaj-tetikleyici-secici-modul-1290) | Aynı protokolleri destekleyen sabit 20 V tetikleyici | 20 V profili yüksüz ve yük altında kararlı mı; kart revizyonu aynı mı? |
| 16.8 V CC/CV şarj katı | [Robotistan: XL4015 akım/voltaj ayarlı](https://www.robotistan.com/xl4015-lipo-sarj-modulu) | [Sanec: XL4015 CC/CV](https://www.sanec.net/xl4015-akim-voltaj-ayarli-dc-dc-voltaj-dusurucu-5a-lipo-sarj-559) | 20 V girişten 16,8 V / 2 A'de ısıl kararlılık; ters polarite koruması var mı? |
| USB-C adaptör + kablo | [Trendyol: Syrox GAN65T 65 W set](https://www.trendyol.com/syrox/gan65-type-c-usb-giris-baslik-type-c-kablo-65w-super-hizli-quick-sarj-set-p-793024101) | [MediaMarkt: Konfulon C99Q 65 W](https://www.mediamarkt.com.tr/tr/product/_konfulon-c99q-65w-pd-destekli-type-c-ve-usb-cift-cikisli-hizli-sarj-adaptoru-beyaz-172794171.html) + doğrulanmış 5 A kablo | Type-C tek başına 20 V / 3,25 A veriyor mu; kutu kablosu 5 A e-marker mı; seri/CE ve garanti bilgisi doğrulanabiliyor mu? |
| 16.8 V / 2 A şarj | [Pil Servisi: 16.8 V 2 A](https://pilservisi.com.tr/urun/16-8v-2a-li-ion-sarj-adaptoru/) | [Pil Mağazası: WEKO 16.8 V 2 A](https://pilmagazasi.com/products/weko-16-8v-2a-4s-li-ion-batarya-sarj-adaptoru-priz-tipi) | CC/CV profili, sonlandırma akımı, uç ölçüsü/polaritesi ve güvenlik belgeleri? |
| 16.8 V / 3 A şarj | [Pilpaketi: WEKO 3 A](https://www.pilpaketi.com/lion-sarj-aleti-4s-16.8-volt-3a-2.1-mm-soket) | [Pilburada: KA 16.8 V 3 A](https://www.pilburada.com/ka-168v-3a-li-ion-sarj-adaptor-94819) | Stok, CC/CV profili ve 4S1P için 3 A'in hücre üretici limitleriyle uygunluğu? |
| Durum LED'i | [Robotistan: 5 mm RGB LED modülü](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) | [Robotistan: WS2812 modülü](https://www.robotistan.com/ws2812-rgb-led-modulu) | Ortak anot/katot ve GPIO sürüşü; WS2812 seçilirse 3.3 V veri seviyesi güvenilir mi? |
| Fonksiyon butonu | [Robotistan: KY-004](https://www.robotistan.com/ky-004-buton-modulu) | Kasa için panel tipi NO buton | Elektriksel ömür, panel derinliği ve titreşim dayanımı? |
| Sert güç anahtarı | [Direnc.net: 16 mm mandallı metal anahtar](https://www.direnc.net/16mm-ledli-12-24v-su-gecirmez-metal-anahtar) | Anahtar + ayrı yüksek akım MOSFET/load-switch topolojisi | Kontak 16.8 V DC'de en az 5 A'i güvenle kesebiliyor mu? Sayfadaki 5 A değeri 12 V DC içindir. |
| Şarj soketi | [Direnc.net: DC-005 5.5 x 2.1](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) | Seçilen adaptöre uygun panel tipi 5.5 x 2.5 | Kontak akımı ve uç ölçüsü/polaritesi? |
| 1/4 W dirençler | [Robotistan: 1/4 W direnç kategorisi](https://www.robotistan.com/14w-direnc) | [Direnc.net: 600'lü metal film set](https://www.direnc.net/1/4w-metal-film-direnc-paketi-600-adet) | 10 kΩ, 330 Ω ve 680 Ω değerleri; RGB modülünde seri direnç var mı? |
| 100 nF seramik | [Robotistan: seramik kondansatörler](https://www.robotistan.com/seramik-kondansator-1) | Aynı değerde X7R, en az 25 V | Paket adedi ve dielektrik bilgisi? |
| Amfi bulk kondansatörü | [Direnc.net: 1000 µF / 25 V](https://www.direnc.net/1000uf25v) | Markalı 105 °C düşük-ESR seri | ESR, ripple akımı ve 105 °C ömür sınıfı nedir? Perakende aday nihai onay değildir. |
| Tweeter deney kondansatörü | [Direnc.net: 2,2 µF / 400 V kutupsuz polyester](https://www.direnc.net/22uf-400v-damla-tipi-polyester-kondansator-225mm) | [Direnc.net: kutupsuz µF kategorisi](https://www.direnc.net/kutupsuz-uf-kondansatorler) | Kapasitans toleransı ölçülebiliyor mu; dört nihai parça aynı seri/lot mu? Değer G2'den önce dondurulmaz. |
| Header ve jumper | [Robotistan: 2,54 mm header](https://www.robotistan.com/header) | [Direnc.net: jumper cap](https://www.direnc.net/jumpers) | Pin aralığı 2,54 mm mi; jumper temas direnci/prototip uygunluğu? |
| Vidalı klemens | [Robotistan: KF128V 5,08 mm klemens](https://www.robotistan.com/klemens-1) | Kasa kararından sonra kilitli JST/Micro-Fit sınıfı | Akım, kablo kesiti, titreşim ve kutuplama gereksinimini karşılıyor mu? |

## Üretici ve birincil teknik kaynaklar

- [Espressif ESP32-S3 veri sayfası](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf): Wi-Fi, BLE, I2S ve bellek/çevrebirim yetenekleri.
- [Espressif ESP32-S3-WROOM-1 veri sayfası](https://www.espressif.com/sites/default/files/documentation/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf): `N8R8` ve anten varyantları.
- [Texas Instruments PCM5102A veri sayfası](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf): 32-bit/384 kHz PCM arayüzü, 2.1 Vrms çıkış ve 112 dB SNR sınıfı teknik sınırlar.
- [Texas Instruments TPA3110D2 veri sayfası](https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf): 8-26 V besleme; 16 V'ta 8 ohm yüke 15 W/kanal değeri %10 THD+N koşulundadır.
- [Aspilsan INR18650A28 üretici veri sayfası](https://www.aspilsan.com/wp-content/uploads/2025/05/A28_Public_Datasheet_.pdf): 3,65 V nominal, 2,8 Ah nominal, 14 A sürekli deşarj ve 4 A azami şarj sınırları.
- [Monolithic Power MP1584 veri sayfası](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584/document_id/204/): buck regülatörün gerçek sınırları; modül kalitesi ayrıca ölçülmelidir.

## Tedarik ilkeleri

- Hücre, BMS ve şarj cihazı birbiriyle kimya/seri sayısı/akım açısından birlikte doğrulanır.
- Lityum hücreler pazaryerindeki markasız satıcılardan alınmaz; izlenebilir satıcı, fatura ve parti bilgisi aranır.
- Dört nihai hoparlörde elektronik kart revizyonları eş tutulur; değişiklik olursa BOM revizyonu ve test tekrarı gerekir.
- `30 W`, `40 A`, `3 A` gibi pazarlama başlıkları tek başına kabul kriteri değildir; çalışma koşulu ve ısıl performans aranır.
- Stokta görünen ürün satın alma anında yeniden kontrol edilir; toplu siparişten önce bir örnek doğrulanır.

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[research-log|Araştırma günlüğü]]
