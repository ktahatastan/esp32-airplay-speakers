---
status: active
owner: hardware-engineer
updated: 2026-08-30
tags: [power, battery, hardware]
---

# Harman Kardom güç ve batarya planı

Güncelleme: 2026-08-30

## Karar özeti

Her aktif hoparlör için önerilen ana enerji mimarisi `4S Li-ion` pakettir:

- 14,8 V nominal, 16,8 V tam dolu.
- XH-A232 / TPA3110 amfi doğrudan BMS çıkışından beslenecek.
- ESP32-S3 ve PCM5102A için ayrı bir 5 V buck regülatör kullanılacak.
- İlk prototipte şarj sırasında çalma kapalı olacak.
- Şarj olurken kesintisiz çalma, doğrulanmış power-path devresiyle ikinci aşamada eklenecek.

Bu seçim, TPA3110D2'nin 16 V beslemede 8 ohm yükte yaklaşık 15 W/kanal veri sayfası çalışma noktasına yakındır. 5S Li-ion paket tam doluyken 21 V'a çıktığı için sürücüler doğrulanmadan kullanılmayacaktır. Hoparlörlerin gerçek empedansı ölçülene kadar nihai güç ve limiter ayarı kilitlenmeyecektir.

## Önerilen blok şema

```text
16,8 V CC/CV şarj adaptörü
             |
          şarj jakı
             |
      4S balanslı BMS
             |
       4S1P / 4S2P paket
             |
       ana sigorta + anahtar
             |
             +--------------------> XH-A232 (batarya gerilimi)
             |
             +--> MP1584 5 V ----> ESP32-S3 + PCM5102A
```

## Batarya seçenekleri

### Kompakt prototip: 4S1P

- 4 adet aynı model ve aynı partiden 18650 hücre.
- Aspilsan A28 kullanılırsa yaklaşık 14,6 V / 2,8 Ah / 40,9 Wh.
- Tam güçte yaklaşık 1 saat, normal dinleme seviyesinde kabaca 3-5 saat hedeflenebilir.
- Öncelik: tek hoparlör prototipi ve elektriksel doğrulama.

### Önerilen nihai paket: 4S2P

- Hoparlör başına 8 adet aynı model ve aynı partiden 18650 hücre.
- Aspilsan A28 kullanılırsa yaklaşık 14,6 V / 5,6 Ah / 81,8 Wh.
- Normal dinleme seviyesinde kabaca 6-10 saat hedeflenebilir.
- Dört hoparlör için toplam 32 hücre gerekir.

Çalışma süresi ses seviyesi, DSP, Wi-Fi kalitesi ve hücre yaşına bağlıdır; prototip ölçümleriyle düzeltilecektir.

## Hoparlör başına güç BOM'u

| Kalem | Adet | Asgari özellik | Durum / aday |
|---|---:|---|---|
| Li-ion hücre | 4 veya 8 | Aynı model/parti; yeni ve eşlenmiş | Aspilsan INR18650A28 2800 mAh seçilen aday; G4 bekliyor |
| BMS | 1 | 4S Li-ion, gerçek balans, en az 10 A sürekli, aşırı akım/kısa devre/aşırı şarj/deşarj; tercihen NTC | Satın almadan önce balans ve NTC doğrulanacak |
| Harici şarj cihazı | 1 | 16,8 V CC/CV, 2-3 A | 4S 16,8 V 3 A adaptör adayı |
| 5 V regülatör | 1 | 4,5-28 V giriş, 5 V / en az 2 A | MP1584EN 3 A modül; 5,10 V'a ayarlanıp yük altında test edilecek |
| Sigorta | 1 | DC uygun, batarya artısına çok yakın | 5 A veya 7,5 A; yük testinden sonra seçilecek |
| Şarj jakı | 1 | 16,8 V / 3 A DC | 5,5 x 2,1 mm veya kilitli DC jak; polarite standardize edilecek |
| Sıcaklık sensörü | 1 | 10k NTC, orta hücreye temas | BMS destekliyorsa zorunlu bağlanacak |
| Güç ölçümü | 1, opsiyonel | 36 V'a kadar çift yönlü akım/gerilim | INA226 modülü prototip telemetrisi için |
| Fonksiyon butonu | 1 | Anlık, normalde açık | Provisioning ve reset; aktif-low GPIO |
| RGB durum LED'i | 1 | Ortak katot, 3 kanal | Her renge seri direnç ve PWM GPIO |
| Fiziksel güç anahtarı | 1 | Kilitlemeli, en az 24 V DC / 5 A kontak hedefi | KM103 / DC-132A seçilen mekanik aday; kontak değeri belgelenmediği ve dahili LED yalnız 12 V olduğu için 16,8 V hatta G3 öncesi bağlanmaz |
| Paket izolasyonu | 1 set | Hücre tutucu, fish-paper halka, Kapton, nikel şerit, ısı makaron | Zorunlu |

## Araştırılan satın alma adayları

- PCM5102A DAC: https://www.aletler.com.tr/urun/pcm5102a-dac-modul
- Aspilsan INR18650A28 hücre: https://www.pilpaketi.com/aspilsan-pil
- Aspilsan A28 üretici veri sayfası: https://www.aspilsan.com/wp-content/uploads/2025/05/A28_Public_Datasheet_.pdf
- 4S 20 A balanslı BMS adayı: https://www.candagrup.com/4s-20a-bms-lityum-18650-balans-pil-sarj-koruma-devresi
- Alternatif 4S BMS kaynağı: https://www.pilmak.com/urun/4s-20a-168v-bms-hx-4s-bm20/
- 16,8 V / 3 A şarj adaptörü adayı: https://www.pilpaketi.com/lion-sarj-aleti-4s-16.8-volt-3a-2.1-mm-soket
- MP1584EN buck modülü: https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down
- INA226 güç ölçüm modülü: https://www.robotistan.com/ina226-i2c-akim-sensoru

Fiyat ve stok bilgisi kalıcı kabul edilmeyecek; satın alma gününde tekrar doğrulanacaktır. BMS ilanındaki `20 A` ifadesi gerçek sürekli akım anlamına gelmeyebilir. Kart üzerindeki MOSFET, bakır kalınlığı, balans dirençleri ve sıcaklık koruması görsel/veri sayfasıyla kontrol edilmeden satın alma kesinleştirilmeyecektir.

## Şarjdayken çalma seçenekleri

### Sürüm 1: güvenli ve basit

- Şarj adaptörü doğrudan BMS'nin ortak şarj/deşarj portuna bağlanır.
- Ana amfi şarj sırasında kapalı tutulur.
- BMS yalnızca koruma ve balans sağlar; CC/CV şarj profilini 16,8 V şarj adaptörü sağlar.
- Bu sürüm ilk kabin ve ses testleri için kullanılacaktır.

### Sürüm 2: gerçek power-path

- Adaptör bağlıyken sistemi adaptörden çalıştıran, kalan gücü bataryaya veren power-path şarj katı kullanılacaktır.
- Aday mimari: TI BQ24610 tabanlı 19 V girişli 4S şarj/power-path kartı.
- Daha gelişmiş USB-C PD seçeneği: TI BQ25792 tabanlı 1-4S buck-boost/NVDC kart; I2C yazılımı ve özel PCB gerektirir.
- Hazır kart bulunmazsa bu iş özel PCB olarak ele alınacaktır. Rastgele bir boost şarj modülü şarjdayken çalma için kullanılmayacaktır.

## Ses ve EMI kuralları

- MP1584 ve BMS, PCM5102A'nın analog çıkışından ve amfi giriş kablolarından uzakta konumlandırılacak.
- 5 V hattında buck çıkışına yakın düşük ESR kapasitör ve gerekirse ferrit/LC filtre denenecek.
- Güç ve analog ses toprakları yıldız noktada birleştirilecek; amfi hoparlör eksi uçları hiçbir zaman şaseye bağlanmayacak.
- Wi-Fi yayın akımı sıçramalarında ESP32 brownout testi yapılacak.
- Şarj adaptörü bağlıyken dip gürültüsü, cızırtı ve ground-loop ölçülecek.

## Mekanik ve güvenlik

- Hücrelere doğrudan havya uygulanmayacak; paket punta kaynakla veya profesyonel paket üreticisiyle hazırlanacak.
- Hücreler aynı model, kapasite, yaş ve başlangıç voltajında olacak.
- Batarya bölmesi akustik hacimden rijit bir duvarla ayrılacak ve dış ortama kontrollü şekilde havalandırılacak.
- BMS, sigorta ve kablolar pasif radyatör/woofer hareket alanına girmeyecek.
- Güç anahtarı amfi ve 5 V buck hattını kesecek; şarj jakı BMS tarafında kalacağı için cihaz kapalıyken şarj mümkün olacak.
- İlk şarjlar yanmaz yüzeyde, gözetim altında ve hücre sıcaklığı izlenerek yapılacak.
- Paket düşme, kısa devre ve ters polarite testinden geçmeden kabin kapatılmayacak.

## Teknik kaynaklar

- TPA3110D2: https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf
- BQ24610: https://www.ti.com/lit/ds/symlink/bq24610.pdf
- BQ25792: https://www.ti.com/lit/ds/symlink/bq25792.pdf
- MP1584: https://pdf.direnc.net/upload/mp1584en-lf-z-datasheet.pdf
- INA226: https://www.ti.com/lit/ds/symlink/ina226.pdf
