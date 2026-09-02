---
status: active
owner: firmware-engineer
updated: 2026-08-31
tags: [credentials, signing, ota, security]
---

# Firmware imzalama anahtarları

Karar ve kapsam [[../07-decisions/ADR-0008-github-releases-ota|ADR-0008]]'de. Burası envanterdir.

## Etkin anahtar

| Alan | Değer |
|---|---|
| Amaç | OTA firmware imzası (`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`) |
| Algoritma | RSA-3072 — ESP32-S3'te tek seçenek |
| Üretim | 2026-08-31, çevrimdışı, depo ağacının dışında |
| Özel yarısı | `release` **ortam** secret'ı `HK_SIGNING_KEY`; yerel kopya `~/.harman-kardom/hk_signing_key.pem` (`0400`) |
| Açık yarısı | `firmware/certs/hk-signing-key.pub.bin` — depoda sabitlenmiş |
| Açık anahtar SHA-256 | `26f34cc477ec428ec62aa26f99fe81b29bd3b182f0539bed7ad04403441a054b` |
| Depoda mı | **Hayır**, ve hiç olmadı |

Parmak izini özel anahtara dokunmadan teyit etmek için:

```bash
shasum -a 256 firmware/certs/hk-signing-key.pub.bin
```

## Yanmış anahtarlar

| Parmak izi | Ne oldu |
|---|---|
| `a568a512…78bad2` | 2026-08-31'de üretildi ve depoya kondu (`9afd991`). Dosya kaldırıldı ama commit geçmişte duruyor ve depo public. İfşa geri alınamaz; önemli olan bir daha imzalayamaması. |

Liste `burned-keys.txt` içinde ve yayın hattı imzalamadan **önce** okuyor. Sınandı: yanmış anahtar git geçmişinden geri getirilip verilse bile reddediliyor.

Yanmış bir anahtar listeden **çıkmaz**. "Artık muhtemelen sorun değil", bir imzalama anahtarının bulunabileceği bir durum değildir.

## Yayın hattının imzalamadan önce yaptığı üç denetim

1. `HK_SIGNING_KEY` boşsa durur — sessizce imzasız yayımlamaz.
2. Açık yarısının parmak izi `burned-keys.txt`'te varsa durur.
3. `firmware/certs/hk-signing-key.pub.bin` ile eşleşmiyorsa durur.

Üçüncüsü kritik: imzayı imzalayan anahtarla doğrulamak hiçbir şey kanıtlamaz, her geçerli RSA-3072 anahtarı geçer. Önemli olan cihazların **zaten güvendiği** anahtar olup olmadığı, çünkü güven çıpası çalışan uygulamanın kendi imza bloğudur. Yanlış ama geçerli bir anahtarla yayımlanan sürümü dört hoparlör de sessizce reddeder; kurtarma yolu dördünü USB'den yeniden yazmaktır.

## İmzalanmış sürümler

| Sürüm | Tarih | Anahtar | Not |
|---|---|---|---|
| — | — | — | Henüz yayımlanmış sürüm yok. |

## Anahtarı kaybedersem

Güven çıpası çalışan uygulamanın imza bloğu olduğu için, cihaz yalnız kendisini imzalayan anahtarla imzalanmış güncellemeleri kabul eder. Anahtar giderse:

- Sahadaki cihazlar **OTA ile güncellenemez**.
- Kurtarma yolu fizikseldir: dört hoparlörü USB'ye takıp yeni anahtarla imzalanmış imajı yazmak.

Dört cihaz için birkaç dakikalık iş; bir ürün için kabul edilemez olurdu. Kabul edilen takas budur.

Yedek isterseniz **depo dışında**: şifreli disk veya parola yöneticisinde güvenli not. Ortam secret'ı okunamaz, yani GitHub bir yedek değildir.

## Kurulum nasıl yapıldı

Tekrar gerekirse:

```bash
espsecure.py generate_signing_key --version 2 --scheme rsa3072 ~/.harman-kardom/hk_signing_key.pem
chmod 400 ~/.harman-kardom/hk_signing_key.pem

espsecure.py extract_public_key --version 2 \
  --keyfile ~/.harman-kardom/hk_signing_key.pem firmware/certs/hk-signing-key.pub.bin

gh api -X PUT repos/ktahatastan/esp32-airplay-speakers/environments/release
gh secret set HK_SIGNING_KEY --env release \
  --repo ktahatastan/esp32-airplay-speakers < ~/.harman-kardom/hk_signing_key.pem
```

Zorunlu inceleyici eklendi (2026-09-02, koruma kuralı sayısı 1). Private'ken API `HTTP 422` ile reddediyordu; koruma kuralları public depolarda ücretsiz. Bir etiket push'u artık onay bekliyor.
