---
status: active
owner: firmware-engineer
updated: 2026-08-31
tags: [credentials, signing, ota, security]
---

# Firmware imzalama anahtarları

OTA imzalama kararı ve kapsamı [[../07-decisions/ADR-0008-github-releases-ota|ADR-0008]]'de. Burası envanterdir.

## Geliştirme anahtarı (etkin, açık)

| Alan | Değer |
|---|---|
| Amaç | Yalnız geliştirme: imzalama zincirini uçtan uca denemek |
| Algoritma | RSA-3072 — ESP32-S3'te tek seçenek |
| Üretim tarihi | 2026-08-31 |
| Özel anahtar | `docs/credentials/hk-dev-signing-key.pem` — **depoda, dolayısıyla açık** |
| Açık anahtar | `docs/credentials/hk-dev-signing-key.pub.bin` |
| Açık anahtar SHA-256 | `a568a512fe625e1c1ae07cc3d4498156936b56c842fb835aa5495c9fa178bad2` |
| Gerçek sürüm imzalayabilir mi | **Hayır.** Yayın hattı reddeder. |

Bu anahtar public bir depoda durduğu için **tasarım gereği yanmıştır**. Bir sır değil, bir test malzemesidir. Onunla imzalanmış hiçbir imaj bir cihaza yazılmamalıdır: yazılırsa o cihaz, internetteki herhangi birinin imzaladığı güncellemeyi kabul eder hale gelir.

Reddin mekanik karşılığı `release.yml` içindedir. `publish` işi imzalamadan önce `HK_SIGNING_KEY`'in açık yarısını çıkarır ve `docs/credentials/*.pem` altındaki her anahtarla karşılaştırır; eşleşirse durur.

## Gerçek anahtar

Henüz **üretilmedi**, çünkü gidecek güvenli bir yeri yok: depo public, `release` ortamı mevcut değil (`total_count: 0`) ve onu oluşturmak için gereken `admin` yetkisi bu hesapta yok.

Üretim koşulları ve sırası `README.md` içindeki "Gerçek sürümden önce ne değişmeli" bölümünde.

## İmzalanmış sürümler

| Sürüm | Tarih | Anahtar parmak izi | Not |
|---|---|---|---|
| — | — | — | Henüz yayımlanmış sürüm yok (`releases: 0`, `tags: 0`). |

Parmak izi, `espsecure.py extract_public_key` çıktısının SHA-256'sıdır. Bir sürümün hangi anahtarla imzalandığını, özel anahtara dokunmadan bununla teyit edebilirsiniz:

```bash
espsecure.py extract_public_key --version 2 --keyfile ~/.harman-kardom/hk_signing_key.pem /tmp/pub.bin
shasum -a 256 /tmp/pub.bin
```

## İmzalanmış sürümler

| Sürüm | Tarih | Anahtar parmak izi | Not |
|---|---|---|---|
| — | — | — | Henüz yayımlanmış sürüm yok. |

## Anahtarı kaybedersem ne olur

Bu projede felaket değil, ama bedava da değil.

Güven çıpası, **çalışan uygulamanın kendi imza bloğudur**. Yani cihaz, yalnızca kendisini imzalayan anahtarla imzalanmış güncellemeleri kabul eder. Anahtar kaybolursa:

- Sahadaki cihazlar **OTA ile güncellenemez.** Yeni anahtarla imzalanmış bir imaj reddedilir.
- Kurtarma yolu fizikseldir: dört hoparlörü USB'ye takıp yeni anahtarla imzalanmış imajı yazmak.
- Dört cihaz için bu birkaç dakikalık bir iştir. Bir ürün için olsaydı bu kabul edilemezdi; burada kabul edilen takas budur.

Gerçek anahtar üretildiğinde yedeği **depo dışında** tutulmalı: şifreli bir disk ya da parola yöneticisinde güvenli not. Geliştirme anahtarının yedeğine gerek yok; kaybolursa yenisi üretilir.

## Anahtar ifşa olursa

Bir kez public bir yere girdiyse — commit edilip itildiyse, bir loga düştüyse, bir ekran görüntüsüne girdiyse — o anahtar yanmıştır. Commit'i silmek yetmez; geçmiş klonlanmış ve taranmış olabilir.

Yapılacaklar, sırayla:

1. Yeni anahtar üret (`espsecure.py generate_signing_key --version 2 --scheme rsa3072`).
2. Bu dosyadaki parmak izini güncelle.
3. Yeni anahtarla imzalanmış imajı dört cihaza **USB'den** yaz. OTA ile geçiş yapılamaz.
4. Eski anahtarla imzalanmış yayımlanmış sürümleri kaldır.

## Sahibinin yapması gerekenler

Ölçülen durum (2026-08-31): `release` ortamı yok, secret yok, `admin: false`.

Depo `private` yapıldıktan sonra, sırasıyla:

```bash
# 1. ortami olustur ve zorunlu inceleyici ekle  (Settings > Environments)
# 2. gercek anahtari CEVRIMDISI uret
espsecure.py generate_signing_key --version 2 --scheme rsa3072 hk_signing_key.pem

# 3. acik yarisini depoda sabitle
espsecure.py extract_public_key --version 2 \
  --keyfile hk_signing_key.pem firmware/certs/hk-signing-key.pub.bin

# 4. ozel yarisini ORTAM secret'i olarak sakla (depo secret'i DEGIL)
gh secret set HK_SIGNING_KEY --env release \
  --repo serbaysancak/esp32-airplay-speakers < hk_signing_key.pem
```

Dördüncü adımdaki ayrım önemli: **depo** secret'ını her iş okuyabilir, `build` dahil. Bu, hattı ikiye bölmenin tek sebebini ortadan kaldırır. Yalnız `release` ortamına bağlı **ortam** secret'ı doğru olanıdır.

Bir uyarı: `release.yml` "environment: release" diyor ama o ortam mevcut değil. GitHub, referans verilen bir ortamı ilk çalışmada **koruma kuralı olmadan** kendisi yaratır. Yani ortam oluşturulup korunmadan atılan ilk `v*.*.*` etiketi, korumasız bir ortam üretir. Bu, [[../01-planning/risk-register|risk kaydında]] açık bir satır olarak durur.
