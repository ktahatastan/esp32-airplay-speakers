---
title: Doküman tutarlılığı, agent altyapısı, firmware aşamalandırma ve şema yenilemesi
status: complete
owner: orchestrator
reviewers: [hardware-engineer, firmware-engineer, qa-engineer]
updated: 2026-08-31
tags: [development-log, process, adr, schematic, kicad, firmware, agents]
---

# 2026-08-31 — Tutarlılık denetimi, agent altyapısı ve şema yenilemesi

## Amaç

Depoyu bir agent'ın yanlış yönlendirilmeden çalışabileceği hâle getirmek: kanonik belgelerle satın alma/şema arasındaki ayrışmayı kapatmak, kararları ADR'ye bağlamak, doğrulamayı tekrarlanabilir yapmak, firmware'i aşamalandırmak ve iki okunmayan çizimi tek profesyonel paftaya indirmek.

## Bulunan tutarsızlıklar

| # | Bulgu | Etki |
|---|---|---|
| K1 | `N8R8` ve `N16R8` paralel yaşıyordu; OTA manifest örneği `prototype-n8r8` diyordu | Flash boyutu A/B OTA bölüm bütçesini belirler |
| K2 | KiCad ile belgeler farklı sayıda test noktası sayıyordu; bir `JTP` başlığının etiketi taşıdığı netlerle uyuşmuyordu; amfi girişi test noktaları buton/LED'e bağlıydı | Bring-up prosedürü ile şema uyuşmuyordu |
| K3 | `docs/10-agentic-development` explorer, hardware, acoustics rolleri çiziyordu; gerçekte Claude ve Cursor'da yalnız üç rol vardı | Belge gerçeği yansıtmıyordu |
| K4 | Beş günlük "Obsidian iç bağlantı kontrolü çalıştırıldı" diyordu ama repoda böyle bir araç yoktu | İddia denetlenemez |
| K5 | İki SVG paftasında metin çakışması, kırpılma ve blokların içinden geçen teller vardı; KiCad scripti her modülü jenerik konektör yapıp bağlantıyı yalnız net etiketiyle kuruyordu | Çizimler okunmuyordu |

## Yapılan işler

### Kararlar

- [[../07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010]] açıldı: kanonik kart `N16R8` (16 MB flash + 8 MB PSRAM). Gerekçe ADR-0008'in çift OTA slotu bütçesi. Kart `accepted`, pin ataması `candidate` kaldı.

### Belge hizalaması

- Güç planı, devre planı, kart/pin seçimi, BOM ve OTA planı ADR-0010'a göre tek değere getirildi. `prototype-n8r8` → `prototype-n16r8`.
- Risk kaydına iki yeni satır: ters polarite koruması yokluğu, belge sapması.
- Test stratejisine belge bütünlüğü kapısı eklendi.
- `AGENTS.md` blokaj listesi, risk kaydındaki açık `Kritik` satırlarla hizalandı ve iki listenin ilişkisi açıkça yazıldı; "kilitli kararlar" bölümü eklendi.
- Eksik `owner` alanları altı belgeye eklendi.

### Agent altyapısı

- Altı rol üç araçta birden tanımlandı: `explorer`, `hardware-worker`, `firmware-worker`, `acoustics-worker`, `hardware-reviewer`, `verifier`. Önce yalnız Codex'te olan `explorer` ile hiç olmayan `hardware-worker` ve `acoustics-worker` eklendi.
- `docs/10-agentic-development/README.md` gerçek rol tablosunu gösterecek şekilde yeniden yazıldı.
- `.claude/settings.json` salt-okunur doğrulama komutları için allowlist, geçmişi değiştiren komutlar için `ask` ve gizli dosyalar için `deny` içerecek şekilde dolduruldu.
- `scripts/check_docs.py` yazıldı: wikilink hedefleri, `docs/` frontmatter alanları, ADR durum sözlüğü, ADR indeksi eksiksizliği, agent/skill tanım şeması ve kilitli kararlardan terim sapması denetleniyor.

### Firmware

- `docs/03-firmware/firmware-plan.md` `F0…F8` aşamalarıyla yeniden yazıldı. Her aşama için önkoşul, çıktı, ölçülebilir kabul ölçütü ve gate eşlemesi tanımlandı. Görev/çekirdek yerleşimi, önerilen depo yerleşimi ve tamamlanma tanımı eklendi.
- `F1` (AirPlay fizibilite spike'ı) pahalı donanım işinden önce konumlandırıldı; başarısız olursa PRD-002'nin yeniden müzakere edileceği açıkça yazıldı.

### Çizimler

- İki eski SVG ve iki PNG kaldırıldı. Yerine tek pafta geldi: `docs/02-hardware/assets/merzarkabul-schematic.svg`.
- Pafta `hardware/diagrams/generate_schematic_svg.py` ile üretiliyor. Koordinatlar blok ve sembol geometrisinden hesaplanıyor; paralel sinyal demetlerinde kanal ataması kaydırma yönünün tersine yapıldığı için hiçbir yol bir diğerini kesmiyor.
- KiCad üreticisi baştan yazıldı: gerçek semboller, `2,54 mm` ızgara, pin konumundan çözülen tel uçları, bitişik parçalar arasında çizilmiş teller, power sembolleri, test noktalarının tek tablodan üretimi ve bir yapısal self-check.

## Commit öncesi bağımsız inceleme

Değişiklik kümesi, birleşmeden önce altı boyutta (sayısal doğruluk, kilitli kararlara uyum, güvenlik iddiaları, Python doğruluğu, referans bütünlüğü, agent sözleşmesi) paralel olarak incelendi ve her bulgu ayrı bir çürütme turundan geçirildi. 29 bulgu açıldı, 17'si çürütüldü, 12'si doğrulandı ve düzeltildi.

Öne çıkan bulgular:

| Ağırlık | Bulgu | Düzeltme |
|---|---|---|
| Blocker | KiCad paftasında `D1` anot pinleri direnç **öncesi** net adlarını taşıyordu; aynı düğümde iki etiket birleşince `R3/R4/R5` seri dirençleri kısa devre oluyordu | `D1` pinleri `LED_x_A` olarak yeniden adlandırıldı; self-check'e "aynı noktada iki net" denetimi eklendi |
| Güvenlik | `.claude/settings.json` içindeki `Read(//Users/**)` izni, ev dizinindeki her dosyayı sorgusuz okunabilir yapıyordu; `deny` kuralları ise yalnız proje kapsamlıydı | Kural tamamen kaldırıldı; gerekçe `.claude/README.md`'ye yazıldı |
| Major | Amfi girişi test noktaları hiçbir yerde var olmayan `AMP_L_IN`/`AMP_R_IN` netlerini gösteriyordu | Amfi giriş düğümü olan `DAC_LOUT`/`DAC_ROUT`'a bağlandı; TP netlerinin varlığı denetleniyor |
| Major | `AGENTS.md`, blokaj listesinin risk kaydının `Kritik` satırlarını birebir yansıttığını iddia ediyordu; satırların bir kısmı eksikti | İki listenin ilişkisi doğru biçimde yazıldı |
| Major | `suppliers.md` ADR-0010'a hizalanmamıştı; `N8R8` eş değerli aday olarak duruyordu | Yalnız yedek olarak işaretlendi |
| Major | SVG'de renk göstergesi antet paneli tarafından üzeri boyanıyor, alt cetvel gizleniyordu | Gösterge kendi şeridine alındı, sayfa yüksekliği artırıldı |
| Major | SVG'de `U1` bloğunda aynı pin adı iki kez kullanılmıştı; sol `GND` pini hiç çizilmiyor, tel yanlış tarafa bağlanıyordu | Pin adları benzersizleştirildi; `Block.build()` artık yinelenen ada hata veriyor |
| Major | KiCad üreticisi her çalıştırmada rastgele UUID üretiyordu: 876 satırlık anlamsız diff | Tüm UUID'ler tek eşleme tablosundan deterministik hâle getirildi; `(path "/…")` referansları korunuyor |

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 87 dosya, 0 hata, 0 uyarı |
| `check_docs` negatif testi (kasıtlı `N8R8` eklendi) | **PASS** — sapmayı yakaladı, geri alındı |
| `python3 hardware/diagrams/generate_schematic_svg.py` | **PASS** — SVG üretildi |
| Pafta görsel denetimi | **PASS** — altı bölge tek tek büyütülerek çakışma/kırpılma kontrol edildi ve düzeltildi |
| `python3 hardware/kicad/generate_merzarkabul.py` | **PASS** — pafta üretildi |
| KiCad self-check | **PASS** — 0 sorun; tel uçları pin/köşe üzerinde, aynı noktada çift net yok, tek bağlantılı netler yalnız `EXPECTED_OPEN_NETS` |
| Self-check negatif testi (`D1` kısa devresi kasten geri kondu) | **PASS** — üç kısa devrenin üçünü de yakaladı, geri alındı |
| KiCad çıktısı determinizmi | **PASS** — iki ardışık çalıştırma birebir aynı; `(path "/…")` referansları sayfa UUID'siyle eşleşiyor |
| Commit öncesi bağımsız inceleme | **PASS** — 29 bulgu, 17 çürütüldü, 12 doğrulandı ve düzeltildi |
| Türkiye satın alma listesi aritmetiği | **PASS** — tüm ara ve genel toplamlar yeniden hesaplandı, birebir tuttu |
| KiCad ERC ve PDF dışa aktarma | **YAPILMADI** — bu makinede KiCad kurulu değil |

## Açık riskler ve sonraki adım

- **KiCad ERC çalıştırılmadı.** `python3 hardware/kicad/generate_merzarkabul.py --validate` KiCad kurulu bir makinede çalıştırılmalı; beklenen sonuç sıfır hata ve yalnız `TBD/NC` pin uyarılarıdır. Bu yapılmadan pafta `candidate` seviyesinden çıkmaz.
- Sürücü empedansı (G0) ve AirPlay multiroom senkronu (G7) ana blokaj olarak duruyor.
- Sıradaki iş: firmware `F0` iskeleti — ESP-IDF sürüm kilidi, partition CSV, boyut bütçesi ve PR CI hattı.
