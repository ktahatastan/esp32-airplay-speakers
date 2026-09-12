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
> profiliyle çalışıyor, ölçülmüş değerle değil (`F3`). Amfinin kazanç strap'i
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

## Sıradaki

Bu ölçümler kapandıkça:

- `C3` 36 dB gösterirse: kazanç düşürülür ve EQ bunun üstüne kurulur. Ne gösterirse
  göstersin: `C3` dolmadan 24 V'ta sürücüde dinleme yok.
- Ürün kartında DSP arka ucuyla uzun bir akış: `dsp block max ... us mean ... us of ... us`
  satırları (üründe 60 s'de bir, tezgâh yapısında 10 s'de bir; sınır 7981 µs) ve
  underrun sayacı ([[test-strategy|test stratejisi]], firmware ölçümleri).
- Hepsi kapandıktan sonra `G1` (tek amfi kukla yükte; sonra dört amfi birden sürülürken `VIN` akım bütçesi) ve amfi çıkışı test noktalarının osiloskop kaydı.
