---
title: Türkiye satın alma listesi ve maliyet hesabı
aliases:
  - Türkiye alışveriş listesi
  - 2026-08-30 fiyat listesi
tags:
  - procurement
  - bom
owner: procurement-researcher
status: candidate
updated: 2026-08-30
---

# Türkiye satın alma listesi ve maliyet hesabı

Bu liste dört adet Merzarkabul Airplay Speakers hoparlörü içindir. Hoparlör sürücüleri ve dört XH-A232 / TPA3110 amfi kartı kullanıcıda bulunduğu için maliyete dahil değildir.

> [!warning] Fiyat ve stok kapsamı
> Fiyatlar 2026-08-30 tarihinde KDV dahil görünen perakende fiyatlardır. Kargo, kasa ve akustik malzemeler dahil değildir. Siparişten hemen önce stok, ürün revizyonu ve fiyat tekrar kontrol edilmelidir.

## Önerilen güç girişi

`19 V masaüstü DC adaptör -> 5,5 × 2,1 mm merkez pozitif jak -> VIN -> XH-A232 (8-26 V) ve MP1584 5,10 V`

Adaptör kabinin dışındadır ve amfiyi doğrudan besler; kutunun içindeki tek güç parçası jak ve 5 V buck'tır. Adaptörün akım sınıfı G1'de ölçülen tepe akımdan gelir ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]).

## Her hoparlörün içine alınacak parçalar

| Parça / marka-model | Ne işe yarar | Satıcı ve bağlantı | Adet / hoparlör | Birim fiyat | 4 hoparlör tutarı | Durum |
|---|---|---|---:|---:|---:|---|
| ESP32-S3 N16R8 geliştirme kartı, TLS Robotik `AY-AK027` | AirPlay yazılımı, Wi-Fi/BLE provisioning, I2S, buton/LED ve OTA; 16 MB flash + 8 MB PSRAM | [TLS Robotik](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/) | 1 | 810,30 TL | 3.241,20 TL | Stokta; **aday**, kart pin dizilimi prototipte doğrulanacak |
| PCM5102A I2S DAC modülü | ESP32'nin I2S dijital sesini line-level stereo analog sese çevirir | [Aletler](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) | 1 | 190,26 TL | 761,04 TL | Stokta; kullanıcı tarafından seçildi |
| MP1584 mini buck modülü | 19 V adaptör girişini ESP32 + DAC için ayarlı 5,10 V'a düşürür | [Robotistan](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | 1 | 72,74 TL | 290,96 TL | Stokta; **aday**, yük/ripple/ısı testi zorunlu |
| 16 mm 10–30 V IP65 anlık metal buton | Kısa/uzun basma ile reset, provisioning ve eşleştirme komutları | [Direnc.net](https://www.direnc.net/16mm-10-30v-mavi-baglanti-kablolu-su-gecirmez-metal-yayli-buton) | 1 | 127,86 TL | 511,44 TL | Stokta; LED'i 19 V hattından, kontak GPIO'dan sürülecek |
| 5 mm RGB LED modülü | Wi-Fi, provisioning, hata ve OTA durumlarını gösterir | [Robotistan](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) | 1 | 17,54 TL | 70,16 TL | Stokta; ortak anot/katot prototipte teyit edilecek |
| 19 V masaüstü DC adaptör | Amfiyi doğrudan, ESP32 + DAC'ı buck üzerinden besler | Aday yok | 1 | — | — | **Fiyatlandırılmadı.** 5,5 × 2,1 mm merkez pozitif uç; sürekli akım G1 tepe akımından sonra yazılır |
| DC-005 5,5 × 2,1 mm DC giriş jakı | Adaptörü kutuya alır | [Direnc.net](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) | 1 | — | — | **Fiyatlandırılmadı.** Kontak akımı ve merkez polaritesi siparişten önce doğrulanır |

**İç donanım ara toplamı (fiyatlı satırlar):** 1.218,70 TL / hoparlör; **4.874,80 TL / dört hoparlör**. 19 V adaptör ve DC jak bu toplamın dışındadır; fiyatları 2026-08-30'da alınmadı.

## Yardımcı pasifler, konnektörler ve prototipleme parçaları

Fonksiyon butonu ve RGB LED modülü yukarıdaki ana tabloda zaten fiyatlandırılmıştır; bu bölümde ikinci kez maliyete eklenmez. Aşağıdaki sepet şemada kullanılan direnç, kondansatör, jumper, klemens ve masaüstü prototipleme parçalarını görünür hale getirir.

| Parça / değer | Devredeki görevi | Satıcı ve bağlantı | Satın alma adedi | Sepet tutarı | Durum / kullanım notu |
|---|---|---|---:|---:|---|
| 1/4 W direnç: 10 kΩ, 330 Ω ve 680 Ω; her değerden 10'lu paket | Buton `R_PU` pull-up ve çıplak RGB LED seçilirse kanal akım sınırlama | [Robotistan 1/4 W dirençler](https://www.robotistan.com/14w-direnc) | 3 paket | 0,69 TL | Dört cihaz için 4×10 kΩ, 8×330 Ω ve 4×680 Ω gereksinimini karşılar. RGB modülünde seri direnç varsa 330/680 Ω parçalar takılmaz. |
| 100 nF seramik kondansatör, 10'lu paket | İsteğe bağlı `C_DB` buton debounce ve lokal bypass | [Robotistan seramik kondansatörler](https://www.robotistan.com/seramik-kondansator-1) | 1 paket | 1,11 TL | Dört cihaz için yeterli; buton davranışı firmware ile test edilerek takılmasına karar verilir. |
| 1.000 µF / 25 V elektrolitik kondansatör | Amfi beslemesi yakınında `C_A` bulk enerji/ripple bastırma | [Direnc.net 1000 µF / 25 V](https://www.direnc.net/1000uf25v) | 4 | 18,60 TL | **Yalnız prototip adayı:** sayfa düşük ESR veya 105 °C sınıfını doğrulamıyor. G1 ölçümünden sonra markalı 105 °C düşük-ESR nihai parça seçilecek. |
| 2,2 µF / 400 V kutupsuz polyester kondansatör | `C_SAFE` tweeter seri koruma filtresi için ölçüm bankası | [Direnc.net 2,2 µF / 400 V](https://www.direnc.net/22uf-400v-damla-tipi-polyester-kondansator-225mm) | 4 | 79,56 TL | **Nihai değer değildir.** Paralel bağlanarak 2,2 / 4,4 / 6,6 / 8,8 µF deney değerleri üretilir; tweeter empedansı ve G2 süpürmesi tamamlanmadan gerçek sürücüye bağlanmaz. |
| 1×40, 2,54 mm erkek pin header | `JP1`, servis noktaları ve geçici test pini | [Robotistan header](https://www.robotistan.com/header) | 1 | 7,68 TL | Bir şerit dört cihazın `JP1` pinleri için yeterlidir; nihai PCB test noktaları lehim pedi olur. |
| 2,54 mm jumper cap | `JP1` USB/system 5 V izolasyon köprüsü | [Direnc.net jumper](https://www.direnc.net/jumpers) | 4 | 1,96 TL | Her cihaz için bir adet. Enerji kaynağı değiştirilmeden önce güç kesilir. |
| KF128V 5,08 mm 2'li vidalı klemens | Güç, woofer ve tweeter kablolarının sökülebilir prototip bağlantısı | [Robotistan klemens](https://www.robotistan.com/klemens-1) | 16 | 105,12 TL | Dört adet/hoparlör prototip varsayımıdır; nihai kilitli konnektör kasa ve titreşim testinden sonra seçilir. |
| 5×10 cm tek yüzlü delikli pertinaks | İlk masaüstü prototip taşıyıcısı | [Robotistan pertinaks](https://www.robotistan.com/5x10-cm-delikli-pertinaks-bakir-tek-yuzlu) | 1 | 18,65 TL | Yalnız ilk prototip; dört nihai cihaz için taşıyıcı PCB hedeflenir. |
| 20 cm dişi-erkek jumper kablo, 40'lı | Düşük akımlı I2S/GPIO masaüstü bağlantıları | [Direnc.net jumper kablo](https://www.direnc.net/40-adet-disi-erkek-jumper-20cm-1) | 1 set | 39,52 TL | Güç, amfi ve hoparlör hatlarında kullanılmaz; o hatlar uygun kesitli silikon kabloyla yapılır. |

**Açıkça fiyatlandırılan yardımcı sepet:** **272,89 TL**. Bunun **178,63 TL**'lik bölümü ilk prototip kurulumunda kullanılabilir; kalan parçalar dört cihaz için paylaşımlıdır. Bu tutar aşağıdaki mevcut 250 TL / 1.000 TL sarf bütçelerinin içindedir ve genel toplama yeniden eklenmemiştir. İlk prototipte kalan 71,37 TL'nin kablo, makaron ve izolasyona yetip yetmediği sipariş öncesi kontrol edilmelidir.

## Maliyet özeti

| Senaryo | İç donanım | Sarf/izolasyon bütçesi | Genel toplam |
|---|---:|---:|---:|
| 1 hoparlör prototipi | 1.218,70 TL | 250,00 TL tahmini | **1.468,70 TL** |
| 4 hoparlör | 4.874,80 TL | 1.000,00 TL tahmini | **5.874,80 TL** |

Her iki toplam da 19 V adaptörü ve DC giriş jakını **içermez**: ikisi de adaydır ve fiyatları henüz alınmadı. Adaptörün akım sınıfı G1 ölçümünden sonra kilitleneceği için fiyatı da o zaman yazılır; her hoparlöre bir adaptör gerekir.

Sarf/izolasyon bütçesi; yukarıda açıkça fiyatlandırılan yardımcı sepetle birlikte silikon kablo, ısı makaronu, nihai JST/kilitli konnektör ve yükselticiler için fiyat tahminidir. Kargo ve kasa dahil değildir.

## Sipariş sırası

1. İlk aşamada yalnız bir hoparlörlük elektronik alınır.
2. Adaptör adayı belirlenince satıcıdan sürekli akım, uç ölçüsü/polaritesi ve çıkışın PE ilişkisi yazılı istenir; jak polaritesi teslimatta ölçü aletiyle doğrulanır.
3. [[../06-testing/test-strategy|G1]] ve G2 geçilmeden kalan üç set toplu alınmaz.

## Satıcıya sorulacak zorunlu sorular

- Adaptör 19 V'ta G1'de ölçülen tepe akımı sürekli veriyor mu; yüksüz çıkış gerilimi ve tolerans nedir?
- Adaptör ucu 5,5 × 2,1 mm ve merkez pozitif mi?
- Adaptör çıkışı koruma toprağına (PE) bağlı mı, izole mi?
- DC-005 jakın kontak akımı ve DC gerilim sınıfı nedir?

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[suppliers|Satıcılar ve ürün adayları]]
- [[../power-plan|Güç planı]]
- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı planı]]
