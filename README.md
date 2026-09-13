# Merzarkabul Airplay Speakers

Harman Kardon Nova sürücülerinden geliştirilen; adaptörle beslenen, tek kabinde sekiz sürücü (dört woofer, dört tweeter) taşıyan, AirPlay 2 ile çalan tek bir aktif hoparlör projesi.

> [!WARNING]
> Bu proje 24 V DC besleme, dört yüksek akımlı Class-D amfi ve hoparlör sürücüsü koruması içerir. Ölçüm ve kabul kapıları geçilmeden sürücülere sinyal verilmez; ilk enerjilenen yol tek amfi, tek woofer ve tek tweeter'dır.

## Durum

Proje araştırma ve prototip aşamasındadır. Nova sürücülerinin DC dirençleri ölçüldü (woofer 4,0 Ω, tweeter 3,5 Ω; ikisi de 4 Ω sınıfı); empedans eğrisi ve `Fs` henüz ölçülmedi, crossover köşesi bu yüzden muhafazakâr bir tahmindir. AirPlay alıcısı bir iPhone tarafından bulundu, eşleşti ve akış aldı; ürün kartında ürünün kendi yolu (PCM5102A → XH-A232) tek amfiyle tezgâhta çaldı. Bunlar dinleme kayıtlarıdır; ses yolunun ölçümü (seviye, clipping, pop) G1'de dummy-load üzerinde alınır ve hiçbir kapı henüz geçilmedi.

## Hedef mimari

Kutuda ESP32-S3 N16R8, PCM5102A I2S DAC, dört özdeş XH-A232/TPA3110 amfi (sekiz BTL kanal), dört woofer + dört tweeter, aktif bölüşüm (DAC `LOUT` woofer bandı, `ROUT` tweeter bandı), 24 V / 2,9 A masaüstü DC adaptör girişi (5,5 x 2,1 mm jak), BLE/SoftAP provisioning, çok işlevli buton ve RGB LED bulunur. Kabin, Nova'nın kendi pasif radyatörleriyle akortlu kapalı bir kutudur; başlangıç varsayımı tek paylaşılan hava hacmi, kararı G0 sonrası.

```text
AirPlay/Wi-Fi -> ESP32-S3 N16R8 -> I2S -> PCM5102A -> 4 x XH-A232 -> 4 woofer + 4 tweeter

24 V DC adaptör -> 5,5x2,1 mm jak (VIN) -+-> 4 x XH-A232 amfi
                                          +-> MP1584 A 5,10 V -> ESP32-S3
                                          `-> MP1584 B 5,10 V -> PCM5102A
```

Kilitli kararlar: [N16R8 kartı](docs/07-decisions/ADR-0010-esp32-s3-n16r8-board.md), [bi-amp sinyal zinciri](docs/07-decisions/ADR-0002-biamp-signal-chain.md), [24 V DC adaptörle besleme](docs/07-decisions/ADR-0020-dc-adapter-power.md), [tek kabin, sekiz sürücü](docs/07-decisions/ADR-0021-single-cabinet.md), [AirPlay yığını](docs/07-decisions/ADR-0007-airplay-stack.md).

## Ürün kimliği

| Yüzey | Varsayılan ad |
|---|---|
| Ürün ailesi | `Merzarkabul Airplay Speakers` |
| AirPlay | `Merzarkabul XXXX` |
| BLE provisioning | `PROV_Merzarkabul-XXXX` (öneki Espressif uygulamalarının liste filtresi ister, ADR-0023) |
| Kurulum Wi-Fi ağı | `PROV_Merzarkabul-XXXX`, açık ağ |
| mDNS | `merzarkabul-xxxx.local` |

`XXXX`, cihaz kimliğinden türetilen kısa benzersiz ektir.

## Dokümantasyon

Depo kökü bir Obsidian kasasıdır. Obsidian ile bu klasörü açın ve [docs/Home.md](docs/Home.md) sayfasından başlayın.

- [Proje yol haritası](docs/01-planning/roadmap.md)
- [Sistem mimarisi](docs/01-architecture/system-architecture.md)
- [Güç planı](docs/power-plan.md)
- [Devre şeması](docs/02-hardware/circuit-and-wiring-plan.md) ve [KiCad kaynağı](docs/02-hardware/kicad-schematic.md)
- [Firmware planı ve aşamalandırma](docs/03-firmware/firmware-plan.md)
- [Kontroller ve provisioning](docs/controls-and-provisioning-plan.md)
- [OTA ve sürüm yönetimi](docs/03-firmware/ota-and-release-plan.md)
- [Satın alma listesi](docs/05-procurement/bom.md)
- [Test ve kabul kapıları](docs/06-testing/test-strategy.md)
- [Karar kayıtları](docs/07-decisions/README.md)
- [Geliştirme günlüğü](docs/08-development-log/README.md)
- [Agent çalışma sistemi](docs/10-agentic-development/README.md)

## Agentic geliştirme

`AGENTS.md` kanonik çalışma sözleşmesidir. Codex, Claude ve Cursor'a özel dosyalar bu sözleşmeye uyar. Tek orkestratör görevleri böler, her dosyanın tek yazma sahibi olur ve birleşen her iş ADR/günlük/test kanıtını günceller.

Repo-yerel skill'ler `.agents/skills/`, Codex çalışanları `.codex/agents/`, Claude çalışanları `.claude/agents/`, Cursor çalışanları ve kuralları `.cursor/` altındadır.

## Doğrulama

```bash
python3 scripts/check_docs.py
```

Birleşme öncesi zorunludur. Wikilink hedeflerini, `docs/` frontmatter alanlarını, ADR durum sözlüğünü, ADR indeksini, agent/skill tanımlarını ve kilitli kararlardan sapan terimleri denetler. Fiziksel kapıların yerine geçmez.

## İlk çalışma sırası

1. Sürücüleri ölç ve G0 kapısını kapat.
2. AirPlay alıcısının bir telefon tarafından bulunup eşleşip akış aldığını kanıtla (PRD-009).
3. Elektriksel prototipi (bir amfi, dummy-load) doğrula; sonra dört amfiyi birlikte tam yükte.
4. DSP korumasını düşük seviyede gerçek sürücülerle doğrula — önce tek woofer ve tek tweeter.
5. Firmware `F0` iskeletini kur ve `F1` AirPlay spike'ını çalıştır.
6. Kalan kapılar (G6, G8) geçildikten sonra BOM'u kilitle.

Güncel iş listesi: [TODO.md](TODO.md).
