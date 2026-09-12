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

Bu liste tek Merzarkabul kabini içindir ([[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]). Sekiz Nova sürücü ve dört XH-A232 / TPA3110 amfi kartı kullanıcıda bulunduğu için maliyete dahil değildir.

> [!warning] Fiyat ve stok kapsamı
> Fiyatlar 2026-08-30 tarihinde KDV dahil görünen perakende fiyatlardır. Kargo, kabin ve akustik malzemeler dahil değildir. Siparişten hemen önce stok, ürün revizyonu ve fiyat tekrar kontrol edilmelidir.

## Önerilen güç girişi

`24 V / 2,9 A masaüstü DC adaptör -> 5,5 × 2,1 mm merkez pozitif jak -> VIN -> 4 × XH-A232 (8-26 V) ve 2 × MP1584 5,10 V (A: ESP32-S3, B: PCM5102A)`

Adaptör kabinin dışındadır ve dört amfiyi doğrudan besler; kabinin içindeki güç parçaları jak, bulk kondansatör ve iki buck'tır. Adaptör 24 V / 2,9 A ile verilidir; yüksüz çıkışı bağlanmadan önce ölçülür ve 25,5 V'un altında olmalıdır ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]). Bu bütçeden alınan şey DSP'nin besleme bütçesi katıdır (`supply_budget_sq` / `supply_window_ms`, iki dalın toplamı üzerinden ortalama güç), `G1` S7'de 4 Ω sınıfı yükte; tepe limiter tavanları `G2`'nindir ve onun altında kalır ([[../07-decisions/ADR-0022-dsp-product-output-backend|ADR-0022]]).

## Kabine alınacak parçalar

| Parça / marka-model | Ne işe yarar | Satıcı ve bağlantı | Adet | Birim fiyat | Tutar | Durum |
|---|---|---|---:|---:|---:|---|
| ESP32-S3 N16R8 geliştirme kartı, TLS Robotik `AY-AK027` | AirPlay yazılımı, Wi-Fi/BLE provisioning, I2S, buton/LED ve OTA; 16 MB flash + 8 MB PSRAM | [TLS Robotik](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/) | 1 | 810,30 TL | 810,30 TL | Stokta; **aday**, kart pin dizilimi prototipte doğrulanacak |
| PCM5102A I2S DAC modülü | ESP32'nin I2S dijital sesini iki bantlı line-level analog sese çevirir; `LOUT`/`ROUT` dört amfiye paralel gider | [Aletler](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) | 1 | 190,26 TL | 190,26 TL | Stokta; kullanıcı tarafından seçildi |
| MP1584 mini buck modülü | 24 V adaptör girişini ayarlı 5,10 V'a düşürür; A: ESP32-S3, B: PCM5102A (ayrı, çünkü paylaşılan buck DAC'a hışırtı verdi) | [Robotistan](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | 2 | 72,74 TL | 145,48 TL | Stokta; **aday**, yük/ripple/ısı testi zorunlu |
| 16 mm 10–30 V IP65 anlık metal buton | Kısa/uzun basma ile reset, provisioning ve eşleştirme komutları | [Direnc.net](https://www.direnc.net/16mm-10-30v-mavi-baglanti-kablolu-su-gecirmez-metal-yayli-buton) | 1 | 127,86 TL | 127,86 TL | Stokta; LED'i 24 V hattından (10-30 V aralığı kapsar), kontak GPIO'dan sürülecek |
| 5 mm RGB LED modülü | Wi-Fi, provisioning, hata ve OTA durumlarını gösterir | [Robotistan](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) | 1 | 17,54 TL | 17,54 TL | Stokta; ortak anot/katot prototipte teyit edilecek |
| 24 V / 2,9 A masaüstü DC adaptör | Dört amfiyi doğrudan, ESP32 ve DAC'ı iki buck üzerinden besler | Aday yok | 1 | — | — | **Fiyatlandırılmadı.** 5,5 × 2,1 mm merkez pozitif uç; yüksüz çıkış < 25,5 V ölçülmeden bağlanmaz |
| DC-005 5,5 × 2,1 mm DC giriş jakı | Adaptörü kabine alır | [Direnc.net](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) | 1 | — | — | **Fiyatlandırılmadı.** Kontak akımı 2,9 A sürekli için tedarikçiden yazılı alınır; merkez polaritesi siparişten önce doğrulanır |

**İç donanım ara toplamı (fiyatlı satırlar):** **1.291,44 TL**. 24 V adaptör ve DC jak bu toplamın dışındadır; fiyatları 2026-08-30'da alınmadı.

## Yardımcı pasifler, konnektörler ve prototipleme parçaları

Fonksiyon butonu ve RGB LED modülü yukarıdaki ana tabloda zaten fiyatlandırılmıştır; bu bölümde ikinci kez maliyete eklenmez. Aşağıdaki sepet şemada kullanılan direnç, kondansatör, jumper, klemens ve masaüstü prototipleme parçalarını görünür hale getirir.

| Parça / değer | Devredeki görevi | Satıcı ve bağlantı | Satın alma adedi | Sepet tutarı | Durum / kullanım notu |
|---|---|---|---:|---:|---|
| 1/4 W direnç: 10 kΩ, 330 Ω ve 680 Ω; her değerden 10'lu paket | Buton `R_PU` pull-up, `R6` DAC susturma pull-down'ı, çıplak RGB LED seçilirse kanal akım sınırlama | [Robotistan 1/4 W dirençler](https://www.robotistan.com/14w-direnc) | 3 paket | 0,69 TL | Kabin için 2×10 kΩ (`R_PU`, `R6`), 2×330 Ω ve 1×680 Ω gereksinimini karşılar. RGB modülünde seri direnç varsa 330/680 Ω parçalar takılmaz. |
| 100 nF seramik kondansatör, 10'lu paket | İsteğe bağlı `C_DB` buton debounce ve lokal bypass | [Robotistan seramik kondansatörler](https://www.robotistan.com/seramik-kondansator-1) | 1 paket | 1,11 TL | Kabin için yeterli; buton davranışı firmware ile test edilerek takılmasına karar verilir. |
| 1.000 µF / 35 V, 105 °C düşük-ESR elektrolitik kondansatör | Jak girişinde `C_A` bulk enerji/ripple bastırma | Aday yok | 1 | — | **Fiyatlandırılmadı.** 24 V rayda 25 V sınıfı kabul edilmez; gerilim sınıfı adaptörün yüksüz çıkışının üstünde olmalı. G1 ripple ölçümünden sonra markalı 105 °C düşük-ESR nihai parça seçilecek. |
| 2,2 µF / 400 V kutupsuz polyester kondansatör | `C_SAFE` tweeter seri koruma filtresi için ölçüm bankası | [Direnc.net 2,2 µF / 400 V](https://www.direnc.net/22uf-400v-damla-tipi-polyester-kondansator-225mm) | 4 | 79,56 TL | **Nihai değer değildir.** Paralel bağlanarak 2,2 / 4,4 / 6,6 / 8,8 µF deney değerleri üretilir; tweeter empedansı ve G2 süpürmesi tamamlanmadan gerçek sürücüye bağlanmaz. Nihai kabin için dört eş parça (ilk seçim 10 µF, aynı seri/lot) ayrıca alınır, fiyatlanmadı. |
| 1×40, 2,54 mm erkek pin header | `JP1`, servis noktaları ve geçici test pini | [Robotistan header](https://www.robotistan.com/header) | 1 | 7,68 TL | Bir şerit kabinin `JP1` pinleri ve test noktaları için yeterlidir; nihai PCB test noktaları lehim pedi olur. |
| 2,54 mm jumper cap | `JP1` USB/system 5 V izolasyon köprüsü (yalnız buck A) | [Direnc.net jumper](https://www.direnc.net/jumpers) | 1 | 0,49 TL | Kabin için bir adet. Enerji kaynağı değiştirilmeden önce güç kesilir. |
| KF128V 5,08 mm 2'li vidalı klemens | Güç, woofer ve tweeter kablolarının sökülebilir prototip bağlantısı | [Robotistan klemens](https://www.robotistan.com/klemens-1) | 16 | 105,12 TL | 4 amfi × 3 (VIN, woofer, tweeter) + 2 buck girişi + 2 VIN yıldız dağıtımı prototip varsayımıdır; nihai kilitli konnektör kabin ve titreşim testinden sonra seçilir. |
| 5×10 cm tek yüzlü delikli pertinaks | İlk masaüstü prototip taşıyıcısı | [Robotistan pertinaks](https://www.robotistan.com/5x10-cm-delikli-pertinaks-bakir-tek-yuzlu) | 1 | 18,65 TL | Yalnız ilk prototip; kabin için taşıyıcı PCB hedeflenir. |
| 20 cm dişi-erkek jumper kablo, 40'lı | Düşük akımlı I2S/GPIO masaüstü bağlantıları | [Direnc.net jumper kablo](https://www.direnc.net/40-adet-disi-erkek-jumper-20cm-1) | 1 set | 39,52 TL | Güç, amfi ve hoparlör hatlarında kullanılmaz; o hatlar uygun kesitli silikon kabloyla yapılır. |

**Açıkça fiyatlandırılan yardımcı sepet:** **252,82 TL** (35 V `C_A` fiyatlanmadı). Bunun **187,12 TL**'lik bölümü ilk tek-amfi prototip kurulumunda kullanılabilir (klemenslerin 6'sı: bir amfi, iki buck, VIN dağıtımı); kalan 65,70 TL diğer üç amfinin klemensleridir. Bu tutar aşağıdaki sarf bütçesinin içindedir ve genel toplama yeniden eklenmemiştir. Bütçenin kalanının silikon kablo, makaron ve izolasyona yetip yetmediği sipariş öncesi kontrol edilmelidir.

## Maliyet özeti

| Kalem | Tutar |
|---|---:|
| İç donanım (fiyatlı satırlar) | 1.291,44 TL |
| Sarf/izolasyon bütçesi (tahmin) | 450,00 TL |
| **Genel toplam** | **1.741,44 TL** |

Toplam 24 V adaptörü, DC giriş jakını ve 35 V `C_A`'yı **içermez**: üçü de adaydır ve fiyatları henüz alınmadı. Adaptörün değeri sabittir (24 V / 2,9 A); fiyatı aday belirlenince yazılır.

Sarf/izolasyon bütçesi bir **tahmindir**: yukarıda açıkça fiyatlandırılan yardımcı sepetle birlikte sekiz sürücü ve dört amfi için silikon kablo, ısı makaronu, nihai JST/kilitli konnektör ve yükselticileri kapsar. Kargo ve kabin dahil değildir.

## Sipariş sırası

1. İlk aşamada tek amfi + bir woofer/tweeter çifti için elektronik alınır; kabinin tamamı için gerekenler (dört `C_SAFE`, klemensler) aynı sepette gelebilir ama diğer üç amfi sürücülere G1/G2 geçmeden bağlanmaz.
2. Adaptör adayı belirlenince satıcıdan yüksüz çıkış gerilimi ve toleransı, 2,9 A sürekli akım, uç ölçüsü/polaritesi ve çıkışın PE ilişkisi yazılı istenir; teslimatta yüksüz çıkış ve jak polaritesi ölçü aletiyle doğrulanır.
3. [[../06-testing/test-strategy|G1]] ve G2 geçilmeden dört amfi birlikte sürülmez.

## Satıcıya sorulacak zorunlu sorular

- Adaptör 24 V'ta 2,9 A'yı sürekli veriyor mu; yüksüz çıkış gerilimi ve toleransı nedir (25,5 V altı şart)?
- Adaptör ucu 5,5 × 2,1 mm ve merkez pozitif mi?
- Adaptör çıkışı koruma toprağına (PE) bağlı mı, izole mi?
- DC-005 jakın kontak akımı 2,9 A sürekli için belgeli mi; DC gerilim sınıfı nedir?

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[suppliers|Satıcılar ve ürün adayları]]
- [[../power-plan|Güç planı]]
- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı planı]]
