---
status: active
owner: orchestrator
updated: 2026-08-31
tags: [credentials, security, moc, rule]
---

# Kimlik bilgileri

## Kural

> Kimlik bilgisi dosyaları `docs/credentials/` altında tutulur; başka hiçbir yerde bulunamaz.
> Sürüm imzalama anahtarının **özel yarısı** buraya da girmez: o, `release` ortam secret'ında yaşar.

Ayrım şu: buradaki dosyalar **kayıttır** — hangi anahtar var, açık parmak izi ne, hangisi yanmış. Sürümü imzalayan sır ise depoda değil, GitHub'ın ortam secret'ında.

Konum kuralı `scripts/check_no_private_keys.py` ile uygulanır. Depodaki her izlenen dosyanın **içeriğine** bakar, adına değil: `key.pem`, `backup.pem` ya da `meeting-notes.md` adıyla saklanmış bir anahtarı da yakalar. İki yerde çalışır — CI'da ve yerel `pre-commit` kancasında:

```bash
git config core.hooksPath .githooks
```

## Şu anki durum (2026-08-31)

| | |
|---|---|
| Depo | `ktahatastan/esp32-airplay-speakers` |
| Görünürlük | **public** |
| Yetki | sahibi, `admin` |
| `release` ortamı | var, **1 koruma kuralı** |
| `HK_SIGNING_KEY` | **ortam** secret'ı olarak saklı |
| Depo secret'ı | **0 adet** |
| Zorunlu inceleyici | **var** |

Depo secret'ının sıfır olması tesadüf değil, tasarımın kendisi: ortam secret'ını yalnız `environment: release` diyen iş okuyabilir, `build` işi okuyamaz. Anahtarı depo secret'ı yapmak hattı çalıştırırdı ve tam da bu özelliği silerdi. Yapmayın.

**Zorunlu inceleyici var.** Depo public olduğu için ortam koruma kuralları ücretsiz; private'ken API `HTTP 422` ile reddediyordu. Bir etiket push'u artık insan onayı bekliyor.

## Anahtar geçmişi

İlk anahtar **yandı**. 2026-08-31'de üretilip depoya konmuştu (`9afd991`) ve o commit hâlâ geçmişte duruyor — depo bir süre private kaldıktan sonra 2026-09-02'de yeniden public yapıldı, yani o dosya yine herkese açık. Bu bilinçli: anahtar zaten ifşa olmuştu ve sonradan private yapmak onu geri çağırmıyordu. Geçmişi yeniden yazmak da geri çağırmaz.

Önemli olan onun bir daha imzalayamaması, ve bu konumla değil parmak iziyle sağlanıyor: dosyayı silmek anahtarı ifşa edilmemiş yapmaz, çünkü geçmişte durur.

Parmak izi `burned-keys.txt` içinde ve yayın hattı o anahtarla imzalamayı **kalıcı olarak** reddediyor. Git geçmişinden geri getirilse bile: bu sınandı.

Yerine geçen anahtar çevrimdışı üretildi, depoya hiç girmedi, açık yarısı `firmware/certs/hk-signing-key.pub.bin` olarak sabitlendi.

## Buraya asla yazılmayanlar

- Sürüm imzalama anahtarının özel yarısı — ortam secret'ında olmalı.
- Wi-Fi parolaları.
- Provisioning parolaları ve PoP değerleri (cihaz zaten yalnız salt/verifier saklar).
- API tokenları.

## İçerik

- [[signing-keys|Firmware imzalama anahtarları]] — envanter, parmak izleri, kayıp ve ifşa prosedürü.
- `burned-keys.txt` — bir daha asla imzalamayacak anahtarların parmak izleri.
