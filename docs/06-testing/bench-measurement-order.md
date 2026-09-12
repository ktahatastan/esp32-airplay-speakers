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
> profiliyle çalışıyor, ölçülmüş değerle değil (`F3`). Tweeter bağlıyken uzun
> süre ses verme. Tezgâhta tek amfi ve tek woofer/tweeter çifti vardır; öteki üç
> amfi sürücülere `G0`-`G2` o çiftte geçmeden bağlanmaz.

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

## Sıradaki

Bu ölçümler kapandıkça:

- `C3` 36 dB gösterirse: kazanç düşürülür ve EQ bunun üstüne kurulur.
- Hepsi kapandıktan sonra `G1` (tek amfi kukla yükte; sonra dört amfi birden sürülürken `VIN` akım bütçesi) ve amfi çıkışı test noktalarının osiloskop kaydı.
