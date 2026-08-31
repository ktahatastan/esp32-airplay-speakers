---
status: active
owner: orchestrator
updated: 2026-08-31
tags: [credentials, security, moc, rule]
---

# Kimlik bilgileri

## Kural

> Kimlik bilgisi dosyaları `docs/credentials/` altında tutulur; başka hiçbir yerde bulunamaz.

İki yarısı var ve ikisi de denetleniyor:

1. **Buraya konabilir.** Bu klasör, geliştirme aşamasında kullanılan kimlik bilgilerinin evidir. Buradaki her şey **açık kabul edilir**.
2. **Başka yere konamaz.** `scripts/check_no_private_keys.py` depodaki her izlenen dosyanın **içeriğine** bakar — adına değil — ve bu klasörün dışında özel anahtar bulursa durur. `key.pem`, `backup.pem` ya da `meeting-notes.md` adıyla saklanmış bir anahtarı da yakalar; `.gitignore`'daki isim listesi yakalayamaz.

Denetim iki yerde çalışır: `firmware-ci.yml` ve `release.yml` içinde her push'ta, ve yerel `pre-commit` kancası olarak. Kancayı bir kez etkinleştirin:

```bash
git config core.hooksPath .githooks
```

## Buradaki her şey açıktır — bu bilinçli

Bu depo şu an **public** (`serbaysancak/esp32-airplay-speakers`, 2026-08-31 itibarıyla `visibility: public`) ve sahibi başka bir hesap; bu depoda çalışan geliştiricinin `admin` yetkisi yok, dolayısıyla görünürlüğü değiştiremiyor.

Bu yüzden buradaki bir özel anahtar, internetteki herkes tarafından indirilebilir. Bu kabul edilmiş ve **geçici** bir durumdur, kaza değil. Kabul edilebilir olmasının tek sebebi şu anki aşama:

| Ölçüm (2026-08-31) | Değer |
|---|---|
| Yayımlanmış sürüm | 0 |
| Etiket | 0 |
| Fork / watcher / star | 0 / 0 / 0 |
| Sahadaki cihaz | 0 — donanım henüz gelmedi |

Yani buradaki anahtar şu an **hiçbir şeyi korumuyor**; koruyacak bir şey yok. Bir cihaz o anahtarla imzalanmış bir imajı çalıştırmaya başladığı andan itibaren bu doğru olmaktan çıkar.

## Gerçek sürümden önce ne değişmeli

Sırasıyla:

1. Depo `private` yapılmalı — bunu yalnız `serbaysancak` yapabilir.
2. **Yeni** bir imzalama anahtarı çevrimdışı üretilmeli. Buradaki anahtar yanmıştır; public bir depoda durduğu için geri döndürülemez şekilde açıktır.
3. Yeni anahtarın açık yarısı `firmware/certs/hk-signing-key.pub.bin` olarak sabitlenmeli.
4. Özel yarısı `release` **ortam** secret'ı olarak `HK_SIGNING_KEY` adıyla saklanmalı — depo secret'ı olarak değil, çünkü depo secret'ını her iş okuyabilir ve bu, hattı ikiye bölme sebebini ortadan kaldırır.

Bunlar yapılana kadar yayın hattı **yayımlayamaz**, ve bu bir temenni değil: `release.yml` içindeki `publish` işi, `docs/credentials/` altında duran bir anahtarla imzalamayı reddeder ve sabitlenmiş açık anahtar yoksa durur.

## Buraya asla yazılmayanlar

Klasör açık olduğu için, açık olmasının bedeli kabul edilebilir olmayan hiçbir şey buraya girmez:

- Wi-Fi parolaları.
- Provisioning parolaları ve PoP değerleri (cihaz zaten yalnız salt/verifier saklar; parola hiçbir yerde durmaz).
- API tokenları, kişisel erişim tokenları.
- Kullanıcıya ait hiçbir şey.

## İçerik

- [[signing-keys|Firmware imzalama anahtarları]] — envanter, parmak izi, kayıp ve ifşa prosedürü.
- `hk-dev-signing-key.pem` — geliştirme imzalama anahtarı. **Açık. Gerçek sürüm imzalamaz.**
- `hk-dev-signing-key.pub.bin` — aynı anahtarın açık yarısı; parmak izi doğrulaması için.
