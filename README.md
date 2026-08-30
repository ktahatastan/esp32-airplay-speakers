# Harman Kardom

Harman Kardon Nova sürücülerinden geliştirilen; bataryalı, aktif ve AirPlay üzerinden senkron çalışması hedeflenen dört bookshelf hoparlör projesi.

> [!WARNING]
> Bu proje Li-ion batarya paketi, yüksek akım ve hoparlör sürücüsü koruması içerir. Ölçüm ve kabul kapıları geçilmeden dört üniteye çoğaltma yapılmaz.

## Durum

Proje araştırma ve tek-hoparlör prototipi aşamasındadır. Nova sürücülerinin gerçek empedansı ve seçilecek ESP32 AirPlay yığınının dört cihazlı multiroom senkron yeteneği henüz doğrulanmadı.

## Hedef mimari

Her kutuda ESP32-S3, PCM5102A I2S DAC, XH-A232/TPA3110 iki kanallı amfi, woofer+tweeter aktif bölüşümü, 4S Li-ion batarya, BLE/SoftAP provisioning, çok işlevli buton, RGB LED ve ayrı fiziksel güç anahtarı bulunur.

```text
AirPlay/Wi-Fi -> ESP32-S3 -> I2S -> PCM5102A -> XH-A232 -> woofer + tweeter
4S Li-ion/BMS -> amfi
              `-> 5 V buck -> ESP32-S3 + DAC
```

## Ürün kimliği

| Yüzey | Varsayılan ad |
|---|---|
| Ürün ailesi | `Harman Kardom` |
| AirPlay | `Harman Kardom XXXX` |
| BLE provisioning | `HarmanKardom-XXXX` |
| Kurulum Wi-Fi ağı | `HarmanKardom-Setup-XXXX` |
| mDNS | `harman-kardom-xxxx.local` |

`XXXX`, cihaz kimliğinden türetilen kısa benzersiz ektir.

## Dokümantasyon

Depo kökü bir Obsidian kasasıdır. Obsidian ile bu klasörü açın ve [docs/Home.md](docs/Home.md) sayfasından başlayın.

- [Proje yol haritası](docs/01-planning/roadmap.md)
- [Sistem mimarisi](docs/01-architecture/system-architecture.md)
- [Güç ve batarya planı](docs/power-and-battery-plan.md)
- [KiCad şeması ve üretim scripti](docs/02-hardware/kicad-schematic.md)
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

## İlk çalışma sırası

1. Sürücüleri ölç ve G0 kapısını kapat.
2. AirPlay senkron fizibilitesini kanıtla.
3. Tek hoparlör elektriksel prototipini dummy-load ile doğrula.
4. DSP korumasını düşük seviyede gerçek sürücülerle doğrula.
5. Batarya/şarj/termal kapıları sonrası dört ünite BOM'unu kilitle.

Güncel iş listesi: [TODO.md](TODO.md).
