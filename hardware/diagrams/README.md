# Merzarkabul belge şeması

Belgelerde kullanılan tek sayfalık, okunabilir devre paftasını üretir. Elektriksel kaynak burası değildir; netlist ve ERC için `hardware/kicad/` esastır.

## Üretme

```bash
python3 hardware/diagrams/generate_schematic_svg.py
```

Çıktı: `docs/02-hardware/assets/merzarkabul-schematic.svg`. Bağımlılık yoktur; yalnız standart kütüphane kullanılır.

## Neden script

Elle yazılan SVG zamanla üst üste binen etiketler, kırpılmış semboller ve blokların içinden geçen teller biriktirir. Burada her koordinat blok ve sembol geometrisinden **hesaplanır**:

- `Block` kendi pin koordinatlarını üretir; teller o koordinatlara bağlanır.
- Paralel sinyal demetleri, kanal ataması kaydırma yönünün tersine yapıldığı için birbirini kesmez.
- Panel gövde satırları tek bir satır aralığından türetilir; başlıkla çakışamaz.
- Simge etiketleri sembolün kendi ölçüsünden konumlanır.

Yeni bir blok eklerken `schematic_lib.Sheet.block()` kullanın ve teli `block.pin("PIN_ADI")` ile bağlayın. Sabit koordinat yazmayın.

## İçerik

Pafta; DC giriş jakını ve 24 V / 2,9 A adaptörü, seri ters polarite adayını ve bulk kondansatörü, iki 5 V buck'ı (A: ESP32-S3, B: PCM5102A), ESP32-S3 N16R8'i, PCM5102A'yı, `LOUT`/`ROUT`'un dört XH-A232 girişine dağıtımını, dört woofer'ı ve dört `C_SAFE` korumalı tweeter'ı, dört amfiye paralel susturma bus'ını, kullanıcı arayüzünü, `TP0…TP34` test noktalarını, güvenlik kurallarını ve zorunlu kapıları tek sayfada gösterir. Kesişen teller yarım daireyle atlar: bağlantı yalnız içi dolu noktadadır.

Kanonik değerler ve gerekçeler için [devre ve bağlantı planına](../../docs/02-hardware/circuit-and-wiring-plan.md) bakın.
