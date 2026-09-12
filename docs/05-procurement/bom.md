---
title: Merzarkabul Airplay Speakers BOM ve satın alma listesi
aliases:
  - BOM
  - Satın alma listesi
tags:
  - procurement
  - hardware
  - bom
owner: procurement-researcher
status: draft
updated: 2026-09-08
---

# BOM ve satın alma listesi

> [!warning] Fiyat ve stok
> Bağlantılar 2026-08-30 tarihinde kontrol edildi. Fiyat, stok, kargo ve ürün revizyonu değişebilir; siparişten hemen önce yeniden doğrulanmalıdır. `Aday` satırları elektriksel ve mekanik doğrulama tamamlanmadan toplu alınmamalıdır.

Ürün tek bir kabindir: bir ESP32-S3, bir PCM5102A, dört XH-A232, dört woofer ve dört tweeter ([[../07-decisions/ADR-0021-single-cabinet|ADR-0021]], [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]]). Adetler o kabin içindir.

## Satın alma stratejisi

1. Önce tek amfi + bir woofer/tweeter çiftiyle masaüstü prototip kurulur; diğer üç amfi G0-G2 o çift üzerinde geçince sürücülere bağlanır.
2. Sürücü empedansı ölçülür; 24 V adaptör beslemesinde amfi sıcaklığı, çıkış gücü, dört amfi birlikte sürülürken 2,9 A bütçesi ve tweeter koruması doğrulanır.

## Ana elektronik BOM

| Kalem | Teknik koşul | Adet | İlk prototip | Durum / kaynak |
|---|---|---:|---:|---|
| ESP32-S3 geliştirme kartı | **16 MB flash + 8 MB PSRAM (`N16R8`)** — [[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]] ile kilitlendi | 1 | 1 | Fiyatlandırılan aday: [TLS Robotik N16R8](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/). Anten tipi (PCB / IPEX) kasa kararıyla ayrıca belirlenecek. `N8R8` yalnız ikincil yedektir ve partition bütçesi kanıtlanmadan kullanılmaz. |
| PCM5102A I2S DAC modülü | Stereo, line-level çıkış; `LOUT` woofer bandı, `ROUT` tweeter bandı; kart pin dizilimi kontrolü | 1 | 1 | Satın alınacak; kullanıcının seçtiği [Aletler PCM5102A modülü](https://www.aletler.com.tr/urun/pcm5102a-dac-modul). |
| XH-A232 / TPA3110 amfi | Stereo Class-D; 24 V adaptörden doğrudan besleme; dördü aynı revizyon, girişleri `LOUT`/`ROUT`'a paralel | 4 | 1 | Elde 4 adet olduğu varsayılıyor; adet ve revizyon fiziksel sayımla doğrulanacak. |
| Harman Kardon Nova sürücüler | 4 woofer + 4 tweeter, tek kabin; her biri kendi amfi kanalında | 4 + 4 | 1 + 1 | Elde olduğu varsayılıyor; her sürücünün DC direnç ve empedans ölçümü zorunlu. |
| 24 V / 2,9 A masaüstü DC adaptör | ≈70 W; 5,5 × 2,1 mm uç, merkez pozitif; yüksüz çıkış bağlanmadan önce ölçülür, < 25,5 V — [[../07-decisions/ADR-0020-dc-adapter-power\|ADR-0020]] | 1 | 1 | **Aday; kaynak ve fiyat henüz yok.** Yüksüz çıkış gerilimi/toleransı ve çıkışın PE'ye bağlı olup olmadığı satıcıya sorulur. |
| DC giriş jakı | 5,5 × 2,1 mm panel tipi; kontak akımı 2,9 A sürekli, belgeli | 1 | 1 | [Direnc.net DC-005](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) adayı; kontak değeri tedarikçi sorusu, fiyatı henüz alınmadı. |
| 5 V buck regülatör | 24 V giriş, sürekli akım ve termal marj; A: ESP32-S3, B: PCM5102A — ayrı, çünkü paylaşılan buck DAC'a hışırtı verdi (ADR-0020) | 2 | 2 | [Robotistan MP1584EN 3 A](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) aday; yük/ısınma ve ses gürültüsü ölçülecek. |
| Çok işlevli anlık buton | NO, panel tipi veya PCB tipi; 3.3 V GPIO için | 1 | 1 | [Robotistan KY-004](https://www.robotistan.com/ky-004-buton-modulu) yalnız prototip adayı; nihai panel butonu kasa tasarımına göre seçilecek. |
| RGB durum LED'i | Ortak anot/katot RGB veya adreslenebilir LED; firmware seçimiyle uyumlu | 1 | 1 | [Robotistan 5 mm RGB modül](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) prototip adayı. |

## Ses yolu ve mekanik tamamlayıcılar

| Kalem | Prototip | Kabin | Not |
|---|---:|---:|---|
| Tweeter seri koruma kondansatörü `C_SAFE` | 2-4 değerlik ölçüm seti | 4 eş adet, aynı seri/lot | İlk seçim 10 µF kutupsuz film ≥ 50 V ([[../02-hardware/driver-measurements\|sürücü ölçüm planı]]); kesin değer tweeter `Fs` ve G2 ölçümünden sonra. |
| Giriş/çıkış filtreleme ve lokal bypass kondansatörleri | 1 set | 1 set | DAC-amfi gürültü ve açılış “pop” testine göre. |
| EMI ferrit / common-mode çözümü | 1 deney seti | Test sonucuna göre | Wi-Fi ve Class-D girişimini ölçmeden toplu alınmayacak. |
| Kablo ve konnektör | 1 set | 1 set | Silikon kablo; `VIN` dağıtımı 2,9 A'e uygun kesit; kilitli konnektör. Kasa/akım testinden sonra ölçülendirilecek. |
| Kabin, ızgara, conta, akustik dolgu | 1 prototip | Tek pasif radyatörlü kabin, 8 sürücü | Hacim, dizilim ve pasif radyatör akordu G0 sonrası ([[../04-acoustics/cabinet-plan\|kabin planı]]). |
| PCB / delikli pertinaks / kablo demeti | 1 | 1 | Prototipte modüler; nihai sürümde servis edilebilir tek taşıyıcı PCB hedeflenir. |
| Vida, yükseltici, ısı iletken ped, kablo bağı | 1 set | 1 set | Kısa devre ve titreşim önleme için. |

## Yardımcı pasifler ve prototipleme BOM'u

| Referans / kalem | Değer veya tip | Adet | İlk prototip satın alımı | Durum |
|---|---|---:|---:|---|
| `R_PU` | 10 kΩ, 1/4 W | 1 | 10'lu paket | Buton pull-up; aday. |
| `R6` | 10 kΩ, 1/4 W | 1 | 10'lu paket | DAC `XSMT` → `STAR_GND` pull-down, modül ucuna monte edilir. **Opsiyonel değil**: susturmayı tutan şey bu, GPIO değil (ADR-0011). Lehimden önce modülün `XSMT` pad'i ile 3,3 V arası direnç ölçülür; sert köprü varsa kesilir, yoksa pull-down bir bölücüye dönüşür ([[../02-hardware/circuit-and-wiring-plan#3.3 PCM5102A modül ayarları\|kablolama planı §3.3]]). |
| `R_LED_R` | 680 Ω, 1/4 W | 1 | 10'lu paket | Yalnız çıplak RGB LED'de; modül üzerinde direnç varsa `DNP`. |
| `R_LED_G`, `R_LED_B` | 330 Ω, 1/4 W | 2 | 10'lu paket | Yalnız çıplak RGB LED'de; modül üzerinde direnç varsa `DNP`. |
| `D2` | Seri Schottky veya ideal-diyot modülü; 2,9 A sürekli taşır, ≈1 W ısı | 1 koşullu | 1 | Ters polarite adayı, `DC_IN` ile `VIN` arasında. G1'de ölçülen düşüm ve ısıyla kabul edilir; ret ise yerine 0 Ω köprü gelir (ADR-0020). |
| `C_DB` | 100 nF seramik | 1 isteğe bağlı | 10'lu paket | Donanım debounce/bypass adayı; firmware testiyle karar verilecek. |
| `C_A` | 1.000 µF / **35 V**, 105 °C düşük-ESR hedef; tek, jak girişinde | 1 | 1 | 24 V rayda 25 V sınıfı kabul edilmez; gerilim sınıfı adaptörün yüksüz çıkışının üstünde olmalı. ESR/ripple/sıcaklık G1'de ölçülecek; amfi başına ek bulk yalnız G1 ripple ölçümü isterse. |
| `C_SAFE` deney bankası | 4×2,2 µF / 400 V kutupsuz film | Değer TBD | 4 ortak deney parçası | Nihai: 4 eş değer, aynı seri/lot (ilk seçim 10 µF, G2 ile kesinleşir). Tweeter empedansı ve G2 süpürmesi olmadan değer dondurulmaz veya sürücüye bağlanmaz. |
| `JP1` + jumper cap | 2 pin 2,54 mm + kısa devre şapkası | 1 | 1×40 header + 1 cap | USB/system 5 V izolasyonu, yalnız buck A (ESP) tarafında; buck B'de jumper yok. Nihai PCB'de bulunur. |
| Vidalı klemens | KF128V, 5,08 mm, 2 pin | 16 | 6 (tek amfi prototipi) | 4 amfi × 3 (VIN, woofer, tweeter) + 2 buck girişi + 2 VIN yıldız dağıtımı = 16 prototip adayı; nihai titreşim dayanımlı kilitli konnektör daha sonra seçilir. |
| Test noktaları `TP0–TP33` | Header kesiti veya prob pedi | Gerektikçe | 1×40 header'dan | Üretimde ayrı BOM parçası değildir. |
| Delikli pertinaks | 5×10 cm, tek yüzlü | 0 nihai | 1 | Yalnız masaüstü prototip. |
| Jumper kablo | 20 cm dişi-erkek, 40'lı | 0 nihai | 1 set | Yalnız düşük akımlı I2S/GPIO prototipleme; güç/ses çıkışında kullanılmaz. |

Fiyat, bağlantı ve paket adetleri için [[turkey-shopping-list-2026-08-30#Yardımcı pasifler, konnektörler ve prototipleme parçaları|yardımcı parça sepetine]] bakın.

## Elde olanların kabul kontrolü

- [ ] Dört XH-A232 kartın parça kodu, kanal yapısı ve görsel revizyonu aynı mı? Aynı kabinde girişleri paralel çalışacaklar; kazanç seçimi dördünde aynı olmalı. Kartta susturma girişi yoktur; olan bir revizyon gelirse bu farklı bir karttır.
- [ ] Dört woofer ve dört tweeter var mı; her birinin DC direnci kaydedildi mi, aynı tipteki dördü birbirine eşleşiyor mu?
- [ ] PCM5102A kartından henüz alınmadıysa ilk sipariş yalnız bir adet mi?
- [ ] Adaptörün etiketi 24 V / 2,9 A mı; yüksüz çıkışı bağlanmadan önce ölçüldü mü ve 25,5 V'un altında mı?
- [ ] Adaptör ucu ve jak merkez pozitif mi; ölçü aletiyle doğrulandı mı?
- [ ] Jak kontağının 2,9 A sürekli akım değeri tedarikçiden yazılı alındı mı?

## İlgili notlar

- [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesi ve maliyet hesabı]]
- [[suppliers|Satıcı ve ürün adayları]]
- [[research-log|Satın alma araştırma günlüğü]]
- [[../power-plan|Güç planı]]
