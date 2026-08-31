---
status: candidate
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [hardware, kicad, schematic, generator]
---

# KiCad şeması ve üretim scripti

Tek hoparlörün A3 modül bağlantı paftası Python ile tekrar üretilebilir. Script, yerel KiCad `.kicad_sch` dosyasını doğrudan oluşturur.

Bu pafta **elektriksel kaynaktır**: netlist, ERC ve ileride PCB buradan türer. Belgelerde kullanılan okunabilir tek sayfalık görsel ayrı bir çıktıdır ve [[circuit-and-wiring-plan#Devre şeması|devre planında]] gösterilir.

## Dosyalar

| Dosya | Rol |
|---|---|
| `hardware/kicad/generate_harman_kardom.py` | Üretici script |
| `hardware/kicad/requirements.txt` | Bağımlılık kilidi |
| `hardware/kicad/generated/harman-kardom.kicad_pro` | KiCad projesi |
| `hardware/kicad/generated/harman-kardom.kicad_sch` | Üretilen şema |

## Çalıştırma

```bash
python3 -m pip install -r hardware/kicad/requirements.txt
python3 hardware/kicad/generate_harman_kardom.py --validate
```

Script her çalıştırmada bir yapısal self-check uygular ve sorun bulursa dosya yazmadan durur:

- Her tel ucu gerçek bir pinin veya köşe noktasının üstünde mi?
- `TP0-TP27` boşluksuz ve tekrarsız mı?
- Referans designator tekrarı var mı?

`--validate` ek olarak KiCad CLI ile ERC raporu ve PDF önizlemesi üretir. Bu geçici çıktılar Git'e girmez.

## Pafta kapsamı

- USB-C PD tetikleyici ve XL4015 16,80 V CC/CV şarj zinciri ([[../07-decisions/ADR-0009-usb-c-pd-charge-chain|ADR-0009]])
- 4S1P hücreler, balanslı BMS, sigorta ve KM103 / DC-132A mekanik güç anahtarı; ayrı 12 V dahili LED dönüşü ve koşullu `R_SW_LED`
- MP1584 5,10 V lojik beslemesi ve USB geri-besleme ayırma jumperı
- ESP32-S3 **N16R8** ([[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]]), fonksiyon butonu ve RGB durum LED'i
- PCM5102A, XH-A232, woofer ve seri `C_SAFE` korumalı tweeter
- `TP0-TP27` güç, I2S, analog, BTL, şarj, NTC ve kullanıcı arayüzü ölçüm noktaları

Test noktası numaralandırması scriptteki tek bir tablodan üretilir; bu tablo [[circuit-and-wiring-plan#7.1 Test noktası yerleşimi|devre planındaki tabloyla]] aynı numaraları kullanır.

## Çizim kuralları

Her yerleşim `2,54 mm` ızgaradadır. Tel uçları elle yazılmaz, pin konumundan çözülür. Bitişik parçalar gerçek telle bağlanır; sayfayı boydan boya geçmesi gereken raylar için net etiketi ve power sembolü kullanılır.

> [!warning]
> Konektör ve KM103 pin sıraları mantıksaldır; satın alınan parçaların fiziksel pin sırası olarak kullanılamaz. BMS baskı yazısı/veri sayfası, güç anahtarının 16,8 V DC kesme kapasitesi, dahili LED akımı ve modül revizyonları ayrıca doğrulanır. `D2` ve `R2` ölçüm yapılana kadar `DNP` kalır.

> [!danger]
> `AMP_L_MINUS` ve `AMP_R_MINUS` BTL anahtarlama çıkışıdır; GND değildir. Osiloskop toprak klipsi hiçbir BTL ucuna bağlanmaz. G0-G4 ölçüm kapıları kapanmadan gerçek sürücülere enerji verilmez.

Ana elektriksel gerekçeler, değerler ve bağlantı tabloları için [[circuit-and-wiring-plan|devre ve bağlantı planı]] esas kaynaktır.
