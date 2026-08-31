---
title: Firmware F0 iskeleti
status: complete
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, esp-idf, ci, f0]
---

# 2026-08-31 — Firmware F0 iskeleti

## Amaç

[[../03-firmware/firmware-plan|Firmware planındaki]] `F0` aşamasını gerçekleştirmek: derlenen, sürümü sabitlenmiş, doğrulanabilir bir ESP-IDF projesi kurmak. Kod depo kökünde değil `firmware/` altında toplandı.

## Yapılanlar

### Proje

- ESP-IDF **v5.5.1** kilitlendi. Sürüm `firmware/README.md` ve CI iş akışında aynı değerle yazılı.
- `firmware/partitions.csv`: 16 MB yerleşimi. İki eşit `0x6E0000` uygulama slotu, `0x2000` `otadata`, ayrı `factory_cal` kalibrasyon bölümü ve ileride NVS şifrelemesi açılırsa yeniden bölümleme gerekmesin diye şimdiden ayrılmış `nvs_keys`.
- `sdkconfig.defaults`: N16R8 (16 MB flash, 8 MB oktal PSRAM), özel partition tablosu, `-Werror` sınıfı uyarı ayarları.
- OTA rollback **kapalı** bırakıldı. Açık olsaydı imaj `pending verify` durumunda kalır ve `F0`'da henüz var olmayan ilk-açılış sağlık kontrolü çağrılmadığı için her OTA geri dönerdi. `F7` ile birlikte açılacak.

### Bileşenler

| Bileşen | İş |
|---|---|
| `hk_pins` | GPIO ataması. Kısıtlar `_Static_assert` ile derleyici tarafından zorlanıyor. |
| `hk_identity` | MAC'ten türetilen AirPlay / BLE / SoftAP / mDNS adları (ADR-0001). |
| `hk_version` | Katı SemVer ayrıştırma ve OTA güncelleme kararı (ADR-0008). |

ESP-IDF bağımlılığı olmayan bileşenler saf C yazıldı; test edilebilirliğin kaynağı bu.

`main/hk_main.c` yalnız açılış raporu basıyor: sürüm, çalışan slot, çip, algılanan flash ve PSRAM, cihaz kimliği ve GPIO tablosu. Ses yok, ağ yok, sürülen GPIO yok — her biri henüz ölçülmemiş bir donanım kapısına bağlı.

### Pin ataması derleyici tarafından zorlanıyor

`hk_pins.h` üç kuralı derleme zamanında uyguluyor: iki işlev aynı pini paylaşamaz, hiçbir pin strapping (`GPIO0/3/45/46`) veya native USB (`GPIO19/20`) üzerine düşemez, hiçbir pin en yüksek ESP32-S3 GPIO'sunu aşamaz. Negatif test yapıldı: butonu `GPIO0`'a almak derlemeyi okunur bir mesajla durduruyor.

Host testi ayrıca tabloyu [[../02-hardware/circuit-and-wiring-plan#3.1 Aday ESP32-S3 pin planı|devre planındaki tabloyla]] işlev işlev karşılaştırıyor; belge ile kod sessizce ayrışamıyor.

### Doğrulama altyapısı

- `firmware/test/`: ESP-IDF gerektirmeyen host birim testleri, `-Wall -Wextra -Werror -Wconversion` ile derleniyor.
- `firmware/tools/check_partitions.py`: çakışma, alt/üst sınır, 64 KB hizalama, eşit slot boyutu, tip/alt-tip eşleşmesi, etiket uzunluğu ve serbest alan payı.
- `firmware/tools/test_check_partitions.py`: doğrulayıcının kendi testleri. Her vaka gerçek tabloyu tek alan değiştirerek bozuyor ve aracın fark etmesini bekliyor.
- `.github/workflows/firmware-ci.yml`: belge bütünlüğü, host testleri, partition kapısı, üretilen çizimlerin güncelliği, firmware derlemesi ve boyut kapısı.

## Commit öncesi bağımsız inceleme

Kod beş boyutta (ESP-IDF API doğruluğu, C doğruluğu, belge uyumu, test kalitesi, araç/CI) paralel incelendi ve her bulgu ayrı bir çürütme turundan geçirildi. **18 bulgu açıldı, 12'si çürütüldü, 6'sı doğrulandı ve düzeltildi.**

| Bulgu | Düzeltme |
|---|---|
| `esp_flash_get_size()` fiziksel boyutu değil, imaj başlığındaki **yapılandırılmış** boyutu döndürüyor. Rapor, derleme ayarını donanım okuması gibi gösteriyordu; `< 16 MB` dalı ise ulaşılamazdı çünkü ESP-IDF küçük bir parçada `app_main`'den önce zaten duruyor. | `esp_flash_get_physical_size()` kullanıldı, ölü dal silindi, gerekçesi yazıldı. Ayrıca PSRAM raporlanıyor: N16 ile N16R8'i ayıran tek şey o. |
| `hk_version_compare`'in major dalı hiç çalıştırılmıyordu; işaret çevirme mutasyonu tüm testleri geçiyordu | Major karşılaştırma ve `should_update` testleri eklendi |
| `check_partitions.py` bölümleri yalnız **ada** göre buluyordu; bootloader ise tip/alt-tipe bakar. `ota_1` alt-tipi `ota_0` olan bir tablo kapıdan geçiyordu | Tip/alt-tip eşleşmesi ve yinelenen app alt-tipi denetimi eklendi |
| `check_partitions.py`'de alt sınır yoktu; `0x9000` altına taşan bir bölüm bootloader'ı ezerdi ama kapıdan geçiyordu | `FIRST_USABLE_OFFSET` ve etiket uzunluğu denetimi eklendi |
| Partition doğrulayıcısının kendi testi yoktu | 10 vakalık `test_check_partitions.py` eklendi ve CI'ya bağlandı |
| Belgeler `factory_calibration` adını zorunlu tutuyordu. Ad **19 karakter**: ESP-IDF partition etiketini 16, NVS namespace'ini 15 karakterle sınırlar — yani ad hem bölüm hem namespace olarak imkânsızdı | Tüm belgelerde `factory_cal` yapıldı ve sınır gerekçesi kaydedildi |

Ayrıca kendi yazdığım bir test yanlıştı: dokuz haneli sürüm bileşenlerinin `uint32_t`'yi taşıracağını varsaymıştım, taşırmıyor. Test suite bunu yakaladı; assertion gerçek taşma değerleriyle değiştirildi.

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 89 dosya, 0 hata, 0 uyarı |
| Host birim testleri | **PASS** — 180 kontrol, 0 hata |
| `check_partitions.py` | **PASS** — 8 bölüm, 0 sorun |
| `test_check_partitions.py` | **PASS** — 10 vaka, 0 hata |
| `idf.py -C firmware build` (temiz) | **PASS** — ESP-IDF v5.5.1, proje kaynaklarında **0 uyarı** |
| İmaj boyutu | 232.576 bayt; slotun **%97'si boş** |
| `PROJECT_VER` | İmajda `0.1.0`, `version.txt` ile eşleşiyor |
| Pin guard negatif testi | **PASS** — `GPIO0` ataması derlemeyi durdurdu |
| Üretilen çizimler | **PASS** — SVG güncel |
| **Donanım üzerinde çalıştırma** | **YAPILMADI** — kart yok |

## Açık riskler ve sonraki adım

- **Hiçbir şey donanımda çalıştırılmadı.** Açılış raporu, GPIO ataması ve PSRAM algılaması denenmedi. Pin tablosu bu yüzden `candidate` kalıyor.
- `firmware/build` çıktısı Git'e alınmıyor; CI kendi derlemesini yapar.
- Sıradaki iş `F1`: AirPlay yığını fizibilite spike'ı ve [[../07-decisions/ADR-0007-airplay-stack|ADR-0007]]'nin kapatılması. Pahalı donanım işinden önce gelmesinin nedeni, başarısız olması hâlinde PRD-002'nin yeniden müzakere edilmesi gerekmesi.
