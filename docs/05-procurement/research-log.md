---
title: Satın alma araştırma günlüğü
tags:
  - procurement
  - research-log
status: living-document
updated: 2026-08-30
---

# Satın alma araştırma günlüğü

Her tarama kaydı tarihli tutulur. Fiyat ve stok geçmiş bilgi sayılır; yeni siparişte yeniden tarama yapılır.

## 2026-08-30 — İlk Türkiye tedarik taraması

### Kesinleşenler

- Kullanıcının seçtiği PCM5102A satın alma kaynağı [Aletler](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) olarak listeye alındı. Önce bir adet prototip alımı önerildi.
- ESP32-S3 için PSRAM'siz `N8` yerine **8 MB PSRAM içeren `N8R8`** varyantı şart koşuldu. Mouser'daki `DevKitC-1U-N8R8` stok kaydı görüldü; `1U` varyantının harici anten gerektirdiği için PCB antenli `DevKitC-1-N8R8` alternatifi de kaydedildi.
- TPA3110D2 üretici verisi 8-26 V besleme aralığını ve 16 V / 8 ohm koşulunda 15 W/kanalı doğruluyor. Bu değer %10 THD+N'dir; `30 W` kart etiketi dört hoparlör için gerçek temiz ses gücü kabul edilmedi.
- 4S batarya tam dolu 16.8 V olduğundan amfi için uygun performans bölgesindedir. ESP32 ve DAC için ayrı 5 V buck gerekir.
- Dört nihai 4S2P paket için toplam **32 aynı model/parti hücre** gerekir. Molicel P28A üretici verisi 2.8 Ah tipik kapasiteyi doğruluyor; Türkiye satıcı stokları değişken.

### Bulunan ürünler ve gözlem

| Ürün | 2026-08-30 gözlemi | Sonuç |
|---|---|---|
| [Mouser ESP32-S3-DevKitC-1U-N8R8](https://www.mouser.com.tr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8R8) | Arama sırasında stok görünüyordu; 8 MB flash + 8 MB Octal PSRAM, IPEX anten | Aday; anten maliyeti/kasa RF yerleşimiyle birlikte değerlendirilecek. |
| [Robo90 ESP32-S3-DevKitC-1-N8R8](https://www.robo90.com/esp32-s3-devkitc-1-n8r8-gelistirme-karti-orjinal) | Ürün açıklaması 8 MB flash + 8 MB PSRAM belirtiyor | Aday; güncel stok ve orijinallik siparişte doğrulanacak. |
| [Pilpaketi Molicel P28A](https://www.pilpaketi.com/molicel-inr18650-p28a-2800-mah-35a-li-ion-pil) | Ürün sayfası stokta yok gösteriyordu | Stok alarmı / alternatif satıcı. |
| [Pilburada Molicel P28A](https://www.pilburada.com/molicel-inr-18650-p28a-37v-2800-mah-li-ion-sarjli-pil-35a-60653) | Ürün sayfası erişilebilirdi | Aday; 32 aynı parti ve eşleme hizmeti sorulacak. |
| [Pilburada HXYP 4S 12 A BMS](https://www.pilburada.com/hxyp-4s-148v-12a-li-ion-bms-devre-85187) | Sayfa Li-ion, 4S, 12 A ve hücre dengeleme belirtiyor | Aday; NTC, balans akımı ve gerçek sürekli akım doğrulanmadan toplu alınmaz. |
| [Elektropil OLT 4S 40 A balanslı BMS](https://www.elektropil.com/olt-4s-40a-168v-balansli-lityum-batarya-bms--mor-pcb--m031125) | Sayfa balanslı Li-ion ve stok bilgisi gösteriyordu | Alternatif aday; 40 A başlığına rağmen ısıl/sürekli akım testi gerekir. |
| [Pilpaketi 16.8 V 3 A](https://www.pilpaketi.com/lion-sarj-aleti-4s-16.8-volt-3a-2.1-mm-soket) | Ürün doğru voltaj/akım sınıfında fakat stokta yoktu | Bekleme listesi; tek kaynak yapılmaz. |
| [Pil Servisi 16.8 V 2 A](https://pilservisi.com.tr/urun/16-8v-2a-li-ion-sarj-adaptoru/) | Sayfa 4S Li-ion, 2 A ve LED gösterge belirtiyor; stok görünüyordu | Prototip adayı; CC/CV ve güvenlik belgesi sorulacak. |
| [Pil Mağazası WEKO 16.8 V 2 A](https://pilmagazasi.com/products/weko-16-8v-2a-4s-li-ion-batarya-sarj-adaptoru-priz-tipi) | 4S Li-ion ve 5.5 x 2.5 mm uç bilgisi görüldü | Alternatif prototip adayı. |
| [Robotistan MP1584EN](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | 4.5-28 V giriş ve ayarlı çıkış modülü olarak listeleniyor | Aday; 3 A etiketi sürekli sessiz-audio performansı olarak kabul edilmez. |
| [Direnc.net mandallı metal anahtar](https://www.direnc.net/16mm-ledli-12-24v-su-gecirmez-metal-anahtar) | 5 A @ 12 V DC ve mandallı çalışma bilgisi görüldü | 16.8 V DC kesme kapasitesi doğrulanacak; uygun değilse anahtar yalnız kontrol sinyali sürecek. |

### Elenen veya ertelenen adaylar

- **LiFePO4 BMS'ler elendi:** 4S LiFePO4 eşikleri 4S Li-ion paketle uyumlu değildir.
- [XR 4S 10 A NTC'li fakat balanssız BMS](https://pilmagazasi.com/collections/yeni-urunler/products/xr-4s-10a-16-8v-kare-lityum-batarya-bms-ntcli-balanssiz) temel gereksinimi karşılamadığı için elendi.
- 5 V girişten 16.8 V üreten küçük Type-C “2 A” şarj modülleri ertelendi: bazı ürün sayfalarında giriş akımı ile gerçek batarya şarj akımı karışıyor; termal ve güç bütçesi belirsiz.
- 16.8 V / 4 A şarj cihazı temel BOM'a alınmadı; 4S1P prototipte hücre ve BMS şarj sınırını aşabilir.
- `ESP32-S3 N8` ürün kodu PSRAM garantisi vermediği için kabul edilmedi; son ek açıkça `R8` olmalı.

### Satıcıya gönderilecek doğrulama soruları

1. BMS, 4S **Li-ion 4.2 V/hücre** için mi; balans başlangıç voltajı ve balans akımı nedir?
2. BMS'in veri sayfasıyla desteklenen sürekli deşarj akımı nedir ve NTC probu dahil midir?
3. Şarj cihazı gerçek CC/CV algoritması kullanıyor mu; CV toleransı ve sonlandırma akımı nedir?
4. Şarj cihazı DC jak ölçüsü ve merkez polaritesi nedir?
5. Molicel hücrelerden 32 adet aynı lot ve kapasite/iç direnç eşlenmiş sağlanabilir mi?
6. ESP32-S3 kart üzerindeki tam modül kodu `ESP32-S3-WROOM-1-N8R8` mi?
7. Güç anahtarı 16.8 V DC'de kaç amperi güvenli kesebilir?

### Bir sonraki tarama

- [ ] Satıcılardan teknik doğrulama yanıtlarını tarihli olarak ekle.
- [ ] İlk prototipin ölçülen tepe ve ortalama akımına göre BMS/sigorta/kabloyu boyutlandır.
- [ ] Hoparlör empedans ölçümünden sonra tweeter koruma parçaları için değer ve tedarikçi belirle.
- [ ] Kasa çizimi çıkınca panel butonu, LED lensi, şarj soketi ve güç anahtarının mekanik ölçülerini dondur.
- [ ] Dört ünite toplu alımından hemen önce stok/fiyat taramasını yenile.

## 2026-08-30 — USB-C zinciri ve fiyatlı sepet taraması

- Türkiye stoklu hazır bir IP2368 tabanlı 4S USB-C şarj modülü bulunamadı; bulunan IP2368 ürünü yalnız çıplak QFN entegre olduğundan prototip BOM'una alınmadı.
- V1 zinciri `20 V PD tetikleyici -> XL4015 16,8 V / 2 A CC/CV -> balanslı 4S BMS` olarak fiyatlandırıldı.
- TLS Robotik ESP32-S3 N16R8 810,30 TL, Aletler PCM5102A 190,26 TL, Robotistan MP1584 72,74 TL ve Pilburada P28A 594,00 TL/adet olarak görüldü.
- Meltis PD tetikleyici 120,40 TL ve Robotistan XL4015 CC/CV 188,88 TL olarak stokta görünüyordu.
- Motorobit HX-4S-20D BMS 112,80 TL idi; ürün başlığı balans iddiası taşısa da balans eşiği/akımı belgelenmediği için `teyit bekliyor` durumunda tutuldu.
- Baseus GaN5 Pro 65 W adaptör 2.299,00 TL, Baseus Pudding 100 W e-marker kablo 399,00 TL olarak fiyatlandırıldı.
- Ayrıntılı adetler, bağlantılar ve toplamlar [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesine]] kaydedildi.

## 2026-08-30 — Aspilsan A28 ve düşük maliyetli şarj seti

- Kullanıcının seçimiyle hücre adayı [Pilpaketi Aspilsan INR18650A28](https://www.pilpaketi.com/aspilsan-pil) olarak değiştirildi; görülen fiyat 119,36 TL/adet ve ürün siparişe açıktı.
- [Aspilsan A28 üretici veri sayfası](https://www.aspilsan.com/wp-content/uploads/2025/05/A28_Public_Datasheet_.pdf) 2.800 mAh nominal kapasite, 3,65 V nominal gerilim, 4,2 V şarj sonu, 1,4 A standart/4 A azami şarj ve **14 A sürekli deşarj** sınırını doğruluyor. Satıcıdaki 25 A ifadesi yalnız SOC/sıcaklık koşullu üst değerdir.
- 4S1P paket yaklaşık 14,6 V / 2,8 Ah / 40,9 Wh; 4S2P yaklaşık 14,6 V / 5,6 Ah / 81,8 Wh olur. 16,8 V tam dolu gerilim ve mevcut 4S mimari değişmez.
- Kablo dahil [Syrox GAN65T 65 W set](https://www.trendyol.com/syrox/gan65-type-c-usb-giris-baslik-type-c-kablo-65w-super-hizli-quick-sarj-set-p-793024101) 865,00 TL ve son bir ürün olarak görünüyordu. [Syrox katalog verisi](https://syrox.com.tr/wp-content/uploads/2024/02/Syrox_Katalog_Download.pdf) tek USB-C kullanımında 20 V / 3,25 A ve 1 m Type-C kablo belirtiyor.
- Setin ayrı kablo maliyetini kaldırmasıyla önceki 2.698,00 TL Baseus adaptör+kablo bütçesi 865,00 TL'ye indi. Kutu kablosunun 5 A/e-marker kimliği katalogda açık yazmadığı için satın alma sonrası USB-C test cihazıyla doğrulama şartı kondu.
- Alternatif olarak [Konfulon C99Q](https://www.mediamarkt.com.tr/tr/product/_konfulon-c99q-65w-pd-destekli-type-c-ve-usb-cift-cikisli-hizli-sarj-adaptoru-beyaz-172794171.html) 949,00 TL; tek Type-C çıkışı 20 V / 3,25 A olarak listelendi, fakat ayrıca kablo gerektiriyor.

## 2026-08-30 — Yardımcı pasifler ve prototipleme sepeti

- Şemadaki `R_PU`, RGB kanal dirençleri, isteğe bağlı `C_DB`, `C_A`, `C_SAFE`, `JP1` ve prototip bağlantıları BOM ile karşılaştırıldı.
- Robotistan'da 10'lu 1/4 W direnç paketleri 10 kΩ, 330 Ω ve 680 Ω için 0,23 TL/paket; 10'lu 100 nF seramik paket 1,11 TL olarak görüldü.
- Direnc.net 1.000 µF / 25 V elektrolitik 4,65 TL/adet olarak görüldü. Ürün sayfası düşük ESR/105 °C sınıfını doğrulamadığı için yalnız G3 prototip adayıdır.
- 2,2 µF / 400 V kutupsuz polyester kondansatör 19,89 TL/adet olarak görüldü. Dört adet paralel kombinasyonla 2,2 / 4,4 / 6,6 / 8,8 µF deney bankası oluşturabilir; bu **nihai tweeter filtresi seçimi değildir**.
- Header, jumper cap, prototip klemens, pertinaks ve düşük akımlı jumper kablo eklendi. Dört cihazı kapsayan açık yardımcı sepet 272,89 TL; ilk prototipte kullanılabilecek bölüm 178,63 TL hesaplandı.
- Buton, sert güç anahtarı, RGB modülü, sigorta ve yuva ana fiyat tablosunda zaten bulunduğu için yardımcı sepette ikinci kez maliyete eklenmedi.
- Yardımcı sepet mevcut sarf/izolasyon bütçesinin içinde tutuldu; genel maliyet toplamları değişmedi. Fiyat ve stok 2026-08-30 erişim görüntüsüdür ve siparişten önce yenilenecektir.

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesi ve maliyet hesabı]]
- [[suppliers|Satıcı ve ürün adayları]]
