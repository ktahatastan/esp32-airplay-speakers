---
title: Harman Kardom BOM ve satın alma listesi
aliases:
  - BOM
  - Satın alma listesi
tags:
  - procurement
  - hardware
  - bom
status: draft
updated: 2026-08-30
---

# BOM ve satın alma listesi

> [!warning] Fiyat ve stok
> Bağlantılar 2026-08-30 tarihinde kontrol edildi. Fiyat, stok, kargo ve ürün revizyonu değişebilir; siparişten hemen önce yeniden doğrulanmalıdır. `Aday` satırları elektriksel ve mekanik doğrulama tamamlanmadan toplu alınmamalıdır.

## Satın alma stratejisi

1. Önce tek bir hoparlörlük masaüstü prototip kurulur.
2. Hoparlör empedansı ölçülür; 4S beslemede amfi sıcaklığı, çıkış gücü ve tweeter koruması doğrulanır.
3. Batarya prototipi 4S1P ile yapılabilir. Nihai hedef 4S2P ise dört hoparlörün her paketi aynı marka/model, aynı üretim partisi ve eşlenmiş sekiz hücreyle kurulmalıdır.
4. Prototip onayından sonra kalan üç ünite alınır/monte edilir.

## Ana elektronik BOM

| Kalem | Teknik koşul | Bir ünite | 1 ünite prototip | 4 ünite nihai | Durum / kaynak |
|---|---|---:|---:|---:|---|
| ESP32-S3 geliştirme kartı | En az 8 MB flash + 8 MB PSRAM; PCB antenli `N8R8` tercih | 1 | 1 | 4 | Satın alınacak; [Mouser ESP32-S3-DevKitC-1U-N8R8](https://www.mouser.com.tr/ProductDetail/Espressif-Systems/ESP32-S3-DevKitC-1U-N8R8) dış antenlidir, anten ve kasa kararı doğrulanacak. PCB antenli [Robo90 N8R8](https://www.robo90.com/esp32-s3-devkitc-1-n8r8-gelistirme-karti-orjinal) alternatif adaydır. |
| PCM5102A I2S DAC modülü | Stereo, line-level çıkış; kart pin dizilimi kontrolü | 1 | 1 | 4 | Satın alınacak; kullanıcının seçtiği [Aletler PCM5102A modülü](https://www.aletler.com.tr/urun/pcm5102a-dac-modul). |
| XH-A232 / TPA3110 amfi | Stereo Class-D; 4S bataryadan doğrudan besleme adayı | 1 | 0 | 0 | Elde 4 adet olduğu varsayılıyor; adet fiziksel sayımla doğrulanacak. |
| Harman Kardon Nova sürücü takımı | Bir woofer + bir tweeter / ünite | 1 takım | 0 | 0 | Elde olduğu varsayılıyor; DC direnç ve empedans ölçümü zorunlu. |
| 5 V buck regülatör | 4S giriş, sürekli akım ve termal marj; ESP32 + DAC beslemesi | 1 | 1 | 4 | [Robotistan MP1584EN 3 A](https://www.robotistan.com/3a-mini-ayarlanabilir-voltaj-dusurucu-regulator-karti-step-down) aday; yük/ısınma ve ses gürültüsü ölçülecek. |
| Çok işlevli anlık buton | NO, panel tipi veya PCB tipi; 3.3 V GPIO için | 1 | 1 | 4 | [Robotistan KY-004](https://www.robotistan.com/ky-004-buton-modulu) yalnız prototip adayı; nihai panel butonu kasa tasarımına göre seçilecek. |
| RGB durum LED'i | Ortak anot/katot RGB veya adreslenebilir LED; firmware seçimiyle uyumlu | 1 | 1 | 4 | [Robotistan 5 mm RGB modül](https://www.robotistan.com/3-renkli-rgb-led-modulu-5mm-rgb-led) prototip adayı. |
| Sert güç anahtarı | Mandallı; en az 24 V DC / 5 A kontak hedefi | 1 | 1 | 4 | [Direnc.net 16 mm mandallı metal anahtar](https://www.direnc.net/16mm-ledli-12-24v-su-gecirmez-metal-anahtar) aday; sayfa 5 A @ 12 V DC belirtiyor, **16.8 V DC kesme kapasitesi satıcıdan yazılı doğrulanacak**. |

## Batarya ve şarj BOM'u

| Kalem | Teknik koşul | Bir ünite 4S1P | 4 ünite 4S1P | Bir ünite 4S2P | 4 ünite 4S2P | Durum / kaynak |
|---|---|---:|---:|---:|---:|---|
| 18650 Li-ion hücre | Aynı model/parti, başsız, yeni ve eşlenmiş | 4 | 16 | 8 | 32 | **Seçilen aday:** [Pilpaketi Aspilsan INR18650A28](https://www.pilpaketi.com/aspilsan-pil), 2.800 mAh. Üretici sınırı 14 A sürekli deşarjdır; satıcıdaki 25 A koşullu değerdir. G4 eşleme/testi tamamlanmadan `approved` değildir. |
| 4S Li-ion BMS | 16.8 V, gerçek sürekli >=10 A, hücre balansı; NTC tercih | 1 | 4 | 1 | 4 | [Pilburada HXYP 4S 12A](https://www.pilburada.com/hxyp-4s-148v-12a-li-ion-bms-devre-85187) aday; satıcı balans belirtiyor ancak NTC ve sürekli akım bağımsız doğrulanacak. [Elektropil OLT 4S 40A balanslı](https://www.elektropil.com/olt-4s-40a-168v-balansli-lityum-batarya-bms--mor-pcb--m031125) alternatif aday. |
| USB-C PD tetikleyici | PD 20 V profilini seçer; 5 A sınıfı Type-C giriş | 1 | 4 | 1 | 4 | [Meltis 5-20 V PD/QC/AFC seçici](https://www.meltisteknoloji.com/pd-qc-afc-hizli-sarj-adaptorunden-voltaj-tetikleyici-secici-modul-1290) aday; 20 V seçimi batarya bağlanmadan ölçülecek. |
| 16.8 V CC/CV şarj katı | 20 V giriş; çıkış 16,80 V ve 2,00 A'e kalibre edilebilir | 1 | 4 | 1 | 4 | [Robotistan XL4015 CC/CV](https://www.robotistan.com/xl4015-lipo-sarj-modulu) aday; ters polarite koruması yoktur, termal/ripple testi zorunludur. |
| Harici USB-C PD adaptör + kablo | Tek USB-C portta 20 V / 3,25 A PD; kablo 5 A/e-marker | 1 ortak veya 1 | 1 ortak veya 4 | 1 ortak veya 1 | 1 ortak veya 4 | **Düşük maliyetli aday:** kablo dahil Syrox GAN65T 65 W set. Kutu kablosunun e-marker/5 A kimliği USB-C test cihazıyla doğrulanacak; ayrıntı için [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesine]] bakın. |
| Ana sigorta + yuva | BMS çıkışına yakın; değer kablo/amfi testine göre | 1 | 4 | 1 | 4 | Satın alınacak; başlangıç değeri kesinleştirilmedi. DC gerilim ve kesme kapasitesi doğrulanacak. |
| NTC sensör | Seçilen BMS ile uyumlu; hücre grubuna termal temas | 1 | 4 | 1 | 4 | BMS ile birlikte tercih; ayrı giriş/pin değeri doğrulanacak. |
| Nikel şerit / hücre izolatörü / fish-paper | Kaynak akımına uygun saf nikel; artı kutup halkaları ve izolasyon | 1 set | 4 set | 1 set | 4 set | Paket üreticisiyle kesinleştirilecek; nikel kaplı çelik kabul edilmeyecek. |
| Kablo ve konnektör | Silikon kablo; akıma uygun kesit; kilitli konnektör | 1 set | 4 set | 1 set | 4 set | Kasa/akım testinden sonra ölçülendirilecek. |

> [!danger] Lityum paket güvenliği
> Hücrelere doğrudan havya ile lehim yapılmamalı; nokta kaynak, hücre izolatörü, sigorta ve mekanik sabitleme kullanılmalıdır. BMS tek başına güvenli paket tasarımının yerine geçmez. İlk şarj yanmaz yüzeyde, gözetim altında ve hücre gerilimleri ayrı ayrı izlenerek yapılmalıdır.

## Ses yolu ve mekanik tamamlayıcılar

| Kalem | Prototip | 4 ünite nihai | Not |
|---|---:|---:|---|
| Tweeter seri koruma kondansatörü / pasif emniyet filtresi | 2-4 değerlik ölçüm seti | 4 eş set | Değer, gerçek tweeter empedansı ve DSP crossover ölçümünden sonra seçilecek. |
| Giriş/çıkış filtreleme ve lokal bypass kondansatörleri | 1 set | 4 set | DAC-amfi gürültü ve açılış “pop” testine göre. |
| EMI ferrit / common-mode çözümü | 1 deney seti | Test sonucuna göre | Wi-Fi ve Class-D girişimini ölçmeden toplu alınmayacak. |
| Kasa, ızgara, conta, akustik dolgu | 1 prototip | 4 eş kasa | Kabin hacmi ve pasif radyatör kararı bekleniyor. |
| PCB / delikli pertinaks / kablo demeti | 1 | 4 | Prototipte modüler; nihai sürümde servis edilebilir tek taşıyıcı PCB hedeflenir. |
| Vida, yükseltici, ısı iletken ped, kablo bağı | 1 set | 4 set | Kısa devre ve titreşim önleme için. |

## Elde olanların kabul kontrolü

- [ ] Dört XH-A232 kartın parça kodu, kanal yapısı ve görsel revizyonu aynı mı?
- [ ] Dört woofer ve dört tweeter var mı; her birinin DC direnci kaydedildi mi?
- [ ] PCM5102A kartından henüz alınmadıysa ilk sipariş yalnız bir prototip adedi mi?
- [ ] BMS üzerinde `Li-ion 4S`, balans, sürekli akım ve NTC özellikleri yazılı belgeyle doğrulandı mı?
- [ ] Şarj cihazının gerçekten CC/CV profilli olduğu ve uç polaritesi doğrulandı mı?
- [ ] Güç anahtarının DC kesme kapasitesi 16.8 V'ta yeterli mi?

## İlgili notlar

- [[turkey-shopping-list-2026-08-30|Türkiye satın alma listesi ve maliyet hesabı]]
- [[suppliers|Satıcı ve ürün adayları]]
- [[research-log|Satın alma araştırma günlüğü]]
- [[../power-and-battery-plan|Güç ve batarya planı]]
