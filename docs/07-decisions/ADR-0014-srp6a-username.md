---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-08
tags: [adr, provisioning, security, srp6a, compatibility]
---

# ADR-0014: SRP6a kullanıcı adı `wifiprov` olur

## Bağlam

Provisioning'in güvenliği [[ADR-0005-dual-provisioning|ADR-0005]] uyarınca ESP-IDF'in Security 2'sidir: SRP6a. Cihaz parolayı hiç tutmaz, yalnız bir **salt** ve bir **verifier** tutar. `firmware/tools/provision_credentials.py` bunları cihaz başına üretir ve bugüne kadar kullanıcı adı olarak `merzarkabul` kullandı. Espressif'in her örneğinde, testinde ve uygulamasında geçen ad ise `wifiprov`.

Bu, isimlendirme tercihi gibi görünüyor ve değil. Kullanıcı adı **verifier'ın içine giriyor**: `v = g^x`, `x = H(salt, H(I ":" P))`. Yani ad değişirse verifier de değişir; yanlış adla hesaplanmış bir kanıt eşleşmez.

Kaynaktan doğrulanan üç şey (ESP-IDF `v5.5.1`, bu makinedeki ağaç):

1. **Adı istemci gönderir, cihaz saklamaz.** `components/protocomm/src/security/security2.c` adı oturumda `in->sc0->client_username`'den alıyor ve doğrudan `esp_srp_exchange_proofs()`'a veriyor. Cihazın karşılaştıracağı kayıtlı bir ad yok.
2. **Yanlış ad sessizce geçmez.** `components/protocomm/src/crypto/srp6a/esp_srp.c` istemci kanıtını `memcmp` ile denetliyor ve tutmazsa `ESP_FAIL` dönüyor. Yani uyuşmazlık, el sıkışmanın **kanıt adımında** ve açık biçimde başarısız olur.
3. **QR yükü adı taşıyor.** `wifi_prov_print_qr()` alanları `ver`, `name`, `username`, `pop`, `transport`. Bizim üreticimiz de aynı alanları basıyor. Demek ki QR ile kurulumda uygulama adı yükten okur ve özel bir ad çalışır.

Buradan çıkan sonuç, riskin nerede olduğunu daraltıyor: **özel ad yalnız QR'sız yolda kırılır.** Kullanıcı cihazı uygulamanın listesinden seçip parolayı elle yazdığında uygulamanın adı kendisi üretmesi gerekir, ve ürettiği ad `wifiprov`'dur.

Bu ADR'nin **açıklamadığı** bir şey var ve karıştırılmamalı: Espressif'in SoftAP uygulamasının `mbedtls_gcm_auth_decrypt : -18` ile düşmesi bu konu değildir. O arıza SRP6a kanıt adımı **geçtikten sonra** oluyor; yukarıdaki (2) gereği kanıt adımının geçmiş olması adların zaten uyuştuğu anlamına gelir. İki sorun ayrıdır ve biri diğerini çözmez.

## Karar

Kullanıcı adı **`wifiprov`** olur.

Gerekçe, adın ne olduğu değil, ne olmadığıdır: **kullanıcı adı sır değildir.** QR'ın içinde açıkça yazar, etikette basılıdır ve tel üzerinde düz metin gider. Koruma tümüyle paroladadır. Dolayısıyla özel bir ad hiçbir güvenlik kazancı sağlamıyor, karşılığında QR'sız kurulum yolunu tamamen kırıyor.

Ürün kimliği bu kararla çelişmiyor: [[ADR-0001-product-identity|ADR-0001]]'in adlandırdığı yüzeyler kullanıcının **gördüğü** yüzeylerdir — AirPlay adı, BLE adı, kurulum SSID'si, mDNS. SRP6a kullanıcı adını son kullanıcı hiçbir akışta görmez ve hiçbir yere yazmaz.

## Sonuçlar

- `provision_credentials.py` bundan sonra `wifiprov` ile üretir. Etiket dosyası adı yazmaya devam eder, çünkü QR okunamadığında elle kurmanın tek yolu odur.
- **`merzarkabul` ile üretilmiş her kimlik bilgisi geçersizdir** ve yeniden üretilmelidir. Maliyeti bugün sıfır: sahada cihaz yok, üretim yapılmadı, ve tezgâh kartının kimlik bilgileri zaten tek kullanımlık.
- Adın yalnız tek bir yerde tanımlı olması korunur. İki yerde tanımlı bir ad, birinin değişip diğerinin kalmasıyla tam olarak bu ADR'nin engellediği hatayı üretir.
- QR'sız kurulum yolu bu kararla **açılmış olmuyor**, yalnız adın onu kapatması ortadan kalkıyor. O yolun kendisi hâlâ [[ADR-0005-dual-provisioning|ADR-0005]]'in açık maddesidir.

## Reddedilen seçenek

**`merzarkabul`'da kalmak.** Marka tutarlılığı gerekçesi burada karşılığı olmayan bir gerekçedir: kullanıcı bu dizeyi hiç görmez. Karşılığında QR okunamayan her durumda — kırık etiket, kamerasız cihaz, listeden seçen kullanıcı — kurulum, sebebi hiçbir yüzeyde yazmayan bir kanıt hatasıyla başarısız olur.

## Doğrulama

- QR'lı BLE kurulumu 2026-09-05'te iOS'ta uçtan uca çalıştı ([[../06-testing/devkit-bring-up|bring-up kaydı]]). O akış bu ADR'den önceydi; bu değişiklikten sonra **tekrarlanmalıdır**, çünkü verifier değişiyor.
- QR'sız kurulum hiç denenmedi ve bu ADR onu denenebilir hâle getirir; denenmesi ayrı bir iştir.
