---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer]
updated: 2026-09-12
tags: [adr, power, hardware, safety]
---

# ADR-0020: 24 V DC adaptörle besleme

## Bağlam

Kabinin iki beslemesi var ve ikisinin gerilimi farklı: dört XH-A232 / TPA3110 amfi kartı 8-26 V ister, ESP32-S3 ile PCM5102A ise 5 V'luk bir lojik rayı. Ürün masaüstünde durur, prizin yanındadır ve taşınmaz. Sorun, bir prizden gelen enerjiyi dört amfiye ve iki lojik tüketicisine güvenle dağıtmaktır — ve dört amfinin aynı rayı paylaşırken birbirini aç bırakmamasıdır.

Bu sorunun en kısa cevabı, amfinin kendi giriş aralığının üst ucunda hazır bir adaptördür.

## Karar

Kabin **24 V / 2,9 A (yaklaşık 70 W) DC masaüstü adaptörle** beslenir. Adaptör kutuya **5,5 × 2,1 mm, merkez pozitif** bir barrel jak üzerinden girer. Jaktan gelen ray şemada **`VIN`** adını taşır ve dört amfiyi doğrudan besler. `VIN`'den iki MP1584 sınıfı buck çıkar: **A** ESP32-S3'ü, **B** PCM5102A'yı besler.

```text
24 V DC adaptör -> 5,5 × 2,1 mm jak (VIN) -+-> 4 x XH-A232 / TPA3110 amfi (8-26 V)
                                            +-> MP1584 A 5,10 V buck -> ESP32-S3
                                            `-> MP1584 B 5,10 V buck -> PCM5102A
```

Bağlayıcı kurallar:

- **Boşta çıkış, bağlamadan önce ölçülür ve 25,5 V'un altında olmalıdır.** 24 V, amfinin 26 V azami girişinin 2 V altındadır; tolerans ve yüksüz yükselme o marjı yiyebilir. Sınırı aşan adaptör derate edilmez, reddedilir. Bu bir `G1` satırıdır.
- **Profilin besleme bütçesi adaptör bütçesinden türetilir; tepe tavanları onun altında kalır.** Sekiz BTL kanal 70 W'ın çok üstünü çekebilir; `supply_budget_sq` / `supply_window_ms`, dört amfi birlikte sürülürken 2,9 A'yı aşmayacak biçimde `G1`'in `S7` adımında 4 Ω sınıfı dummy-load ile belirlenir ve tam yükte `VIN` çökmesiyle doğrulanır ([[ADR-0022-dsp-product-output-backend|ADR-0022]]). Bütçeyi taşıyan şey tepe tavanları değil bu alandır: iki dalın toplamı üzerinden bir ortalama, çünkü adaptörün 2,9 A'sı bir ortalamadır ve dört woofer ile dört tweeter tek rayı paylaşır; tepe tavanları `G2`'nin sürücü koruma sayılarıdır. Çöken `VIN` ESP32-S3'ü şarkı ortasında sıfırlayabilir; bu bir ses kalitesi sorunu değil, bir kesinti sorunudur.
- `VIN` amfileri **doğrudan** besler; arada regülatör yoktur. Bulk kapasitör `C_A` tek ve jak girişindedir; amfi başına bulk yalnız `G1` ripple ölçümü isterse eklenir.
- Lojik tarafı **iki ayrı** buck besler; ikisi de yüksüz 5,10 V'a ayarlanır. Ayrılmalarının sebebi tezgâhta duyulan bir şeydir: paylaşılan tek buck DAC'a duyulur hışırtı verdi (sahibin gözlemi, 2026-09-12; düzenek ayrıntısı kaydedilmedi, bu bir kapı geçişi değildir). Buck B'de `JP` köprüsü yoktur; USB'den geri besleme yalnız ESP32 geliştirme kartında vardır.
- **Güç anahtarı yoktur.** Cihaz adaptörü çekilerek kapatılır; boşta bekleme durumunu firmware'in idle standby'ı karşılar (`standby_min`).
- Ters polarite koruması iki katmanlıdır: (1) ilk enerjilendirmeden önce jak polaritesi ölçü aletiyle doğrulanır — `G1` satırı; (2) `VIN` üzerinde seri bir Schottky veya ideal-diyot (`D2`) kablolama planında **aday** olarak durur. 2,9 A'da bir Schottky yaklaşık 1 W veya üstü ısınır; bu, 0 Ω köprü sonucunu güçlendirir ama karar `G1`'deki düşüm ve ısı ölçümünündür.
- Jak kontaklarının 2,9 A sürekli akım değeri tedarikçiye sorulur; belgelenmemiş bir jak BOM'a kilitlenmez.
- Nominal besleme firmware'de bir Kconfig sabitidir: `CONFIG_HK_SUPPLY_MV`, varsayılan `24000`, aralık `8000-26000`, `firmware/main/Kconfig.projbuild` içinde. Ses profilinin tavan ölçekleme alanı bu değere göre çalışır: tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV = 12000`'dir, çünkü tavanlar 12 V'ta alındı; 24 V'ta tavanın aşağı ölçeklenmesi kısıcı yöndedir (yazıldığı durumda sessize doğru hata yapar) ve tek yönlüdür — referansın altındaki bir besleme tavanı yükseltmez. Sürücüdeki gerilimi tezgâh seviyesinde tuttuğu iddiası [[ADR-0022-dsp-product-output-backend|ADR-0022]] ile geri çekildi: TPA3110D2 sabit kazançlıdır (SLOS528F Tablo 2, Tablo 3), çıkış kazanç × giriştir ve ray kırpana kadar beslemeyi izlemez; yarıya inen dijital tavan sürücüdeki voltu sabit tutmaz — tezgâh seviyesi 12 V rayını kırpmadıysa yarıya indirir; kırptıysa 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye tezgâhın duyduğundan fazlasını verebilir. Hangisinin geçerli olduğunu `C3` (amfinin kazanç köprüsü) söyler (`hk_profile.h`).

## Gerekçe

- **Dört amfi tek rayı paylaşıyor ve headroom istiyor.** Aynı rayda dört Class-D kart, tepe anlarında birbirinin gerilimini çeker. 24 V, amfinin kendi aralığının üst noktasıdır ve dört amfi dinleme seviyesinde sürülürken en fazla marjı bırakır. Bu bir dinleme-seviyesi yargısıdır, ölçüm değil; ölçüm `G1`'dedir.
- **Tek besleme, tek ölçüm.** Üç rayın da kaynağı aynı adaptör olduğundan `G1`'deki brownout, besleme çöküşü ve açma/kapama pop ölçümleri tek bir tezgâh düzeninde alınır.
- **24 V / 2,9 A sınıfı adaptör hazır ve sertifikalıdır.** 5,5 × 2,1 mm jak ile 70 W sınıfı masaüstü adaptör ucuz ve yaygındır; kutunun içindeki güç parçaları bir jak, bir bulk kapasitör ve iki buck'tan ibarettir.
- **Anahtar bir bedel, kazanç değil.** DC hattında anahtar, 24 V / 2,9 A kontak değeri belgelenmiş bir parça ve ark testi ister; karşılığında kazanılan şey adaptörü çekmekten ibarettir.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| 19 V adaptör | Dört amfi tek rayda dinleme seviyesinde sürülürken daha az headroom. Bu bir dinleme-seviyesi yargısıdır; iki gerilim arasında ölçülmüş bir karşılaştırma yoktur. 24 V, amfi aralığının üst ucudur ve boşta ölçüm kuralı 2 V'luk marjı korur. |
| 12 V adaptör | Amfi aralığının alt yarısı; aynı sürücülerde en az headroom. Tezgâh referansı olarak kalır (`HK_BENCH_REFERENCE_SUPPLY_MV`), ürün beslemesi olarak değil. |
| Tek paylaşılan 5 V buck | Tezgâhta DAC'a duyulur hışırtı verdi. İki buck'ın bedeli bir modül ve bir test noktasıdır. |
| DC hattında güç anahtarı | Belgeli DC kontak değeri ve `G1` ark/yük testi ister; karşılığı adaptörü çekmekten ibarettir. İstenirse bu ADR'yi supersede eden yeni bir ADR ile açılır. |
| Amfilere ayrı bir regülatör katı | Ek ısı, ek gürültü kaynağı ve ek arıza noktası; amfiler `VIN`'i doğrudan kabul ediyor. |

## Sonuçlar ve açık koşullar

- Kablolama planı, SVG paftası ve KiCad üreteci `VIN`'i jaktan dört amfiye ve iki buck'a dağıtır; test noktası tablosunda `VIN`, `5V_A` ve `5V_B` ayrı ölçüm noktalarıdır.
- `C_A`'nın gerilim sınıfı adaptörün boşta çıkışının üstünde olmalıdır; 25 V sınıfı bir kapasitör 24 V rayda kabul edilmez.
- `G1` kontrol listesine şu satırlar girer: adaptör boşta çıkışı (< 25,5 V), jak polaritesi, tek amfi dummy-load, dört amfi tam yükte `VIN` çökmesi ve brownout, `D2` düşümü ve ısısı, iki buck'ın gürültüsü (DAC'ta ve amfi girişlerinde), açma/kapama pop kaydı.
- Adaptörün akım değeri sabittir (2,9 A); ondan türeyen şey profilin besleme bütçesidir ([[ADR-0022-dsp-product-output-backend|ADR-0022]]), tersi değil. Tepe limiter tavanları `G2`'nin sürücü koruma sayılarıdır; o bütçenin altında kalır, ondan türemez.
- Seri Schottky / ideal-diyot **aday**dır. Kabul edilmesi `G1`'de ölçülen gerilim düşümüne ve ısıya bağlıdır.
- Amfi seviyesi ve limiter eşiklerinin sürücü tarafı bu ADR'nin kapsamında değildir; onlar `G0` ve `G2`'ye bağlıdır ve sürücü empedansı ölçülmeden verilmez. Bu ADR yalnız besleme bütçesinden gelen üst sınırı koyar.
- Bu ADR yalnız besleme topolojisini kilitler. Güç anahtarı veya başka bir besleme kaynağı istenirse yeni bir ADR ile açılır ve `AGENTS.md`'nin kilitli kararlar listesi o ADR ile güncellenir.
