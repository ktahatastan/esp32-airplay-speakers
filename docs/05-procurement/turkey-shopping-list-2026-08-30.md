---
title: Türkiye satın alma listesi ve maliyet hesabı
aliases:
  - Türkiye alışveriş listesi
  - 2026-08-30 fiyat listesi
tags:
  - procurement
  - bom
  - battery
  - usb-c
status: candidate
updated: 2026-08-30
---

# Türkiye satın alma listesi ve maliyet hesabı

Bu liste dört adet Harman Kardom hoparlör içindir. Hoparlör sürücüleri ve dört XH-A232 / TPA3110 amfi kartı kullanıcıda bulunduğu için maliyete dahil değildir. Ana hesap 4S1P batarya paketine göredir.

> [!warning] Fiyat ve stok kapsamı
> Fiyatlar 2026-08-30 tarihinde KDV dahil görünen perakende fiyatlardır. Kargo, profesyonel nokta kaynak işçiliği, kasa ve akustik malzemeler dahil değildir. Siparişten hemen önce stok, ürün revizyonu ve fiyat tekrar kontrol edilmelidir.

## Önerilen güç ve şarj zinciri

`65 W USB-C PD adaptör + doğrulanmış 5 A/e-marker Type-C kablo -> 20 V PD tetikleyici -> 16.8 V / 2 A ayarlı XL4015 CC/CV -> 4S balanslı BMS -> 4S1P hücre paketi`

USB-C soketi 4S pakete doğrudan bağlanmaz. PD tetikleyici yalnız 20 V anlaşmasını yapar; şarj profilini XL4015 CC/CV katı sağlar. BMS ise son şarj gerilimini ayarlayan şarj cihazı değildir, hücre koruma ve balans katmanıdır. V1'de [[../07-decisions/ADR-0004-v1-charge-policy|şarj sırasında çalma kapalıdır]].

## Her hoparlörün içine alınacak parçalar

| Parça / marka-model | Ne işe yarar | Satıcı ve bağlantı | Adet / hoparlör | Birim fiyat | 4 hoparlör tutarı | Durum |
|---|---|---|---:|---:|---:|---|
| ESP32-S3 N16R8 geliştirme kartı, TLS Robotik `AY-AK027` | AirPlay yazılımı, Wi-Fi/BLE provisioning, I2S, buton/LED ve OTA; 16 MB flash + 8 MB PSRAM | [TLS Robotik](https://www.tlsrobotik.com/urun/esp32-s3-n16r8-wifi-bluetooth-gelistirme-karti/) | 1 | 810,30 TL | 3.241,20 TL | Stokta; **aday**, kart pin dizilimi prototipte doğrulanacak |
| PCM5102A I2S DAC modülü | ESP32'nin I2S dijital sesini line-level stereo analog sese çevirir | [Aletler](https://www.aletler.com.tr/urun/pcm5102a-dac-modul) | 1 | 190,26 TL | 761,04 TL | Stokta; kullanıcı tarafından seçildi |
| MP1584 mini buck modülü | 4S bataryayı ESP32 + DAC için ayarlı 5,10 V'a düşürür | [Robotistan](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) | 1 | 72,74 TL | 290,96 TL | Stokta; **aday**, yük/ripple/ısı testi zorunlu |
| Aspilsan INR18650A28, 2.800 mAh | 4S1P paketin enerji hücreleri; aynı parti ve eşlenmiş alınmalı | [Pilpaketi](https://www.pilpaketi.com/aspilsan-pil) | 4 | 119,36 TL | 1.909,76 TL | **Seçilen aday**; 16 adet aynı parti/eşleme teyidi ve G4 testi gerekli. Üretici sürekli deşarj sınırı 14 A'dir |
| HX-4S-20D / 4S 20 A Li-ion BMS | Hücre aşırı şarj/deşarj, kısa devre koruması ve satıcı beyanına göre balans | [Motorobit](https://www.motorobit.com/4s-20a-li-ion-18650-bms-batarya-koruyucu-balans-devresi) | 1 | 112,80 TL | 451,20 TL | 10+ stok; **teyit bekliyor**, balans akımı/eşiği ve gerçek sürekli akım sorulacak |
| 10K NTC 3950 su geçirmez prob | Hücre paketi sıcaklığını ESP32 ADC üzerinden izler | [Motorobit](https://www.motorobit.com/kablolu-su-gecirmez-10k-ntc) | 1 | 37,83 TL | 151,32 TL | Stokta; yalnız telemetri, bağımsız termal kesme değildir |
| 5–20 V PD/QC/AFC tetikleyici | USB-C adaptörden sabit 20 V PD profili ister | [Meltis Teknoloji](https://www.meltisteknoloji.com/pd-qc-afc-hizli-sarj-adaptorunden-voltaj-tetikleyici-secici-modul-1290) | 1 | 120,40 TL | 481,60 TL | 17 stok; DIP 20 V seçimi yük bağlamadan ölçülecek |
| XL4015 CC/CV Li-ion şarj modülü `20040` | 20 V'u 16,80 V CC/CV şarja çevirir; akım 2,00 A ile sınırlandırılır | [Robotistan](https://www.robotistan.com/xl4015-lipo-sarj-modulu) | 1 | 188,88 TL | 755,52 TL | Stokta; **aday**, kalibrasyon, ters polarite ve termal test zorunlu |
| 16 mm 10–30 V IP65 anlık metal buton | Kısa/uzun basma ile reset, provisioning ve eşleştirme komutları | [Direnc.net](https://www.direnc.net/16mm-10-30v-mavi-baglanti-kablolu-su-gecirmez-metal-yayli-buton) | 1 | 127,86 TL | 511,44 TL | Stokta; LED'i batarya hattından, kontak GPIO'dan sürülecek |
| ASW-13D 12 V / 20 A kalıcı rocker anahtar | Cihazın batarya çıkışını fiziksel olarak keser | [Hatfon](https://www.hatfon.com/urun/anahtar-panel-tip-mavi-asw-13d-isikli-3-pin-12v-20a-kalici-tip) | 1 | 55,34 TL | 221,36 TL | **Teyit bekliyor**; 16,8 V DC kesme değeri ve LED bağlantısı sorulacak |
| 5 mm RGB LED modülü | Wi-Fi, provisioning, hata, şarj ve OTA durumlarını gösterir | [Robotistan](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) | 1 | 17,54 TL | 70,16 TL | Stokta; ortak anot/katot prototipte teyit edilecek |
| 5x20 kablolu sigorta yuvası | Ana sigortayı BMS çıkışına ve hücrelere yakın taşır | [Direnc.net](https://www.direnc.net/fuse-fuse-holders?ps=2) | 1 | 10,23 TL | 40,92 TL | Aday; DC kullanım ve kablo kesiti kontrol edilecek |
| F5AL 5 A hızlı cam sigorta | Kablo/paket kısa devre enerjisini sınırlar | [Robotistan](https://www.robotistan.com/f5al250v-cam-sigorta) | 1 | 1,47 TL | 5,88 TL | Başlangıç adayı; G3 akım ölçümünden sonra değer kesinleşir |

**İç donanım ara toplamı:** 2.223,09 TL / hoparlör; **8.892,36 TL / dört hoparlör**.

## Harici USB-C adaptör ve kablo

| Parça / marka-model | Teknik gerekçe | Satıcı | Adet | Birim fiyat | Toplam |
|---|---|---|---:|---:|---:|
| Syrox GAN65T 65 W adaptör + 1 m Type-C/Type-C kablo seti | Tek USB-C kullanımında 20 V / 3,25 A; 16,8 V / 2 A CC/CV katına yeterli güç marjı | [Trendyol](https://www.trendyol.com/syrox/gan65-type-c-usb-giris-baslik-type-c-kablo-65w-super-hizli-quick-sarj-set-p-793024101) | 1 ortak | 865,00 TL | 865,00 TL |

Syrox üretici kataloğu adaptör profilini ve 1 m Type-C kabloyu doğruluyor; kablonun 5 A/e-marker kimliğini açıkça yazmıyor. Bu nedenle set `candidate` durumundadır: batarya bağlanmadan PD test cihazıyla 20 V profili ve kablo e-marker/5 A bilgisi okunacak, ardından elektronik yükte 40 W yük testi yapılacaktır.

Bir adaptör ve kablo dört hoparlör arasında sırayla kullanılabilir. Dört hoparlörü aynı anda ayrı prizlerden şarj etmek istenirse her ikisinden de dört adet gerekir.

## Maliyet özeti

| Senaryo | İç donanım | Adaptör + kablo | Sarf/izolasyon bütçesi | Genel toplam |
|---|---:|---:|---:|---:|
| 1 hoparlör prototipi + 1 adaptör/kablo | 2.223,09 TL | 865,00 TL | 250,00 TL tahmini | **3.338,09 TL** |
| 4 hoparlör + ortak 1 adaptör/kablo | 8.892,36 TL | 865,00 TL | 1.000,00 TL tahmini | **10.757,36 TL** |
| 4 hoparlör + 4 adaptör/kablo | 8.892,36 TL | 3.460,00 TL | 1.000,00 TL tahmini | **13.352,36 TL** |

Sarf/izolasyon bütçesi; saf nikel şerit, fish-paper, pozitif kutup halkası, silikon kablo, ısı makaronu, klemens/JST, yükseltici ve küçük pasif elemanlar için fiyat tahminidir. Nokta kaynak hizmeti, kargo ve kasa dahil değildir. 4S2P seçilirse dört paket için 16 ek Aspilsan A28 gerekir; yalnız hücre farkı **1.909,76 TL**'dir.

## Sipariş sırası

1. İlk aşamada yalnız bir hoparlörlük elektronik ve dört aynı-parti hücre alınır.
2. BMS satıcısından balans başlangıç gerilimi, balans akımı, sürekli deşarj akımı ve bağlantı sırası yazılı istenir.
3. Güç anahtarı satıcısından 16,8 V DC'deki kesme akımı teyit edilir; teyit gelmezse ayrı DC-rated load switch/MOSFET çözümüne geçilir.
4. XL4015, bataryaya bağlanmadan laboratuvar yükünde 16,80 V / 2,00 A olarak ayarlanır; 30 dakika sıcaklık ve ripple testi yapılır.
5. [[../06-testing/test-strategy|G3 güç]] ve G4 batarya kapıları geçilmeden kalan üç set toplu alınmaz.

## Satıcıya sorulacak zorunlu sorular

- Aspilsan A28 hücrelerin 16 adedi aynı üretim lotundan mı; kapasite ve iç direnç eşleme raporu verilebilir mi?
- Syrox set içindeki Type-C kablo 5 A e-marker içeriyor mu; adaptör tek Type-C kullanımında gerçek 20 V / 3,25 A PD profilini sunuyor mu?
- HX-4S-20D gerçekten pasif balanslı mı; balans eşiği ve akımı kaç mA?
- BMS'nin 10 A sürekli yükte MOSFET sıcaklığı ve koruma eşikleri belgeli mi?
- ASW-13D anahtarın 16,8 V DC'de güvenli kesme akımı kaç A?
- XL4015 kartının ters giriş koruması olmadığı kabul ediliyor mu; kart revizyonu tüm dört adette aynı mı?

## İlgili notlar

- [[bom|BOM ve satın alma listesi]]
- [[suppliers|Satıcılar ve ürün adayları]]
- [[../power-and-battery-plan|Güç ve batarya planı]]
- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı planı]]
