---
title: Tezgâh ölçüm sırası — ses zinciri ve susturma hatları
status: in-progress
owner: orchestrator
reviewers: [hardware-reviewer, verifier]
updated: 2026-09-08
tags: [testing, bench, audio, grounding, mute, procedure]
---

# Tezgâh ölçüm sırası

`2026-09-08` tezgâh oturumunda açık kalan her soruyu kapatmak için. Sıra keyfî
değil: önce **hiçbir alet gerektirmeyen** ve en büyük bilinmezi kapatan test,
sonra **lehimden önce yapılması zorunlu** güvenlik ölçümleri, sonra gerisi.

Her adımda ne ölçüleceği, aletin hangi konumda olacağı, **beklenen değer** ve o
değer çıkmazsa ne anlama geldiği yazıyor. Sonucu buraya yaz; bir agent fiziksel
bir testi geçmiş sayamaz, operatör kaydeder.

> **Her ölçümden önce:** hoparlör terminallerinde ne olduğuna bak. Nova
> sürücülerinin empedansı hâlâ ölçülmedi (`G0`, açık `Kritik`), ve yüksek geçiren
> filtre ile limiter yok (`F3`). Tweeter bağlıyken uzun süre ses verme.

---

## A — Alet gerekmiyor: gürültünün kaynağı ekran mı?

En büyük açık soru bu, ve cevabı kulakla veriliyor. Karttaki firmware 8 saniye
paneli normal sürüyor, 8 saniye **hiçbir şey göndermiyor**, ve her geçişi
yazıyor:

```text
W hk_lcd: noise probe: panel bus ACTIVE (30 fps) -- listen now
W hk_lcd: noise probe: panel bus SILENT (no transfers at all) -- listen now
```

### A1. Parazit döngüyü takip ediyor mu?

Hiçbir şey çalmadan dinle, en az üç tam döngü.

| gözlem | anlamı |
|---|---|
| Sessiz yarıda parazit **kesiliyor** | Kaynak ekran veri yolu. Çözüm yazılımda değil: kablo ayrımı, SPI hızını düşürmek, seri direnç. |
| İki yarıda da **aynı** | Ekran değil. Sıradaki şüpheliler Wi-Fi ve ortak 5 V rayı. |
| Sessiz yarıda **azalıyor ama bitmiyor** | Birden fazla kaynak var; ekran bir tanesi. |

**Sonuç:** _(yazılacak)_

### A2. Buton da aynı döngüyü takip ediyor mu?

Aynı derlemede, **her iki yarıda da** butona bas.

| gözlem | anlamı |
|---|---|
| Sessiz yarıda düzgün, aktif yarıda garip | Ekran veri yolu butona da biniyor — parazitle **aynı kök sebep**. |
| İki yarıda da aynı | Ekranla ilgisi yok; buton hattına ayrıca bakılır. |

**Sonuç:** _(yazılacak)_

### A3. Çalma durunca fısıltı tamamen kesiliyor mu?

Bir şey çal, sonra durdur ve birkaç saniye bekle. `hk_audio_hw` sekansı geri
sarmalı ve `XSMT` tekrar düşmeli. Seri kayıtta görülmesi gereken:

```text
hk_audio: PLAYING -> MUTING: ...
hk_audio: MUTING -> SILENT:  amp gpio21=0 dac xsmt gpio13=0 ...
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

### B4. XH-A232'de `SD` padi var mı?

`hk_pins.h` bunu bir **rezervasyon** olarak işaretlemiş; şemada da kesikli
çizili. TPA3110 çipinin `SD` pininden kartta erişilebilir bir noktaya süreklilik
ara. Bulursan o noktayı GND'ye göre de ölç: kart `SD`'yi kendi yukarı çekiyorsa
10 kΩ baskın gelmeyebilir ve `R7` yeniden hesaplanmalı.

| sonuç | anlamı |
|---|---|
| erişilebilir nokta **var** | `GPIO21` bağlanır, `R7` takılır, şemadaki kesikli dal düz çizgiye döner. |
| **yok** | Firmware tarafından kontrol edilebilir amfi mute'u olmaz; sadece DAC'ın `XSMT`'si kalır. Kapanış "pop"u bastırılamaz. Bu bir karar olarak yazılır. |

**Sonuç:** _(yazılacak)_

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
arasında **yalnızca sinyal referansı** taşıyan bir toprak teli olur. Gürültünün
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
planlı yıldız noktada birleşir."* Şemada ESP, DAC, amfi, buck — hepsi ayrı ayrı
`STAR_GND`'ye gidiyor, birbirine değil.

**Yapılacak:** ESP GND ve DAC GND, birbirine değil, **ikisi de ayrı tellerle tek
bir yıldız noktasına** (pratikte güç girişinin toprağı). Sonra A1'i tekrarla.

**Sonuç:** _(yazılacak)_

---

## Sıradaki

Bu ölçümler kapandıkça:

- `A1`/`A2` ekranı işaret ederse: SPI hızı, kablo ayrımı ve seri direnç değerlendirilir.
- `B4` bir `SD` noktası bulursa: şemadaki kesikli dal kesinleşir, `R7` takılır.
- `C3` 36 dB gösterirse: kazanç düşürülür ve EQ bunun üstüne kurulur.
- Hepsi kapandıktan sonra `G1` (amfi kukla yükte) ve `TP30`/`TP31` osiloskop kaydı.
