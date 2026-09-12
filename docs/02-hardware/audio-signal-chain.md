---
status: proposed
owner: hardware-engineer
updated: 2026-08-30
tags: [audio, hardware]
---

# Ses sinyal zinciri

Tek kabin, tek DAC, dört özdeş amfi ([[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]], [[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]). DAC'ın iki kanalı stereo değil, iki frekans bandıdır:

- PCM5102A `LOUT` (woofer bandı) -> dört XH-A232'nin `L` girişine paralel -> her amfinin `L` BTL çıkışı bir woofer sürer: dört woofer.
- PCM5102A `ROUT` (tweeter bandı) -> dört XH-A232'nin `R` girişine paralel -> her amfinin `R` BTL çıkışı, o tweeter'ın kendi seri `C_SAFE`'i üzerinden bir tweeter sürer: dört tweeter.
- ESP32 DSP'si tek mono programı (AirPlay L+R toplamı) iki banda ayırır: bant başına EQ, subsonic yüksek geçiren, LR4 crossover ve her dalda kendi limiter'ı. Sekiz sürücünün hepsi aynı programın kendi bandını alır.

Hat seviyesinde dört yola dağıtım sorun değildir: XH-A232 girişi 10 kΩ sınıfıdır, dördü paralel yaklaşık 2,5 kΩ eder ve PCM5102A'nın hat çıkışı bunu rahatça sürer; bu bir aritmetiktir, `G1` dört giriş bağlıyken DAC çıkış seviyesini kaydeder. Dört amfi özdeştir ve kazançları `G1`'de eşleştirilir; aynı kabinde kazanç farkı duyulur.

Bu eşleme stereo kutu değildir: tek kabin, tek program; stereo kapsam dışıdır (ADR-0021). Tweeter yolu güvenli HPF ve mute sıralaması doğrulanmadan sürücüye bağlanmaz. XH-A232 üzerindeki gerçek TPA3110 topolojisi, kazanç ve çıkış filtresi kart bazında incelenir.

DSP zinciri çalışıyor ve host'ta test edilmiş; sayıları ölçüm bekliyor. Crossover köşesi tweeter `Fs` ölçülene kadar muhafazakâr bir tahmindir, subsonic köşe pasif radyatör akordu ölçülene kadar yer tutucudur; zincir bitmiş sayılmaz ([[../04-acoustics/measurement-and-dsp-plan|ölçüm ve DSP planı]]).

PCM5102A çipi 16/24/32-bit I2S veri kabul eder ve 384 kHz'e kadar örnekleme destekler. Gerçek proje formatı AirPlay stack ve DSP yüküne göre seçilir. Modülün 5 V besleme ilanı, çipin elektriksel sınırlarıyla karıştırılmaz.
