---
title: F6 depolama ve kalibrasyon duvarı
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, f6, storage, nvs, prd-008]
---

# 2026-08-31 — Depolama ve kalibrasyon duvarı

## Amaç

`F6`'nın donanım gerektirmeyen yarısı: iki NVS deposu, şema kararları ve PRD-008'in gerçek garantisi. Güç telemetrisi (paket gerilimi, NTC, düşük gerilim kapanışı) G4 bekliyor.

## Tasarımın özü: iki depo eşdeğer değil

| Depo | İçerik | Kaybedilirse |
|---|---|---|
| `user_settings` | Sahibin seçtikleri | Bir dakikada yeniden ayarlanır |
| `factory_cal` | Sürücü koruma profili, crossover ve limiter sınırları, cihaza özel provisioning kimlik bilgileri | **Geri gelmez.** Tezgâhta, ekipmanla, bu sürücülere karşı bir kez ölçülmüştür |

Bu asimetri her kuralı belirliyor. Kullanıcı ayarını atmak küçük bir sıkıntı; kalibrasyonu atmak, tweeter'ın G2'de kurulan korumasını kaybetmesi demek.

`hk_schema` her durumu iki depo için ayrı çözüyor:

| Bulunan | `user_settings` | `factory_cal` | Neden |
|---|---|---|---|
| Eşleşiyor | kullan | kullan | — |
| Eski | ileri taşı | ileri taşı | — |
| **Yeni** (rollback sonrası) | varsayılana dön | **salt okunur** | Daha yeni firmware bu derlemenin ifade edemeyeceği bir profil yazmış olabilir; üzerine yazmak tezgâh zamanını yok eder |
| **Bozuk** | varsayılana dön | **fail-safe, silme** | Okunamayan bir profil elle kurtarılabilir; sessizce silmek hoparlörü ölçülmemiş varsayılanlarla korumasız tweeter sürerken bırakır |
| **Eksik** | varsayılan yaz | **fail-safe** | Bu cihaz hiç kalibre edilmemiş. Profil uydurmak projenin tam olarak yasakladığı şey |

`factory_cal` için **hiçbir durumda "varsayılan yaz" yok** — bu bir invariant olarak test ediliyor.

Kalibrasyon yoksa veya güvenilmezse `hk_storage_audio_permitted()` false döner ve ses güvenli durumunda kalır. Uydurma bir varsayılan profil, çalışan bir profille aynı görünür — ta ki bir tweeter korumasız sürülene kadar.

## PRD-008 artık yorum değil, denetim

"Kullanıcı reseti fabrika kalibrasyonunu silmez" garantisi üç katmanlı:

1. **Ayrı partition.** İki depo aynı partition'da iki namespace değil. Partition sınırı bir garanti, isimlendirme kuralı bir vaat.
2. **Salt okunur açılış.** Bu firmware'de kalibrasyon yazıcısı yok (o G2 ile geliyor), bu yüzden `factory_cal` her zaman `NVS_READONLY` açılıyor. Yazamadığı bir depoyu bozamaz.
3. **`tools/check_storage_isolation.py`.** Kaynağı tarıyor: hiçbir silme/format çağrısı kalibrasyon partition'ını adlandıramaz ve kalibrasyon yazılabilir açılamaz. CI'da çalışıyor.

Denetim negatif test edildi: kalibrasyon partition'ını silen bir çağrı eklemek ve depoyu `NVS_READWRITE` açmak — ikisi de yakalandı.

Butonun 12 saniyelik dalı artık gerçekten çalışıyor: `hk_storage_user_reset()` yalnız kullanıcı namespace'ini siliyor, ardından Wi-Fi kimlik bilgileri temizleniyor.

## Diğer kararlar

- **`factory_cal` asla formatlanmıyor.** Boş bir partition'da `nvs_flash_init_partition` başarısız olur; doğru cevap budur. Formatlamak "hiç kalibre edilmemiş"i "hiçbir şeyle kalibre edilmiş"e çevirir ve ikisi bir tweeter korumasız sürülene kadar birbirinden ayırt edilemez.
- **Depo hatası ölümcül değil.** `hk_storage_init()` her zaman `ESP_OK` döner ve sorunu bildirir. Açılmayan bir hoparlör nedenini kimseye söyleyemez.
- Sürüm alanında `0` okumak bozukluk sayılıyor: sürüm olarak `0` hiç yazılmaz, dolayısıyla oradan `0` gelmesi o alanda başka bir şey olduğu anlamına gelir.

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 93 dosya, 0 hata |
| Host birim testleri | **PASS** — 10.212 kontrol, 0 hata |
| `check_storage_isolation.py` | **PASS** — 0 sorun; iki ihlal senaryosu negatif test edildi |
| Partition kapısı ve kendi testleri | **PASS** |
| `idf.py -C firmware build` | **PASS** — 0 proje uyarısı; slotun %84'ü boş |
| **Donanımda çalıştırma** | **YAPILMADI** |

## Kalan iş

- Güç telemetrisi: paket gerilimi, NTC, düşük gerilimde kontrollü kapanış, `OTA_MIN_PACK_MV` eşiği. Hepsi G4 ölçümüne bağlı.
- Kalibrasyon yazıcısı G2 ile gelecek; o zamana kadar `factory_cal` salt okunur.
- Cihaz başına salt/verifier üreten üretim aracı hâlâ yok; provisioning bu yüzden açılmıyor (kasıtlı).
