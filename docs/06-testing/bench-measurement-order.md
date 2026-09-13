---
title: Tezgâh ölçüm sırası — ses zinciri ve susturma hatları
status: in-progress
owner: orchestrator
reviewers: [hardware-reviewer, verifier]
updated: 2026-09-12
tags: [testing, bench, audio, grounding, mute, procedure]
---

# Tezgâh ölçüm sırası

`2026-09-08` tezgâh oturumunda açık kalan her soruyu kapatmak için. Sıra keyfî
değil: önce **hiçbir alet gerektirmeyen** dinleme testi, sonra **lehimden önce
yapılması zorunlu** güvenlik ölçümleri, sonra gerisi.

Her adımda ne ölçüleceği, aletin hangi konumda olacağı, **beklenen değer** ve o
değer çıkmazsa ne anlama geldiği yazıyor. Sonucu buraya yaz; bir agent fiziksel
bir testi geçmiş sayamaz, operatör kaydeder.

> **Her ölçümden önce:** hoparlör terminallerinde ne olduğuna bak. Nova
> sürücülerinin DC direnci ölçüldü ama empedans eğrisi ve `Fs` ölçülmedi (`G0`,
> açık `Kritik`); yüksek geçiren filtre ile limiter yer tutucu tezgâh
> profiliyle — ya da onun `factory_cal`'a yazılmış, kaynağı `provisional`
> olan kopyasıyla (§E) — çalışıyor, ölçülmüş değerle değil (`F3`). Amfinin kazanç strap'i
> okunmadı (`C3`): **24 V'ta sürücüde dinleme yok**, tezgâh profili 12 V'ta
> dinlendi ve amfi sabit kazançlıdır. Tweeter bağlıyken uzun süre ses verme.
> Tezgâhta tek amfi ve tek woofer/tweeter çifti vardır; öteki üç amfi sürücülere
> `G0`-`G2` o çiftte geçmeden bağlanmaz.

---

## A — Alet gerekmiyor: dinleme testi

### A1. Çalma durunca fısıltı tamamen kesiliyor mu?

Bir şey çal, sonra durdur ve birkaç saniye bekle. `hk_audio_hw` sekansı geri
sarmalı ve `XSMT` tekrar düşmeli. Seri kayıtta görülmesi gereken:

```text
hk_audio: PLAYING -> MUTING: ...
hk_audio: MUTING -> SILENT: dac xsmt gpio13=0 (0=muted, read back) ...
```

Bu satırlar geliyor ama fısıltı sürüyorsa, kaynak DAC'ın çıkışı değil, amfinin
kendi giriş/besleme gürültüsüdür.

**Sonuç:** _(yazılacak)_

---

## B — Güç KAPALI, direnç ve süreklilik. Lehimden önce zorunlu.

Multimetre direnç (Ω) konumunda, modül beslemesiz.

### B1. `XSMT` padi ↔ `3V3` — bu bir güvenlik ölçümü

`GPIO13` zaten bu pade bağlandı, o yüzden bunu geciktirme.

| okuma | anlamı |
|---|---|
| birkaç yüz Ω'un **altı** | Sert köprü. GPIO13 aşağı sürüldüğünde **pini 3V3 rayına kısa devre ediyor**, üstelik mute de çalışmaz (10 kΩ sert bağa karşı sadece bölücüdür). Köprüyü kes, sonra yeniden ölç. |
| onlarca kΩ | Zayıf çekme. Sorun yok; mute çalışırken sürekli küçük bir akım akar. |
| açık devre | En temizi. |

Sende mute çalıştığına göre sert köprü beklenmiyor — ama "çalışıyor" ile
"güvenli" aynı şey değil.

**Sonuç:** _(yazılacak)_

### B2. `FMT`, `FLT`, `DEMP` padleri ↔ `GND`

Üçü de LOW olmalı. Boşta bir CMOS girişi "muhtemelen LOW"dur, kesin değil.

| okuma | anlamı |
|---|---|
| ~0 Ω | Modül kendi üstünde çekmiş. Bir şey yapma. |
| açık devre | Pad gerçekten boşta. **GND'ye bağla.** Özellikle `FMT` yüksek okursa DAC sol-hizalı format bekler ve I²S'i yanlış çözer. |

**Sonuç:** _(yazılacak)_

### B3. Pad isimleri gerçekten çipe gidiyor mu?

Kablolama planının uyarısı: *"pad ismine bakıp körlemesine lehim yapılmaz."* Mor
PCM5102A modüllerinde alttaki köprüler satıcıya göre değişir. Her padden
PCM5102A çipinin ilgili pinine süreklilik bak.

**Sonuç:** _(yazılacak)_

### B4. XH-A232'de `SD` padi var mı? — KAPANDI (2026-09-12)

Soru, kartta TPA3110'un `SD` pininden erişilebilir bir noktaya süreklilik olup
olmadığıydı; firmware bunun için `GPIO21`'i rezerve etmiş, şema dalı kesikli
çizmişti.

**Sonuç:** **Yok.** Sahibin kart üzerindeki tespiti: XH-A232'de güç girişi, ses
girişi ve hoparlör çıkışları dışında hiçbir bağlantı yok. Sonuç karar olarak
yazıldı ([[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]]):
firmware tarafından kontrol edilebilir amfi susturması yoktur, `GPIO21`
rezervasyonu kaldırıldı, sıralayıcı iki hat sürer (I²S saati ve `XSMT`).
Zincirdeki tek susturma DAC'ın `XSMT`'sidir; amfinin kendi açılış/kapanış
pop'u `G1`'de, DAC susturuluyken, olduğu gibi kaydedilir. Dört kart yine aynı
revizyon olmak zorundadır (`C3`, kazanç).

---

## C — Güç AÇIK, gerilim.

### C1. `GPIO13`, çalarken ve boştayken

| durum | beklenen | çıkmazsa |
|---|---|---|
| boşta | ~0 V | Sekans `SILENT`'e dönmemiş. |
| çalarken | ~3,3 V | "Çalıyor" olayı `hk_audio_hw`'a ulaşmıyor; DAC'ı **biz** susturuyoruz. |

**Sonuç:** _(yazılacak)_

### C2. DAC beslemesi

`VIN` ve modülün kendi 3,3 V regülatör çıkışı. Ses varken ve yokken ölç; ses
verirken belirgin düşüyorsa besleme yetersiz ve gürültünün bir kısmı oradan.

**Sonuç:** _(yazılacak)_

### C3. Amfi kazanç seçimi — "kısıkta bile yüksek"in muhtemel sebebi

TPA3110'un iki kazanç seçim girişi var ve dört bileşim **20 / 26 / 32 / 36 dB**
veriyor (kesin eşleme için çipin veri sayfasındaki kazanç tablosuna bak; pin
numarasını ezberden yazmıyorum). Her iki pini GND'ye ve besleme rayına göre ölç.
Dört amfide de oku ve dördünün aynı olduğunu yaz: girişleri paralel ve
programları aynı olan dört amfinin kazancı farklıysa dört woofer aynı seviyede
çalmaz ve bu bir crossover ya da EQ sorunu gibi ölçülür.

PCM5102A tam ölçekte ~2,1 Vrms veriyor. 36 dB'lik bir amfi tam çıkışa ~0,1 Vrms
ile ulaşır — yani zincir yaklaşık **26 dB fazla sıcak**. Bu tek başına hem
"kısıkta bile yüksek" hem de "dip gürültüsü duyuluyor" şikâyetini açıklar, çünkü
DAC'ın gürültü tabanı da aynı kazançla büyür.

| bulunan kazanç | ne yapılır |
|---|---|
| 36 dB | 20 veya 26 dB'ye al. Fısıltı doğrudan 10–16 dB düşer. |
| 20–26 dB | Kazanç sorun değil; gürültü başka yerden. |

Bu ölçümün sonucu iki yere gider ve bir kapıyı tutar:

- **Profile.** Şema 2'nin `amp_gain_db` alanı (20/26/32/36; 0 = okunmadı). Firmware
  bu sayıyla hesap yapmaz, taşır — kaynağı adlandırmak için. Dört kartta aynı
  değilse profil değil kartlar düzeltilir.
- **24 V dinleme kapısına.** Amfi sabit kazançlıdır: çıkış, ray kırpana kadar
  kazanç × giriştir. Firmware'in kendi notuna göre (`hk_airplay_output_i2s.c`,
  tezgâh profili) 2026-09-08 dinlemesi 12 V tezgâh kaynağında yapıldı ve
  kaydırıcının ~%80'inde amfi bitti, yani seviye rayla sınırlıydı, kazançla değil.
  DSP'nin tavan ölçeklemesi 24 V'ta dijital tavanı yarıya indirir. Tezgâh 12 V
  rayını kırpmadıysa bu sürücüdeki voltu da yarıya indirir; kırptıysa — yukarıdaki
  not öyle diyor — 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye
  tezgâhın duyduğundan fazlasını verebilir, aynı kaydırıcı konumunda iki katına
  kadar ([[../04-acoustics/measurement-and-dsp-plan|DSP planı]]). Hangisinin
  geçerli olduğunu, ve fazlasının ne kadar olduğunu, bu satır — `C3` — söyler.
  Bu satır doldurulmadan **hiçbir sürücü 24 V'ta
  dinlenmez**; dolduktan sonra da kademeli, tek çift, düşük seviye (`AGENTS.md`).
  Tezgâh yapısı ürün beslemesinde (`CONFIG_HK_SUPPLY_MV` = 24000, tezgâh
  referansı 12000) derlendiğinde açılışta bunu ayrıca söyler.

**Sonuç:** _(yazılacak)_

---

## KAPANDI — ses zinciri uçtan uca çalışıyor (2026-09-08)

İlk kez: AirPlay → I²S → PCM5102A → XH-A232 → ses. **Mono, temiz, dip gürültüsü
yok.** Oraya giden yol üç ayrı arızadan geçti ve üçü de birbirini maskeliyordu.

### 1. Kendi mute'umuz DAC'ı susturuyordu

`XSMT` hiçbir yere bağlı değildi ve modülde boştayken aşağı okunuyordu — yani
DAC kalıcı olarak sessizdi. Firmware `GPIO13`'ü sürüyordu ama tel yoktu. `GPIO13`
bağlanınca sekans onu bıraktı ve ses geldi.

Şemada da yoktu: `GPIO13 DAC_XSMT` teli olmayan bir etiketti ve `U6`'nın hiç
`XSMT` pini yoktu. Daha kötüsü, KiCad üreticisinin **tek bağlantılı ağları
yakalayan denetimi** `DAC_XSMT` ve `AMP_MUTE`'u "beklenen açık ağ" listesine
almıştı — yani eksikliği bildirmek yerine onaylıyordu.

### 2. Papatya zinciri toprak, gürültünün kaynağıydı

ESP `GND` → DAC `GND` doğrudan bağlıyken parazit vardı. O tel DAC'ın **bütün
besleme akımını** taşıyordu ve aynı zamanda analog referanstı. Sökünce gitti.

### 3. Ama toprağı tamamen kaldırınca ses de gitti

DAC ayrı bir 5 V regülatöre alınıp ESP ile arasında hiç toprak kalmayınca ne ses
kaldı ne parazit. Sebep: `BCK`/`LRCK`/`DIN` gerilim sinyalleridir ve DAC bir pinin
"yüksek" olduğunu **yalnız kendi toprağına göre** anlayabilir. İki ayrı besleme,
ortak referans yok, sinyal tanımsız. Alıcı bu sırada kusursuz çalışıyordu —
`gaps=0 under=0`, `rtp` ilerliyor — yani ESP geçerli örnekler yazıyor, DAC
alamıyordu.

**Doğru düzenleme, ve neden:** DAC beslemesini kendi regülatöründen alır, ESP ile
arasında **yalnızca sinyal referansı** taşıyan bir toprak teli olur. Bu, DAC'ın
kendi 5 V buck'ını (buck B) almasının sebebidir ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]). Gürültünün
sebebi telin varlığı değil, üzerinden geçen besleme akımıydı. Aynı tel, farklı
görev: besleme dönüşü regülatöre, sinyal referansı ESP'ye — ve I²S üçlüsüyle yan
yana çekilir ki ilmek alanı küçük kalsın.

**Ders:** üç arıza aynı anda açıktı ve her biri diğerinin belirtisini taklit
ediyordu. "Ses yok" hem susturulmuş bir DAC hem de referanssız bir DAC demekti;
"gürültü var" hem toprak ilmeği hem de kazanç fazlalığı demekti. Tek tek
ayırmadan hiçbiri çözülmüyordu.

### Açık soru — o gün hangi yapı çaldı? (operatöre, 2026-09-12)

Bu bölüm sesin geldiğini yazıyor ama **hangi çıkış arka ucunun** derli olduğunu
yazmıyor. Kayıt bu konuda ikiye ayrılıyor: `firmware/README.md` o günkü tezgâh
yapısının amfinin önünde DSP zinciriyle çaldığını söylüyor, firmware planı ise
zincirin ürün kartında çaldığının tek kaydının `86f629c` commit mesajı olduğunu.
İkisi de kanıt değil; bunu yalnız o gün tezgâhta olan söyleyebilir. 2026-09-12'den
beri DSP zinciri ürünün çıkış arka ucudur ([[../07-decisions/ADR-0022-dsp-product-output-backend|ADR-0022]])
ve açılış raporu arka ucu adıyla basar, yani bundan sonraki her dinleme kaydı bu
satırı kendisi taşır.

| soru | cevap |
|---|---|
| Hangi commit ve hangi `SDKCONFIG_DEFAULTS` parçaları (`sdkconfig.bench`? yerel parça?) | _(yazılacak)_ |
| Açılış raporunun arka uç satırı ne diyordu (DSP zinciri mi, vendor düz geçişi mi) | _(yazılacak)_ |
| Tezgâh profili uyarısı ("provisional", "not measured") basıldı mı | _(yazılacak)_ |
| Tweeter'ın önünde kondansatör var mıydı, hangi değer | _(yazılacak)_ |

**Sonuç:** _(yazılacak)_

---

## D — Yıldız topraklama

`2026-09-08`: ESP'nin GND'si DAC'a **doğrudan** bağlıyken parazit vardı, sökünce
gitti. Sökmek ilmeği kapattı ama toprağı kaldırmadı — dönüş akımı şimdi ortak
5 V beslemenin toprağından dolaşıyor, yani uzun ve tesadüfi bir yoldan. 1,4 MHz
BCK için bu hem ışıma yapar hem veri hatası riski taşır.

Topraklama planı zaten doğrusunu yazmış: *"Analog/dijital/güç dönüş akımları
planlı yıldız noktada birleşir."* Şemada ESP, DAC, dört amfi, iki buck — hepsi ayrı ayrı
`STAR_GND`'ye gidiyor, birbirine değil.

**Yapılacak:** ESP GND ve DAC GND, birbirine değil, **ikisi de ayrı tellerle tek
bir yıldız noktasına** (pratikte güç girişinin toprağı). Sonra A1 dinleme testini tekrarla.

**Sonuç:** _(yazılacak)_

---

## Sahip gözlemi — paylaşılan buck DAC'a hışırtı bindirdi (2026-09-12)

Sahibin bildirdiği gözlem: ESP32-S3 ile PCM5102A aynı 5 V buck'tan beslenirken
DAC çıkışında duyulur bir hışırtı vardı; DAC'a kendi buck'ı verilince gitti.
**Kurulum ayrıntılı kaydedilmedi** — hangi buck modülü, hangi yük, hangi kablo
uzunluğu, ne ölçüldü, hiçbiri yazılmadı. Bu yüzden bu bir ölçüm değil, bir
gözlemdir ve hiçbir kapıyı açmaz; yukarıdaki §3 ile birlikte iki ayrı 5 V
buck kararının (buck A ESP32-S3, buck B DAC; ADR-0020) tezgâh gerekçesidir.
Gürültü tabanı G1'de dummy-load üzerinde osiloskopla, iki buck'la ve
kaydedilmiş kurulumla ölçülür; ancak o zaman "geçti" yazılır.

**Sonuç:** _(G1'de ölçülecek)_

---

## Adaptör yüksüz gerilimi — ölçüldü (2026-09-12)

ADR-0020'nin ilk kuralı: adaptörün boşta çıkışı kabine bağlanmadan önce
ölçülür ve 25,5 V'un altında olmalıdır. Sahibi ölçtü: fiş boşta, DMM DC volt.

| ölçüm | değer | koşul | sonuç |
|---|---|---|---|
| Yüksüz çıkış | **24,49 V** | < 25,5 V | **GEÇTİ** — etiketin %2 üstünde, 26 V amfi sınırına 1,5 V pay |
| Etiket | **24 V / 2,91 A, 70 W max** | 24 V / 2,9 A sınıfı (ADR-0020) | uyuyor; marka/model kaydedilmedi |
| Fiş | "standart" barrel | 5,5 × 2,1 mm merkez pozitif (ADR-0020) | dış çap standart görünüyor; **iç pim 2,1 mi 2,5 mm mi ölçülmedi** — kabin jakı buna göre seçilir |
| Polarite | **iç kontak +, dış kontak −** | merkez pozitif | **GEÇTİ** — DMM ile fişte okundu |
| Şebeke tarafı | PC güç kablosu, **topraklı** (IEC C14 girişli, Sınıf I) | — | DC `−` ucunun PE'ye bağlı olup olmadığı **ölçülmedi**: Sınıf I adaptörlerin bir kısmı DC `−`'yi PE'ye bağlar, bir kısmı bağlamaz |

DMM modeli ve kademesi kaydedilmedi; tek okuma, ısınma izlenmedi. Bu satırlar
tek başına `G1`'i açmaz: tam yük gerilimi ve `VIN` çökmesi `G1`'in dummy-load
satırlarıdır. Bütçe aritmetiği etiketle aynı kalır: 2,91 A × 24 V ≈ 70 W,
`supply_budget_sq` bu akıma karşı `S7`'de alınır.

Kalan ölçüm, adaptör prize takılı ve DC fişi boştayken: DMM ohm kademesinde
fişin **dış kontağı ↔ C14 girişinin toprak pimi** (ya da duvar fişinin PE
pimi). Sonsuz → çıkış izole, `POWER_GND` yüzer; ~0 Ω → `POWER_GND` şebeke
toprağına bağlı, osiloskop toprak klipsi onunla aynı potansiyelde ve kabinin
tek toprak noktası PE olur (§D yıldız topraklama buna göre okunur). İkisi de
kabul edilebilir; hangisi olduğu kaydedilmeden scope bağlanmaz.

**Sonuç:** yüksüz gerilim, etiket ve polarite satırları kapandı; iç pim çapı
ve DC `−` ↔ PE ilişkisi açık.

---

## E — Oynatma testi (ürün yolu, sonraki oturum)

Sahibin isteği: crossover bu değerlerle hazır olsun, sonraki oturum düzeneği
kurup bir oynatma testiyle bitsin. Hazırlık depoda yapıldı — sayılar
`docs/assets/measurements/drivers/profile-2026-09-12-provisional.json`
dosyasında, `firmware/tools/write_profile.py` onları `factory_cal`'ın `profile`
blob'una çeviriyor ([[../04-acoustics/measurement-and-dsp-plan#Profili yazmak — değerler dosyası, araç, flaş|DSP planı]]).
Bu bölüm tezgâhta ne yapılacağını sırasıyla yazar.

Bir kez daha, çünkü testin bütün anlamı burada: **bu bir dinleme testidir,
kapı değil.** Profil `provisional` adını taşır, içindeki iki DC direnç dışında
hiçbir sayı ölçülmüş değildir, ve yazılmış olması ürünün ses yolunu **açar** —
`hk_main` onu `ok` bulur, `hk_storage` ses iznini verir, akış gelince DAC canlı
amfilere bırakılır, tezgâh istisnası olmadan. O yüzden yazılma anı, kademeli
çiftin (tek amfi, bir woofer, `C_SAFE`'li bir tweeter) tezgâhta hazır olduğu
andır; sekiz sürücüye bağlı bir karta yazılmaz.

### E1. `GPIO13 → XSMT` jumper'ını lehimle

12 Eylül gecesi üç kez çıktı ([[../08-development-log/2026-09-12-fs-bench-session|günlük]]);
8 Eylül'de aynı belirti — o gün tel hiç bağlı değildi. Çıkan tel DAC'ı sessizce susturur ve "ses yok" başka her
arızayı taklit eder. Lehimlendikten sonra **`B1` tekrarlanır** (`XSMT` ↔ `3V3`,
güç kapalı): lehim bir köprü yapmışsa `GPIO13` düşükken 3V3 rayına kısa devredir,
ve `B1`'in bir güvenlik ölçümü olmasının sebebi budur.

- [ ] Lehimlendi, tel çekmeye dayanıyor.
- [ ] `B1` yeniden ölçüldü: _(değer)_

### E2. `C3` — amfi kazanç strap'ini oku

Yukarıdaki `C3` satırı: kullanılacak amfide, mümkünse dördünde. Sonuç iki yere
yazılır — `C3`'ün sonuç satırına ve değerler dosyasının `amp_gain_db` alanına
(20/26/32/36; okunmadıysa 0 kalır: doğrulayıcı sıfırı reddetmez; 24 V'ta
sürücü dinlemesi ise `C3`'ten bağımsız kapalıdır, E4). Bu adım profil yazılmadan **önce** gelir,
çünkü profil kaynağını taşır ve okunan kazanç oraya girer; sonradan okunursa
dosya güncellenir, blob yeniden yazılır, `factory_cal` bir kez daha flaşlanır.
36 dB bulunursa `C3` tablosu ne yapılacağını söyler; strap değiştirildiyse
dosyaya değiştirilmiş hâli yazılır.

- [ ] Amfi #_: `GAIN0`/`GAIN1` okuması → _ dB
- [ ] Dört kart aynı mı: _(yazılacak)_

### E3. `C_SAFE` tweeter'ın önünde mi?

Sürücü ölçüm planının açık sorusu ([[../02-hardware/driver-measurements|sürücü ölçüm planı]], `C_SAFE` bölümü):
7 µF parça arızalı çıktı ve takılı değildi, 10 µF'in takıldığına dair kayıt
yok. Bu testte tweeter, önünde 10 µF kutupsuz film kondansatör olmadan
**bağlanmaz**: DSP crossover'ının köşesi bir tahminden türedi ve yanlışsa
kondansatör tweeter'ın tek korumasıdır. Kondansatör amfinin R çıkışı ile
tweeter arasındadır ve BTL olduğu için kutupsuz olmak zorundadır. Yoksa test
yalnız woofer'la yapılır ve öyle yazılır.

- [ ] Değer ve tip: _(yazılacak)_ · takılı değilse tweeter bağlanmadı: [ ]

### E4. Düzenek — tek amfi, bir çift

`AGENTS.md`'nin kademeli kuralı: ilk enerjilenen yol bir amfi, bir woofer, bir
tweeter. Öteki üç amfi sürücüye bağlanmaz; girişleri de boş kalır.

| bağlantı | nereden | nereye | not |
|---|---|---|---|
| woofer bandı | DAC `LOUT` | amfi **L** girişi | ADR-0002: sol = woofer |
| tweeter bandı | DAC `ROUT` | amfi **R** girişi | sağ = tweeter |
| woofer | amfi **L** çıkışı | woofer | terminal polaritesini yaz |
| tweeter | amfi **R** çıkışı | `C_SAFE` → tweeter | E3 |
| toprak | ESP `GND`, DAC `GND` | ayrı tellerle yıldız noktasına | §D; birbirine değil |

BTL: hoparlör eksi uçları şasi toprağı değildir; hiçbirine skop toprağı ya da
ortak tel bağlanmaz. Çapraz kabloya dikkat: mono programda çaprazlanmış bir
çift görünmezdir ve kendini ilk kez crossover çalışınca, tweeter'a bas
göndererek belli eder (`86f629c`); E6 bası bu yüzden önce woofer'da arar.

Dummy-load varsa çıkışlar önce ona gider: iki çıkışa 4 Ω sınıfı direnç,
sürücüler henüz bağlı değil; yoksa E8'e "dummy-load yok" yazılır. Bu çift
8 ve 12 Eylül'de sürücüde enerjilendiği için bu bir ilk enerjilendirme değil,
yeniden enerjilendirmedir — kademeli kuralın "önce dummy-load" adımı burada
varsa yapılır, şart koşulmaz. Sürücülere geçiş güç kapalıyken yapılır (E6).

Besleme: **akım sınırlı tezgâh kaynağı, 12 V — her hâlde ≤ 15 V — ve `C3`
okunmuş olsa da öyle.** Adaptörün ilk bağlantısı `G1`'in dummy-load satırıdır
(ADR-0020); sürücüye 24 V ise `C3`'ten bağımsız olarak kapalı: risk kaydının
Kritik satırı (TPA3110D2 15 V üstünde asgari 4,8 Ω BTL yük, iki Nova da 4 Ω
sınıfı) `G0` `Z_min` ve `G1`'in 24 V / 4 Ω dummy-load kaydı olmadan hiçbir
sürücünün 24 V'ta bağlanmamasını ister. `C3`'ün buradaki işi kazancı bilmek,
24 V'un kapısını açmak değil. Kaynağın gerilimi ve akım sınırı yazılır.

- [ ] Amfi #_, woofer #_, tweeter #_; kaynak _ V (≤ 15) / _ A sınır

### E5. Profili yaz, kartı aç, açılış raporunu oku

1. Değerler dosyasını aç, E2'nin kazancını `profile.amp_gain_db`'ye yaz,
   `provenance.amp_gain_db.status`'u `placeholder`dan `measured`a çevir ve
   `source`'a `C3`'ün sonuç satırını yaz; başka hiçbir alana dokunma.
2. `python3 firmware/tools/write_profile.py <değerler.json> --device-dir <dizin>`
   — dizin, kartın kendi cihaz dizinidir (`provision_credentials.py`'nin o kart
   için yazdığı, `factory_cal.csv`'nin durduğu dizin; `IDF_PATH` gerekir).
   Araç `profile` satırını **o** CSV'ye ekler, `factory_cal.bin`'i yeniden üretir
   ve flaş komutunu basar. ADR-0023 öncesi kartın dizinindeki eski kimlik
   satırları varsa olduğu gibi kalır (firmware onları okumaz). Flaşlamaz.
3. Basılan `esptool … write_flash 0x13000 …` komutunu **sen** çalıştırırsın:
   `factory_cal` bölümü `0x13000`'de, boyut `firmware/partitions.csv`'den
   (`0xd000`). Flaşlamadan önce imajın tam `0xd000` bayt olduğuna bak — kısa
   bir imaj o ofsette kimlik bilgilerini siler ve yerine hiçbir şey koymaz
   (`firmware/README.md`). `nvs` bölümüne dokunulmaz; kartın katıldığı Wi-Fi
   durur. Aracın bastığı ikinci çift komut — `read_flash` ve `--dump` — bölümü
   geri okuyup cihazın yargılayacağı sayıları tablo olarak gösterir; açılış
   raporundan önce buna bak.
4. Yapı: ürün yapısı (`sdkconfig.defaults`), çünkü sınanan şey ürünün yazılı
   bir profille çalmasıdır. Tezgâh yapısı (`sdkconfig.bench`) da olur — profil
   varken istisnası bir şey yapmaz, yalnız telemetri satırı 10 s'de bir gelir.
   Hangisi olduğu yazılır; 8 Eylül'ün açık sorusu ("hangi yapı çaldı") bu kez
   açılış raporundan cevaplanır.
5. Seri hatta, `hk` etiketinde, sırayla:

```text
hk: profile     present, judged ok at 44100 Hz for a 24000 mV supply
W hk: profile     source 'provisional-2026-09-12' is PROVISIONAL: a bench listening profile, not a calibration. Staged pair only (one amplifier, one woofer, one tweeter), low level, supply at or below 15 V; nothing in it was measured except the two DC resistances.
hk: audio       profile present:ok · verdict PERMITTED
```

ve arka uç (`hk_out_dsp`) başlarken:

```text
hk_out_dsp: calibration <yyyymmdd> from '<source>': crossover 3500 Hz, subsonic 55 Hz, release/hold 150/20 and 150/20 ms, delay 0/0 samples, tweeter in phase, supply budget 1.000 over 100 ms, amplifier gain ...
hk_out_dsp: DSP path built at 44100 Hz: ... Left = WOOFER, right = TWEETER (ADR-0002) ...
```

`present:ok` ses iznini tek başına veren tek gate durumudur. `present:`
sonrasında başka bir sözcük varsa (`schema`, `source`, `frequency`, `ceiling`,
`budget`, `amp-gain`, …) profil o alan yüzünden reddedilmiştir, ses susturulu
kalır ve `profile     present and REFUSED: <ad>` satırı da gelir: değerler
dosyasını düzelt, yeniden yaz, dinlemeye geçme. Tezgâh yapısındaysan
`BENCH PROFILE IN USE AND IT WAS NOT MEASURED` uyarısı artık **gelmez** —
saklanan profil derlenmiş olanın önüne geçer — ve profilin ölçülmediğini
söyleyen tek şey `from '<source>'` içindeki `provisional` sözcüğüdür. Satırı
okurken bunu bil. `24000 mV` yapının beyanıdır (`CONFIG_HK_SUPPLY_MV`),
kaynağın ölçümü değil; profilin referansı 12000 olduğu için iki tavan yarıya
iner, sessize doğru hata.

- [ ] Commit, yapı parçaları, imaj boyutu, üç satır: _(yazılacak)_

### E6. Düşük seviyede çal, dinle

Dummy-load varsa önce onunla: akış aç, kaydırıcı en altta, çıkışta DMM AC ya
da skop ile sinyalin geldiği görülür, akış durdurulur, **güç kapatılır**,
sürücüler bağlanır. Sonra telefondan, kaydırıcı en altta; yavaş yavaş yukarı.
Üç şeye kulak:

- **Crossover ayrımı duyuluyor mu.** Kulak sırayla her sürücüye: bas yalnız
  woofer'dan, tiz yalnız tweeter'dan gelmeli. Tweeter'dan bas geliyorsa çapraz
  kablo (E4), profil değil — akışı durdur.
- **Boşta hışırtı var mı.** Akış dururken ve `SILENT`'a döndükten sonra
  sessizlik tam olmalı (`A1`). Hışırtı varsa §D yıldız toprak ve `C3`; ikisi
  de bu testte kapanmaz, yazılır.
- **Akış başı/sonu pop.** `PLAYING -> MUTING -> SILENT` satırlarıyla birlikte;
  amfi susturmasız olduğu için "pop yok" vaadi yok, ne duyulduğu yazılır.

- [ ] Kaydırıcı konumu ve üç gözlem: _(yazılacak)_

### E7. Telemetri satırını kaydet

En az bir tam aralık (üründe 60 s, tezgâh yapısında 10 s) çaldıktan sonra
`hk_out_dsp` şu satırı basar; olduğu gibi kopyala:

```text
dsp block max M us mean m us of 7981 us (per 352-frame block, normalised; ...); supply mean_sq max S gain min g
```

`M < 7981` F3 ölçütünün sorduğu sorudur, ama tek satır 30 dakikalık kanıt
değildir. `g` 1,000 ise besleme katı hiç devreye girmemiştir — yer tutucu
bütçeyle beklenen bu. Alıcının kendi satırındaki `under=` sayacı yanına
yazılır.

- [ ] Satır(lar): _(yazılacak)_ · `under=`: _

### E8. Ne yazılır

[[test-log|Test kaydına]] bir satır, "kapı değil" sonucuyla; ayrıntı buraya:

| alan | değer |
|---|---|
| tarih, kart kimliği, commit, yapı parçaları | |
| değerler dosyası, `source` dizesi, `amp_gain_db` | |
| `factory_cal.bin` boyutu; flaş komutu | |
| açılış raporu: `profile`, `audio`, `calibration` satırları | |
| amfi #, `C3` okuması, `C_SAFE` değeri, sürücü kimlikleri, polarite | |
| besleme: kaynak, gerilim, akım sınırı | |
| dummy-load adımı yapıldı mı, ne görüldü | |
| kaydırıcı konumu; ayrım / hışırtı / pop | |
| telemetri satırı(ları), `under=` | |
| düzenekte çıkan, kopan, değişen şeyler | |

### E9. Ne iddia edilmez

- **`G0` değil.** Empedans eğrisi ve iki `Fs` ölçülmedi; profil bunları
  taşımıyor, `provisional` adı bu yüzden.
- **`G1` değil.** Dummy-load'da skop kaydı, `VIN` çökmesi, adaptör yok;
  besleme bütçesi yer tutucu.
- **`G2` değil.** Tavanlar, kazançlar, gecikme ve polarite ölçülmedi; "ayrım
  duyuldu" bir dinleme gözlemidir, crossover'ın doğru yerde olduğunun kanıtı
  değil.
- **`F2`/`F3` kabulü değil.** I2S test noktalarında skop yok, dummy-load'da
  −40/−20 dBFS yok; bir telemetri satırı 30 dakika değil.
- Profil yazılmış olmakla kalibrasyon olmaz: blob, kaynağında yazdığı gibi,
  kayıttaki geçici sayıların taşıyıcısıdır. Öteki üç amfi bağlanmadı ve bu
  testten sonra da bağlanmaz.

---

## Sıradaki

Bu ölçümler kapandıkça:

- Sonraki oturum §E ile biter: jumper lehimli, `C3` okunmuş, `C_SAFE` yerinde,
  tek amfi ve bir çift, `provisional` profil `factory_cal`'da, açılış raporunda
  `present:ok`, telefondan düşük seviyede dinleme ve bir telemetri satırı.
  Kapı değil; sonucu test kaydına girer.
- `C3` 36 dB gösterirse: kazanç düşürülür ve EQ bunun üstüne kurulur. Ne gösterirse
  göstersin, sürücüde 24 V dinleme `C3` ile açılmaz: `G0` `Z_min` ve `G1`'in
  24 V / 4 Ω dummy-load kaydı ister (risk kaydının Kritik satırı); o güne kadar
  kaynak ≤ 15 V.
- Ürün kartında DSP arka ucuyla uzun bir akış: `dsp block max ... us mean ... us of ... us`
  satırları (üründe 60 s'de bir, tezgâh yapısında 10 s'de bir; sınır 7981 µs) ve
  underrun sayacı ([[test-strategy|test stratejisi]], firmware ölçümleri).
- Hepsi kapandıktan sonra `G1` (tek amfi kukla yükte; sonra dört amfi birden sürülürken `VIN` akım bütçesi) ve amfi çıkışı test noktalarının osiloskop kaydı.
