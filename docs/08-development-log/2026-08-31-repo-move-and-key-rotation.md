---
title: Depo taşındı, private yapıldı ve imzalama anahtarı döndürüldü
status: partial
owner: orchestrator
reviewers: [firmware-engineer]
updated: 2026-08-31
tags: [development-log, security, credentials, ota]
---

# 2026-08-31 — Depo taşıması ve anahtar rotasyonu

## Ne değişti

Depo `ktahatastan/esp32-airplay-speakers`'a taşındı ve **private** yapıldı. Kullanıcı bu deponun sahibi ve `admin`, yani daha önce imkânsız olan her şey mümkün hâle geldi.

Bu, birkaç saat önce yazdığım güvenlik belgelerinin **dayanağını** değiştirdi. O belgeler "depo public, sahibi başka hesap, admin yok" üzerine kuruluydu; hepsi güncellendi.

## Anahtar yandı ve döndürüldü

İlk imzalama anahtarı depo **public'ken** depoya konmuştu (`9afd991`, 15:33 UTC). Depo sonradan private yapıldı ve dosya kaldırıldı, ama bu ifşayı geri almaz: bir süre herkese okunabilirdi ve GitHub'ın genel olay akışı dakikalar içinde taranıyor. Sonradan private yapmak, yayımlanmış bir sırrı geri çağırmaz.

Yapılanlar:

1. Yeni anahtar çevrimdışı üretildi, depo ağacının dışında, `0400`. Depoya hiç girmedi.
2. Açık yarısı `firmware/certs/hk-signing-key.pub.bin` olarak sabitlendi (`26f34cc4…1a054b`).
3. Eski anahtarın parmak izi `docs/credentials/burned-keys.txt`'e yazıldı (`a568a512…78bad2`).
4. Eski anahtarın özel yarısı depodan silindi.

Yanmış liste kalıcı. Bir anahtar oradan **çıkmaz**; "artık muhtemelen sorun değil" bir imzalama anahtarının bulunabileceği durum değil.

Denetimi üç durumda sınadım:

| Verilen anahtar | Sonuç |
|---|---|
| Yeni anahtar (ortam secret'ındaki) | geçer |
| Yanmış anahtar, **git geçmişinden geri getirilerek** | reddedilir |
| Üçüncü, geçerli ama alakasız anahtar | reddedilir (sabitlenmişle eşleşmiyor) |

İkincisi önemli: anahtarı silmek yetmez, çünkü geçmişte duruyor. Reddin parmak izine bağlı olması bunu kapatıyor.

## Bölünme artık gerçek

`release` ortamı oluşturuldu ve `HK_SIGNING_KEY` **ortam** secret'ı olarak saklandı. Depo secret'ı sayısı **0**.

Bu sayı önemli: ortam secret'ını yalnız `environment: release` diyen iş okuyabilir, `build` işi okuyamaz. Yani "derleme işi anahtarı görmez" artık bir tasarım niyeti değil, doğrulanmış bir özellik. Anahtarı depo secret'ı yapmak hattı çalıştırırdı ve tam da bu özelliği silerdi.

## Alınamayan şey, olduğu gibi

**Zorunlu inceleyici eklenemedi.** Private bir depoda ortam koruma kuralları ücretli plan istiyor; API `HTTP 422` ile reddediyor:

```
Failed to create the environment protection rule.
Please ensure the billing plan supports the required reviewers protection rule.
```

Yani bir etiket push'u insan onayı olmadan yayımlar. Ortam kapsamı geçerli, onay kapısı değil. Bunu "korumalı ortam" diye yazmıyorum — birkaç saat önce tam da bu hatayı yapmıştım ve düzeltmiştim.

Not: bu, private yapmanın **bedeli**. Public depoda koruma kuralları ücretsizdi. Dört cihazlık bir hobi projesi için doğru takas, ama takas olduğu kayda geçsin.

## Doğrulama

101 belge / 0 hata · 10373 ana makine kontrolü / 0 hata · her iki workflow ayrıştı · firmware uyarısız derleniyor · anahtar konum denetimi 0 sorun · imzalama uçtan uca yeni anahtarla sınandı.

## Açık kalanlar

- `release` ortamında onay kapısı yok (plan kısıtı).
- Yayımlanmış sürüm hâlâ yok; hat hiç çalıştırılmadı.
- Ses tarafı pin ataması araştırılıyor — ayrı iş.
