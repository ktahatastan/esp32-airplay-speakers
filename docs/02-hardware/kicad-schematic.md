---
status: candidate
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-30
tags: [hardware, kicad, schematic, generator]
---

# KiCad şeması ve üretim scripti

Tek hoparlörün A3 modül bağlantı paftası Python ile tekrar üretilebilir. Script, eski `.sch` dönüşümüne ihtiyaç duymadan yerel KiCad `.kicad_sch` dosyası oluşturur.

## Dosyalar

- Script: `hardware/kicad/generate_harman_kardom.py`
- Bağımlılık kilidi: `hardware/kicad/requirements.txt`
- KiCad projesi: `hardware/kicad/generated/harman-kardom.kicad_pro`
- Üretilen şema: `hardware/kicad/generated/harman-kardom.kicad_sch`

## Çalıştırma

```powershell
python -m pip install -r hardware/kicad/requirements.txt
python hardware/kicad/generate_harman_kardom.py --validate
```

`--validate`, KiCad CLI ile ERC raporu ve PDF önizlemesi üretir. Bu geçici çıktılar Git'e girmez. 2026-08-30 doğrulamasında KiCad 10.0.6 şemayı açıp PDF'e çizdi; ERC sonucu `0 error / 11 warning` oldu. On bir uyarı gelecekte kullanılacak tekil `TBD/NC` netlerdir.

## Pafta kapsamı

- USB-C PD tetikleyici ve XL4015 16,80 V CC/CV şarj zinciri
- 4S1P hücreler, balanslı BMS, sigorta ve mekanik güç anahtarı
- MP1584 5,10 V lojik beslemesi ve USB geri-besleme ayırma jumperı
- ESP32-S3, fonksiyon butonu ve RGB durum LED'i
- PCM5102A, XH-A232, woofer ve seri `C_SAFE` korumalı tweeter
- TP0-TP25 güç, I2S, analog, BTL, şarj ve kullanıcı arayüzü ölçüm noktaları

> [!warning]
> Konektör pin sıraları mantıksaldır; satın alınan modüllerin fiziksel pin sırası olarak kullanılamaz. BMS baskı yazısı/veri sayfası, güç anahtarının 16,8 V DC kesme kapasitesi ve modül revizyonları ayrıca doğrulanır.

> [!danger]
> `AMP_L_MINUS` ve `AMP_R_MINUS` BTL anahtarlama çıkışıdır; GND değildir. Osiloskop toprak klipsi hiçbir BTL ucuna bağlanmaz. G0-G4 ölçüm kapıları kapanmadan gerçek sürücülere enerji verilmez.

Ana elektriksel gerekçeler, değerler ve bağlantı tabloları için [[circuit-and-wiring-plan|devre ve bağlantı planı]] esas kaynaktır.
