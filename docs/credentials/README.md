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
| Görünürlük | **private** |
| Yetki | sahibi, `admin` |
| `release` ortamı | var |
| `HK_SIGNING_KEY` | **ortam** secret'ı olarak saklı |
| Depo secret'ı | **0 adet** |
| Zorunlu inceleyici | **yok** — aşağıya bakın |

Depo secret'ının sıfır olması tesadüf değil, tasarımın kendisi: ortam secret'ını yalnız `environment: release` diyen iş okuyabilir, `build` işi okuyamaz. Anahtarı depo secret'ı yapmak hattı çalıştırırdı ve tam da bu özelliği silerdi. Yapmayın.

**Zorunlu inceleyici eklenemedi.** Private bir depoda ortam koruma kuralları ücretli plan istiyor; API `HTTP 422` ile reddediyor. Yani bir etiket push'u insan onayı olmadan yayımlar. Ortam kapsamı geçerli, onay kapısı değil. Risk kaydında satırı var.

## Anahtar geçmişi

İlk anahtar **yandı**. 2026-08-31'de üretilip depoya konmuştu ve o sırada depo public'ti (`9afd991`, 15:33 UTC). Depo artık private ve dosya kaldırıldı, ama bir süre herkese okunabilirdi — GitHub'ın genel olay akışı dakikalar içinde taranıyor. Sonradan private yapmak bunu geri almaz.

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
