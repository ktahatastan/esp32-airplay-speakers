---
status: pending
owner: acoustics-engineer
updated: 2026-09-08
tags: [drivers, measurements, gate]
---

# Sürücü ölçüm planı — G0

## Eldeki sürücüler (operatör kaydı, 2026-09-08)

Harman Kardon Nova'dan sökülmüş. Etiketlerde yalnız üretici iç kodları var;
bunlar veri sayfasına çevrilmiyor, dolayısıyla **ölçüm tek yol**.

| sürücü | etiketteki kodlar |
|---|---|
| woofer/mid | `66057-0001006` · `110066` · `06801` · `2313351` |
| tweeter | `660056-0001004` · `310013` · `01007` · `2113356` |

**Woofer'ın arkasına ek mıknatıs yapıştırılmış.** Bu bir not değil, ölçümü
etkileyen bir gerçek: ek mıknatıs motor kuvvetini (`Bl`) değiştirir, dolayısıyla
`Fs`, `Qts` ve `Qes` fabrika değerlerinden sapar. Aynı modelin başka bir yerde
yayınlanmış parametresi bulunsa bile bu sürücüye uymaz. Aşağıdaki ölçümler bu
yüzden isteğe bağlı değil.

## Ölçüldü — DC direnç (operatör, 2026-09-08)

| sürücü | okunan `Re` | çıkarım |
|---|---|---|
| woofer/mid | **4,0 Ω** | nominal **4 Ω** |
| tweeter | **3,5 Ω** | nominal **4 Ω** |

Kural: `Re` genelde nominal empedansın 0,75–0,85 katıdır. 4 Ω'luk bir sürücü
tipik olarak 3,0–3,6 Ω okur, 6 Ω'luk 4,4–5,0 Ω. Woofer'ın 4,0'ı iki aralığın
sınırında duruyor; **prob direnci çıkarılmadıysa** gerçek değer ~3,7 Ω olur ve
tereddüt kalmaz. Tweeter'ın 3,5'i zaten net biçimde 4 Ω sınıfı.

### Bu neyi açıyor, neyi açmıyor

**Açtığı:** her iki sürücü de 4 Ω sınıfı. TPA3110 BTL için tipik asgari yük
4 Ω'dur, yani zincir sınırın içinde — ama sınırın *üstünde* değil, tam üstünde.
Bu, güvenli amfi seviyesi ve termal bütçe için ilk somut girdi.

**Açmadığı:** `Re` bir sayıdır, empedans eğrisi değildir. Hâlâ eksik olanlar:

- **Tweeter `Fs`.** Yüksek geçiren filtrenin en düşük güvenli kesim frekansını bu
  belirler (kural: `Fs`'nin en az iki katı). Bu ölçülmeden crossover köşesi
  muhafazakâr bir tahmindir, ölçüm değil.
- **Empedans minimumu.** Müzikte gerçek yük `Re`'nin altına inebilir; amfinin
  akım sınırı buna bakar.
- **Woofer `Fs`/`Qts`.** Arkasına ek mıknatıs yapıştırıldığı için yayınlanmış
  hiçbir değer geçerli değil.

Yani `AGENTS.md`'deki "sürücü empedansı doğrulanmadı" tıkayıcısı **kısmen**
kapandı: nominal empedans biliniyor, koruyucu filtrenin köşesi hâlâ bilinmiyor.

## Tweeter seri kondansatörü `C_SAFE` (2026-09-08)

**Seçilen değer: 10 µF, kutupsuz film, ≥ 50 V.**

Bu bir crossover değil, **emniyet supabı**: asıl filtreleme DSP'de olacak
(LR4, 24 dB/oktav). Bunun işi firmware çökerse, DSP yanlış yüklenirse veya biri
tam bantlı sinyal gönderirse tweeter'ı hayatta tutmak.

Değeri seçen şey `Fs`'ye olan mesafe. 4 Ω'da `C = 1/(2π·R·Fc)`:

| C | köşe | `Fs`=1200 | `Fs`=1500 | `Fs`=2000 |
|---:|---:|---:|---:|---:|
| **10 µF** | 3980 Hz | 3,3× | 2,7× | **2,0×** |
| 15 µF | 2650 Hz | 2,2× | 1,8× | 1,3× |
| 22 µF | 1810 Hz | 1,5× | 1,2× | 0,9× |

25 mm kubbe için makul `Fs` aralığı 1200–2000 Hz. 10 µF bu aralığın **tamamının**
en az iki katı üstünde kalıyor. Daha büyük bir kondansatörün köşesi rezonansa
yaklaşır, ve orada seri kondansatör koruma sağlamaz — empedans tepesiyle birlikte
rezonans devresi kurar ve yanıtı tepelendirir, yani korumak istediği yerde
eksürsiyonu artırır.

**Bedeli:** 3,5 kHz'de −3,6 dB, yani DSP kesimiyle üst üste biniyor ve akustik
geçiş noktasını yukarı itiyor. `Fs` ölçülene kadar bu kabul ediliyor: şu anda
DSP crossover'ı yok, dolayısıyla bu kondansatör tweeter'ın tek koruması, ve
bilinmeyen bir `Fs`'ye karşı sağlamlık geçiş bandındaki 3 dB'den önce gelir.

**Kutupsuz olması şart, sebebi BTL:** amfi köprülü çıkışlı, hoparlörün eksi ucu
toprak değil, o da salınıyor. Kutuplu bir kondansatör orada ters gerilim görür.

`Fs` ölçüldükten sonra yeniden değerlendirilir; değiştirmek tek lehim noktasıdır.

## Empedans eğrisi ve `Fs` — tezgâh yordamı

Yukarıdaki "açmadığı" listesini ve `C_SAFE`'in beklediği `Fs`'yi kapatan ölçüm
bu. Gereken alet: **kart, tek bir direnç ve elindeki multimetre.** Osilatör,
ses kartı, empedans köprüsü, ölçüm mikrofonu yok.

> **Hiçbir sonuç burada "ölçüldü" olarak kayıtlı değil.** Aşağıdaki tabloların
> hepsi boş ve öyle kalacak. Bir agent fiziksel bir testi geçmiş sayamaz; sayıyı
> tezgâhtaki operatör yazar.

### 1. Firmware: taramayı üreten mod

`hk_tone.c`'deki sabit 1 kHz tonun yerine adım adım ilerleyen bir sinüs taraması
koyar. Üç Kconfig sembolü, ve üçü de yerel, **commit edilmeyen** bir parçaya
yazılır (`sdkconfig.bench` bunları taşımaz; taramayı açmak tezgâha bakarak
verilen bir karardır, dosyadan miras alınan bir ayar değil):

```
CONFIG_HK_BENCH_TONE_INSTEAD_OF_AIRPLAY=y
CONFIG_HK_BENCH_SWEEP=y
CONFIG_HK_BENCH_SWEEP_FINE_HZ=0        # 0: kaba tarama
```

```bash
idf.py -C firmware -B firmware/build-bench \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench;<yerel-parça>" \
  -D SDKCONFIG="$PWD/firmware/build-bench/sdkconfig" build
```

Tarama başlayınca **sonuna kadar kendi kendine gider**: 10 s sessizlik, 20 s
kurulum adımı, sonra 22 adım × 8 s. Toplam 3 dk 26 s. Buton yok, giriş yok —
adım başına tuşa basılması gereken bir ölçüm dokuzuncu adımda yarım kalır, ve
yarım bir tablo hiç tablo olmamasından kötüdür çünkü birileri ondan sonuç
çıkarmaya çalışır.

`sdkconfig.bench`'teki iki tezgâh istisnası da gerekli, yoksa `XSMT` düşük kalır
ve DAC hiç çıkış vermez. Bu, bir sonraki bölümün konusu.

### 2. GÜVENLİK — amfi devrede OLMAYACAK

> **`CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE` ve
> `CONFIG_HK_BENCH_AUDIO_WITHOUT_POWER_TELEMETRY` olmadan DAC susturulmuş kalır,
> yani tarama için bu iki sembol zorunlu. Ama aynı iki sembol amfinin mute
> hattını da bırakıyor. DAC'ı açıp amfiyi kapalı tutan bir ayar yok.**
>
> Amfiyi devre dışı bırakmak **yapılandırmayla değil, kabloyla** olur: TPA3110'un
> girişini sök ya da beslemesini kes. Firmware kartın nasıl kablolandığını
> göremez; bu yüzden tarama uyarıyı basıp **ilk adımdan önce 10 saniye susuyor**.
> O 10 saniye, uyarıyı okuyup güç anahtarına yetişmek içindir.

Amfi devrede kalırsa ne olur, açıkça: `-6 dBFS`, 36 dB'lik bir kazançla
(bkz. [[../06-testing/bench-measurement-order|tezgâh ölçüm sırası]], `C3`) 4 Ω'a
kırpma noktasının çok ötesinde, ve tweeter'ın rezonansının yakınında **sekizer
saniye** tutuluyor. Bir tweeter'a yapılabilecek en zorlayıcı şeylerden biri budur.

**Tweeter'ı bu devrede ne koruyor:**

1. **Amfi yok.** Tek cümlelik cevap bu, gerisi ayrıntı. Zincirde 470 Ω var;
   sürücüye giden güç rezonans dışında ~0,02 mW, en yüksek tepede yarım
   milivatın altında. Tweeter'ların anma gücü watt mertebesinde — arada dört
   basamak var.
2. **Seri direnç akımı mutlak sınırlıyor.** Sürücü ne yaparsa yapsın, kısa devre
   olsa bile, akım 2,2 mA rms'i geçemez.
3. **DC yok.** Her blok tam sayıda çevrim taşıyor, ortalaması sıfır; bobini
   merkezden kaydıracak bir ofset üretilmiyor.
4. **Adımlar tıklamıyor.** Her blok faz sıfırda başlayıp faz sıfırda bitiyor,
   yani frekans değişirken dalga formu iki taraftan da sıfırdan geçiyor: eğimi
   değişiyor, değeri sıçramıyor. Sıçrasaydı her adım tweeter'a geniş bantlı bir
   darbe olurdu ve 22 adımda 22 darbe ederdi.
5. **Susturma kapısı yerinde duruyor.** `hk_tone` mute hatlarına dokunmuyor;
   `hk_audio_hw` sekansı bu iş için değiştirilmedi.

Koruyamadığı tek şey yanlış kablolamadır, ve o yüzden bu bölüm bu kadar uzun.

**Neden DAC'ın hat çıkışında, amfinin çıkışında değil:** `AGENTS.md` —
*"BTL amplifier speaker negatives are not chassis ground."* Aynı seri direnç
numarasını TPA3110'un çıkışında denemek, metrenin şasi ucunu sürülen bir düğüme
bağlamak olur. DAC'ın hat çıkışında `−` gerçekten topraktır; ölçüm ancak orada
geçerli.

### 3. Devre

```text
DAC hat çıkışı (L ya da R) ──[ Rs = 470 Ω, 1/4 W ]──┬── sürücü (+)
                                                    │
                                                (sürücü)
                                                    │
DAC GND ────────────────────────────────────────────┴── sürücü (−)

                       multimetre: AC volt, SÜRÜCÜNÜN üstünde
```

Tarama aynı örneği iki kanala da yazıyor, yani `L` ya da `R` fark etmez.

**Tek, çıplak sürücü.** Seri başka hiçbir şey olmayacak: tweeter'ın 10 µF
`C_SAFE` kondansatörü takılıysa bu ölçüm için **çıkar**. 40 Hz'de o kondansatör
yaklaşık 400 Ω'dur ve ölçtüğün şey bobin değil kondansatör olur.

**Sürücü serbest havada olmalı.** Masaya yüzükoyun yatırılmış bir sürücünün
önünde hapsolan hava `Fs`'yi yukarı çeker. Kenarından tut ya da as; ölçüm
sırasında elleme, konumunu değiştirme.

#### Neden 470 Ω

- Sürücülerin DC direncinin **100 katından fazlası**. Akımı sabit saymak
  rezonans dışında ~%1, 30 Ω'luk bir tepede ~%6 hata verir — ve o %6'nın tam
  düzeltmesi aşağıda.
- DAC'ın gördüğü yük ~474 Ω olur; bir hat çıkışı için hafif. Daha küçük direnç
  daha büyük okuma verir ama PCM5102A'nın çıkış katını kulaklık yüküne doğru
  iter, orada davranışı tanımlı değildir ve ölçüm sürücüyle ilgili olmaktan
  çıkar.
- `-6 dBFS` ile birlikte **eğrinin tamamını tek bir 200 mV kademesine sokar**.
- Üzerinde `2,1² / 470 = 9,4 mW` harcanır; 1/4 W fazlasıyla yeter.
- E12 değeri, yani gerçekten elde bulunur.

Firmware bu değeri yalnız log'a basar, hiçbir hesapta kullanmaz. Başka bir değer
taktıysan hesabı onunla yap ve tabloya **taktığın değeri** yaz.

#### Metre nasıl ayarlanır

- **AC volt (`V~`).** Manuel kademesi varsa **200 mV**. `-6 dBFS` ve 470 Ω tam
  olarak bunun için seçildi: eğrinin tamamı ≈8 mV ile ≈150 mV arasına düşer.
- Otomatik kademeli metre de olur; 8 saniyelik bekleme kademe değişimini de
  kapsayacak kadar uzun.
- Woofer'ın tepesi 200 mV'u **taşabilir** — çift mıknatıs yığını `Qes`'i
  düşürüp tepeyi yükselttiği için bu hiç de uzak bir ihtimal değil. O tek okumayı
  2 V kademesinde al: 200 mV civarında 2 V kademesinin 1 mV çözünürlüğü zaten
  %0,5'tir, kaybedilen bir şey yok. **Küçük okumaları 2 V kademesinde alma.**
  Tek kademede kalmayı tercih ediyorsan 470 yerine 680 Ω tak; taban ~6 mV'a iner
  ama tepe kesin sığar.

**Metrenin AC bant genişliği — tweeter için asıl tuzak.** Ucuz bir multimetrenin
AC volt özelliği genelde yalnız 40–400 Hz arası için tanımlıdır. Woofer'da sorun
yok. Tweeter'ın `Fs`'si 1 kHz'in üstünde ve orada böyle bir metre giderek daha
düşük okur.

Rolloff yumuşak, rezonans tepesi keskin olduğu için tepe çoğu zaman yine
görünür — ama yerini birkaç yüzde kaydırabilir. Yapılacaklar:

1. Metrenin veri sayfasındaki AC bant genişliğine bak ve tabloya **yaz**.
   ≥5 kHz true-RMS ise mesele yok.
2. Değilse: aynı taramayı **ikinci kez**, metre bu sefer `Rs`'nin üstünde koş.
   İki okumanın **oranı** metrenin frekans hatasını götürür, çünkü ikisi de aynı
   hatayla ölçülüyor:

   ```text
   Z = Rs × V_sürücü / V_Rs
   ```

   Bu formül `Rs >> Z` varsayımına da ihtiyaç duymaz, yani hem metrenin bandını
   hem de sabit-akım yaklaşımını aynı anda ortadan kaldırır. Bedeli iki koşu ve
   iki tablo. Tweeter için buna değer.

### 4. Okumayı ohm'a çevirmek

`Rs` empedanstan çok büyük olduğu için akım neredeyse sabit:

```text
I ≈ V_kaynak / Rs ≈ 1,05 V / 470 Ω ≈ 2,2 mA rms
```

Yani sürücünün üstündeki gerilim empedansla **doğru orantılı**. Ölçekleme için
ayrı bir referans frekansına gerek yok — kalibrasyon noktası zaten elimizde:
tablodaki **en küçük okuma**, `Z`'nin `Re`'ye en yakın olduğu noktadır, ve `Re`
yukarıda ölçüldü (woofer 4,0 Ω, tweeter 3,5 Ω).

```text
Z(f) ≈ Re × V(f) / V_min
```

Bu yaklaşım tepede empedansı **biraz küçük** gösterir, çünkü orada akım gerçekten
sabit değildir. Okuma `V_min`'in ~5 katını geçtiğinde düzelt:

```text
Z = Z_yak × Rs / (Rs + Re − Z_yak)
```

Örnek (woofer, `Rs` = 470 Ω, `Re` = 4,0 Ω):

```text
V_min  = 8,8 mV  @ 501,1 Hz
V_tepe = 96,0 mV @ 159,8 Hz
Z_yak  = 4,0 × 96,0 / 8,8            = 43,6 Ω
Z      = 43,6 × 470 / (470 + 4,0 − 43,6) = 47,6 Ω
Fs     ≈ 160 Hz
```

> **`Fs` için bu aritmetiğin hiçbirine gerek yok.** Yukarıdaki düzeltmelerin
> hepsi monoton: tepe hangi satırdaysa `Fs` odur. Aritmetik yalnız tepenin
> **yüksekliğini** ohm'a çevirmek için lazım. Yani ölçekleme yanlış yapılsa bile
> bu ölçümün asıl ürünü — koruyucu yüksek geçirenin köşesini belirleyen frekans —
> etkilenmez.

### 5. `Fs`: en BÜYÜK okuma

Rezonansta empedans **tepe** yapar, dip değil. Sürücü orada en az akımı çeker ve
metre orada en büyük gerilimi görür. Tablodaki en büyük satır `Fs`'dir.

Kaba tarama `Fs`'yi ~%13 içinde verir. İnce taramayı çalıştır: kaba taramanın
tepe frekansını `CONFIG_HK_BENCH_SWEEP_FINE_HZ` değerine yaz ve tekrar yükle.
İkinci koşu o frekansın iki yanında 2/3 oktav boyunca 1/12 oktavlık 17 adımdır
ve `Fs`'yi ~%3'e indirir. Yüksek geçiren köşesinin türetildiği sayı bunu hak
ediyor.

Sonra: **HPF köşesi ≥ 2 × `Fs`** ([[../04-acoustics/measurement-and-dsp-plan|ölçüm
ve DSP planı]], 3. ve 5. maddeler).

### 6. Beklenen değerler — yanlış cevabı fark etmek için

| sürücü | beklenen `Fs` | beklenen `Z_maks` | şundan şüphelen |
|---|---|---|---|
| woofer/mid, 60 mm | ~100–250 Hz | ~20–80 Ω, belki daha yüksek | < 60 Hz veya > 400 Hz |
| tweeter, 25 mm kubbe | ~1200–2000 Hz | ~8–30 Ω | < 700 Hz veya > 3 kHz |

Tweeter satırı, yukarıdaki `C_SAFE` bölümünün zaten kullandığı aralıktır. Ölçüm
o aralığın dışına düşerse **kondansatör kararı da yeniden bakılır**, çünkü 10 µF
seçimi tam olarak o aralığın en az iki katı üstünde kalmak üzere seçildi.

Ek mıknatıs konusunda yukarıdaki notu biraz keskinleştirmek gerekiyor, çünkü
ölçüm sırasında ne bekleneceğini değiştiriyor: çift mıknatıs yığını `Bl`'yi
büyütür, bu esas olarak `Qes`'i düşürür ve **tepenin boyunu uzatır**. `Fs`'yi
hareketli kütle ve süspansiyon belirler, mıknatıs ikisine de dokunmaz. Yani
fabrika değerinden asıl sapma tepenin **yüksekliğinde** beklenmeli, yerinde
değil — ve alışılmadık derecede yüksek bir `Z_maks` bir hata değil, bu motorun
imzasıdır.

### 7. Eğri ne söylüyor

| gözlem | anlamı |
|---|---|
| Tek, temiz tepe | Beklenen. |
| İkinci, daha küçük tepe | Sürücü hâlâ bir kabinin içindeyse normaldir: kapalı kabinde tepe yukarı kayar, bas refleks kabinde **ikiye ayrılır** ve aradaki çukur port ayar frekansıdır. Sürücü serbest havadaysa mekaniktir — gevşek örümcek, ayrılmış tozluk, aralıkta yabancı madde. |
| Aynı frekansta **tekrarlanan** pürüz | Mekanik. `Fs` civarında kulağını da yaklaştır: sürtünme sesi aranıyor. DSP planındaki 4. madde ("düşük seviyede tek tek sürücü taraması; sürtünme/bozulma kontrolü") tam olarak budur. |
| Rastgele, tekrarlamayan pürüz | Ölçüm. Krokodil uçlarını ve `Rs`'nin lehimini kontrol et; uzun ölçü kabloları 50 Hz şebeke gerilimi de toplar. Aynı adımı tekrar oku. |
| Tepe çok alçak ve geniş | Ağır sönümleme. Tweeter'da ferro-sıvı bunu yapar ve normaldir. `Fs` daha zor yerleşir; ince taramayı yine de koş ve tepe yerine bir **aralık** kaydet. |
| Tepe ilk adımda (40,0 Hz) | Gerçek tepe aralığın altında kaldı. `CONFIG_HK_BENCH_SWEEP_FINE_HZ=40` ile ince tarama 25 Hz'e kadar iner. |
| İlk adımdan itibaren düzenli **düşen** eğri, tepe yok | Seri `C_SAFE` hâlâ devrede. Çıkar, tekrar koş. |
| Her frekansta ~1 V, ya da 200 mV'da sürekli taşma | Sürücü açık devre; metre kaynağın tamamını görüyor. Uçları ve lehimi kontrol et. |
| Her frekansta ~0 mV | Kısa devre, **ya da** susturma hiç bırakılmadı. Seri kayıttaki `hk_audio` satırlarına bak: sekans `SILENT`'ten çıkmadıysa iki tezgâh istisnası eksiktir. |
| Birkaç mV, dümdüz, tepe yok | Sinyal var ama aralık yanlış — ya da sürücü değil bir direnç ölçülüyor. |

### 8. Seri kayıtta ne göreceksin

```text
W hk_tone: THE AMPLIFIER MUST NOT BE IN THE PATH. Wire it: DAC line output -> 470 ohm ...
W hk_tone: ONE BARE DRIVER, nothing else in series with it. ...
W hk_tone: amplitude -6 dBFS (16422 of 32767 counts, about half of full scale). ...
I hk_tone: 22 steps, 40.0 Hz to 4900.0 Hz, 8 s each, after a 20 s setup step: about 3 min 26 s ...
W hk_tone: SILENT for the next 10 s. If the amplifier is still connected ... reset the board NOW.
W hk_tone: SETUP STEP, not a data point: 315.0 Hz for 20 s. ...
I hk_tone: step  1/22     40.0 Hz   read the meter now (8 s)
I hk_tone: step  2/22     50.0 Hz   read the meter now (8 s)
...
I hk_tone: step 22/22   4900.0 Hz   read the meter now (8 s)
I hk_tone: SWEEP COMPLETE: 22 readings. ...
W hk: the bench signal has ended; the mute lines are going back down.
```

Her adımın frekansı **sesleneceği andan önce** basılıyor, çünkü metreye bakan
kişinin elindeki sayının hangi satıra ait olduğunu bilmesi gerekiyor. `step
14/22` biçimi de bunun için: seri konsol kayar, ve bir bakışta kaçıncı adımda
olduğunu çıplak bir frekanstan çıkaramazsın.

### 9. Kaba tarama tablosu

Frekanslar firmware'in ürettiği **gerçek** değerlerdir (`44100 / k`), log'da
yazan da bunlardır.

| # | frekans | woofer `V` (mV) | woofer `Z` (Ω) | tweeter `V` (mV) | tweeter `Z` (Ω) |
|---:|---:|---|---|---|---|
| 1 | 40,0 | | | | |
| 2 | 50,0 | | | | |
| 3 | 63,0 | | | | |
| 4 | 80,0 | | | | |
| 5 | 100,0 | | | | |
| 6 | 124,9 | | | | |
| 7 | 159,8 | | | | |
| 8 | 199,5 | | | | |
| 9 | 250,6 | | | | |
| 10 | 315,0 | | | | |
| 11 | 400,9 | | | | |
| 12 | 501,1 | | | | |
| 13 | 630,0 | | | | |
| 14 | 801,8 | | | | |
| 15 | 1002,3 | | | | |
| 16 | 1260,0 | | | | |
| 17 | 1575,0 | | | | |
| 18 | 2004,5 | | | | |
| 19 | 2450,0 | | | | |
| 20 | 3150,0 | | | | |
| 21 | 4009,1 | | | | |
| 22 | 4900,0 | | | | |

### 10. İnce tarama tablosu

Frekanslar merkeze göre değişiyor; log'dan kopyala. Adım sayısı 17'den az
olabilir: aralığın üst ucunda ardışık hedefler aynı `k`'ye yuvarlanırsa firmware
tekrarı atıyor, çünkü aynı frekansı iki kez okumak veri değil, tabloyu yanlış
okuma fırsatıdır.

| # | frekans (log'dan) | `V` (mV) | `Z` (Ω) |
|---:|---:|---|---|
| 1 | | | |
| 2 | | | |
| 3 | | | |
| 4 | | | |
| 5 | | | |
| 6 | | | |
| 7 | | | |
| 8 | | | |
| 9 | | | |
| 10 | | | |
| 11 | | | |
| 12 | | | |
| 13 | | | |
| 14 | | | |
| 15 | | | |
| 16 | | | |
| 17 | | | |

### 11. Sonuç

| büyüklük | woofer/mid | tweeter |
|---|---|---|
| `Fs` (kaba tarama) | _(yazılacak)_ | _(yazılacak)_ |
| `Fs` (ince tarama) | _(yazılacak)_ | _(yazılacak)_ |
| `Z_maks` @ `Fs` | _(yazılacak)_ | _(yazılacak)_ |
| `Z_min` ve frekansı | _(yazılacak)_ | _(yazılacak)_ |
| `Rs` (takılan) | _(yazılacak)_ | _(yazılacak)_ |
| metre modeli ve AC bant genişliği | _(yazılacak)_ | _(yazılacak)_ |
| tarih / firmware sürümü | _(yazılacak)_ | _(yazılacak)_ |

Son iki satır süs değil: tweeter'ın `Fs`'si metrenin bandının üst ucuna
düşüyorsa okumanın ne kadar güvenilir olduğunu **yalnız** metre modeli söyler,
ve [[../04-acoustics/measurement-and-dsp-plan|DSP planı]] her profilin kaynak
ölçümünü, firmware sürümünü ve tarihini taşımasını şart koşuyor.

Ham veriler ve fotoğraflar: `docs/assets/measurements/drivers/`.

### 12. Bu neyi kapatmıyor

- **Faz ölçülmüyor.** Buradan yalnız `|Z|` çıkıyor. Amfinin gördüğü gerçek yük
  karmaşıktır; akım sınırı için genlik iyi bir yaklaşım, tam cevap değil.
- **`Qts`, `Vas`, `Mms` çıkmıyor.** Onlar için eklenen kütle ya da bilinen kabin
  yöntemi gerekir. Crossover köşesi ve limiter için şart değil; kabin tasarımı
  için şart.
- **Küçük sinyal ölçümü.** Milivat mertebesinde. Büyük sinyalde süspansiyon
  yumuşar, `Fs` düşer ve `Z` değişir.
- **Serbest hava ölçümü.** Woofer'ın `Fs`'si kabine girince yukarı çıkar.
  Tweeter'ınki kendi arka hacmine sahip olduğu için pek değişmez — ve zaten HPF
  köşesini belirleyen odur, yani bu ölçüm doğru soruyu cevaplıyor.
- **`G0`'ı tek başına kapatmıyor.** [[../06-testing/test-strategy|Test
  stratejisi]] `G0` için polarite kaydını da istiyor.

## Bilinenler

- Nova'dan **60 mm woofer** ve **25 mm kubbe tweeter** çıktı (operatör ölçümü,
  2026-09-08). Önceki "yaklaşık 63 mm / 35 mm" notu tahmindi ve tweeter'da
  yanlıştı; 25 mm, kondansatör seçimini değiştirdiği için önemli.
- Woofer'ın arkasında **çift mıknatıs yığını** var (fotoğrafla doğrulandı):
  mıknatıs, çelik plaka, ikinci mıknatıs. 60 mm'lik bir sürücü için alışılmadık
  derecede güçlü bir motor — `Bl` yüksek, dolayısıyla `Qts` düşük ve `Fs`
  fabrika değerinden sapmış olmalı.
- Orijinal sistem bi-amp/DSP kullandığı için tek tek sürücü ohm değeri sistem ilanından çıkarılamaz.
- ~~Kesin DC direnç ve nominal empedans henüz doğrulanmadı.~~ **Ölçüldü**, yukarıdaki `Ölçüldü — DC direnç` bölümüne bakın. Empedans eğrisi ve `Fs` hâlâ açık.

## Her sürücü için

- Ön/arka/etiket/mıknatıs fotoğrafı ve benzersiz kimlik.
- Multimetreyle DC direnç; prob direnci dahil.
- Polarite ve terminal işareti.
- Empedans eğrisi ve rezonans bölgesi.
- Düşük seviyeli tarama; sürtünme/bozulma kontrolü.

Ham veriler `docs/assets/measurements/drivers/` altında tutulur. Tüm sürücüler ölçülmeden nominal ohm veya güvenli crossover kilitlenmez.
