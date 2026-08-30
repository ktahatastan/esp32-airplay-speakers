---
title: Yardımcı pasifler ve prototipleme BOM'u
status: completed
owner: orchestrator
updated: 2026-08-30
tags: [development-log, procurement, hardware]
---

# 2026-08-30 — Yardımcı pasifler ve prototipleme BOM'u

## Amaç

Devre planında kullanılan fakat ana satın alma tablosunda yalnız genel “sarf” bütçesi altında kalan direnç, kondansatör, jumper, klemens ve prototipleme parçalarını açık adet ve fiyatla kaydetmek.

## Yapılanlar

- `R_PU`, RGB seri dirençleri, `C_DB`, `C_A`, deneysel `C_SAFE`, `JP1`, test bağlantıları, klemens, pertinaks ve jumper kablo BOM'a eklendi.
- Fonksiyon butonu, sert güç anahtarı, RGB modülü, sigorta ve sigorta yuvasının ana tabloda zaten fiyatlandırıldığı doğrulandı; çift maliyet önlendi.
- Dört cihazı kapsayan yardımcı sepet 272,89 TL, ilk prototipte kullanılabilecek bölüm 178,63 TL olarak hesaplandı ve mevcut sarf bütçesine bağlandı.
- `C_SAFE` değerinin tweeter empedansı ile G2 süpürmesi tamamlanmadan nihai seçim olmadığı; 1.000 µF elektrolitiğin de düşük-ESR/105 °C bilgisi doğrulanmadan üretim parçası sayılamayacağı kaydedildi.

## Doğrulama

- Satın alma listesi, kanonik BOM, tedarikçi kısa listesi ve araştırma günlüğü birlikte güncellendi.
- Obsidian iç bağlantı kontrolü ve `git diff --check` çalıştırıldı.

## Açık riskler

- Woofer/tweeter empedansı hâlâ ölçülmedi; `C_SAFE` yalnız deney bankasıdır.
- İlk prototipin 250 TL sarf bütçesinde açık sepetten sonra kalan 71,37 TL, silikon kablo ve batarya izolasyonu için dar olabilir.
- Nihai güç/sürücü konnektörleri kasa, akım ve titreşim testinden sonra seçilecektir.

## İlgili notlar

- [[../05-procurement/turkey-shopping-list-2026-08-30|Türkiye satın alma listesi]]
- [[../05-procurement/bom|BOM]]
- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı planı]]
- [[../06-testing/test-strategy|Test stratejisi]]
