---
title: Kurulumdan PIN kalktı — Security 1, açık kurulum ağı, PROV_ öneki; telefonda henüz denenmedi
status: done
owner: orchestrator
reviewers: [verifier]
updated: 2026-09-13
tags: [development-log, firmware, provisioning, security, ble, softap, identity, tools, docs]
---

# 2026-09-13 — Kurulumdan PIN kalktı

Bu oturumda hiçbir karta dokunulmadı; iş tümüyle kod ve kayıt. Sahibin isteği tek cümleydi ve olduğu gibi kaydedildi:

> "Wifi Prov ve BLE prov aşamasından pin olayını kaldıralım wifi açık olsun evde kullanacağım sorun yok kolay olsun kurulumu."

Üç şey istiyor: uygulamanın sorduğu PIN — sahiplik kanıtı — kalksın, kurulum ağı parolasız olsun, kurulum kolay olsun. Gerekçesi tek ve yeterli: cihaz evde duracak. Karar [[../07-decisions/ADR-0023-pinless-provisioning|ADR-0023]]'te; burada ne değiştiği, neden bu şekilde değiştiği ve neyin hâlâ denenmediği var.

## 1. Security 1, Security 0 değil

PIN'i kaldırmak Security 2'den çıkmak demek: ESP-IDF v5.5.1, sürüm 2 için `NULL` parametreyi "Security params cannot be null" ile reddediyor (`protocomm.c:328-331`); "Security 2, kanıtsız" diye bir kip yok. Geriye iki seçenek kalıyordu, ikisi de "PIN yok"; fark, PIN'in yokluğunda ne kaldığı.

**Security 0 düz metindir.** `protocomm_security0`'ın `encrypt`/`decrypt`'i yok; istek olduğu gibi işleyiciye gidiyor. BLE bağlantı şifrelemesi bu projede zorlanmıyor (`CONFIG_WIFI_PROV_BLE_FORCE_ENCRYPTION` ESP-IDF'te varsayılan kapalı, hiçbir izlenen fragman açmıyor), yani ev Wi-Fi parolası eşleşmemiş bir GATT bağlantısında açık giderdi. Üstüne, stok Espressif uygulamaları `sec_ver 0` ilan eden cihazı varsayılan "Secured" ayarıyla geri çeviriyor; kullanıcı ayarı elle "Unsecured"a çevirmeden kurulum başlamıyor. Security 0 hem daha az koruyor hem **bir adım fazla** istiyor.

**Security 1, sahiplik kanıtı `NULL`.** Yönetici `capabilities.no_pop = true` yapıp bunu şifresiz `proto-ver`'de `cap [no_pop, wifi_scan]` olarak yayınlıyor; oturum anahtarı yalnız X25519'dan türüyor, `SHA256(pop)` yalnız kanıt varsa anahtara XOR'lanıyor (`security1.c:313-326`). İki telefon kütüphanesi de `no_pop` görünce boş kanıtla **sormadan** açıyor; `esp_prov.py` kendiliğinden 1 seçiyor. Pasif dinleyiciye karşı gizlilik var, iki yönde de kimlik doğrulama yok. Sahibin istediği tam bu: sorulmayan bir PIN — ve karşılığında kaybedilenin adı konmuş (aşağıda §6).

Üç `wifi_prov_mgr_start_provisioning()` çağrısı da artık `(WIFI_PROV_SECURITY_1, NULL, <ad>, NULL)`. Security 2 Kconfig arkasına alınmadı, **silindi**: CI'ın derlemediği, tezgâhın açmadığı ikinci bir güvenlik tasarımı sessizce bozulur ve bozulduğu ancak biri ona döndüğünde anlaşılır. Geri dönüş bir ADR'dir; kod `git`'te (`ef0e501` ve öncesi).

## 2. Kurulum ağı açık

`start_setup_ap()` `WIFI_AUTH_OPEN`, anahtar yok; `start_softap_only()` `service_key = NULL`, ki `scheme_softap.c` boş parolayı zaten açık ağa çeviriyor. Soft-AP kipinde OWE yok (`esp_wifi_types_generic.h:530`): "açık" burada şifresiz 802.11 çerçevesi demek, "Enhanced Open" diye bir ara seçenek yok.

Portal düz form olarak kaldı ve sayfaya bir cümle geldi, kullanıcının gördüğü yerde:

> Bu kurulum ağı açıktır ve bu form şifrelenmeden gider: buraya yalnız ev Wi-Fi parolanızı, yalnız hoparlör kurulumdayken yazın.

Cümle `hk_portal.c`'de tek bir makro (`HK_PORTAL_OPEN_NETWORK_NOTE`), sayfada `<p class=note>` olarak. `test_portal.c` değişmedi, çünkü sayfayı üretmiyor — yalnız form ayrıştırıcısını sınıyor; cümlenin imaja girdiği ürün `.bin`'inde `grep` ile görüldü, host testinde değil. Bu, ADR-0015'in beş gün önce reddettiği "Açık SoftAP + düz form" satırının ta kendisi; ADR-0023 bunu adıyla söylüyor ve o karara geri dönüşün nedenini yazıyor.

## 3. `PROV_` öneki: QR'sız yolun şartı

Bugüne kadar bir şeyi QR gizliyordu: stok "ESP BLE Provisioning" ve "ESP SoftAP Prov" uygulamaları BLE ve SoftAP listesini varsayılan `PROV_` önekiyle süzüyor (Android `build.gradle:34-35`, iOS `Utility.swift:75`). `Merzarkabul-XXXX` adlı bir cihaz, kullanıcı uygulamanın ayarından öneki değiştirmeden listede **görünmüyor**; QR yolu önekten bağımsız olduğu için 5 Eylül'de bu fark edilmedi. QR isteğe bağlı olunca liste yolunun kendi başına çalışması gerekiyor.

Bu yüzden iki kurulum yüzeyi tek ad aldı: BLE yayın adı da kurulum ağının SSID'si de `PROV_Merzarkabul-XXXX` (21 karakter; BLE sınırı 29, SSID sınırı 32). AirPlay adı `Merzarkabul XXXX` ve mDNS adı `merzarkabul-xxxx` değişmedi. `hk_identity.h`'de `HK_NAME_PREFIX_SETUP`, `test_identity.c` iki adı da `PROV_Merzarkabul-A1B2` diye iddia ediyor; doğrulayıcı öneki geri alan bir mutasyonda testin dört yerde düştüğünü gördü, yani test canlı. Eski adlar (`Merzarkabul-XXXX`, `Merzarkabul-Setup-XXXX`) firmware'de artık yok: iki bring-up kaydındaki adlar o günün adlarıdır, bu imajla flaşlanan kart yeni adla ilan verir.

## 4. QR isteğe bağlı ve sırsız

Yük `{"ver":"v1","name":"PROV_Merzarkabul-XXXX","transport":"ble","security":1}`, SoftAP eşi `"transport":"softap"`. `pop`, `username`, `password` yok. `security` alanı zorunlu değil, dürüst olanı: kütüphaneler eksik alanı 2 varsayıyor ama düzeyi cihazın `proto-ver`'inden alıyor.

`firmware/tools/provision_credentials.py` adını korudu — CI ve belgeler ona bakıyor; docstring'i adın tarihsel olduğunu söylüyor — ama yaptığı iş değişti: `srp6a` içe aktarılmıyor, `secrets` yok, `chmod` yok. Cihaz dizinine `factory_cal.csv` (`cal` ad alanı, yalnız `schema` `u32` `1`), iki yükü taşıyan `qr.txt` ve cihaz adı ile QR metnini taşıyan `label.txt` yazıyor; `IDF_PATH` yalnız `--image` için gerekiyor. Yeni `test_provision_credentials.py` 23 durumla bunu sabitliyor — hiçbir dosyada sır sözcüğü yok, hiçbir dosya `0600` değil, kaynağında `srp6a` yok — ve firmware'in `test_identity.c`'siyle adı çapraz denetliyor. `write_profile.py`'nin dizin sözleşmesi buna göre: `cal` ad alanlı ve `schema` satırlı `factory_cal.csv` zorunlu, üç `.bin` dosyası isteğe bağlı; hâlâ taşıyan eski bir dizin eskisi gibi birleşiyor ve `--dump` anahtarlarını, eski tasarımın ölü verisi olduklarını söyleyen tek satırla listeliyor. Uçtan uca (araç → `write_profile.py` → `--dump`) doğrulayıcı tarafından IDF ortamında koşuldu; dizinde `password`/`pop`/`salt`/`verif` sözcüğü yok.

## 5. Kaldırılanlar ve geride kalan ölü veri

`hk_network.c`'den giden: salt/verifier/`ap_pass` NVS anahtar makroları, `s_salt`/`s_verifier`/`s_ap_password`, `load_security2_credentials()`, `load_ap_password()`, `s_have_security2`/`s_have_ap_password` kapıları ve üç ret yolu. Bileşen artık `hk_storage`'a bağımlı değil; `factory_cal`'dan hiçbir şey okumuyor. Dört eski açılış satırı yok; tek yeni satır:

```text
setup security: protocomm security 1, no proof of possession; setup network open (ADR-0023)
```

Sonuç: **boş bir kalibrasyon deposu kurulumu açıyor.** `firmware/README.md`'nin ve risk kaydının bu sabaha kadar anlattığı "kimlik bilgisi yoksa provisioning açılmaz" reddi artık yok — kararla kaldırıldı, düzeltilerek değil. Ses izni bu karardan bağımsız: hâlâ `schema` + `profile` + geçerli karar (ADR-0022).

`prov_salt`/`prov_verif`/`ap_pass` taşıyan kartlar yeniden flaşlanmıyor. Ürün kartı `932C` üçünü de taşıyor (8 Eylül kaydı); geliştirme kartı (ADR-0012) `06C4` salt ve verifier'ı taşıyor (5 Eylül kaydı, `ap_pass` ADR-0015'ten önce yoktu). Firmware hiçbirini okumuyor, bölüm salt-okunur açılıyor, `write_profile.py` birleştirirken satırları koruyor. Sahibin elindeki iki cihaz dizini de bu eski biçimde ve henüz `profile.bin` taşımıyor; yani `write_profile.py`'nin ilk gerçek koşusu eski-dizin yolundan geçecek — araç testi tam bu durumu (üç satır bayt bayt korunuyor) kapsıyor.

CI'a bir koruma adımı girdi (`partition-layout` işi, araç zinciri gerektirmeyen yer): `hk_network.c`'de `WIFI_PROV_SECURITY_1, NULL` ve `WIFI_AUTH_OPEN` arıyor, `WIFI_PROV_SECURITY_2` ya da `WIFI_AUTH_WPA2_PSK` görürse düşüyor. Yerelde `ef0e501`'in dosyasında `exit 1`, bugünkünde geçti.

## 6. Kabul edilen risk, düz sözlerle

ADR-0023 bunları adıyla yazıyor; burada kısaca:

1. **Kurulum penceresi açıkken menzildeki herkes hoparlörü istediği ağa kurabilir.** Sahiplik kanıtı yok. Bunu yapan ev parolasını öğrenmez; sahibi 5 sn basışla ağı unutturup yeniden kurar.
2. **Pasif dinleyici hiçbir şey öğrenmez** — uygulama yolunda oturum X25519 + AES-CTR ile şifreli.
3. **Pencere sırasında menzildeki aktif bir saldırgan oturuma girebilir** (man-in-the-middle): hiçbir şey sahipliği kanıtlamadığı ve istemci cihazı doğrulamadığı için sahte bir `PROV_Merzarkabul-XXXX` yayını sahibin telefonundan ev parolasını alabilir. Security 2 bunu engelliyordu.
4. **Pencere ne zaman açık:** ilk açılışta ve ağ sıfırlamasından sonra kurulana kadar; kurulmuş cihazda yalnız butonla (çalarken iki basış), 10 dakika.
5. **Uygulamasız portal yolunda ev parolası açık ağda düz HTTP ile gider.** Pencere sırasında menzildeki bir dinleyici formu gönderildiği saniyelerde okuyabilir. BLE/uygulama yolunda bu maruziyet yok; sayfa bunu kullanıcıya söylüyor.
6. **Kabul evin içindir.** Ortak alan, ofis, konuk ağı bu kabulün dışında.

## 7. Kayda geçen bir gerçek: "üç başarısız katılım" dalı cihazda erişilemez

`hk_provision.c:150-154` (`ef0e501`), üç ardışık `CONNECT_FAIL`'den sonra provisioning'i butonsuz ve sınırsız açan bir dal taşıyor. Host testinde çalışıyor, cihazda çalışmıyor: `hk_main.c` politikaya yalnız `TICK`, `BUTTON_SHORT`, `NETWORK_RESET` ve `FACTORY_RESET` veriyor; `CONNECT_OK`/`CONNECT_FAIL`/`CREDENTIALS`'ı modülün dışında çağıran yok; `hk_network` beş denemeden sonra `error = true` ile duruyor ve `on_network_status()` bunu politikaya çevirmiyor. Üç doğrulayıcı bunu ayrı ayrı `grep`'le teyit etti. Sahiplik kanıtı kalkınca "yönlendiriciyi kapatıp cihazı ele geçirme" yolu **açılmıyor** — ama kullanıcı için "ağ değişince kendiliğinden kurulum penceresi" de **yok**; yönlendirici değişirse yol butondur. Politika değişmedi, yalnız kaydedildi; dalı bağlamak ya da silmek `TODO.md`'de ayrı bir karar maddesi.

## Kayıt

- **ADR-0023** yeni, `accepted`; ADR-0014'ü tümüyle aşıyor (SRP6a yoksa kullanıcı adı da yok). ADR-0015 ve ADR-0016'ya tarihli notlar; ADR-0016 `accepted` kalıyor (taşıma kuralı orada). ADR-0015'in durumu ADR-0016'nın 8 Eylül'de bıraktığı gibi `superseded` kaldı: bu turun kararı "accepted kalır" demişti, ama dosya zaten aşılmıştı ve geri çevirmek ADR-0016'nın zincirini sessizce bozardı — not yerinde, durum olduğu gibi. ADR-0001'in kimlik cümlesi iki kurulum yüzeyi için `PROV_` adını aldı.
- **`AGENTS.md`:** kilitli kararlara provisioning satırı; kimlik bilgisi paragrafı artık var olmayan bir sırrı anmıyor (kural aynen: kullanıcının hiçbir sırrı depoya girmez). `docs/credentials/README.md` düzeltildi.
- **Plan, PRD, risk kaydı, firmware planı, `TODO.md`, beceri tanımı:** kurulum akışı kullanıcının gördüğüyle yeniden yazıldı; PRD-004 PIN'siz kurulumu istiyor; risk kaydı "kimlik bilgisi yazılmadı" satırını kararla kapattı, açık pencere için kabul edilen-risk satırı ve §7'nin gerçeği için bir satır ekledi; `TODO.md` üç tezgâh maddesi aldı.
- **`scripts/check_docs.py`:** `provisioning-pop` sapma kuralı (depo kapsamı). Verilen desenin `(?i)` hâli ses notlarındaki "pop"u — açılış/kapanış pop'unu — 22 dosyada yakalıyordu; kural `PoP`'u tam büyük-küçük harfle, tanımlayıcıları sözcük sınırıyla arıyor (`ap_pass`, vendor'daki `ap_password`'ü görmesin diye). İzin listesi ağacın gerçekten bildirdiğine indirildi.
- **Tarihli kayıtlar** (31 Ağustos, 5 ve 8 Eylül günlükleri, iki bring-up kaydı) yeniden yazılmadı; `git status` altlarında değişiklik göstermiyor.

Kayıt yazılırken bir şey daha netleşti ve düzeltmeye değer: 5 Eylül'de geliştirme kartındaki SoftAP **açıktı** (`service_key NULL`, Security 2 yalnız protocomm'daydı); WPA2 anahtarı 8 Eylül'de ADR-0015 ile geldi ve `ap_pass` ilk kez ürün kartına yüklendi. "5 Eylül WPA2'nin arkasında çalıştı" diyen her cümle yanlıştır; bir tanesi `firmware/README.md`'de duruyor (aşağıda).

## Doğrulama

Hepsi otomatik; hiçbiri bir telefon ya da kartın söylediği bir şey değil.

| Kontrol | Sonuç |
|---|---|
| Ürün derlemesi (ESP-IDF v5.5.1) | geçti, 0 uyarı; 1.695.440 B, slotun %76'sı boş; `CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1=y` |
| Geliştirme kartı derlemesi (`sdkconfig.devkit`) | geçti, 0 uyarı; 1.681.952 B, slotun %44'ü boş |
| Tezgâh derlemesi (`sdkconfig.bench`) | geçti, 0 uyarı; 1.697.616 B |
| Host takımı | 684.874 kontrol, 0 hata (`-Wall -Wextra -Werror -Wconversion` temiz) |
| `test_provision_credentials.py` | 23 test, geçti (IDF ortamı dışında 2 atlama, ortamda 0) |
| `test_write_profile.py` | 54 test, geçti (dışarıda 4 atlama, ortamda 0; uçtan uca durum dahil) |
| `test_check_partitions` / `test_make_manifest` / `test_recover` | 22 / 28 / 10, 0 hata |
| `check_no_credential_logs.py` | 139 kaynak, 544 log çağrısı, 0 sorun |
| `check_storage_isolation.py`, `check_no_private_keys.py` | 0 sorun; 384 dosya, 0 sorun |
| Ürün imajı dizgileri | yeni açılış satırı 1, `PROV_Merzarkabul-` 1, portal cümlesi 1; `Merzarkabul-Setup-` 0, dört eski satır 0 |
| CI koruma adımı, yerelde | bugünkü `hk_network.c`'de geçti; `ef0e501`'in dosyasında `exit 1` |
| `scripts/check_docs.py` | 123 not, 383 dosya, **3 hata**, 0 uyarı — üçü de `provisioning-pop`, hiçbiri bu turun dosyalarında (aşağıda) |
| `git diff --check` | temiz |

ESP-IDF alıntıları (`protocomm.c`, `manager.c`, `security1.c`, `security0.c`, `scheme_softap.c`, `esp_prov.py`) bağımsız doğrulayıcılar tarafından `~/esp/esp-idf` (v5.5.1) üzerinde yeniden okundu; uygulama iddiaları GitHub `master` kaynaklarından (iOS `2ecd878`, Android `512ab02`) — mağaza ikilileri değil.

**Hiçbir şey telefonda denenmedi.** Ne BLE'de `no_pop`'un PIN sormadan geçtiği, ne `PROV_` cihazın listede göründüğü, ne açık SSID'nin kilitsiz çıktığı, ne portal cümlesinin sayfada durduğu bir telefondan görüldü. Kayıttaki her provisioning PASS'i Security 2 ve WPA2 ile ölçüldü ve bu tasarımı anlatmıyor. Tezgâh maddeleri `TODO.md`'de; operatör kaydı olmadan PASS yazılmaz.

## Açık riskler ve sonraki adım

Doğrulayıcıların bulguları aynı gece, birleşmeden önce kapatıldı; her biri doğrulayıcının yazdığı biçimde:

- `check_docs`'un üç sapma hatası (`security-and-recovery.md` Security 2 anlatımı, `measurement-and-dsp-plan.md`'nin eski `write_profile` sözleşmesi, `hk_ui.h`'daki "SRP6a" yorumu) düzeltildi; kök `README.md` kimlik tablosu `PROV_Merzarkabul-XXXX`'e çekildi; `ADR-0005`'in 8 Eylül notu ve `.gitignore` yorumu güncellendi; `bench-measurement-order.md` E5 artık "kayıtlı PoP" demiyor.
- CI: `test_provision_credentials.py` üç iş akışı adımına eklendi (firmware işinde IDF ortamıyla, imaj durumları koşar); koruma adımı üç çağrının **üçünü** sayıyor, `.authmode = WIFI_AUTH_OPEN,` atamasını arıyor ve Security 0'ı da, her türlü anahtarlı erişim noktasını da (`WIFI_AUTH_WPA*`, `WEP`) reddediyor.
- `provision_credentials.py` var olan `factory_cal.csv`'yi artık ezmiyor (profil satırı ya da eski kimlik satırları taşıyan dizin olduğu gibi kalır; testi var); `test_provision_credentials.py` öneki `#define HK_NAME_PREFIX_SETUP "PROV_"` satırında arıyor, yorumda değil.
- `firmware/README.md`: 5 Eylül anlatımı (WPA2 kurulum ağı üç gün sonra, ADR-0015 ile geldi; telefondan hiç doğrulanmadı), eski anahtar satırları (ürün kartı üç, geliştirme kartı iki), `--image` örneğinin başındaki `export.sh` satırı, ve `PROV_` liste davranışının "uygulama kaynağından okundu, telefonda denenmedi" kaydı.
- ADR-0023: `06C4` cümlesi kayda çekildi (yalnız salt ve verifier, 2026-09-05), `manager.c` alıntısı `1633-1651`, `test_portal`'ın sayfa cümlesini denetlemediği açıkça yazıldı.
- Kontrol ve kurulum planı: Android örnek uygulamasının QR'dan otomatik katılması açık ağla eşleşmez (`ESPDevice.java:197-199`), telefon ağa kendi ayarlarından katılır; `TODO.md`'nin üç üstü çizili satırı yine `- [x]` biçiminde.
- Sertleştirme yapıldı: `sdkconfig.defaults` protocomm'un 0 ve 2 seviyelerini derleme dışı bırakıyor; ürün imajı 0x19c1e0 bayta indi ve artık SRP6a kodunu taşımıyor. CI'daki grep ikinci bekçi.

Kalan, telefon ve tezgâh isteyen:

- Hiçbir şey telefonda denenmedi: `PROV_` listeleme, PoP'suz Security 1 el sıkışması, açık ağ + portal — üçü de `TODO.md`'deki tezgâh maddesi.
- Uygulama davranışı kaynaktan okundu (`master`), mağaza sürümlerinde doğrulanmadı.

Karar bekleyen:

- `hk_provision`'ın bağlantı olayları (§7): ya bağlanır ve pencerenin sınırı o gün kararlaştırılır, ya dal testiyle birlikte silinir.
- Kartların yeni adla ilan vereceği ilk flaş: eski adla çekilmiş her fotoğraf ve not o günün adıdır.

Hiçbir fiziksel kapı açılmadı. `G0` kısmi kayıt olarak duruyor.
