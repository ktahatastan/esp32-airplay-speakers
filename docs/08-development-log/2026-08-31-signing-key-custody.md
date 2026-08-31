---
title: İmzalama anahtarı depoya kondu — kabul edilmiş geçici durum
status: partial
owner: orchestrator
reviewers: [firmware-engineer]
updated: 2026-08-31
tags: [development-log, security, credentials, ota, f7]
---

# 2026-08-31 — İmzalama anahtarının yeri

## Ne yapıldı

RSA-3072 imzalama anahtarı üretildi ve kullanıcının talimatıyla `docs/credentials/hk-dev-signing-key.pem` olarak depoya kondu. Kimlik bilgisi dosyalarının yeri kural haline getirildi.

## Endişe, bir kez, sonra karar

Depo **public** ve sahibi başka bir hesap; bu depoda çalışan geliştiricinin `admin` yetkisi yok, dolayısıyla görünürlüğü değiştiremiyor. Bunu `PATCH .../private=true` ile kuru denedim: **404**, GitHub'ın "admin değilsin" deme biçimi.

Bu söylendi. Kullanıcı, depo kendisine geçtiğinde halledeceğini ve bu aşamada önemli olmadığını belirtti. Karar onun ve bu aşamada dayanağı var — ölçülen durum:

| | |
|---|---|
| Yayımlanmış sürüm | 0 |
| Etiket | 0 |
| Fork / watcher / star | 0 / 0 / 0 |
| Sahadaki cihaz | 0 |
| Geçmişte anahtar izi | Yok — 19 commit'in tamamı tarandı |

Anahtar şu an hiçbir şeyi korumuyor, çünkü koruyacak bir şey yok. Bu, bir cihaz o anahtarla imzalanmış bir imajı çalıştırmaya başladığı an değişir.

## Bu yüzden yorum değil, kapı

Kabul edilen risk, ileride sessizce felakete dönüşebilecek bir risk. "Gerçek sürümden önce anahtarı değiştir" diye bir not yeterli değil; unutulur. Bunun yerine `publish` işi imzalamadan önce iki denetimden geçiyor:

1. Anahtarın açık yarısı `docs/credentials/` altındaki bir anahtarla eşleşiyorsa **durur**. Yani depodaki anahtarla gerçek sürüm imzalamak mümkün değil.
2. Sabitlenmiş `firmware/certs/hk-signing-key.pub.bin` ile eşleşmiyorsa **durur**.

Dört durumun dördü de sınandı: depodaki anahtar → red; sabitlenmiş anahtar yok → red; doğru anahtar → geçer; yanlış ama geçerli anahtar → red.

## Yan ürün: üç gerçek kusur

Anahtar araştırması sırasında `release.yml`'de üç kusur çıktı ve üçü de düzeltildi.

**İmza doğrulaması totolojikti.** `verify_signature --keyfile signing_key.pem`, imzayı az önce imzalayan anahtarla doğruluyordu; her geçerli RSA-3072 anahtarı geçer. Önemli olan cihazların **zaten güvendiği** anahtar olup olmadığı, çünkü güven çıpası çalışan uygulamanın kendi imza bloğu. Yanlış ama geçerli bir anahtarla yayımlanan sürümü dört hoparlör de sessizce reddederdi ve tek kurtarma yolu dördünü USB'den yeniden yazmaktı. Artık sabitlenmiş açık anahtarla karşılaştırılıyor.

**Anahtar silme adımı imzalama bloğunun içindeydi.** İmzalama ya da karşılaştırma başarısız olursa `signing_key.pem` çalışma alanında kalırdı. Ayrı bir `if: always()` adımına taşındı.

**Release asset yolları yanlıştı.** `upload-artifact` ortak-ata kuralı gereği arşivin kökü `firmware/build-release/`; indirilince dosyalar `artifacts/bootloader/bootloader.bin` ve `artifacts/partition_table/partition-table.bin` oluyor. `release.yml` bunları `artifacts/bootloader.bin` diye arıyordu, yani `gh release create` eksik dosyada patlar ve **hiçbir sürüm yayımlanmazdı**.

## Ve bir fazla iddia, geri alındı

ADR-0008 §6, `publish` işinin "korumalı `release` ortamına bağlı" olduğunu yazıyordu. Bunu dün ben yazdım ve **doğru değil**: ortam mevcut değil (`environments` → `total_count: 0`) ve GitHub, referans verilen bir ortamı ilk çalışmada koruma kuralı olmadan kendisi yaratır. Yani koruma, deponun sahip olduğu bir özellik değil, henüz yapılmamış bir kurulum adımı. ADR düzeltildi ve risk kaydına satır eklendi.

Bunun etrafından `HK_SIGNING_KEY`'i depo secret'ı yaparak dolaşmak mümkün ama yasak: hattı çalıştırırdı ve anahtarı `build` dahil her işe okuturdu — yani hattı ikiye bölmenin tek sebebini silerdi.

## Kural

Kimlik bilgisi dosyaları `docs/credentials/` altında, başka hiçbir yerde. `scripts/check_no_private_keys.py` bunu depodaki her dosyanın **içeriğine** bakarak uyguluyor, adına değil: `key.pem`, `backup.pem`, hatta `meeting-notes.md` adıyla saklanmış bir anahtarı da yakalıyor. CI'da ve `pre-commit` kancası olarak çalışıyor; kancanın bir commit'i gerçekten durdurduğu sınandı.

Kullanıcıya ait sırlar bu klasöre de girmez: Wi-Fi parolaları, provisioning parolaları ve PoP değerleri, API tokenları. Klasör açık olduğu için, açık olmasının bedeli kabul edilebilir olmayan hiçbir şey oraya konmaz.

## Gerçek sürümden önce

1. Depo `private` — yalnız `serbaysancak` yapabilir.
2. Yeni anahtar çevrimdışı. Buradaki yanmıştır.
3. Açık yarısı `firmware/certs/hk-signing-key.pub.bin` olarak sabitlenir.
4. Özel yarısı `release` **ortam** secret'ı olarak saklanır ve ortama zorunlu inceleyici eklenir.
