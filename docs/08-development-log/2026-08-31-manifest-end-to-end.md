---
title: Manifest üreticisi ile doğrulayıcı arasındaki köprü
status: partial
owner: firmware-engineer
reviewers: [orchestrator]
updated: 2026-08-31
tags: [development-log, firmware, f7, ota, testing]
---

# 2026-08-31 — Üretici ve doğrulayıcı, değerler üzerinden

## Boşluk

OTA zincirinin iki ucu vardı: `make_manifest.py` manifest yazıyor, `hk_manifest_validate()` cihazın onu kurup kuramayacağına karar veriyor. İkisi yalnız **alan adları** üzerinden karşılaştırılıyordu.

Bir **değer** uyuşmazlığı — ayrıştırıcının farklı yazdığı bir sürüm, yanlış kutuda bir özet, `uint32`'yi taşıran bir boyut, tampona sığmayan bir URL — depodaki her testten geçer ve dört hoparlörün her sürümü reddetmesiyle keşfedilirdi.

## Kök sebep mimariydi

JSON ayrıştırma `hk_ota_client.c` içinde, HTTPS kodunun yanında duruyordu. O dosya ESP-IDF olmadan derlenmiyor, dolayısıyla ayrıştırıcı host'tan erişilemezdi.

cJSON zaten bağımsız C99 — ESP bağımlılığı yok, tek başına derleniyor. Ayrıştırmayı `hk_manifest_json.c`'ye taşımak cihazda hiçbir şeye mal olmuyor ve bütün yolu test edilebilir yapıyor.

## Taşıma bir hata ortaya çıkardı

Fonksiyon `static` iken tek bir yerden, hep geçerli argümanlarla çağrılıyordu, o yüzden argümanlarını hiç denetlemiyordu. Genel API olunca test koşumundan da çağrıldı ve `memset(out, ...)` `NULL` üstünde **segfault** verdi. Testin ilk koşusu 139 ile çıktı. Denetimler eklendi.

## Şimdi ne doğrulanıyor

`build/host-tests/manifest_e2e` gerçek üretilmiş bir manifest'i gerçek ayrıştırıcıdan ve gerçek doğrulayıcıdan geçiriyor. Elle sondaladım:

| Manifest'e yapılan | Cihazın kararı |
|---|---|
| hiçbir şey | `ok` |
| `sha256` büyük harfe çevrildi | `bad_sha256` |
| `sha256` 32 karaktere kısaltıldı | `bad_sha256` |
| `size = 0` | `bad_size` |
| cihaz `secure_version 5`, manifest `0` | `secure_version_rollback` |
| `target = esp32` | `wrong_target` |
| 300 karakterlik varlık URL'si | `field_missing` — kırpılmıyor, düşürülüyor |

Sonuncusu önemli: tampona sığmayan bir alan **kırpılmıyor**, yok sayılıyor. Kırpılmış bir ürün adı cihazınkiyle eşleşebilir, kırpılmış bir özet hâlâ 64 karakter bir şey olabilirdi.

## CI'da bir incelik

Host testleri işi ESP-IDF **olmadan** çalışıyor; bu bilinçli, çünkü saf modüllerin ESP-IDF görmemiş bir makinede derlenebildiğini kanıtlıyor. Sonucu şu: orada cJSON yok, ayrıştırıcı testleri atlanır — yani CI'da hiç koşmazlardı.

İki adım eklendi. ESP-IDF'siz iş artık `manifest_e2e`'nin kurulmadığını **doğruluyor** (atlamanın kaza değil tasarım olduğunu kanıtlar), ve ESP-IDF'li iş paketi cJSON ile yeniden derleyip ayrıştırıcı ve köprü testlerini çalıştırıyor.

Bu tersinden de işe yaradı: alan adı denetimi kaynağının taşındığını fark etti ve boş kümeyle sessizce geçmek yerine düştü.

## Doğrulama

ESP-IDF'siz 377.425 kontrol · cJSON ile 377.479 · 23 araç testi · `check_docs` 105 dosya 0 hata · firmware uyarısız derleniyor.
