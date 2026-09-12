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

## Satın alma stratejisi

1. Önce tek bir hoparlörlük masaüstü prototip kurulur.
2. Hoparlör empedansı ölçülür; 19 V adaptör beslemesinde amfi sıcaklığı, çıkış gücü ve tweeter koruması doğrulanır.
3. Prototip onayından sonra kalan üç ünite alınır/monte edilir.

## Ana elektronik BOM

| Kalem | Teknik koşul | Bir ünite | 1 ünite prototip | 4 ünite nihai | Durum / kaynak |
|---|---|---:|---:|---:|---|
| ESP32-S3 geliştirme kartı | **16 MB flash + 8 MB PSRAM (`N16R8`)** — [[../07-decisions/ADR-0010-esp32-s3-n16r8-board\|ADR-0010]] ile kilitlendi | 1 | 1 | 4 | Fiyatlandırılan aday: [TLS Robotik N16R8](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/). Anten tipi (PCB / IPEX) kasa kararıyla ayrıca belirlenecek. `N8R8` yalnız ikincil yedektir ve partition bütçesi kanıtlanmadan kullanılmaz. |
| PCM5102A I2S DAC modülü | Stereo, line-level çıkış; kart pin dizilimi kontrolü | 1 | 1 | 4 | Satın alınacak; kullanıcının seçtiği [Aletler PCM5102A modülü](https://www.aletler.com.tr/urun/pcm5102a-dac-modul). |
| XH-A232 / TPA3110 amfi | Stereo Class-D; 19 V adaptörden doğrudan besleme | 1 | 0 | 0 | Elde 4 adet olduğu varsayılıyor; adet fiziksel sayımla doğrulanacak. |
| Harman Kardon Nova sürücü takımı | Bir woofer + bir tweeter / ünite | 1 takım | 0 | 0 | Elde olduğu varsayılıyor; DC direnç ve empedans ölçümü zorunlu. |
| 19 V masaüstü DC adaptör | 5,5 × 2,1 mm uç, merkez pozitif; sürekli akım en az G1 tepe akımı — [[../07-decisions/ADR-0020-dc-adapter-power\|ADR-0020]] | 1 | 1 | 4 | **Aday; kaynak ve fiyat henüz yok.** Akım sınıfı G1 ölçümünden sonra yazılır; çıkışın PE'ye bağlı olup olmadığı satıcıya sorulur. |
| DC giriş jakı | 5,5 × 2,1 mm panel tipi; kontak akımı en az ölçülen tepe akım + %50 | 1 | 1 | 4 | [Direnc.net DC-005](https://www.direnc.net/dc-005-55-x-21mm-siyah-dc-guc-adaptoru-jak-soketi-modulu) adayı; fiyatı henüz alınmadı. |
| 5 V buck regülatör | 19 V giriş, sürekli akım ve termal marj; ESP32 + DAC beslemesi | 1 | 1 | 4 | [Robotistan MP1584EN 3 A](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) aday; yük/ısınma ve ses gürültüsü ölçülecek. |
| Çok işlevli anlık buton | NO, panel tipi veya PCB tipi; 3.3 V GPIO için | 1 | 1 | 4 | [Robotistan KY-004](https://www.robotistan.com/ky-004-buton-modulu) yalnız prototip adayı; nihai panel butonu kasa tasarımına göre seçilecek. |
| RGB durum LED'i | Ortak anot/katot RGB veya adreslenebilir LED; firmware seçimiyle uyumlu | 1 | 1 | 4 | [Robotistan 5 mm RGB modül](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) prototip adayı. |

## Ses yolu ve mekanik tamamlayıcılar

| Kalem | Prototip | 4 ünite nihai | Not |
|---|---:|---:|---|
| Tweeter seri koruma kondansatörü / pasif emniyet filtresi | 2-4 değerlik ölçüm seti | 4 eş set | Değer, gerçek tweeter empedansı ve DSP crossover ölçümünden sonra seçilecek. |
| Giriş/çıkış filtreleme ve lokal bypass kondansatörleri | 1 set | 4 set | DAC-amfi gürültü ve açılış “pop” testine göre. |
| EMI ferrit / common-mode çözümü | 1 deney seti | Test sonucuna göre | Wi-Fi ve Class-D girişimini ölçmeden toplu alınmayacak. |
| Kablo ve konnektör | 1 set | 4 set | Silikon kablo; akıma uygun kesit; kilitli konnektör. Kasa/akım testinden sonra ölçülendirilecek. |
| Kasa, ızgara, conta, akustik dolgu | 1 prototip | 4 eş kasa | Kabin hacmi ve pasif radyatör kararı bekleniyor. |
| PCB / delikli pertinaks / kablo demeti | 1 | 4 | Prototipte modüler; nihai sürümde servis edilebilir tek taşıyıcı PCB hedeflenir. |
| Vida, yükseltici, ısı iletken ped, kablo bağı | 1 set | 4 set | Kısa devre ve titreşim önleme için. |

## Yardımcı pasifler ve prototipleme BOM'u

| Referans / kalem | Değer veya tip | Bir ünite | İlk prototip satın alımı | Dört ünite için toplam | Durum |
|---|---|---:|---:|---:|---|
| `R_PU` | 10 kΩ, 1/4 W | 1 | 10'lu paket | 4 | Buton pull-up; aday. |
| `R6` | 10 kΩ, 1/4 W | 1 | 10'lu paket | 4 | DAC `XSMT` → `STAR_GND` pull-down, modül ucuna monte edilir. **Opsiyonel değil**: susturmayı tutan şey bu, GPIO değil (ADR-0011). Lehimden önce modülün `XSMT` pad'i ile 3,3 V arası direnç ölçülür; sert köprü varsa kesilir, yoksa pull-down bir bölücüye dönüşür ([[../02-hardware/circuit-and-wiring-plan#3.3 PCM5102A modül ayarları\|kablolama planı §3.3]]). |
| `R7` | 10 kΩ, 1/4 W | 1 koşullu | 10'lu paket | 4 koşullu | Amfi `SD` → `POWER_GND` pull-down, amfi ucuna monte edilir. **Koşullu**: XH-A232'de erişilebilir `SD` pad'i bulunduğu ölçümle doğrulanana kadar takılmaz; şemalarda kesikli çizilidir. Kart `SD`'yi kendi üzerinde yukarı çekiyorsa değer o pull-up ölçülerek yeniden hesaplanır (ADR-0011, kablolama planı §3.4). |
| `R_LED_R` | 680 Ω, 1/4 W | 1 | 10'lu paket | 4 | Yalnız çıplak RGB LED'de; modül üzerinde direnç varsa `DNP`. |
| `R_LED_G`, `R_LED_B` | 330 Ω, 1/4 W | 2 | 10'lu paket | 8 | Yalnız çıplak RGB LED'de; modül üzerinde direnç varsa `DNP`. |
| `D2` | Seri Schottky veya ideal-diyot modülü; G1 tepe akımını taşır | 1 koşullu | 1 | 4 koşullu | Ters polarite adayı, `DC_IN` ile `VIN` arasında. G1'de ölçülen düşüm ve ısıyla kabul edilir; ret ise yerine 0 Ω köprü gelir (ADR-0020). |
| `C_DB` | 100 nF seramik | 1 isteğe bağlı | 10'lu paket | 4 | Donanım debounce/bypass adayı; firmware testiyle karar verilecek. |
| `C_A` | 1.000 µF / 25 V, 105 °C düşük-ESR hedef | 1 | 1 | 4 | Bulunan perakende ürün yalnız prototip adayı; ESR/ripple/sıcaklık G1'de ölçülecek. 25 V sınıfı 19 V'ta yeterli; adaptör gerilimi yukarı çekilirse 35 V sınıfı seçilir. |
| `C_SAFE` deney bankası | 4×2,2 µF / 400 V kutupsuz film | Değer TBD | 4 ortak deney parçası | Nihai: 4 eş değer | Tweeter empedansı ve G2 süpürmesi olmadan değer dondurulmaz veya sürücüye bağlanmaz. |
| `JP1` + jumper cap | 2 pin 2,54 mm + kısa devre şapkası | 1 | 1×40 header + 1 cap | 4 cap | USB/system 5 V izolasyonu; nihai PCB'de bulunur. |
| Vidalı klemens | KF128V, 5,08 mm, 2 pin | Prototipte 4 | 4 | 16 | Prototip adayı; nihai titreşim dayanımlı kilitli konnektör daha sonra seçilir. |
| Test noktaları `TP0–TP21` | Header kesiti veya prob pedi | Gerektikçe | 1×40 header'dan | Nihai PCB pedi | Üretimde ayrı BOM parçası değildir. |
| Delikli pertinaks | 5×10 cm, tek yüzlü | 0 nihai | 1 | 1 ortak | Yalnız masaüstü prototip. |
| Jumper kablo | 20 cm dişi-erkek, 40'lı | 0 nihai | 1 set | 1 ortak | Yalnız düşük akımlı I2S/GPIO prototipleme; güç/ses çıkışında kullanılmaz. |

Fiyat, bağlantı ve paket adetleri için [[turkey-shopping-list-2026-08-30#Yardımcı pasifler, konnektörler ve prototipleme parçaları|yardımcı parça sepetine]] bakın.

## Elde olanların kabul kontrolü

- [ ] Dört XH-A232 kartın parça kodu, kanal yapısı ve görsel revizyonu aynı mı?
- [ ] Dört woofer ve dört tweeter var mı; her birinin DC direnci kaydedildi mi?
- [ ] PCM5102A kartından henüz alınmadıysa ilk sipariş yalnız bir prototip adedi mi?
- [ ] Adaptör ucu ve jak merkez pozitif mi; ölçü aletiyle doğrulandı mı?
- [ ] Adaptörün sürekli akımı G1'de ölçülen tepe akımı karşılıyor mu?

## İlgili notlar

- [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesi ve maliyet hesabı]]
- [[suppliers|Satıcı ve ürün adayları]]
- [[research-log|Satın alma araştırma günlüğü]]
- [[../power-plan|Güç planı]]
