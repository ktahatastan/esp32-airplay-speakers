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

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[suppliers|Satıcı ve ürün adayları]]
