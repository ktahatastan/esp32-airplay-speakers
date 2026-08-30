---
title: KM103 / DC-132A güç anahtarı seçimi
status: completed
owner: orchestrator
updated: 2026-08-30
tags: [development-log, procurement, hardware, safety]
---

# 2026-08-30 — KM103 / DC-132A güç anahtarı seçimi

## Sonuç

Kullanıcının verdiği [KM103 / DC-132A 12 V beyaz nokta ışıklı 3P rocker](https://www.direnc.net/dc-132a-12v-yuvarlak-nokta-isikli-on-off-anahtar-3p-beyaz), Harman Kardom için seçilen mekanik güç anahtarı adayı olarak BOM'a işlendi. Elektriksel kabul verilmedi: satıcı sayfası kontak akımı ve 16,8 V DC kesme değerini belirtmiyor.

## Yapılanlar

- Dört cihaz için 4 adet ve 81,36 TL güncel maliyet kaydedildi.
- İç donanım ve üç satın alma senaryosu 140,00 TL toplam tasarrufla yeniden hesaplandı.
- Kontak değeri için yazılı satıcı doğrulaması ve G3 yük/ark testi zorunlu tutuldu.
- 12 V dahili LED'in 16,8 V 4S hatta doğrudan bağlanması yasaklandı; ilk prototipte LED pini açık bırakıldı ve koşullu `R_SW_LED` BOM satırı eklendi.
- KiCad üreticisindeki `S1` değeri KM103 / DC-132A olarak değiştirildi; dahili `D2` 12 V LED ve ölçüm yapılana kadar `DNP` kalan `R2 / R_SW_LED` mantıksal olarak eklendi.
- BOM, satıcı listesi, güç planı, kontrol planı, risk kaydı ve araştırma günlüğü birlikte güncellendi.

## Doğrulama

- Maliyet farkı ve tüm senaryo toplamları aritmetik olarak yeniden hesaplandı.
- KiCad üreticisi paketlenmiş Python çalışma zamanı ve KiCad 10.0.6 CLI ile yeniden çalıştırıldı; şema üretildi, PDF dışa aktarıldı ve ERC sonucu `0 error / 11 warning` oldu. Uyarılar bilerek açık bırakılan `TBD/NC` tekil netlerdir.
- Obsidian iç bağlantı kontrolü ve `git diff --check` çalıştırıldı.

## Açık kapı

Anahtarın satıcı/üretici tarafından belgelenmiş en az 16,8 V DC / 5 A kontak değeri bulunamazsa veya G3 testi başarısız olursa parça ana batarya hattında kullanılamaz; DC-rated alternatif gerekir.

## İlgili notlar

- [[../05-procurement/turkey-shopping-list-2026-08-30|Türkiye satın alma listesi]]
- [[../power-and-battery-plan|Güç ve batarya planı]]
- [[../controls-and-provisioning-plan|Kontroller ve provisioning]]
- [[../06-testing/test-strategy|Test stratejisi]]
