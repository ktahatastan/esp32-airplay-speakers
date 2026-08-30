---
status: proposed
owner: hardware-engineer
updated: 2026-08-30
tags: [audio, hardware]
---

# Ses sinyal zinciri

Planlanan bi-amp eşleme:

- PCM5102A sol analog kanal -> amfi kanal A -> woofer.
- PCM5102A sağ analog kanal -> amfi kanal B -> tweeter.
- ESP32 DSP aynı mono programı iki yola ayırır; her yola ayrı filtre, gain, delay ve limiter uygular.

Bu eşleme stereo kutu değildir. Tweeter yolu güvenli HPF ve mute sıralaması doğrulanmadan sürücüye bağlanmaz. XH-A232 üzerindeki gerçek TPA3110 topolojisi, kazanç ve çıkış filtresi kart bazında incelenir.

PCM5102A çipi 16/24/32-bit I2S veri kabul eder ve 384 kHz'e kadar örnekleme destekler. Gerçek proje formatı AirPlay stack ve DSP yüküne göre seçilir. Modülün 5 V besleme ilanı, çipin elektriksel sınırlarıyla karıştırılmaz.
