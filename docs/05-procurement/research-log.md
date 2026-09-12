---
title: Satın alma araştırma günlüğü
tags:
  - procurement
  - research-log
owner: procurement-researcher
status: living-document
updated: 2026-08-30
---

# Satın alma araştırma günlüğü

Her tarama kaydı tarihli tutulur. Fiyat ve stok geçmiş bilgi sayılır; yeni siparişte yeniden tarama yapılır.

## 2026-08-30 — İlk Türkiye tedarik taraması

### Kesinleşenler

- Kullanıcının seçtiği PCM5102A satın alma kaynağı [Aletler](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) olarak listeye alındı. Önce bir adet prototip alımı önerildi.
- ESP32-S3 için PSRAM'siz `N8` yerine **8 MB PSRAM içeren `N8R8`** varyantı şart koşuldu. Mouser'daki `DevKitC-1U-N8R8` stok kaydı görüldü; `1U` varyantının harici anten gerektirdiği için PCB antenli `DevKitC-1-N8R8` alternatifi de kaydedildi.
- TPA3110D2 üretici verisi 8-26 V besleme aralığını ve 16 V / 8 ohm koşulunda 15 W/kanalı doğruluyor. Bu değer %10 THD+N'dir; `30 W` kart etiketi dört amfi kartı için gerçek temiz ses gücü kabul edilmedi.
- TPA3110'un 8-26 V aralığı 24 V masaüstü adaptörü kapsar (26 V tavanına 2 V); dört amfi adaptörden doğrudan beslenir. ESP32 ve DAC için birer 5 V buck gerekir.

### Bulunan ürünler ve gözlem

| Ürün | 2026-08-30 gözlemi | Sonuç |
|---|---|---|
| [Mouser ESP32-S3-DevKitC-1U-N8R8](https://www.mouser.com.tr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8R8) | Arama sırasında stok görünüyordu; 8 MB flash + 8 MB Octal PSRAM, IPEX anten | Aday; anten maliyeti/kasa RF yerleşimiyle birlikte değerlendirilecek. |
| [Robo90 ESP32-S3-DevKitC-1-N8R8](https://www.robo90.com/esp32-s3-devkitc-1-n8r8-gelistirme-karti-orjinal) | Ürün açıklaması 8 MB flash + 8 MB PSRAM belirtiyor | Aday; güncel stok ve orijinallik siparişte doğrulanacak. |
| [Robotistan MP1584EN](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | 4.5-28 V giriş ve ayarlı çıkış modülü olarak listeleniyor | Aday; 3 A etiketi sürekli sessiz-audio performansı olarak kabul edilmez. |

### Elenen veya ertelenen adaylar

- `ESP32-S3 N8` ürün kodu PSRAM garantisi vermediği için kabul edilmedi; son ek açıkça `R8` olmalı.

### Satıcıya gönderilecek doğrulama soruları

1. ESP32-S3 kart üzerindeki tam modül kodu `ESP32-S3-WROOM-1-N8R8` mi?

### Bir sonraki tarama

- [ ] Satıcılardan teknik doğrulama yanıtlarını tarihli olarak ekle.
- [ ] Adaptör 24 V / 2,9 A ile verilidir; kabloyu ve jak kontağını 2,9 A sürekli akıma göre boyutlandır, adaptör adayının yüksüz çıkışını (< 25,5 V) satıcıdan iste.
- [ ] Hoparlör empedans ölçümünden sonra tweeter koruma parçaları için değer ve tedarikçi belirle.
- [ ] Kasa çizimi çıkınca panel butonu, LED lensi ve DC giriş jakının mekanik ölçülerini dondur.
- [ ] Kabin montajından hemen önce stok/fiyat taramasını yenile.

## 2026-08-30 — Fiyatlı sepet taraması

- TLS Robotik ESP32-S3 N16R8 810,30 TL, Aletler PCM5102A 190,26 TL ve Robotistan MP1584 72,74 TL olarak görüldü.
- Ayrıntılı adetler, bağlantılar ve toplamlar [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesine]] kaydedildi.

## 2026-08-30 — Yardımcı pasifler ve prototipleme sepeti

- Şemadaki `R_PU`, RGB kanal dirençleri, isteğe bağlı `C_DB`, `C_A`, `C_SAFE`, `JP1` ve prototip bağlantıları BOM ile karşılaştırıldı.
- Robotistan'da 10'lu 1/4 W direnç paketleri 10 kΩ, 330 Ω ve 680 Ω için 0,23 TL/paket; 10'lu 100 nF seramik paket 1,11 TL olarak görüldü.
- Direnc.net 1.000 µF / 25 V elektrolitik 4,65 TL/adet olarak görüldü. 25 V sınıfı 24 V rayda kullanılamaz; `C_A` için 35 V sınıfı, düşük ESR/105 °C bir parça fiyatlanacak.
- 2,2 µF / 400 V kutupsuz polyester kondansatör 19,89 TL/adet olarak görüldü. Dört adet paralel kombinasyonla 2,2 / 4,4 / 6,6 / 8,8 µF deney bankası oluşturabilir; bu **nihai tweeter filtresi seçimi değildir**.
- Header, jumper cap, prototip klemens, pertinaks ve düşük akımlı jumper kablo eklendi. Tek kabini kapsayan açık yardımcı sepet 252,82 TL (35 V `C_A` fiyatsız); ilk tek-amfi prototipinde kullanılabilecek bölüm 187,12 TL hesaplandı.
- Buton ve RGB modülü ana fiyat tablosunda zaten bulunduğu için yardımcı sepette ikinci kez maliyete eklenmedi.
- Yardımcı sepet mevcut sarf/izolasyon bütçesinin içinde tutuldu; genel maliyet toplamları değişmedi. Fiyat ve stok 2026-08-30 erişim görüntüsüdür ve siparişten önce yenilenecektir.

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesi ve maliyet hesabı]]
- [[suppliers|Satıcı ve ürün adayları]]
