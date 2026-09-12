---
status: candidate
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [hardware, kicad, schematic, generator]
---

# KiCad şeması ve üretim scripti

Tek kabinin (bir ESP32-S3, bir PCM5102A, iki buck, dört XH-A232, sekiz sürücü) A3 modül bağlantı paftası Python ile tekrar üretilebilir. Script, yerel KiCad `.kicad_sch` dosyasını doğrudan oluşturur.

Bu pafta **elektriksel kaynaktır**: netlist, ERC ve ileride PCB buradan türer. Belgelerde kullanılan okunabilir tek sayfalık görsel ayrı bir çıktıdır ve [[circuit-and-wiring-plan#Devre şeması|devre planında]] gösterilir.

## Dosyalar

| Dosya | Rol |
|---|---|
| `hardware/kicad/generate_merzarkabul.py` | Üretici script; Git'teki kaynak |
| `hardware/kicad/requirements.txt` | Bağımlılık kilidi |
| `hardware/kicad/generated/merzarkabul.kicad_pro` | KiCad projesi; üretilir, **şu an depoda yok** |
| `hardware/kicad/generated/merzarkabul.kicad_sch` | Üretilen şema; **şu an depoda yok** |

Üretilen iki dosya bu makinede oluşturulamıyor: KiCad sembol kütüphaneleri kurulu olmadığı için `kicad-sch-api` sembol yerleştirmeyi reddediyor. Bayat bir çizimi depoda tutmak hiç tutmamaktan kötüdür — bayat olduğu anlaşılamayan bir paftadan birisi lehim yapar. Pafta KiCad kurulu bir makinede yeniden üretilir ve `scripts/check_generated_kicad.py --record` ile üreteç özeti yanına yazılır; komutlar `hardware/kicad/README.md` içindedir. Pafta yokken aynı script CI'da bir notla geçer, pafta varken üreteçle uyumunu denetler.

## Çalıştırma

```bash
python3 -m pip install -r hardware/kicad/requirements.txt
python3 hardware/kicad/generate_merzarkabul.py --validate
```

Script her çalıştırmada bir yapısal self-check uygular ve sorun bulursa dosya yazmadan durur:

- Her tel ucu gerçek bir pinin veya köşe noktasının üstünde mi?
- `TP0-TP33` boşluksuz ve tekrarsız mı?
- Tek bağlantılı net var mı? (`EXPECTED_OPEN_NETS` boştur; tek pinli her net hatadır.)
- Referans designator tekrarı var mı?

`--validate` ek olarak KiCad CLI ile ERC raporu ve PDF önizlemesi üretir. Bu geçici çıktılar Git'e girmez.

## Pafta kapsamı

- 5,5 × 2,1 mm DC giriş jakı, 24 V / 2,9 A adaptör (yüksüz çıkış < 25,5 V ölçülür), seri ters polarite adayı `D2` ve 35 V sınıfı `C_A` bulk kondansatör ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]])
- İki MP1584: `U3` buck A (ESP32-S3, `JP1` USB geri-besleme ayırma jumperıyla) ve `U4` buck B (PCM5102A, jumper yok)
- ESP32-S3 **N16R8** ([[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]]), fonksiyon butonu ve RGB durum LED'i
- PCM5102A, dört XH-A232 (`U7`-`U10`; `DAC_LOUT` dört `L` girişine, `DAC_ROUT` dört `R` girişine paralel), dört woofer (`J2`/`J4`/`J6`/`J8`) ve dört seri `C_SAFE` (`C2`-`C5`) korumalı tweeter (`J3`/`J5`/`J7`/`J9`); `R6` `XSMT` pull-down'ı, sayfadaki tek susturma — XH-A232'de susturma girişi yoktur ([[../07-decisions/ADR-0011-audio-side-gpio-reservation|ADR-0011]], [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]])
- `TP0-TP33` güç, I2S, analog bus, dört amfinin BTL çıkışları, DAC susturma ve kullanıcı arayüzü ölçüm noktaları

Dört amfi ayrı konnektör olarak çizilir ki ERC her neti ayrı görsün; `DAC_LOUT` ve `DAC_ROUT` net etiketleri dağıtımı taşır, her amfinin çıkış netleri `AMPn_*` adını alır.

Test noktası numaralandırması scriptteki tek bir tablodan üretilir; bu tablo [[circuit-and-wiring-plan#7.1 Test noktası yerleşimi|devre planındaki tabloyla]] aynı numaraları kullanır.

## Çizim kuralları

Her yerleşim `2,54 mm` ızgaradadır. Tel uçları elle yazılmaz, pin konumundan çözülür. Bitişik parçalar gerçek telle bağlanır (dört `C_SAFE` → tweeter, buton düğümü); sayfayı boydan boya geçmesi gereken raylar için net etiketi ve power sembolü kullanılır.

> [!warning]
> Konektör pin sıraları mantıksaldır; satın alınan parçaların fiziksel pin sırası olarak kullanılamaz. Jak polaritesi, `D2` düşümü ve modül revizyonları ayrıca doğrulanır.

> [!danger]
> `AMPn_L_MINUS` ve `AMPn_R_MINUS` (dört amfi) BTL anahtarlama çıkışıdır; GND değildir. Osiloskop toprak klipsi hiçbir BTL ucuna bağlanmaz. G0-G2 ölçüm kapıları kapanmadan gerçek sürücülere enerji verilmez.

Ana elektriksel gerekçeler, değerler ve bağlantı tabloları için [[circuit-and-wiring-plan|devre ve bağlantı planı]] esas kaynaktır.
