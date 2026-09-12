---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-09-12
tags: [adr, power, hardware, safety]
---

# ADR-0020: 19 V DC adaptörle besleme

## Bağlam

Her hoparlörün iki beslemesi var ve ikisinin gerilimi farklı: XH-A232 / TPA3110 amfi kartı 8-26 V ister, ESP32-S3 ile PCM5102A ise 5 V'luk bir lojik rayı. Ürün masaüstü bookshelf hoparlörüdür; prizin yanında durur ve taşınmaz. Sorun, bir prizden gelen enerjiyi iki raya güvenle dağıtmaktır.

Bu sorunun en kısa cevabı, amfinin kendi giriş aralığında hazır bir adaptördür.

## Karar

Her hoparlör **19 V DC masaüstü adaptörle** beslenir. Adaptör kutuya **5,5 × 2,1 mm, merkez pozitif** bir barrel jak üzerinden girer. Jaktan gelen ray şemada **`VIN`** adını taşır.

```text
19 V DC adaptör -> 5,5 × 2,1 mm jak (VIN) -+-> XH-A232 / TPA3110 amfi (8-26 V)
                                            `-> MP1584 5,10 V buck -> ESP32-S3 + PCM5102A
```

Bağlayıcı kurallar:

- `VIN` amfiyi **doğrudan** besler; arada regülatör yoktur. 19 V, XH-A232'nin 8-26 V giriş aralığının içindedir.
- Mantık tarafını mevcut 5 V buck (MP1584 sınıfı) besler; girişi `VIN`, çıkışı yüksüz 5,10 V'a ayarlanır ve ESP32-S3 ile DAC'ı sürer.
- **V1'de güç anahtarı yoktur.** Cihaz adaptörü çekilerek kapatılır; boşta bekleme durumunu firmware'in idle standby'ı karşılar (`standby_min`).
- Ters polarite koruması iki katmanlıdır: (1) ilk enerjilendirmeden önce jak polaritesi ölçü aletiyle doğrulanır — bu bir `G1` kontrol satırıdır; (2) `VIN` üzerinde seri bir Schottky veya ideal-diyot, kablolama planında **aday** olarak durur ve `G1`'de ölçülen düşümle ya kabul edilir ya da düşer.
- Nominal besleme firmware'de bir Kconfig sabitidir: `CONFIG_HK_SUPPLY_MV`, varsayılan `19000`, aralık `8000-26000`, `firmware/main/Kconfig.projbuild` içinde. Ses profilinin tavan ölçekleme alanı bu değere göre çalışır: tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV = 12000`'dir, çünkü tavanlar 12 V'ta alındı; 19 V'ta tavanın aşağı ölçeklenmesi doğru yöndür ve biri 24 V'luk bir adaptör takarsa tweeter'ı koruyan şey yine bu tek çarpımdır.

## Gerekçe

- **Amfi zaten bu gerilimi istiyor.** 8-26 V giriş aralığı olan bir Class-D kartı için 19 V, ara kat gerektirmeyen bir noktadır. Tek dönüşüm, zaten var olan 5 V buck'tır.
- **19 V dizüstü adaptörlerin standart gerilimidir.** 5,5 × 2,1 mm jak ile 3-4 A sınıfı adaptör ucuz, yaygın ve sertifikalıdır; kutunun içindeki tek "güç parçası" bir jak ve bir buck'tır.
- **Tek besleme, tek ölçüm.** İki rayın da kaynağı aynı adaptör olduğundan `G1`'deki brownout, besleme çöküşü ve açma/kapama pop ölçümleri tek bir tezgâh düzeninde alınır.
- **Anahtar bir bedel, kazanç değil.** DC hattında anahtar, 19 V DC kontak değeri belgelenmiş bir parça ve ark testi ister; V1'de bunun karşılığında kazanılan şey adaptörü çekmekten ibarettir. Firmware'in idle standby'ı boşta bekleme davranışını zaten veriyor.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| 12 V adaptör | Amfi aralığının içinde ama alt yarısında; aynı sürücülerde daha az headroom. 19 V hem yaygın hem 26 V'un altında güvenli marj bırakır. |
| 24 V adaptör | Amfi aralığının üst sınırına 2 V kalır; adaptör toleransı ve yüksüz çıkış yükselmesi sınırı aşabilir. Tavan ölçeklemesi bu durumu yakalar ama tasarım o kenara yaslanmaz. |
| DC hattında güç anahtarı (V1) | Belgeli DC kontak değeri ve `G1` ark/yük testi ister; V1'de karşılığı yok. Sonraki sürüm için kapalı değil, açık bir madde olarak kablolama planındadır. |
| Amfiye ayrı bir regülatör katı | Ek ısı, ek gürültü kaynağı ve ek arıza noktası; amfi zaten `VIN`'i doğrudan kabul ediyor. |

## Sonuçlar ve açık koşullar

- Kablolama planı, SVG paftası ve KiCad üreteci `VIN`'i jaktan iki raya dağıtır; test noktası tablosunda `VIN` ve buck çıkışı ayrı ölçüm noktalarıdır.
- `G1` kontrol listesine üç satır girer: jak polaritesi ölçümü, `VIN` üzerinde brownout / besleme çöküşü davranışı ve açma/kapama pop kaydı.
- Adaptörün akım değeri tahmin edilmez, ölçülür: seçilen amfi seviyesinde sürekli çekilen akım artı dijital tarafın payı `G1`'de dummy-load üzerinde kaydedilir ve BOM'daki adaptör satırı o rakama göre kilitlenir.
- Seri Schottky / ideal-diyot **aday**dır. Kabul edilmesi `G1`'de ölçülen gerilim düşümüne ve ısıya bağlıdır.
- Amfi seviyesi ve limiter eşiği bu ADR'nin kapsamında değildir; onlar `G0` ve `G2`'ye bağlıdır ve sürücü empedansı ölçülmeden verilmez.
- Bu ADR yalnız besleme topolojisini kilitler. Güç anahtarı veya başka bir besleme kaynağı istenirse yeni bir ADR ile açılır ve `AGENTS.md`'nin kilitli kararlar listesi o ADR ile güncellenir.
