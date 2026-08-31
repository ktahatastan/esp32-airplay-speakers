# Harman Kardom belge şeması

Belgelerde kullanılan tek sayfalık, okunabilir devre paftasını üretir. Elektriksel kaynak burası değildir; netlist ve ERC için `hardware/kicad/` esastır.

## Üretme

```bash
python3 hardware/diagrams/generate_schematic_svg.py
```

Çıktı: `docs/02-hardware/assets/harman-kardom-schematic.svg`. Bağımlılık yoktur; yalnız standart kütüphane kullanılır.

## Neden script

Elle yazılan SVG zamanla üst üste binen etiketler, kırpılmış semboller ve blokların içinden geçen teller biriktirir. Burada her koordinat blok ve sembol geometrisinden **hesaplanır**:

- `Block` kendi pin koordinatlarını üretir; teller o koordinatlara bağlanır.
- Paralel sinyal demetleri, kanal ataması kaydırma yönünün tersine yapıldığı için birbirini kesmez.
- Panel gövde satırları tek bir satır aralığından türetilir; başlıkla çakışamaz.
- Simge etiketleri sembolün kendi ölçüsünden konumlanır.

Yeni bir blok eklerken `schematic_lib.Sheet.block()` kullanın ve teli `block.pin("PIN_ADI")` ile bağlayın. Sabit koordinat yazmayın.

## İçerik

Pafta; USB-C PD şarj zincirini, 4S paketi ve BMS'i, sigorta ve ana anahtarı, 5 V lojik beslemesini, ESP32-S3 N16R8'i, PCM5102A'yı, XH-A232 BTL bi-amp'ı, woofer ve `C_SAFE` korumalı tweeter'ı, kullanıcı arayüzünü, `TP0…TP27` test noktalarını, güvenlik kurallarını ve zorunlu kapıları tek sayfada gösterir.

Kanonik değerler ve gerekçeler için [devre ve bağlantı planına](../../docs/02-hardware/circuit-and-wiring-plan.md) bakın.
