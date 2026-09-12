---
status: proposed
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-08
tags: [hardware, wiring, schematic, power, audio]
---

# Devre ve bağlantı şemaları

Bu belge **tek Merzarkabul Airplay Speakers hoparlörü** için modül-temelli prototip bağlantı planıdır. Dört hoparlörde aynı devre tekrarlanır. Şema, özel üretim PCB şeması değildir; modüllerin gerçek baskı yazıları ve süreklilik ölçümleri görülmeden kablo bağlanmaz.

> [!danger] Enerji verme yasağı
> Nova woofer/tweeter değerleri G0 ile ölçülmeden gerçek sürücülere tam güç uygulanmaz. XH-A232 önce akım sınırlı laboratuvar kaynağı ve dummy-load ile G1 testinden geçer. Tweeter, `C_SAFE` seri koruma kondansatörü ile DSP HPF/limiter doğrulanmadan bağlanmaz.

## Devre şeması

![Merzarkabul Airplay Speakers tek hoparlör devre şeması](assets/merzarkabul-schematic.svg)

Tek sayfalık pafta; DC giriş jakı ve 19 V adaptör, ters polarite adayı ve bulk kondansatör, 5 V lojik beslemesi, ESP32-S3 N16R8, PCM5102A, XH-A232 BTL bi-amp, sürücüler, kullanıcı arayüzü, GPIO13/GPIO21 susturma hatları ve `TP0-TP21` ölçüm noktalarını gösterir. Ölçeklenebilir SVG'dir; Obsidian ve GitHub üzerinde doğrudan açılır.

Pafta `hardware/diagrams/generate_schematic_svg.py` ile üretilir ve elle düzenlenmez. Bu çizim modül-temelli prototip içindir; üretim PCB şeması yerine geçmez.

### Düzenlenebilir KiCad paftası

Netlist, ERC ve ileride PCB için elektriksel kaynak KiCad projesidir: [[kicad-schematic|KiCad şeması ve üretim scripti]]. Script `hardware/kicad/` altında Git'te tutulur. Üretilen `.kicad_sch` ve `.kicad_pro` **depoda yok**: bu makinede KiCad sembol kütüphaneleri kurulu olmadığı için `kicad-sch-api` çözemediği bir sembolü yerleştirmeyi reddediyor ve üreteç burada **hiç çalışmıyor**, yalnız `--validate` değil. Yani ERC ve PDF çıktısı alınmadı; üreteçteki yapısal self-check ayrı olarak koşturulmadan pafta doğrulanmış sayılmaz. Pafta depoda yokken `scripts/check_generated_kicad.py` bunu bir notla geçer; pafta yeniden üretilip eklendiğinde üreteç ile çıktının ayrışmasını CI'da yakalar. Geri getirme komutları `hardware/kicad/README.md` içinde.

İki çıktı bilerek ayrıdır: SVG paftası okunabilirlik ve bring-up prosedürü için, KiCad paftası elektriksel doğrulama için tutulur. İkisi de aynı net adlarını ve aynı `TP0-TP21` numaralandırmasını kullanır.

## 1. Sistem bağlantı özeti

```mermaid
flowchart LR
    PHONE[Apple cihazı / Wi-Fi] -->|AirPlay - doğrulanacak| ESP[ESP32-S3 N16R8]
    ESP -->|BCLK + LRCK + DATA| DAC[PCM5102A I2S DAC]
    DAC -->|LOUT = woofer yolu| AL[XH-A232 kanal L]
    DAC -->|ROUT = tweeter yolu| AR[XH-A232 kanal R]
    AL -->|L+ / L- BTL| W[Woofer - ohm TBD]
    AR -->|R+ / R- BTL + C_SAFE| T[Tweeter - ohm TBD]

    ADP[19 V masaüstü DC adaptör] --> J1[5.5 x 2.1 mm DC jak - merkez pozitif]
    J1 --> D2[D2 ters polarite adayı]
    D2 --> VIN[VIN 19 V]
    VIN --> AMP[XH-A232 8-26 V]
    VIN --> BUCK[MP1584 5.10 V]
    BUCK --> ESP
    BUCK --> DAC

    BTN[Çok işlevli buton] --> ESP
    ESP --> LED[Ortak katot RGB LED]
```

## 2. DC giriş ve ana güç dağıtımı

Besleme topolojisi [[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]] ile kilitlidir: 19 V masaüstü DC adaptör, kutuya **5,5 × 2,1 mm merkez pozitif** barrel jak üzerinden girer ve jaktan gelen ray `VIN` adını taşır. `VIN` amfiyi doğrudan besler; TPA3110'un 8-26 V ve MP1584'ün 4,5-28 V giriş pencereleri 19 V'u içerir. **V1'de güç anahtarı yoktur**: cihaz adaptörü çekilerek kapatılır, boşta beklemeyi firmware'in idle standby'ı karşılar.

Merkez pozitiflik satın alınan adaptör ve jak üzerinde **ölçü aletiyle** doğrulanır; bu bir `G1` kontrol satırıdır, etiket okumak yerine geçmez. Adaptörün akım sınıfı tahmin edilmez: `G1`'de dummy-load üzerinde ölçülen tepe akımdan gelir. İlk enerjilendirme yine adaptörle değil, akım sınırlı laboratuvar kaynağıyla yapılır.

### 2.1 Ana güç dağıtımı

```mermaid
flowchart TB
    JACK[J1 DC jak: TP0 DC_IN 19 V / TP2 POWER_GND] --> D2[D2 seri Schottky / ideal-diyot ADAY - yoksa 0 ohm köprü]
    D2 --> VIN[TP1: VIN 19 V]
    VIN --> CMAIN[C_A: 470-1000 uF / 25 V düşük ESR aday]
    VIN --> AMP[XH-A232 VCC]
    VIN --> BIN[MP1584 IN+]
    PGND[POWER_GND] --> AMP_G[XH-A232 GND]
    PGND --> BING[MP1584 IN-]

    BIN -->|önce yüksüz ayarla| BOUT[TP3: MP1584 OUT+ = 5.10 V]
    BING --> BOUTG[MP1584 OUT-]
    BOUT --> JP1[JP1 servis güç ayırma jumperı]
    JP1 --> ESP5[ESP32-S3 5V/VBUS pini]
    JP1 --> DAC5[PCM5102A VIN]
    BOUTG --> STAR[TPG: STAR_GND]
    STAR --> ESPG[ESP32 GND]
    STAR --> DACG[PCM5102A GND/AGND]
    STAR --> PGND
```

- `D2` ters polarite koruması **adaydır**: seri Schottky, ideal-diyot modülü ya da hiçbiri. Kararı `G1`'de ölçülen ileri düşüm ve ısınma verir; takılmazsa yerine 0 Ω köprü gelir ki `DC_IN` ile `VIN` tek net olsun. Şemada parça olarak durur, çünkü bir not sipariş edilemez ve denetlenemez.
- MP1584 çıkışını ESP32/DAC bağlı değilken `5.10 V` değerine ayarla; sonra elektronik yükle doğrula.
- ESP32 USB ile programlanırken `JP1` açılır. Geliştirme kartının USB ile harici `5 V` hattını güvenle OR'ladığı kanıtlanmadıkça iki kaynak aynı anda bağlanmaz.
- XH-A232 girişinde kart üzerinde yeterli bulk kapasitör yoksa amfiye yakın `470-1000 µF / 25 V` düşük-ESR kondansatör adayı denenir. 25 V sınıfı 19 V'ta yeterlidir; adaptör gerilimi yukarı çekilirse 35 V sınıfı seçilir. Değer G1 ölçümüyle kesinleşir.
- Adaptörün çıkışı koruma toprağına bağlı olabilir. Adaptörle çalışırken osiloskop bağlamadan önce §7.3'teki izolasyon ölçümü yapılır.

## 3. ESP32-S3 -> PCM5102A -> XH-A232 ses zinciri

### 3.1 Aday ESP32-S3 pin planı

Bu GPIO tablosu [[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]] ile kilitlenen ESP32-S3 **N16R8** (16 MB flash + 8 MB PSRAM) kartı için **prototip adayıdır**. Kart seçimi `accepted`, pin ataması `candidate`: satın alınan kartın şeması ve boot testi görülmeden pin tablosu `accepted` yapılmaz.

| İşlev | ESP32-S3 aday GPIO | Modül ucu | Not |
|---|---:|---|---|
| I2S BCLK | GPIO4 | PCM5102A `BCK` | Kısa ve GND referanslı kablo |
| I2S LRCLK/WS | GPIO5 | PCM5102A `LCK/LRCK` | Kanal saat sinyali |
| I2S DATA OUT | GPIO6 | PCM5102A `DIN` | ESP -> DAC tek yön |
| DAC SCK/MCLK | Bağlanmaz | PCM5102A `SCK` -> GND | 3-wire BCK-PLL modu; modül jumperı doğrulanır |
| Fonksiyon butonu | GPIO7 | Buton -> GND | Active-low; 10 kΩ pull-up aday |
| RGB kırmızı | GPIO8 | `R_R` -> LED R | PWM |
| RGB yeşil | GPIO9 | `R_G` -> LED G | PWM |
| RGB mavi | GPIO10 | `R_B` -> LED B | PWM |
| Amfi susturma | GPIO21 | TPA3110 `SD` | **Aktif düşük.** `R7` 10 kΩ pull-down zorunlu, amfi ucuna monte edilir. Rezervasyon: `SD` pad erişimi doğrulanmadı, bkz. §9 |
| DAC susturma | GPIO13 | PCM5102A `XSMT` | **Aktif düşük.** `R6` 10 kΩ pull-down zorunlu. Bağlamadan önce modül köprüsü ölçülür, bkz. §3.3 |

Tablo [[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]] ile genişletildi. Dokuz GPIO'nun tamamı `firmware/components/hk_pins/include/hk_pins.h` içinde aynı numaralarla tanımlıdır ve host testi bu tabloyu işlev işlev doğrular.

`GPIO14-17` bilerek boş bırakıldı. Serbest pinler arasında hem RTC yetenekli hem de strapping/USB/UART0/JTAG rolü olmayan tek dörtlü onlar, yani susturma hatlarının yedek havuzu.

`GPIO6` ürünün dışında ikinci bir işe daha koşuluyor: tezgâhtaki geliştirme kartında S/PDIF çıkışı aynı pinden sürülüyor, bkz. 3.5. Ürün kablolamasında bu pin yalnız `DIN`'e gider.

Sıralamanın kendisi `firmware/components/hk_audio/` içinde yazılı ve host'ta test edilmiş: açarken saat -> DAC -> amfi, kapatırken **önce amfi**. TPA3110 veri sayfası kapanış pop'u için shutdown'ın güçten önce verilmesini söylüyor; DAC'ı önce susturup amfiyi sonra kapatmak, DAC'ın kendi geçişini canlı bir amfiden geçirirdi.

**Susturma hatlarındaki pull-down opsiyonel değildir; susturma mekanizmasının kendisidir.** Bu parçadaki her GPIO reset'ten yüksek empedanslı çıkar ve ROM, bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce milisaniye. O pencerede amfiyi susturan tek şey harici dirençtir. Yazılımın işi susturmayı **bırakmaktır**; firmware hiç çalışmazsa hoparlörler sessiz kalır.

Bu iki direncin referans numaraları `R6` (`XSMT` → `STAR_GND`) ve `R7` (`SD` → `POWER_GND`); ikisi de BOM'da ve iki şema üretecinde çizili parçalardır. 2026-09-08'e kadar değildiler: firmware (`firmware/components/hk_audio/`) her iki pini açılıştan itibaren sürüyordu ama şemada iki pin de uçsuz etiketti, PCM5102A blokunda `XSMT` pini hiç yoktu ve pull-down'lar yalnız bir notta geçiyordu. Bir notun sipariş edilmesi, lehimlenmesi veya denetlenmesi mümkün değildir.

Her iki direnç de **modül ucuna** monte edilir, ESP ucuna değil. Uçan kablo koparsa pad kendi pull-down'unu görmeye devam eder; direnç ESP ucunda olsaydı kopan kablo pad'i serbest bırakırdı.

`GPIO18/19/20` susturma hattı olamaz: silikon bunları açılışta HIGH sürer. `GPIO0/39/43/44` de olamaz: zayıf dahili pull-up ile açılırlar. İkisi de yazılım var olmadan amfiyi serbest bırakırdı.

Kaçınılan pinler: boot/strapping `GPIO0/3/45/46`, native USB `GPIO19/20`, SPI flash `GPIO26-32`, **oktal PSRAM `GPIO33-37`** (N16R8'in R8'i), UART0 konsolu `GPIO43/44`, ve S3 die'ında var olmayan `GPIO22-25`. Tamamı `hk_pins.h` içinde derleme zamanında reddedilir — ESP-IDF bu kart yapılandırmasında `GPIO33-37`'yi rezerve **etmez**, gerekçesi ADR-0011'de. Kesin kart farklıysa tablo yeniden hazırlanır.

### 3.2 I2S ve analog bağlantı

```mermaid
flowchart LR
    G4[ESP GPIO4] -->|TP5: BCLK| BCK[PCM BCK]
    G5[ESP GPIO5] -->|TP6: LRCLK / WS| LCK[PCM LCK]
    G6[ESP GPIO6] -->|TP7: I2S DATA| DIN[PCM DIN]
    DG[ESP GND] --- PG[PCM GND]
    P5[5.10 V] --> VIN[PCM VIN]
    SCK[PCM SCK] -->|3-wire PLL için| SGND[GND]

    G13[ESP GPIO13] -->|TP20: aktif düşük| XSMT[PCM XSMT]
    XSMT --- R6[R6 10 kOhm pull-down] --- SG[STAR_GND]
    G21[ESP GPIO21] -.->|TP21: pad ADAY| SD[XH-A232 SD]
    SD -.- R7[R7 10 kOhm pull-down] -.- PG2[POWER_GND]

    LOUT[TP8: PCM LOUT] -->|ekranlı/kısa| LIN[TP10: XH-A232 L input]
    ROUT[TP9: PCM ROUT] -->|ekranlı/kısa| RIN[TP11: XH-A232 R input]
    AG[PCM AGND] --- AING[XH-A232 input GND]

    LPLUS[TP12: XH L+] --> WPLUS[Woofer +]
    LMINUS[TP13: XH L-] --> WMINUS[Woofer -]
    RPLUS[TP14: XH R+] --> CSAFE[C_SAFE bipolar film - değer TBD]
    CSAFE --> TPLUS[Tweeter +]
    RMINUS[TP15: XH R-] --> TMINUS[Tweeter -]
```

### 3.3 PCM5102A modül ayarları

Mor PCM5102A modül ailesinde kontrol padleri genellikle `FLT`, `DEMP`, `XSMT`, `FMT` olarak çıkar. Modül revizyonu süreklilik ölçümüyle doğrulandıktan sonra başlangıç hedefi:

| Sinyal | Başlangıç hedefi | Amaç |
|---|---|---|
| `SCK` | GND | 3-wire I2S; saat BCK üzerinden dahili PLL |
| `FMT` | LOW | Standart I2S formatı |
| `FLT` | LOW | Normal latency filtre |
| `DEMP` | LOW | De-emphasis kapalı |
| `XSMT` | **`R6` ile LOW tutulur, GPIO13 sürer** | Açılışta DAC susturulmuş; susturmayı firmware bırakır ([[../07-decisions/ADR-0011-audio-side-gpio-reservation\|ADR-0011]]) |

Bu satır 2026-09-08'e kadar "HIGH / kart varsayılanı" diyordu. O okuma kabul edilmiş ADR-0011 ile, yukarıdaki 3.1 pin tablosuyla, BOM'un `R6`/`R7` satırıyla ve pini açılıştan itibaren süren firmware ile çelişiyordu; dördü de aynı şeyi söylediği için düzeltilen taraf bu satır oldu. Karar ADR-0011'dir ve **modül varsayılanı kullanılmaz**: varsayılan "sesi aç" demektir ve tam olarak yazılımın var olmadığı pencerede geçerlidir.

PCM5102A çipi harici SCK olmadan BCK PLL ile çalışabilir. Ancak modülün altındaki lehim köprüleri satıcıdan satıcıya farklı olabilir; pad ismine bakıp körlemesine lehim yapılmaz.

> [!danger] `XSMT` pad'i, GPIO13 bağlanmadan önce ölçülür
> Mor modüllerin bir kısmında `XSMT` pad'i kart üzerinde 3,3 V'a **sert bağlıdır** (lehim köprüsü veya 0 Ω). O durumda 10 kΩ pull-down hiçbir şey tutmaz — yalnız bir bölücü kurar, pad HIGH kalır — ve GPIO13 pini LOW sürdüğü anda 3,3 V rayına kısa devre olur. Yani bu ölçüm atlandığında hem susturma çalışmaz hem de ESP pini zorlanır.
>
> Operatör sırası, modül **enerjisizken**:
>
> 1. `XSMT` pad'i ile modülün `VIN`/3,3 V ucu arasındaki direnci ölç. Kaydet.
> 2. Değer birkaç yüz Ω'un altındaysa köprü vardır: köprüyü kes ve kesildiğini aynı ölçümle doğrula.
> 3. `R6`'yı modül ucuna, pad'in yanına lehimle. `R6` ile `GND` arasında 10 kΩ okunmalı.
> 4. GPIO13'ü ancak bundan sonra bağla.
> 5. İlk enerjilendirmede `TP20`'yi osiloskopla kaydet: reset anından firmware susturmayı bırakana kadar LOW kalmalı.
>
> Adım 1, 3 ve 5 ölçüm kaydı olmadan susturma katmanı doğrulanmış sayılmaz; kaydı operatör [[../06-testing/test-strategy|test kaydına]] yazar. Aynı köprü riski `FMT`, `FLT` ve `DEMP` için de geçerlidir, ama onlarda yanlış sonuç ses formatıdır, susturmanın kaybı değildir.

### 3.4 Kanal ve sürücü kuralları

- Firmware sol dijital kanalı `woofer`, sağ dijital kanalı `tweeter` yolu olarak üretir.
- XH-A232 `L+ / L-` ve `R+ / R-` çıkışları BTL'dir. Hiçbir `-` hoparlör ucu GND/şaseye bağlanmaz.
- `C_SAFE` tek başına crossover değildir; DSP HPF ve limiter'a karşı son savunma katmanıdır.
- `C_SAFE` değeri tweeter nominal empedansı ve güvenli alt frekansı ölçülmeden yazılmaz. İlk hesap: `C = 1 / (2π × R_tweeter × f_safe)`; seçilen değer G2 test raporuna girer.
- Amfi kanal eşlemesi kabin içinde etiketlenir; firmware ve kablo aynı sürüm numarasını taşır.

> [!warning] Amfi susturması bir rezervasyondur, kurulmuş bir devre değil
> `GPIO21` → `SD` dalı, XH-A232 üzerinde erişilebilir bir `SD` pad'i bulunmasına bağlıdır ve bu §9'da hâlâ açık bir karardır. Şemalarda bu yüzden **kesikli** çizilir: pad bulunmazsa `R7` de bu dal da takılmaz.
>
> Pad bulunmazsa sonucu açıkça yazmak gerekir: **firmware kontrollü amfi susturması yoktur.** Geriye tek katman olarak DAC `XSMT` kalır — sinyali keser, ama TPA3110'un kendi açılış/kapanış geçişini kesmez, çünkü TPA3110 veri sayfası kapanış pop'u için shutdown'ın güçten önce verilmesini istiyor. `hk_audio` sıralaması o pad yokken de aynı komutları verir ve hiçbir şey olmaz; operatör bunu yalnız `TP21`'i ölçerek fark eder.
>
> Pad araması: kart enerjisizken TPA3110'un `SD` bacağı ile kart üzerindeki test pad/via/direnç ucu arasında süreklilik ara. Bulunan nokta ile `GND` arasındaki direnci de ölç: kart `SD`'yi kendi üzerinde besleme rayına çekiyorsa (yaygın), 10 kΩ pull-down o pull-up'a karşı yeterli olmayabilir — o zaman oran ölçülüp `R7` değeri yeniden hesaplanır veya kart üzerindeki pull-up kaldırılır. Sonuç ne olursa olsun §9 satırı ölçüm kaydıyla kapatılır.

### 3.5 S/PDIF tezgâh çıkışı — ürün kablolamasında yoktur

> [!warning] Bu üç parça kabine girmez
> Ürünün ses yolu [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]] ile I2S -> PCM5102A -> XH-A232'dir ve öyle kalır. Aşağıdaki devre yalnız geliştirme kartında, kullanıcının kendi harici DAC'ına bağlanmak içindir. BOM'a, kabin içi kablo demetine ve `TP` numaralandırmasına dahil değildir.

Neden var: geliştirme kartında PCM5102A yok (ADR-0012) ve sipariş edilen modüller gelmedi, yani I2S'in üç pini hiçbir şeye sürüyordu — zincirin duyulabilir hiçbir çıktısı olmuyordu. Vendor edilen alıcı `audio_output_spdif.c` taşıyor: BMC (biphase-mark) kodlamasını modern `i2s_std` sürücüsü üzerinden bit-bang edip tek veri pininden gerçek bir S/PDIF akışı üretiyor. Bit ve kelime saati dışarı hiç çıkmaz. Açılışta `SPDIF output ready rate=44100x2 dma=192x2` satırı görülür; buradaki `x2` BMC'nin iki katına çıkardığı bit hızıdır, örnekleme oranı değil. 2026-09-05'te bir FiiO Q15'in coax girişi 44.1 kHz'e temiz kilitlendi ve ses duyuldu; ham kayıt [[../06-testing/devkit-bring-up|geliştirme kartı bring-up notundadır]].

**Pin çakışması bu bölümün asıl uyarısıdır.** Çıkış, 3.1 tablosunda `I2S DATA OUT` olarak duran `GPIO6`'dır: ADR-0011 o pini ses verisine ayırmıştı ve `hk_airplay.c` seçimi derleme anında `hk_pins`e karşı doğruluyor. S/PDIF, I2S'in yanına eklenmez, **yerine geçer**. Bu çıkış etkinken `BCLK` ve `LRCLK` sürülmez; aynı anda bir PCM5102A beslenemez.

Arayüz üç pasif parçadır. Şartname 75 Ω yükte `0.5 V ±%20` tepe ister, yani `0.4-0.6 V`; GPIO ise 3.3 V sallar. `R1/R2` bölücüsü hem seviyeyi indirir hem kaynak empedansını 75 Ω'a yaklaştırır, `100 nF` DC'yi keser:

```text
GPIO6 --[R1]--+--[100 nF]-- coax merkez
              |
            [R2]
              |
GND ----------+------------ coax toprak
```

| R1 / R2 | 75 Ω yükte tepe | Kaynak Z | Not |
|---|---:|---:|---|
| 210 Ω / 110 Ω | 0.578 V | 72.2 Ω | Vendor edilen dosyanın kendi değeri; %1 seri gerekir |
| 220 Ω / 120 Ω | 0.572 V | 77.6 Ω | E24; kaynak empedansı 75 Ω'a en yakın olan |
| 220 Ω / 100 Ω | 0.538 V | 68.8 Ω | E24 |
| 270 Ω / 150 Ω | 0.516 V | 96.4 Ω | E24; seviye içeride, kaynak Z 75 Ω'dan uzak |
| 330 Ω / 100 Ω | 0.379 V | — | **Kullanma**; şartnamenin 0.4 V tabanının altında |

Son satır tabloya bilerek konuldu: `330/100` bu iş için yaygın olarak önerilir ve tepe değeri şartnamenin alt sınırının altında kalır. Bir DAC onunla kilitlenebilir de kilitlenmeyebilir de; kilitlenirse bu alıcının tolerans payıdır, devrenin doğruluğu değil. Direnç kutusunda `210/110` yoksa `220/120` alınır.

Akış 44.1 kHz / 16 bit'te sabittir. Bunun nedeni bu kart değildir ve firmware ayarıyla yükseltilemez; gerekçesi bring-up notundadır.

Bu çıkış hiçbir fiziksel kapıyı ilerletmez: dönüşümü kullanıcının kendi DAC'ı yapar ve bilinmeyen bir sürücüye enerji verilmez. Bölüm başındaki enerji verme yasağı ile G0-G2 sırası aynen geçerlidir.

## 4. Buton ve RGB LED

```mermaid
flowchart LR
    V33[ESP 3.3 V] --> RPU[R_PU 10 kΩ aday]
    RPU --> BTN_NODE[TP16: GPIO7 / BUTTON]
    BTN_NODE --> SWBTN[NO anlık buton]
    SWBTN --> GND[GND]
    BTN_NODE -.-> CDB[C_DB 100 nF opsiyonel]
    CDB -.-> GND

    GR[TP17: GPIO8 PWM] --> RR[R_R 680 Ω aday]
    RR --> LR[RGB LED R anot]
    GG[TP18: GPIO9 PWM] --> RG[R_G 330 Ω aday]
    RG --> LG[RGB LED G anot]
    GB[TP19: GPIO10 PWM] --> RB[R_B 330 Ω aday]
    RB --> LB[RGB LED B anot]
    LR --> CC[Ortak katot]
    LG --> CC
    LB --> CC
    CC --> GND
```

- ESP32 dahili pull-up kullanılabilir; harici `10 kΩ` gürültülü kabin ortamında başlangıç adayıdır.
- `100 nF` donanımsal debounce opsiyoneldir; firmware yine 50 ms debounce uygular.
- LED dirençleri gerçek LED ileri gerilimi ve hedef 2-5 mA akıma göre hesaplanır. Hazır RGB modülde direnç varsa harici dirençler yeniden değerlendirilir.
- LED ve buton kabloları Class-D hoparlör kablolarından ayrılır.

## 5. Kablo demeti ve konnektör planı

| Konnektör | Pinler | Öneri |
|---|---|---|
| J1 DC giriş | `DC_IN`, `POWER_GND` | 5,5 × 2,1 mm panel tipi barrel jak; merkez pozitif ölçü aletiyle doğrulanır; kontak akımı en az ölçülen tepe akım + %50 |
| J2 güç dağıtımı | `VIN`, `POWER_GND` | Amfi ve buck için yıldız dağıtım |
| J3 I2S | `GND`, `BCK`, `LCK`, `DIN` | GND-sinyal eşleşmeli, kısa; hoparlör çıkışından uzak |
| J4 DAC analog | `LOUT`, `AGND`, `ROUT` | Kısa ekranlı kablo; ekran tek uç/topoloji G1 gürültü ölçümünde test edilir |
| J5 woofer | `L+`, `L-` | Bükümlü çift, polarize |
| J6 tweeter | `R+ -> C_SAFE`, `R-` | Bükümlü çift, farklı anahtar/konnektör ile yanlış takma önlenir |
| J7 UI | `3V3`, `BUTTON`, `LED_R/G/B`, `GND` | Düşük akım, güç/speaker kablolarından ayrı |

## 6. Fiziksel yerleşim

```mermaid
flowchart LR
    subgraph NOISY[Kirli / yüksek akım bölgesi]
      JACK2[DC jak + D2 + C_A]
      BUCK2[MP1584]
      AMP2[XH-A232]
    end
    subgraph QUIET[Sessiz / sinyal bölgesi]
      ESP2[ESP32-S3]
      DAC2[PCM5102A]
      AIN2[Amfi analog giriş ucu]
    end
    subgraph RF[RF açıklığı]
      ANT[ESP PCB anteni]
    end
    JACK2 --> AMP2
    JACK2 --> BUCK2 --> ESP2 --> DAC2 --> AIN2 --> AMP2
```

- PCB anteninin önünde metal veya kablo demeti bırakılmaz.
- MP1584 indüktörü ve XH-A232 çıkış indüktörleri PCM5102A analog ucundan uzak tutulur.
- Analog ses kablosu hoparlör çıkış kablosuyla paralel uzun mesafe gitmez.
- Güç ve analog dönüşler gelişigüzel zincirlenmez; `STAR_GND` noktası G1 gürültü ölçümünde belirlenir.
- Adaptör kabinin dışındadır; kabine yalnız jak girer. Elektronik bölme akustik hacimden ayrılır ve kablolar pasif radyatör/woofer hareket alanına girmez.

## 7. Test noktaları ve osiloskop planı

### 7.1 Test noktası yerleşimi

PCB veya kablo dağıtım kartında test noktaları iğne probla erişilebilir, kısa devre oluşturmayacak aralıkta ve ipek baskıda `TPx` adıyla işaretlenir. Güç test noktalarında halka/klips tipi, hızlı dijital ve analog ses noktalarında küçük pad kullanılır.

| TP | Konum | Referans | Beklenen değer / dalga | Araç ve ilk kontrol |
|---|---|---|---|---|
| TP0 | DC jak `+` / `DC_IN` | TP2 | 19 V DC nominal; adaptör etiketi ± %5 | DMM; **ilk enerjilendirmeden önce** polarite: merkez pozitif |
| TP1 | `VIN` (`D2` sonrası) | TP2 | TP0 eksi `D2` ileri düşümü; köprüyse TP0 ile aynı | DMM yükte; `TP0-TP1` mV düşüm ve `D2` ısısı G1 kaydına |
| TP2 | DC jak `−` / `POWER_GND` | TP2 | 0 V yük referansı | DMM/scope ground referansı |
| TP3 | MP1584 5 V çıkışı | TPG | 5.10 V ayar; hedef 5.00-5.20 V | DMM + scope; yükte droop/ripple |
| TP4 | ESP32 kart 3V3 | TPG | Taslak hedef 3.15-3.45 V | DMM + scope; brownout gözlemi |
| TP5 | I2S BCLK | TPG | 0-3.3 V; 44.1 kHz için 1.4112/2.8224 MHz veya 48 kHz için 1.536/3.072 MHz | Scope 10x; gerçek slot genişliğine göre |
| TP6 | I2S LRCLK/WS | TPG | 0-3.3 V; 44.1 veya 48 kHz | Scope/logic analyzer |
| TP7 | I2S DATA | TPG | 0-3.3 V veri | Scope/logic analyzer |
| TP8 | PCM5102A LOUT | PCM AGND | Woofer analog yolu; 0 dBFS'te çip sınırı yaklaşık 2.1 Vrms | Scope AC coupling; önce -40 dBFS |
| TP9 | PCM5102A ROUT | PCM AGND | Tweeter analog yolu | Scope AC coupling; önce -40 dBFS |
| TP10 | XH-A232 L input | giriş GND | TP8'e yakın, kart giriş ağına bağlı | Scope AC coupling |
| TP11 | XH-A232 R input | giriş GND | TP9'a yakın | Scope AC coupling |
| TP12/TP13 | XH `L+` / `L-` | Birbirine diferansiyel | Woofer BTL PWM + diferansiyel audio | Diferansiyel prob veya CH1-CH2 |
| TP14/TP15 | XH `R+` / `R-` | Birbirine diferansiyel | Tweeter BTL PWM + diferansiyel audio | Diferansiyel prob veya CH1-CH2 |
| TP16 | Buton GPIO7 | TPG | Boşta yaklaşık 3.3 V, basılı 0 V | DMM/scope; debounce |
| TP17/18/19 | LED R/G/B anot sürüşü | TPG | 0-3.3 V PWM | Scope; PWM frekansı ve audio paraziti |
| TP20 | PCM5102A `XSMT` / GPIO13 | TPG | Reset anından firmware susturmayı bırakana kadar **LOW** (≤0,4 V); sonra 3,3 V | Scope single-shot, reset'ten tetikle; enerjisizken önce pad↔3V3 direnci (§3.3) |
| TP21 | XH-A232 `SD` / GPIO21 | TP2 | Pad varsa reset'ten itibaren **LOW**; pad yoksa satır uygulanmaz ve bu **kayda geçer** | Scope single-shot; önce pad erişimini ve kart pull-up'ını süreklilik/ohm ile doğrula (§3.4) |

`TP4` ESP32 geliştirme kartının gerçek `3V3` pininden alınır. Kart regülatörü ve USB güç topolojisi görülmeden 3V3 hattına harici enerji verilmez.

### 7.2 Test noktalarının şematik görünümü

```mermaid
flowchart LR
    P0[TP0 DC_IN] --> D2[D2 aday] --> P1[TP1 VIN]
    P1 --> AMP3[XH-A232]
    P1 --> BUCK3[MP1584]
    BUCK3 --> P3[TP3 5.10 V] --> ESP3[ESP32-S3]
    ESP3 --> P4[TP4 3V3]
    ESP3 --> P5[TP5 BCLK]
    ESP3 --> P6[TP6 LRCLK]
    ESP3 --> P7[TP7 DATA]
    P5 --> DAC3[PCM5102A]
    P6 --> DAC3
    P7 --> DAC3
    DAC3 --> P8[TP8 LOUT]
    DAC3 --> P9[TP9 ROUT]
    P8 --> P10[TP10 AMP L IN]
    P9 --> P11[TP11 AMP R IN]
    AMP3 --> PL[TP12 L+ / TP13 L-]
    AMP3 --> PR[TP14 R+ / TP15 R-]
    G[TP2 / TPG GND] --- BUCK3
    G --- ESP3
    G --- DAC3
    G --- AMP3
```

### 7.3 Osiloskop güvenlik kuralları

> [!danger] BTL çıkışa şase klipsi takma
> Masa tipi osiloskopların prob GND klipsleri çoğunlukla koruma toprağına ve birbirine bağlıdır. `TP12`, `TP13`, `TP14` veya `TP15` uçlarından hiçbirine GND klipsi takma. Bu uçlar “hoparlör eksi/GND” değildir; iki uç da Class-D yarım-köprü çıkışıdır.

```mermaid
flowchart LR
    AMP4[XH-A232 sol BTL kanal]
    AMP4 -->|L+| TP12[TP12]
    AMP4 -->|L-| TP13[TP13]
    TP12 --> P1[CH1 10x prob ucu]
    TP13 --> P2[CH2 10x prob ucu]
    P1 --> MATH[Scope MATH: CH1 - CH2]
    P2 --> MATH
    PG1[CH1 GND klipsi] --> TP2A[TP2 POWER_GND]
    PG2[CH2 GND klipsi] --> TP2A
    MATH --> AUDIO[Diferansiyel hoparlör dalga şekli]
```

Sağ kanal ölçümünde aynı bağlantı `TP14=CH1`, `TP15=CH2` olarak tekrarlanır. Diferansiyel prob varsa prob doğrudan iki BTL ucu arasına bağlanır ve masa tipi tek uçlu prob yöntemine tercih edilir.

BTL ölçümü için tercih sırası:

1. Yeterli common-mode ve diferansiyel gerilim sınıfına sahip **diferansiyel probu** `L+ ↔ L-` veya `R+ ↔ R-` arasına bağla.
2. Diferansiyel prob yoksa iki aynı `10x` prob kullan: her iki probun GND klipsi yalnız `TP2 POWER_GND` noktasına; CH1 ucu `L+`, CH2 ucu `L-`; osiloskop matematiği `CH1 - CH2`. Sağ kanal için aynı yöntem.
3. Tek probu doğrudan hoparlör uçları arasına bağlama; probun şase klipsi çıkışa değmemeli.
4. Prob/osiloskop girişinin `VIN`, PWM overshoot ve common-mode gerilim sınırlarını karşıladığını doğrula.
5. Amfi testi adaptör yerine önce izolasyonu/toprak ilişkisi bilinen akım sınırlı laboratuvar kaynağıyla yapılır.
6. DC adaptör bağlıyken osiloskop kullanmadan önce adaptör çıkışının PE/toprak ve DUT ile izolasyon ilişkisini ölç; belirsizse adaptörle scope bağlama.

TP1/TP3 besleme ripple ölçümü:

- `10x` prob, ground-spring veya çok kısa GND bağlantısı kullan.
- `20 MHz bandwidth limit` aç; önce DC coupling ile seviye, sonra AC coupling ile ripple gözle.
- TP3 için ilk taslak hedef: normal yükte `≤50 mVpp`, Wi-Fi akım sıçramasında `5 V` hattı `4.75 V` altına düşmemeli. Bunlar modül datasheet garantisi değil, G1 proje kabul hedefidir.
- TP4 için brownout/reset oluşturan çökme olmamalı; minimum değer kesin ESP32 kart ve brownout ayarıyla test raporunda kilitlenir.
- TP1 için adaptörle çalışırken bas tepesinde besleme çöküşü kaydedilir; amfi ve buck birlikte çektiğinde adaptörün akım sınırına girip girmediği bu kayıttan okunur.

### 7.4 Ses dalga şekli testi

Başlangıç test sinyali: `1 kHz`, önce `-40 dBFS`, ardından `-20 dBFS`. Tweeter bağlı değilken her iki DSP yolunun seviyesi TP8/TP9'da doğrulanır.

| Kademe | Yük | Ölçüm | Geçiş şartı |
|---|---|---|---|
| A | XH girişsiz | TP12-TP13 ve TP14-TP15 | Anormal DC/fault/ısınma yok |
| B | 8 Ω / en az 50 W non-inductive dummy-load | Diferansiyel 1 kHz çıkış | Önce 1.0 Vrms; temiz ve kararlı |
| C | 8 Ω dummy-load | Diferansiyel 2.83 Vrms | Yaklaşık 1 W; clipping yok |
| D | 8 Ω dummy-load | Kademeli Vrms + sıcaklık | Clipping ve termal sınır kaydedilir; sürücü yok |
| E | Woofer, düşük seviye | Diferansiyel + akustik | G2 woofer koruması |
| F | Tweeter + C_SAFE, çok düşük seviye | TP9 ve diferansiyel R çıkışı | HPF/limiter ölçümle doğrulanmış |

Dummy-load gücü `P = V_RMS² / R` ile hesaplanır. Osiloskop PWM'li ham BTL çıkışta yanlış RMS gösterebilir; diferansiyel prob, bant sınırı/filtre ve mümkünse true-RMS ölçüm veya audio analyzer ile çapraz kontrol edilir. TPA3110D2 tipik anahtarlama frekansı yaklaşık `310 kHz` olup veri sayfası aralığı `250-350 kHz`'dir; bu bileşen audio sinyali sanılmaz.

### 7.5 Güç açma/kapatma kaydı

Scope single-shot kaydı için kanallar:

- CH1: TP1 `VIN`.
- CH2: TP3 `5.10 V`.
- CH3: TP4 `3V3`.
- CH4: TP8 veya TP9 DAC analog çıkışı.

Susturma hatları için **ayrı ve zorunlu** bir single-shot kayıt alınır: CH1 `TP1`, CH2 `TP20` (`XSMT`), CH3 `TP21` (`SD`, pad varsa), CH4 `TP8`/`TP9`. Tetik reset kenarındadır ve kaydın kapsaması gereken şey açılış penceresinin tamamıdır — ROM, bootloader ve uygulama başlangıcı. Beklenen: her iki mute hattı bu pencere boyunca LOW; DAC analog çıkışında adım yok; susturma yalnız firmware bıraktığında kalkıyor.

Kapanış V1'de adaptörün çekilmesidir; bu yüzden kapanış kaydı da adaptör çekilerek alınır, lab kaynağının çıkış düğmesiyle değil. Firmware `VIN`'i ölçmez ve çöküşü önceden göremez. Sıralamayı veren şey iki giriş penceresinin farkıdır: TPA3110 8 V'un altında kendi düşük gerilim kilidiyle susar, MP1584 ise 4,5 V girişe kadar 5 V vermeye devam eder; yani `VIN` çökerken amfi, DAC ve ESP hâlâ ayaktayken kapanır. Bu kayıt, o sıralamanın gerçekten böyle gerçekleşip gerçekleşmediğini ve pop olup olmadığını gösterir; besleme kaybında pop'suz kapanış G8'in kabul ölçütüdür.

Bu kayıt olmadan `G1` için "pop yok" denemez: pop'un olmaması, susturmanın çalıştığını değil yalnız o denemede duyulmadığını gösterir. Açılış/kapanışta DAC pop, amfi pop, ESP brownout ve rail sıralaması ayrıca kaydedilir. TPA3110D2 için en iyi power-off pop davranışı güç kesilmeden önce shutdown uygulanmasıdır; XH-A232 üzerinde `SD` erişimi yoksa bu açık donanım kararı olarak kalır ve `TP21` satırı "uygulanmadı" olarak kapatılır.

## 8. Kademeli kurulum ve ölçüm planı

| Adım | Bağlanacaklar | Enerji kaynağı | Geçiş koşulu |
|---|---|---|---|
| S0 | Yalnız sürücü ölçümü | Enerjisiz | G0 verileri kayıtlı |
| S1 | XH-A232 + dummy-load | Akım sınırlı lab kaynağı 8-19 V | G1 güç, DC offset, clipping, termal |
| S2 | ESP32 + buck | Akım sınırlı lab kaynağı | 5 V ripple/brownout güvenli |
| S3 | ESP32 + PCM5102A + amfi + dummy-load | Lab kaynağı | I2S, kanal eşleme, pop ve gürültü |
| S4 | Woofer, düşük seviye | Lab kaynağı | Woofer HPF/limiter güvenli |
| S5 | Tweeter + `C_SAFE`, çok düşük seviye | Lab kaynağı | G2 crossover/limiter doğrulandı |
| S6 | Tam tek-hoparlör prototipi | 19 V adaptör | G1 adaptörle tekrar: jak polaritesi, besleme çöküşü, pop, gürültü; G8 kapalı kabin termal |
| S7 | Dört tekrar | Dört adaptör | G7/G8 senkron ve soak |

Her adım için [[../templates/test-report|test raporu]] oluşturulur. Fiziksel ölçüm kaydı olmadan gate `PASS` yapılmaz. Dört üniteye çoğaltma G0-G2 geçmeden yapılmaz.

## 9. Açık kararlar

- [ ] Nova woofer ve tweeter DC direnci/empedans eğrisi.
- [ ] `C_SAFE` tipi ve değeri.
- [ ] Kesin ESP32-S3 kartı ve aday GPIO tablosunun boot/I2S doğrulaması.
- [ ] XH-A232 kartların dört fiziksel revizyonunun aynı olup olmadığı.
- [ ] XH-A232 kartında erişilebilir `SD/MUTE` noktası bulunup bulunmadığı; bulunursa pop önleme devresi ve kart üzerindeki pull-up'a karşı `R7` değerinin doğrulanması. Bulunmazsa firmware kontrollü amfi susturmasının olmadığı yazılı olarak kapatılır (§3.4).
- [ ] PCM5102A modülünde `XSMT` pad'inin 3,3 V'a sert bağlı olup olmadığı; köprü varsa kesilmesi (§3.3). `R6` takılıp `TP20` açılışta LOW ölçülene kadar susturma katmanı doğrulanmamıştır.
- [ ] 19 V adaptör: marka/model, akım sınıfı (G1 tepe akımından), jak polaritesi, çıkışın PE/izolasyon ilişkisi.
- [ ] `D2` ters polarite koruması: Schottky, ideal-diyot modülü ya da 0 Ω köprü; G1'de ölçülen düşüm ve ısıyla karar.
- [ ] Kablo kesiti ve konnektör akım sınıfı.
- [ ] USB ile harici 5 V arasında jumper, Schottky OR veya load-switch seçimi.

## 10. Teknik kaynaklar

- [TI PCM5102A veri sayfası](https://www.ti.com/lit/ds/symlink/pcm5102a.pdf): 3-wire I2S/BCK PLL, kontrol pinleri ve analog çıkış.
- [TI TPA3110D2 veri sayfası](https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf): BTL çıkış, 8-26 V besleme, shutdown ve decoupling/layout.
- [XH-A232 modül referansı](https://www.taydaelectronics.com/tpa3110-xh-a232-digital-stereo-audio-power-amplifier-board.html): kart sınıfı, 8-26 V ve 4-8 Ω satıcı bilgisi; güç etiketi ölçüm yerine geçmez.
- [Seçilen PCM5102A satın alma kaynağı](https://www.aletler.com.tr/urun/pcm5102a-dac-modul): fiziksel modül revizyonu teslim alınınca karşılaştırılır.
- [Monolithic Power MP1584 veri sayfası](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584/document_id/204/): 4,5-28 V giriş penceresi; modül kalitesi ayrıca ölçülür.

## 11. İlgili belgeler

- [[audio-signal-chain|Ses sinyal zinciri]]
- [[driver-measurements|Sürücü ölçüm planı]]
- [[board-and-pin-selection|Kart ve pin seçimi]]
- [[grounding-emi-thermal|Topraklama, EMI ve termal]]
- [[../power-plan|Güç planı]]
- [[../controls-and-provisioning-plan|Kontroller ve provisioning]]
- [[../06-testing/test-strategy|Test kapıları]]
- [[../06-testing/devkit-bring-up|Geliştirme kartı bring-up kaydı]]
- [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002 — Bi-amp sinyal zinciri]]
- [[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010 — Kanonik N16R8 kartı]]
- [[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011 — Ses tarafı GPIO rezervasyonu ve susturma pull-down'ları]]
- [[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020 — 19 V DC adaptörle besleme]]
