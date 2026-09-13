---
status: pending
owner: acoustics-engineer
updated: 2026-09-12
tags: [drivers, measurements, gate]
---

# Sürücü ölçüm planı — G0

## Eldeki sürücüler (operatör kaydı, 2026-09-08)

Harman Kardon Nova'dan sökülmüş sekiz sürücü: dört woofer/mid ve dört tweeter,
hepsi tek kabine girecek ([[../04-acoustics/cabinet-plan|kabin planı]],
[[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]). Etiketlerde yalnız
üretici iç kodları var; bunlar veri sayfasına çevrilmiyor, dolayısıyla **ölçüm
tek yol**. Aşağıdaki kodlar okunan birer örnekten; sekiz sürücünün her biri
kendi satırını ve kimliğini alır (bkz. "Sekiz sürücünün her biri için").

| sürücü | etiketteki kodlar (okunan örnek) |
|---|---|
| woofer/mid | `66057-0001006` · `110066` · `06801` · `2313351` |
| tweeter | `660056-0001004` · `310013` · `01007` · `2113356` |

**Woofer'ın arkasına ek mıknatıs yapıştırılmış.** Bu bir not değil, ölçümü
etkileyen bir gerçek: ek mıknatıs motor kuvvetini (`Bl`) değiştirir, dolayısıyla
`Fs`, `Qts` ve `Qes` fabrika değerlerinden sapar. Aynı modelin başka bir yerde
yayınlanmış parametresi bulunsa bile bu sürücüye uymaz. Aşağıdaki ölçümler bu
yüzden isteğe bağlı değil.

## Ölçüldü — DC direnç (operatör, 2026-09-08)

Bir woofer ve bir tweeter okundu; diğer altı sürücü henüz okunmadı ve aynı
tablonun kendi satırlarını bekliyor.

| sürücü | okunan `Re` | çıkarım |
|---|---|---|
| woofer/mid (1 adet) | **4,0 Ω** | nominal **4 Ω** |
| tweeter (1 adet) | **3,5 Ω** | nominal **4 Ω** |

Aynı programı aynı kutuda çalacakları için dört woofer birbirine, dört tweeter
birbirine karşı da eşleştirilir: `Re` ve `Fs` farkı kaydedilir, ve sınıf dışı
kalan bir sürücü kabine girmeden önce bilinir.

Firmware'in tezgâh profili 2026-09-12'ye kadar tweeter için **3,7 Ω** taşıyordu
(`hk_airplay_output_i2s.c`, "measured" notuyla); bu kayıt 3,5 der. İki sayının
hangisinin tweeter'ın okuması olduğunu yalnız operatör söyleyebilir. Kayıt kazandı
ve firmware 3,5'e çekildi; **operatör tweeter'ın `Re`'sini yeniden okuyup prob
direncinin çıkarılıp çıkarılmadığıyla birlikte buraya yazana kadar** bu satır bir
teyit beklemektedir. Profil bu değerle hiçbir hesap yapmaz, yalnız taşır; yani
yanlışsa koruma değil kaynak kaydı yanlıştır.

Kural: `Re` genelde nominal empedansın 0,75–0,85 katıdır. 4 Ω'luk bir sürücü
tipik olarak 3,0–3,6 Ω okur, 6 Ω'luk 4,4–5,0 Ω. Woofer'ın 4,0'ı iki aralığın
sınırında duruyor; **prob direnci çıkarılmadıysa** gerçek değer ~3,7 Ω olur ve
tereddüt kalmaz. Tweeter'ın 3,5'i zaten net biçimde 4 Ω sınıfı.

### Bu neyi açıyor, neyi açmıyor

**Açtığı:** her iki sürücü de 4 Ω sınıfı. Bu, güvenli amfi seviyesi ve termal
bütçe için ilk somut girdi — ve bir soruyu da açtı. Bu paragraf 2026-09-12'ye
kadar "TPA3110 BTL için tipik asgari yük 4 Ω" diyordu; veri sayfası öyle demiyor.
SLOS528F'in Mutlak Azami Değerleri (§7.1) BTL için asgari yükü `PVCC ≤ 15 V`'ta
3,2 Ω, `PVCC > 15 V`'ta **4,8 Ω** verir, ve 4 Ω'luk karakterizasyon eğrileri 16 V'ta
biter (24 V / 4 Ω için yayımlanmış çıkış gücü yoktur). Ürün beslemesi 24 V'tur
([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]); yani 4 Ω sınıfı Nova
sürücüleri o beslemede veri sayfasının asgari yükünün **altında** durur ve amfi bu
çalışma noktasında karakterize edilmemiştir. Bu bir ölçüm değil, veri sayfası
okumasıdır; ADR-0020'nin sahibine açık bir donanım sorusudur ve
[[../01-planning/risk-register|risk kaydında]] `Kritik` satırdır. `G0`'ın
ölçeceği `Z_min` ve `G1`'in 24 V'ta 4 Ω sınıfı dummy-load'a alacağı termal ve
koruma (kısa devre kilidi) kaydı olmadan hiçbir sürücü 24 V'ta bağlanmaz.

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

## `Fs` TAHMİNLERİ — ölçüm değil (2026-09-08), woofer kaba taramayla düzeltildi (2026-09-12)

Ölçüm 2026-09-08'de yarım kaldı: telefondaki sinyal jeneratörü uygulaması
**100 Hz'in altına inemiyor**, ve woofer'ın rezonansı tam orada başlıyor.
2026-09-12'de kartın kendi tarama modu koşuldu (aşağıdaki §9 tablosu): woofer'ın
tepesi **50 Hz'de** çıktı — 8 Eylül tahmininin yarısı. Tahmin yanlıştı çünkü
100 Hz'deki 11,6 Ω okuması tepenin kendisi değil, tepenin **üst yamacıydı**.
İnce tarama henüz yok; tweeter satırı hâlâ tahmindir.

| sürücü | `Fs` | dayanağı |
|---|---:|---|
| woofer | **≈ 50 Hz** (kaba tarama, ±%13; ince tarama bekliyor) | 2026-09-12 kaba tarama: 40 Hz 40 mVpp, **50 Hz 98 mVpp**, 63 Hz 38 mVpp (§9). 8 Eylül'ün "~100 Hz" tahmini kaldırıldı. |
| tweeter | **~2200 Hz** (tahmin; 2026-09-12 kaba tarama 1–3 kHz'de düz ~2 × `Re`, tepe çözülemedi) | Veride tek özellik: 2000–2450 Hz'de 3,3 → 3,9 Ω kabarma. Küçük kubbelerde ferrofluid rezonansı 20 Ω'dan 5 Ω'a bastırır, ve gördüğümüz buna benziyor; 12 Eylül taraması da 1–3 kHz boyunca alçak, geniş bir tümsek gördü, nokta vermedi. 25 mm kubbe için 1200–2400 Hz tipik. |

### Bunlardan türeyen provisional ayarlar

Bu tablo tezgâh yapısının **derlediği** yer tutucuları yazar (`hk_airplay_output_i2s.c`,
`bench_provisional_chain()`). 2026-09-12 sabahına kadar burada 3500 Hz / 50 Hz
yazıyordu, kodda 2800 / 55 vardı ve test fikstürü 4000 kullanıyordu; tablo koda
çekildi. Aynı gece kaba tarama geldi ve crossover, sahibinin yetkisiyle, 2800'den
**3500 Hz'e** alındı (gerekçe satırda). Test fikstürü 4000'de kalır, o `C_SAFE`
köşesini sınar.

| parametre | değer | neden |
|---|---:|---|
| `crossover_hz` | **3500** | 2026-09-12 kaba taraması tweeter'da tepe çözemedi (1–4,9 kHz düz ~2 × `Re`); en iyi nokta tahmini hâlâ 8 Eylül'ün ~2200 Hz tümseği. 3500, onun ~1,6 katı: LR4 ile 2,2 kHz'de tweeter dalı −16 dB; 60 mm woofer'ın huzmelenmeden taşıyabileceği sınırın içinde. Bu belgenin "≥ 2 × `Fs`" kuralı 4400 isterdi; ağır sönümlü kubbe (empedans tepesi yok, rezonansta eksürsiyon kazancı az) ve tweeter'ın en iyi oktavını harcamamak için bilerek altında kalındı — sahibinin 2026-09-12 kararı. Ölçülmüş bir `Fs` noktası gelene kadar yer tutucu; `G2` `C_SAFE` etkileşimiyle birlikte karar verir. |
| `woofer_hpf_hz` | 55 | Pasif radyatör akordunun hemen altı (aşağıdaki kabin bölümü: 50 → 55). **2026-09-12 kaba taraması woofer `Fs`'sini ≈ 50 Hz'e koydu, yani bu köşe serbest hava rezonansının tam üstünde**; kabin içinde `Fs` yukarı çıkacağı ve köşeyi `Fb` belirleyeceği için kalıcı karar ince tarama ve kabin akordu sonrasıdır. Operatörün itirazı yerindeydi: subsonic filtre bası kısmaz, sese dönüşmeyen eksürsiyonu atar — ama nereye konacağını `Fb` belirler, ve o henüz ölçülmedi. Filtre dördüncü derecedir (aşağıya bakın). |
| kanal kazançları | woofer 0,25, tweeter 0,18 | Tweeter woofer'ın ~3 dB altında; hassasiyet ölçümü yok, hata payı tweeter'ı korumak yönünde. İkisi de mutlak olarak düşük, çünkü amfinin kazanç strap'i okunmadı (`C3`) ve tezgâhta zincir amfinin istediğinden ~26 dB sıcaktı (36 dB varsayımıyla). |
| `supply_budget_sq` / pencere | 1,0 / 100 ms | Bir tam ölçekli dalın ortalama-karesi, doğrulayıcı sınırının (2,0) yarısı; `G1` S7'nin dolduracağı yer tutucu. Bu kazançlarla (0,25² + 0,18² ≈ 0,095) ancak kullanıcı EQ'su yükseltirse devreye girebilir. |
| EQ | düz | Akustik ölçüm olmadan voicing uydurmak tahmindir. Bantlar açık, kulakla ayarlanacak. |

## Kabin: pasif radyatör kullanılacak (2026-09-08)

Orijinal Nova'nın pasif radyatörleri operatörün elinde ve kabinde kullanılacak:
tek kabin, pasif radyatörle akortlanmış refleks hizalama, kanal yok
([[../04-acoustics/cabinet-plan|kabin planı]], ADR-0021). Dört woofer o kabini
paylaşır; başlangıç varsayımı tek ortak hava hacmidir, ayrı hacimler `G0`
(`Fs`, `Vas`) sonrasında kararlaşır ve sürücü dizilimi henüz belirlenmedi. Bu,
subsonic filtreyi **daha önemli** hâle getiriyor, daha az değil — ve sebebi
kapalı kutunun tersi:

Akort frekansının (`Fb`) altında pasif radyatör akustik yükü üstlenir ve woofer
havasız kalır. Koni serbest salınır, eksürsiyon hızla artar, karşılığında ses
üretilmez. Kapalı kutuda hava yayı hiç değilse frenler; burada o fren yok.

**Bu yüzden `woofer_hpf_hz` 50 → 55 Hz.** Perakende/inceleme kaynakları sistemin
−6 dB noktasını 55 Hz, PR akordunu 60–65 Hz veriyor; 55 Hz akordun hemen altı.

### `Fb` kabin hacmine bağlıdır ve ÖLÇÜLEBİLİR

60–65 Hz orijinal kutunun iç hacminde geçerli. Yeni kabin daha büyükse akort
aşağı, küçükse yukarı kayar; PR'a kütle eklemek `Fb`'yi düşürür.

Tahmin etmeye gerek yok: **aynı empedans düzeneğiyle woofer'lar kutunun
içindeyken ölçülür.** Pasif radyatörlü bir sistemde empedans eğrisi **iki
tepe** verir ve aradaki çukurun frekansı tam olarak `Fb`'dir. Ortak hacimde
dört woofer'ın tamamı takılıyken tek bir eğri alınır; ayrı hacimlere gidilirse
her hacim kendi eğrisini verir. Kabin bittiğinde 10 dakikalık bir ölçüm,
subsonic frekansını tahminden çıkarır.

### Sonraki tur için not — kodda kapandı (2026-09-12)

Bu bölüm subsonic filtrenin **ikinci derece** (12 dB/oktav) olduğunu ve PR'lı
sistem için **24 dB/oktav** gerektiğini yazıyordu — `Fb` altında eksürsiyon çok
hızlı artar, 12 dB/oktav geç kalır. Kod tarafı kapandı: `hk_profile_chain_t.woofer_hpf`
artık iki bölümlük bir dördüncü derece **Butterworth**'tur (`Q` 0,5412 ve 1,3066,
aynı köşede; `hk_butterworth4_highpass()`), LR4 değil — LR4 köşeyi −6 dB'ye
koyardı, Butterworth `woofer_hpf_hz`'nin **−3 dB** anlamını korur, ve bu anlam
korunduğu için değişiklik şema 2'ye girdi. Host testi 24 dB/oktav eğimi ve
köşedeki −3 dB'yi doğruluyor. Köşenin kendisi hâlâ `G0`'ındır: kabin içi
empedansın iki tepesi arasındaki `Fb` ölçülene kadar 55 Hz bir yer tutucudur.

## Tweeter seri kondansatörü `C_SAFE` (2026-09-08)

**Seçilen değer: 10 µF, kutupsuz film, ≥ 50 V — dört adet, her tweeter'a biri.**

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

**Bedeli:** tezgâhın derlediği köşe 3500 Hz (2026-09-12 gecesinden itibaren;
öncesinde 2800) ve orada 10 µF'lik kondansatör 4 Ω'a **yaklaşık −3,6 dB ve
+49°** (el hesabı: birinci derece köşe 3980 Hz, `f/fc` = 0,88). Yani DSP'nin
LR4'ü ile kondansatör üst üste biniyor, LR4'ün düz-toplam özelliği geçiş
bandında tam tutmuyor (tweeter dalı köşede ~−10 dB, iki dalın toplamı ~−2 dB)
ve akustik geçiş noktası biraz yukarı kayıyor; 2800'de bu −4,8 dB / +55° idi,
3500 kondansatöre yaklaştığı için fark küçüldü. Bu bir hesaptır, ölçüm değil; ne kadar olduğu
`G2`'de ölçülür ve karar orada verilir: köşeyi kondansatörünkine yaklaştırmak,
`Fs` ölçülünce kondansatörü değiştirmek ya da DSP'de telafi etmek. `Fs`
ölçülene kadar bu kabul ediliyor: DSP crossover'ının köşesi tahmini bir `Fs`'den
türetilmiş bir yer tutucu, yani firmware doğru yüklenmediğinde ya da köşe yanlış
tahmin edildiğinde bu kondansatör tweeter'ın tek koruması **olur** — takılıysa.

> **Operatöre soru:** tezgâhta bugün tweeter'ın önünde bir `C_SAFE` takılı mı,
> takılıysa hangi değer? Firmware'in kendi notu daha önceki 7 µF parçanın arızalı
> çıktığını ve **takılı olmadığını** söylüyor; 10 µF'in takıldığına dair bir kayıt
> yok. Takılı değilse tweeter'ın önündeki tek şey LR4'tür ve bu, yukarıdaki
> "tek koruma" cümlesinin bugün için geçerli olmadığı anlamına gelir.
> **Sonuç:** _(yazılacak)_

**Kutupsuz olması şart, sebebi BTL:** amfi köprülü çıkışlı, hoparlörün eksi ucu
toprak değil, o da salınıyor. Kutuplu bir kondansatör orada ters gerilim görür.

`Fs` ölçüldükten sonra yeniden değerlendirilir; değiştirmek dört lehim
noktasıdır, her amfinin ucunda biri. Dört parça aynı seri ve lot olmalı: aynı
kabinde dört tweeter arasındaki köşe farkı duyulur.

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

`sdkconfig.bench`'teki tezgâh istisnası da gerekli, yoksa `XSMT` düşük kalır
ve DAC hiç çıkış vermez. Bu, bir sonraki bölümün konusu.

### 2. GÜVENLİK — amfi devrede OLMAYACAK

> **`CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE` olmadan DAC susturulmuş kalır,
> yani tarama için bu sembol zorunlu. Ama amfinin kendi susturma girişi yok:
> DAC açıldığı anda amfi de yayındadır. DAC'ı açıp amfiyi kapalı tutan bir
> ayar yok, çünkü tutacak bir hat yok.**
>
> Amfiyi devre dışı bırakmak **yapılandırmayla değil, kabloyla** olur: TPA3110'un
> girişini sök ya da beslemesini kes. Firmware kartın nasıl kablolandığını
> göremez; bu yüzden tarama uyarıyı basıp **ilk adımdan önce 10 saniye susuyor**.
> O 10 saniye, uyarıyı okuyup lab kaynağının çıkışını kapatmaya yetişmek içindir.

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
5. **Susturma kapısı yerinde duruyor.** `hk_tone` DAC susturmasına dokunmuyor;
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
| İkinci, daha küçük tepe | Sürücü hâlâ bir kabinin içindeyse normaldir: kapalı kabinde tepe yukarı kayar, pasif radyatörlü kabinde **ikiye ayrılır** ve aradaki çukur akort frekansı `Fb`'dir. Sürücü serbest havadaysa mekaniktir — gevşek örümcek, ayrılmış tozluk, aralıkta yabancı madde. |
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
W hk: the bench signal has ended; the DAC mute is going back down.
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
| 1 | 40,0 | 40 (Vpp) | ~15 † | | |
| 2 | 50,0 | **98 (Vpp)** | ~38 † | | |
| 3 | 63,0 | 38 (Vpp) | ~15 † | | |
| 4 | 80,0 | okunamadı ‡ | | | |
| 5 | 100,0 | okunamadı ‡ | | | |
| 6 | 124,9 | okunamadı ‡ | | | |
| 7 | 159,8 | okunamadı ‡ | | | |
| 8 | 199,5 | okunamadı ‡ | | | |
| 9 | 250,6 | okunamadı ‡ | | | |
| 10 | 315,0 | okunamadı ‡ | | | |
| 11 | 400,9 | okunamadı ‡ | | | |
| 12 | 501,1 | okunamadı ‡ | | | |
| 13 | 630,0 | okunamadı ‡ | | | |
| 14 | 801,8 | okunamadı ‡ | | | |
| 15 | 1002,3 | okunamadı ‡ | | ~20 (2 kare @ 10 mV/div) | ~8 † |
| 16 | 1260,0 | okunamadı ‡ | | | |
| 17 | 1575,0 | okunamadı ‡ | | | |
| 18 | 2004,5 | 32 (Vpp) | ~12 † | | |
| 19 | 2450,0 | 36 (Vpp) | ~14 † | | |
| 20 | 3150,0 | 40 (Vpp) | ~15 † | ~20 (2 kare, ~2,9 kHz okuması) | ~8 † |
| 21 | 4009,1 | 48 (Vpp) | ~18 † | | |
| 22 | 4900,0 | 53 (Vpp) | ~20 † | ~20 (2 kare) | ~8 † |

**2026-09-12 koşusu (woofer, operatör: sahibi).** Değerler osiloskop `Vpp`
olarak okundu (metre değil), sürücünün iki ucunda; `Rs` dış 680 Ω. † `Z`
sütunu türetilmiştir, ölçülmemiştir: DAC modülünün çıkışında **dahili ~470 Ω**
seri direnç olduğu yük ölçümlerinden çıkarıldı (`LOUT` yüksüz 3,0 Vpp, 680 Ω +
sürücü ile 1,82 Vpp, 220 Ω + sürücü ile 0,999 Vpp — üçü de 3,0 × R_yük /
(470 + R_yük) ile uyuşuyor), yani toplam seri direnç ≈ 1150 Ω, akım ≈ 2,6 mApp,
`Z ≈ V / 2,6 mA`. Bu varsayım doğruysa `Z_maks` ≈ 40 Ω; değilse yalnız `Fs`
geçerlidir — tepe hangi satırdaysa `Fs` odur ve bu aritmetiğe bağlı değildir.
‡ Rezonans dışı ~10 mVpp, osiloskopun otomatik ölçümü bu seviyede `0` gösterdi;
okuma alınamadı. **Tweeter (aynı gece, aynı düzenek):** 40–800 Hz'de kutu `0`
(taban, ~1 kare altı); 1002 Hz'den 4,9 kHz'e kadar dalga **2 kare = ~20 mVpp,
düz, tepe yok** (operatör: "hep 2 kareydi"); ara adımlar (1260–2450) ayrı ayrı okunmadı, "2 kare" aralığın
tamamı için söylendi. 10 mV/div'de 1 karelik çözünürlükle bu, `Z` ≈ 2 × `Re`
mertebesinde, geniş ve ağır sönümlü bir tümsek demektir (ferro-sıvılı küçük
kubbe; §7 "tepe çok alçak ve geniş" satırı). 8 Eylül'ün 2000–2450 Hz'de 3,3 →
3,9 Ω gözlemiyle uyumlu. Tweeter `Fs` bu çözünürlükte nokta değil **aralık**:
1–3 kHz. Sonraki koşuda kanal 10 mV/div, tetik `LOUT` kanalından,
imleçle okunacak. Yükselen kuyruk (2–4,9 kHz, 32 → 53 mVpp) bobin
endüktansıdır; woofer eğrisinin beklenen şekli.

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
| `Fs` (kaba tarama) | **≈ 50 Hz** (40/50/63 Hz satırları: 40 / 98 / 38 mVpp; ±%13) | **1–3 kHz aralığı**, tepe çözülemedi (1002–2900 Hz'de ~20 mVpp düz; ağır sönümlü) |
| `Fs` (ince tarama) | _(denendi, okunamadı — aşağıya bakın)_ | _(yazılacak)_ |
| `Z_maks` @ `Fs` | ≈ 40 Ω † (dahili 470 Ω varsayımıyla; doğrulanmadı) | ≈ 8 Ω † (aynı varsayım; `Re`'nin ~2 katı, geniş) |
| `Z_min` ve frekansı | _(okunamadı; 80–1575 Hz satırları skop tabanının altında)_ | _(yazılacak)_ |
| `Rs` (takılan) | 680 Ω dış (+ modülde ~470 Ω dahili, çıkarım) | 680 Ω dış (aynı düzenek) |
| metre modeli ve AC bant genişliği | Osiloskop (model kaydedilmedi), otomatik `Vpp`; DMM 6 V AC kademesi bu seviyeyi göstermedi | Osiloskop, 10 mV/div, kare sayarak (otomatik ölçüm tutunamadı) |
| tarih / firmware sürümü | 2026-09-12, `2954e67` üstü tarama yapısı (`sdkconfig.bench` + `sdkconfig.sweep`), ikinci N16R8 kartı (kimlik `056C`) | aynı |

**İnce tarama denemesi (2026-09-12, 31,5–79,3 Hz, merkez 50 Hz):** sürücü
düğümündeki ~10–100 mVpp skop otomatik ölçümüyle okunamadı; `LOUT` düğümünden
okuma denendi (dış `Rs` 220 Ω), 31,5–79,3 Hz boyunca 0,989–1,00 Vpp düz —
üç haneli okuma %1'lik değişimi çözemedi. `Fs` ince değeri **açık**. Sonraki
koşu: sürücü kanalı 10 mV/div, tetik `LOUT` kanalından, 10 ms/div, imleçle.

**Bu oturumda düzenekte bulunanlar** (hepsi ölçümden önce giderildi): DAC
modülünün `SCK` pini ESP'ye bağlıymış — kesildi ve GND'ye alındı (3-telli mod
şartı, §3.3); DAC ile ESP toprağı ayrıydı — ortak toprak olmadan I2S çalışmaz,
ESP `GND` ↔ DAC `GND` doğrudan tel çekildi; seri direnç 680 **kΩ** çıktı
(mavi-gri-sarı) — 680 Ω ile değiştirildi; GPIO13 → `XSMT` teli bir kez
çıkmıştı (8 Eylül'ün arızası). DAC modülünün çıkışındaki dahili ~470 Ω, dış
direnç ne olursa olsun sürücüdeki sinyali ~6 mV rms'te tutuyor; §3'ün
"470 Ω yerine 680 Ω" hesabı bu dahili direnci **bilmiyordu** ve yeniden
yazılacak.

Son iki satır süs değil: tweeter'ın `Fs`'si metrenin bandının üst ucuna
düşüyorsa okumanın ne kadar güvenilir olduğunu **yalnız** metre modeli söyler,
ve [[../04-acoustics/measurement-and-dsp-plan|DSP planı]] her profilin kaynak
ölçümünü, firmware sürümünü ve tarihini taşımasını şart koşuyor.

**Profil dosyası (2026-09-12 gecesi).** Bu tablonun ve yukarıdaki "provisional
ayarlar" tablosunun sayıları artık tek bir yerde durur:
`docs/assets/measurements/drivers/profile-2026-09-12-provisional.json`,
`firmware/tools/write_profile.py`'nin `factory_cal` profil blob'una çevirdiği
dosya ([[../04-acoustics/measurement-and-dsp-plan#Profili yazmak — değerler dosyası, araç, flaş|DSP planı]];
flaş sahibinin işidir, araç yalnız komutu basar). Dosya **geçicidir** ve her
alanının yanında bunu söyler. Ölçülmüş olan yalnız iki DC direnç (woofer 4,0 Ω,
tweeter 3,5 Ω — tweeter'ınki prob direnciyle yeniden okunacak); gerisi bekliyor:
`crossover_hz` (3500) tweeter `Fs`'sinin **noktasını** — ince tarama ya da oran
yöntemi — ve `G2`'nin `C_SAFE` kararını; `woofer_hpf_hz` (55) woofer ince
taramasını ve kabin akordunun `Fb`'sini; dal kazançları, iki tavan, dal başına
release/hold, hizalama gecikmesi ve tweeter polaritesi `G2`'yi;
`supply_budget_sq` / `supply_window_ms` `G1` S7'yi; `amp_gain_db` (0, okunmadı)
`C3`'ü. `reference_supply_mv` (12000) 8 Eylül'ün tezgâh kaynağının kaydıdır,
ölçüm değil. Bunlardan biri ölçüldüğünde değişen yer dosyadır, sonra blob
yeniden yazılır: bu tablo ölçümü taşır, dosya ölçümden türeyen sayıyı.

Ham veriler ve fotoğraflar: `docs/assets/measurements/drivers/`.

### 12. Bu neyi kapatmıyor

- **Faz ölçülmüyor.** Buradan yalnız `|Z|` çıkıyor. Amfinin gördüğü gerçek yük
  karmaşıktır; akım sınırı için genlik iyi bir yaklaşım, tam cevap değil.
- **`Qts`, `Vas`, `Mms` çıkmıyor.** Onlar için eklenen kütle ya da bilinen kabin
  yöntemi gerekir. Crossover köşesi ve limiter için şart değil; kabin tasarımı
  için şart — ortak hacim mi ayrı hacimler mi sorusu ve pasif radyatör akordu
  bunları bekliyor ([[../04-acoustics/cabinet-plan|kabin planı]], ADR-0021).
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

## Sekiz sürücünün her biri için

- Ön/arka/etiket/mıknatıs fotoğrafı ve benzersiz kimlik (woofer 1-4, tweeter 1-4; amfi numarasıyla eşleşir).
- Multimetreyle DC direnç; prob direnci dahil.
- Polarite ve terminal işareti.
- Empedans eğrisi ve rezonans bölgesi.
- Düşük seviyeli tarama; sürtünme/bozulma kontrolü.
- Aynı tipteki dört sürücü arasında `Re` ve `Fs` farkı; aynı kabinde çalacakları için eşleşme kaydı.

Ham veriler `docs/assets/measurements/drivers/` altında tutulur. Tüm sürücüler ölçülmeden nominal ohm veya güvenli crossover kilitlenmez.
