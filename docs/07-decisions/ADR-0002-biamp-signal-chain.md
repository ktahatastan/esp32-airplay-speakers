---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, firmware-engineer, acoustics-engineer]
updated: 2026-09-12
tags: [adr, audio, hardware]
---

# ADR-0002: Bi-amp sinyal zinciri — tek DAC, dört özdeş amfi

## Bağlam

Ürün tek bir kabindir: dört Nova woofer ve dört Nova tweeter, tek bir mono program ([[ADR-0021-single-cabinet|ADR-0021]]). Sinyal kaynağı bir ESP32-S3 ve bir PCM5102A'dır; DAC'ın iki analog kanalı vardır. Sekiz sürücünün her biri kendi korumalı amfi kanalını ister (PRD-003), yani sekiz BTL kanal gerekir ve elde iki kanallı bir DAC vardır.

Sorun, iki DAC kanalından sekiz korumalı amfi kanalı çıkarmak — DSP'yi bir kez çalıştırarak.

## Karar

DAC'ın iki kanalı stereo değil, **iki frekans bandıdır**. DSP zinciri (mono L+R toplamı, kullanıcı EQ, subsonic yüksek geçiren, LR4 crossover, dal kazançları, iki dalı birden kısan besleme bütçesi katı, dal başına birer tepe limiter) tek programı iki banda böler:

- PCM5102A **`LOUT` = woofer bandı**, **`ROUT` = tweeter bandı**. Bu eşleme firmware'in `Left = WOOFER, right = TWEETER` kuralıdır (`hk_dsp`, `hk_airplay_output_i2s`) ve stereo bir eşleme değildir.
- `LOUT`, **dört özdeş XH-A232**'nin (TPA3110D2, 2 × BTL) L girişine paralel dağıtılır; `ROUT` dördünün R girişine.
- Her amfinin L çıkışı **bir woofer**, R çıkışı **bir tweeter** sürer; tweeter, kendi seri `C_SAFE`'i üzerinden bağlanır. Dört amfi × iki kanal = sekiz BTL kanal, hepsi aynı programın kendi bandı.
- Zincirdeki tek susturma DAC'tadır: `DAC_XSMT` (GPIO13) ve harici pull-down'ı. XH-A232'de susturma girişi yoktur; dört amfi `VIN` geldiği andan itibaren canlıdır ve DAC'ın verdiği her şeyi sürücüye taşır ([[ADR-0011-audio-side-gpio-reservation|ADR-0011]]).
- BTL çıkışların hoparlör eksileri şasi toprağı değildir.

```text
                                  +-> XH-A232 #1  L -> woofer 1     R -> C_SAFE -> tweeter 1
ESP32-S3 -> I2S -> PCM5102A LOUT -+-> XH-A232 #2  L -> woofer 2     R -> C_SAFE -> tweeter 2
                            ROUT -+-> XH-A232 #3  L -> woofer 3     R -> C_SAFE -> tweeter 3
                                  `-> XH-A232 #4  L -> woofer 4     R -> C_SAFE -> tweeter 4
   (LOUT dört L girişine, ROUT dört R girişine paralel)
```

### Paralel giriş yükü

XH-A232'nin hat girişi 10 kΩ sınıfındadır ve dört girişin paralel yükü yaklaşık **2,5 kΩ**'dur — **yalnız amfi 36 dB'ye köprülenmişse.** TPA3110D2'nin giriş empedansı kazanç seçimine bağlıdır (SLOS528F Tablo 2: 20 / 26 / 32 / 36 dB için tipik 60 / 30 / 15 / 9 kΩ, parça başına ±%20, mutlak asgari 7,2 kΩ); kartların köprüsü okunmadı (tezgâh maddesi `C3`, [[ADR-0022-dsp-product-output-backend|ADR-0022]]), yani dört girişin paralel yükü okunana kadar tipik 2,25 kΩ (36 dB) ile 15 kΩ (20 dB) arasında, uçlarda 1,8–18 kΩ'dur. PCM5102A'nın hat çıkışı (asgari yük 1 kΩ) bu aralığın tamamını sürer. Bu bir aritmetiktir, ölçüm değildir: `G1`, DAC çıkışındaki seviye ve bozulmayı dört giriş bağlıyken kaydeder, `C3` kazancı okur ve profil onu `amp_gain_db` alanında taşır.

### DSP zincirinin durumu

Zincir çalışıyor ve host testleriyle doğrulanmış; sayıları henüz ölçülmüş değil. Zincir **ürünün çıkış arka ucudur** ([[ADR-0022-dsp-product-output-backend|ADR-0022]]): ürün ve sürüm yapıları onunla derlenir ve profil olmadan hiçbir şey çalmaz. Subsonic filtre artık dördüncü derece Butterworth'tür (pasif radyatörlü kabinin istediği eğim); profil şeması 2 dal başına limiter zamanlaması, dal gecikmesi ve tweeter polaritesi, adaptör bütçesini iki dalın toplam ortalaması olarak taşıyan bir besleme bütçesi alanı ve amfi kazancının okunduğu yer olan `amp_gain_db`'yi taşır. Crossover köşesi tweeter `Fs` ölçülene kadar muhafazakâr bir tahmindir; subsonic köşe pasif radyatör akordu ölçülene kadar yer tutucudur; tavanlar, zamanlamalar, gecikme, polarite ve bütçe `G0`, `G1` ve `G2`'nin sayılarını bekler. Biçim üründe, sayılar tezgâhta; zincir bitmiş sayılmaz ([[../04-acoustics/measurement-and-dsp-plan|ölçüm ve DSP planı]]).

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| Stereo: DAC L/R → iki amfi, sol ve sağ sürücü grupları | Tek kutuda tek program; stereo kapsam dışı (ADR-0021). Stereo yol, sekiz sürücüye tek DSP geçişinden bant vermeyi de imkânsız kılar. |
| Sürücü tipi başına tek amfi kanalı, dört sürücü paralel | Dört adet 4 Ω sürücü paralel 1 Ω; TPA3110'un asgari yükünün çok altında. Sürücü başına koruma da kalmaz. |
| Dört DAC / dört ESP32, her amfiye kendi kaynağı | Kazanç yok: aynı program dört kez çözülür, dört kurulum, dört saat, dört OTA hedefi. |
| Pasif crossover, DAC'tan tam bant | Tweeter koruması yazılımda kalmalı (HPF + limiter + susturma sırası); pasif bir devre ölçülmemiş sürücülere göre tasarlanamaz. `C_SAFE` yalnız son sigortadır, crossover değil. |

## Sonuçlar ve açık koşullar

- Kablolama planı, SVG paftası ve KiCad üreteci tek DAC'tan beslenen **dört amfi** çizer; BOM dört `C_SAFE` ve dört amfi taşır.
- Dört amfi kartı aynı revizyon olmalıdır. Farklı revizyonlar farklı giriş yükü ve farklı kazanç demektir; aynı kabinde ikisi de duyulur.
- `G1` satırları: dört amfinin her biri dummy-load üzerinde ayrı ayrı; dört girişin paralel yükü DAC çıkışında; profilin besleme bütçesi (`supply_budget_sq` / `supply_window_ms`, [[ADR-0022-dsp-product-output-backend|ADR-0022]]) 2,9 A adaptör bütçesinden dört amfi birlikte sürülürken, 4 Ω sınıfı dummy-load üzerinde türetilir ve `VIN` çökmesiyle doğrulanır; tepe tavanlar onun altında kalır ([[ADR-0020-dc-adapter-power|ADR-0020]]).
- `G2` satırları: HPF, crossover ve limiter önce **tek woofer ve tek tweeter** ile, tek amfide; diğer üç amfi sürücülere ancak bundan sonra bağlanır.
- Bu ADR topolojiyi kilitler; kabul `G1`-`G2` ölçümlerine **koşulludur**. Crossover köşesi, limiter eşikleri ve `C_SAFE` değeri bu ADR'nin değil, `G0`/`G2` ölçümlerinin çıktısıdır.
