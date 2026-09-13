---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-13
supersedes: ADR-0014
tags: [adr, provisioning, security, ble, softap, captive-portal, identity]
---

# ADR-0023: Kurulum PIN'siz olur — Security 1 (PoP yok), açık kurulum ağı, `PROV_` öneki

[[ADR-0014-srp6a-username|ADR-0014]]'ün yerine geçer: SRP6a kullanılmayınca kullanıcı adı diye bir şey kalmaz. [[ADR-0015-softap-captive-portal|ADR-0015]]'in **güvenlik** seçimini — WPA2 kurulum ağı, etiketteki cihaz başına parola, "kimlik bilgisi yoksa provisioning açılmaz" özelliği — ve [[ADR-0016-simultaneous-dual-transport|ADR-0016]]'nın o karardan aynen devraldığı "WPA2 kurulum ağı" ve "Security 2" cümlelerini aşar. Kalanlar olduğu gibi kalır: ADR-0016'nın taşıma kuralı (iki taşıma birlikte açılır, yöneticiyi BLE sürer, erişim noktasını ve portalı `hk_network` kaldırır) ve ADR-0015'in portal seçimi (`hk_portal`, `wifi_prov_mgr_configure_sta()`). [[ADR-0001-product-identity|ADR-0001]]'in kimlik cümlesi iki kurulum yüzeyi için `PROV_` önekini alır; AirPlay ve mDNS adları değişmez.

## Bağlam

Sahibin isteği, 2026-09-13, olduğu gibi:

> "Wifi Prov ve BLE prov aşamasından pin olayını kaldıralım wifi açık olsun evde kullanacağım sorun yok kolay olsun kurulumu."

Üç ayrı şey istiyor: uygulamanın sorduğu PIN'in — proof of possession'ın — kalkması, kurulum ağının parolasız olması, ve kurulumun kolay olması. Gerekçesi tek: cihaz evde duracak.

### Security 2 ne aldı, ne verdi

Bugünkü tasarımda PIN, cihaz başına bir sırdır. BLE'de o sır SRP6a parolasıdır (Security 2); uygulamasız yolda aynı parola kurulum ağının WPA2 anahtarıdır (ADR-0015). `hk_network.c` (`ef0e501`) yöneticiyi üç yerde `WIFI_PROV_SECURITY_2` ile başlatıyor (517-518 SoftAP ayağı, 592-594 çift taşımanın BLE ayağı, 644-646 tek taşıma BLE); `factory_cal`'da `prov_salt`/`prov_verif` yoksa provisioning'i açmayı reddediyor (529-537), `ap_pass` yoksa uygulamasız ayağı reddediyor (494-500, 579-585); çift taşıma penceresinin erişim noktasını `WIFI_AUTH_WPA2_PSK` ile kendisi kaldırıyor (439, 454). `provision_credentials.py` her kart için parola, salt/verifier ve `ap_pass` üretip `factory_cal.csv`'ye yazıyor; QR yükü `pop` ve `username` taşıyor.

Aldığı gerçekti: parola cihazda hiç durmuyordu (yalnız salt ve verifier), **istemci cihazı doğruluyordu** (ESP-IDF `provisioning.rst:92-94`'ün ikinci maddesi; SRP6a kanıtı iki yönlüdür), pasif de aktif de dinleyici oturuma giremiyordu, ve kurulum ağına yalnız etiketi bilen katılabiliyordu.

Verdiği de gerçekti, ve sahibin "kolay olsun"u tam bunlara dokunuyor:

- Her kart için ESP-IDF isteyen bir üretim aracı ve kartın flash'ına yazılan bir kimlik bilgisi; kimlik bilgisi yazılmamış bir kart kurulamıyordu — reddediyordu (`hk_network.c:529-537`). Bu bir güvenlik özelliğiydi ve aynı zamanda her yeni kartın önündeki bir adımdı.
- Sır taşıyan bir etiket ve QR (`label.txt`, `qr.txt`, `ap_pass.bin`, `chmod 600`): kaybolursa kurulum yolu yok; QR okunamazsa 12 karakterlik parola elle yazılıyor.
- WPA2 anahtarı `factory_cal`'da düz metin duruyordu — ADR-0015'in "bedeli sanıldığından küçük" dediği kayıp.
- Kullanıcı adı verifier'a girdiği için QR'sız yol bir kez kırıldı ve bir ADR'ye mal oldu (ADR-0014).
- Espressif'in "ESP SoftAP Prov" uygulaması SRP6a kanıtı **geçtikten sonra** AES-GCM katmanında düşüyordu (2026-09-03, `mbedtls_gcm_auth_decrypt : -18`). O katman Security 2'nindir; uygulamanın Security 1'de (AES-CTR) ne yaptığı hiç denenmedi ve "çalışır" denemez.

### 2026-09-05 kaydı olduğu gibi durur

BLE + QR kurulumu 2026-09-05'te iOS'ta, geliştirme kartında (ADR-0012), **Security 2 ile** uçtan uca çalıştı ([[../06-testing/devkit-bring-up|bring-up kaydı]]). Ürün kartı 2026-09-08'de salt/verifier ve `ap_pass`'ı yükledi, iki taşımayı birlikte açtı ([[../06-testing/product-board-bring-up|ürün kartı kaydı]]). Bu kayıtlar o gün ölçüleni anlatır ve yeniden yazılmaz; bu ADR'nin tasarımının kanıtı değildirler. Bu tasarımla hiçbir şey henüz bir telefondan denenmedi.

### ESP-IDF v5.5.1'den doğrulananlar (bu makinedeki ağaç)

1. **Security 2 PIN'siz çalışamaz.** `protocomm_set_security()` sürüm 2 için `NULL` parametreyi "Security params cannot be null" ile `ESP_ERR_INVALID_ARG` döndürerek reddeder (`components/protocomm/src/common/protocomm.c:328-331`). PIN'i kaldırmak Security 2'den çıkmak demektir; "Security 2, PoP'suz" diye bir kip yoktur.
2. **Security 1'de PoP isteğe bağlıdır ve cihaz bunu ilan eder.** `wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NULL, …)` `capabilities.no_pop = true` yapar (`components/wifi_provisioning/src/manager.c:1633-1651 (`no_pop = true` 1648. satırda)`) ve bunu şifresiz `proto-ver` JSON'unda `cap: [no_pop, wifi_scan]` olarak yayınlar (`manager.c:275-280`). Oturum anahtarı yalnız X25519'dan türer; `SHA256(pop)` yalnız PoP varsa anahtara XOR'lanır (`components/protocomm/src/security/security1.c:313-326`). İstemci doğrulama adımı yalnız aynı anahtar değişiminin tamamlandığını kanıtlar. Belge kipi adıyla anıyor: "No Auth (Null PoP) — Shared key derived through key exchange only" (`docs/en/api-reference/provisioning/provisioning.rst:141-147`). Sonuç: pasif dinleyiciye karşı **gizlilik var**, iki yönde de **kimlik doğrulama yok**.
3. **Security 0 düz metindir.** `protocomm_security0` yalnız `.ver` ve `.security_req_handler` tanımlar, `encrypt`/`decrypt` yoktur (`security0.c:104-107`); `protocomm_req_handle()` `decrypt` yokken isteği olduğu gibi işleyiciye verir ("No encryption", `protocomm.c:246-251`). BLE bağlantı şifrelemesi bu projede zorlanmıyor: `CONFIG_WIFI_PROV_BLE_FORCE_ENCRYPTION` ESP-IDF'te varsayılan kapalı (`components/wifi_provisioning/Kconfig:33-36`) ve hiçbir izlenen `sdkconfig.*` fragmanı açmıyor. Yani Security 0, ev Wi-Fi parolasını eşleşmemiş bir GATT bağlantısında açık gönderirdi.
4. **Boş anahtar açık erişim noktası demektir.** `scheme_softap.c:44-46` boş parolada `WIFI_AUTH_OPEN`, doluda `WIFI_AUTH_WPA_WPA2_PSK` kurar; `service_key` `NULL` geçilince parola boş kalır. Çift taşıma penceresinde erişim noktasını `scheme_softap` değil `hk_network.c`'nin `start_setup_ap()`'i kaldırdığından (ADR-0016), açık ağ orada `authmode` değişikliğidir. Soft-AP kipinde OWE desteklenmez (`components/esp_wifi/include/esp_wifi_types_generic.h:530`): açık ağ, şifresiz 802.11 çerçevesi demektir, "Enhanced Open" seçeneği yoktur.
5. **Referans istemci** `tools/esp_prov/esp_prov.py`: `--sec_ver` verilmezse `no_sec` varsa 0, yoksa 1 seçer ve `==== Security Scheme: 1 ====` basar; Security 1'de `no_pop` ilan edilmişse PoP sormaz ve verilen `--pop`'u yok sayar (`esp_prov.py:446-457`).

### Espressif'in telefon uygulamaları (kaynaktan; mağaza ikilileri doğrulanmadı)

Kütüphane ve örnek uygulama kaynakları GitHub `master`'dan okundu (iOS `2ecd878`, Android `512ab02`). Mağazadaki "ESP BLE Provisioning" ve "ESP SoftAP Prov" ikilileri bu sürümler olmayabilir; aşağıdakiler donanımda **doğrulanmadı**.

- **Güvenlik düzeyini cihaz söyler.** İki kütüphane de düzeyi cihazın `proto-ver`'indeki `sec_ver`'den alır; `no_pop` yeteneği varsa oturumu boş PoP ile **sormadan** açar (iOS `ESPDevice.swift:568-570`; Android `ESPDevice.java:712-714`, `Security1.java:151-156`). Yani Security 1 + `no_pop`, kullanıcıya hiçbir şey yazdırmadan biten kurulumdur.
- **Security 0'ı stok uygulamalar reddediyor.** İki örnek uygulama da varsayılan "Secured" ayarıyla `sec_ver 0` ilan eden cihazı "security mismatch" diye geri çeviriyor; ayar bir kez elle "Unsecured"a çevrilmeden kurulum başlamıyor, QR bu ayarı ezemiyor (Android `BLEProvisionLanding.kt:196-204`, `AddDeviceActivity.kt:1062-1064`; iOS `Utility.swift:80`, `ScannerViewController.swift:142`, `ESPDevice.swift:553-556`). Security 0, Security 1/`no_pop`'tan **bir adım fazla** demektir.
- **Liste `PROV_` ile süzülüyor.** İki örnek uygulama da BLE ve SoftAP listesini varsayılan `PROV_` önekiyle süzüyor (Android `app/build.gradle:35`, `BLEProvisionLanding.kt:110-112`; iOS `Utility.swift:75`, `BLELandingViewController.swift:103`). `Merzarkabul-XXXX` adlı bir cihaz, kullanıcı uygulamanın ayarından öneki değiştirmeden listede **görünmez**; QR yolu önekten bağımsız. Bugüne kadar bunu QR gizliyordu.
- **QR alanları.** `security` isteğe bağlıdır ve yoksa 2 varsayılır, ama oturumda cihazın `sec_ver`'i onu ezer; `password` (SoftAP parolası) isteğe bağlı bir anahtardır (Android README QR tablosu; iOS `ESPScanResult.swift:49, 58`). ADR-0015'in "QR biçiminde kurulum ağı parolası için alan yok" cümlesi bugünkü kütüphaneler için yanlıştır; kayda geçti, ADR-0015 yeniden yazılmadı.

## Seçenekler

| Seçenek | Değerlendirme |
|---|---|
| **A. Security 0, iki taşımada** | PIN'siz ama düz metin: ev parolası BLE'de eşleşmemiş GATT üzerinden, SoftAP'ta açık ağda açık gider (yukarıda 3). Üstelik stok uygulamalar `sec_ver 0` cihazı "Unsecured" ayarı çevrilmeden reddediyor — kullanıcıya B'den **fazla** adım. Kazancı yok, bedeli var. **Reddedildi.** |
| **B. Security 1, PoP `NULL`, açık kurulum ağı** | PIN yok. Uygulama yolu pasif dinleyiciye karşı şifreli; kütüphaneler `no_pop`'ta sormadan geçiyor; `esp_prov.py` kendiliğinden 1 seçiyor. Uygulamasız portal yolu açık ağda düz metin — sahibin "wifi açık olsun"unun doğrudan sonucu. **Seçildi.** |
| **C. Security 2'yi Kconfig seçimi olarak tutmak** | Ölçülmüş yolu yaşatır gibi görünür, ama CI'ın derlemediği, tezgâhın açmadığı ikinci bir güvenlik tasarımıdır: sessizce bozulur ve bozulduğu ancak biri o seçeneğe döndüğünde anlaşılır. Üretim aracı sır üretmeye, etiket parola basmaya, `write_profile.py` üç `.bin` dosyasını şart koşmaya devam eder — istek sadelikti. Kod `git`'te duruyor (`ef0e501` ve öncesi); geri dönüş bir ADR'dir, bir Kconfig anahtarı değil. **Reddedildi.** |
| D. WPA2 kalsın, anahtar QR'ın `password` alanında gitsin | Portal yolunu şifreli tutar; ama sahip açık ağ istedi, uygulamasız yolda anahtar yine elle yazılır, ve Android 10+'da uygulamanın QR'dan kendiliğinden katılması doğrulanmadı. Portal yolu için tek makul geri dönüş budur ve kaydı buradadır. **Reddedildi.** |

## Karar

1. **Her üç `wifi_prov_mgr_start_provisioning()` çağrısı `WIFI_PROV_SECURITY_1` ve `NULL` PoP ile yapılır**, iki taşımada da. Cihaz `sec_ver 1`, `cap [no_pop, wifi_scan]` ilan eder; AES-CTR anahtarı yalnız X25519'dan türer. **Security 2 ve SRP6a salt/verifier firmware'den kaldırılır, Kconfig arkasında tutulmaz:** `s_have_security2` / `s_have_ap_password` kapıları ve yükleyicileri gider; açılış raporunun onları anan satırı bugün doğru olanı söyler. Security 0 seçilmez (yukarıda A).
2. **Kurulum ağı açıktır.** `start_setup_ap()` `WIFI_AUTH_OPEN`, anahtar yok; `start_softap_only()` `service_key = NULL` (`scheme_softap.c` bunu `WIFI_AUTH_OPEN`'a çevirir). `hk_portal` düz formu sunmaya devam eder ve sayfaya görünür bir cümle gelir: kurulum ağı açıktır ve form şifresiz gider; oraya yalnız ev Wi-Fi parolası, yalnız hoparlör kurulumdayken yazılır. `test_portal.c` geçmeye devam eder; sayfa metnini üretiyorsa cümlenin sayfada olduğunu da denetler.
3. **Kurulum adları `PROV_` önekini alır.** BLE yayın adı `PROV_Merzarkabul-XXXX`, kurulum ağı SSID'si `PROV_Merzarkabul-XXXX` — iki yüzey için tek ad. Gerekçe yukarıdaki liste süzgecidir: stok Espressif uygulamaları cihazı **QR'sız ve önek ayarını değiştirmeden** listeler. `hk_identity.h`/`.c` ve `test_identity.c` buna göre değişir; ADR-0001'in kimlik cümlesi ve [[../controls-and-provisioning-plan|planın]] kimlik tablosu izler. AirPlay adı ve mDNS adı değişmez.
4. **QR sırsızdır ve isteğe bağlıdır.** Yük `{"ver":"v1","name":"PROV_Merzarkabul-XXXX","transport":"ble","security":1}`; SoftAP eşi `"transport":"softap"`, `password` alanı yok. `pop` ve `username` yok. `security` alanı zorunlu değil, dürüst olanıdır: kütüphaneler eksik alanı 2 varsayar, ama düzeyi cihazdan alır. `firmware/tools/provision_credentials.py` adını korur (CI ve belgeler ona bakıyor; adı tarihseldir ve docstring'i bunu söyler), ama salt/verifier/`ap_pass` üretmez ve hiçbir parola basmaz: cihaz dizinine `factory_cal.csv` (`cal` ad alanı, yalnız `schema` `u32` `1`), iki yükü taşıyan `qr.txt` ve yalnız cihaz adı ile QR metnini taşıyan `label.txt` yazar; `srp6a` artık içe aktarılmaz, `IDF_PATH` yalnız `--image` için gerekir. `write_profile.py`'nin dizin sözleşmesi: `cal` ad alanlı ve `schema` satırlı `factory_cal.csv` zorunludur; üç kimlik-bilgisi `.bin` dosyası isteğe bağlıdır — hâlâ taşıyan eski bir dizin eskisi gibi birleşir ve `--dump` anahtarlarını listeler.
5. **`factory_cal`'da `prov_salt`/`prov_verif`/`ap_pass` taşıyan kartlar yeniden flaşlanmaz.** Ürün kartı `932C` üç anahtarı da taşıyor (2026-09-08 kaydı); geliştirme kartı `06C4` salt ve verifier'ı (2026-09-05 kaydı, `ap_pass` ADR-0015'ten önce yoktu). Firmware onları **hiç okumaz**; ölü veridir, bölüm salt-okunur açıldığı için orada zararsız durur. Yeni bir `factory_cal` imajı yazılırsa `write_profile.py` satırları korur.

## Sonuçlar

### Kullanıcının gördüğü (kaynaktan; telefonda denenmedi)

- **ESP BLE Provisioning (iOS/Android):** Listede `PROV_Merzarkabul-XXXX` görünür — önek ayarına dokunmadan; istenirse QR taranır. Uygulama bağlanır, `proto-ver` okur, **PIN sormaz**, ağ listesini gösterir; ev ağı seçilir, ev parolası yazılır; "provisioning succeeded" ile pencere kapanır ve BLE yığını serbest bırakılır.
- **ESP SoftAP Prov:** Yalnız BLE serbest bırakıldıktan sonraki yeniden açılışta (ADR-0016: çift taşıma penceresinde protocomm portalın sunucusuna bağlı değildir). Telefon açık `PROV_Merzarkabul-XXXX`'e katılır, uygulama PIN sormaz. Bu uygulamanın Security 1'de çalışıp çalışmadığı bilinmiyor.
- **Uygulamasız:** Telefonun Wi-Fi listesinde `PROV_Merzarkabul-XXXX`, **kilit simgesiz**. Katılınca işletim sistemi `http://192.168.4.1/` sayfasını açar; sayfa ağın açık olduğunu ve formun şifresiz gittiğini söyler; ağ seçilir, ev parolası yazılır, "Katıl".
- **`esp_prov.py --transport ble`:** `==== Security Scheme: 1 ====`, PoP sorusu yok.
- **Etiket / QR:** Cihaz adı ve iki QR metni. Sır yok; kaybolursa kaybedilen bir şey yok, çünkü ad zaten cihazın ilanında.

### Kabul edilen riskler, düz sözlerle

Sahibin sözü "evde kullanacağım, sorun yok". Bu ADR onun neyi kabul ettiğini adıyla yazar:

1. **Kurulum penceresi açıkken menzildeki herkes hoparlörü istediği bir ağa kurabilir.** İki taşımada da sahiplik kanıtı yoktur. Bunu yapan kişi ev Wi-Fi parolasını öğrenmez; sahibi 5 saniyelik basışla ağı unutturup yeniden kurar.
2. **Pasif bir dinleyici hiçbir şey öğrenmez.** Uygulama yolunda oturum X25519 + AES-CTR ile şifrelidir; havadan dinleyen biri ev parolasını okuyamaz.
3. **Pencere sırasında menzildeki aktif bir saldırgan oturumu araya girerek ele geçirebilir** (man-in-the-middle): hiçbir şey sahipliği kanıtlamadığı ve istemci cihazı doğrulamadığı için, sahte bir `PROV_Merzarkabul-XXXX` yayını sahibin telefonundan ev parolasını alabilir. Security 2 bunu engelliyordu; Security 1/PoP yok engellemez.
4. **Pencere ne zaman açık:** ilk açılışta ve 5 sn'lik ağ sıfırlamasından sonra, kurulana kadar; kurulmuş bir cihazda yalnız butonla — çalarken ilk basış sorar, 5 sn içindeki ikincisi açar — ve **10 dakika** ([[../controls-and-provisioning-plan|plan]]). Kimse pencereyi uzaktan açamaz; aşağıdaki "üç hata" dalı için de bkz. sonraki bölüm.
5. **Uygulamasız portal yolunda ev parolası açık ağda düz HTTP ile gider.** Pencere sırasında menzildeki bir dinleyici formu gönderildiği saniyelerde okuyabilir; aynı adlı sahte bir ağ onu doğrudan toplar. **BLE/uygulama yolunda bu maruziyet yoktur** (madde 2). Sayfa bunu kullanıcıya söyler.
6. **Kabul evin içindir.** Cihazı ortak bir alanda, ofiste ya da konuk ağında kurmak bu kabulün dışındadır; kimin menzilde olduğu bilinmeyen bir yerde pencere açmak, yukarıdaki 1, 3 ve 5'i o kişilere açmak demektir.

### Kayda geçen bir gerçek: "üç başarısız katılım" dalı bugün erişilemez

`hk_provision.c`'nin `HK_PROV_EV_CONNECT_FAIL` işleyicisi, `HK_PROV_MAX_FAILURES` (3) ardışık başarısızlıktan sonra provisioning'i **butonsuz ve sınırsız** açan bir dal taşıyor (`ef0e501`: `hk_provision.c:150-154`). Bu dal host testinde çalışıyor, cihazda çalışmıyor: `hk_main.c` politikaya yalnız `HK_PROV_EV_TICK` (1237), `HK_PROV_EV_BUTTON_SHORT` (841), `HK_PROV_EV_NETWORK_RESET` (858) ve `HK_PROV_EV_FACTORY_RESET` (862) veriyor; `HK_PROV_EV_CONNECT_OK`, `CONNECT_FAIL` ve `CREDENTIALS`'ı modülün dışında çağıran yok. `hk_network.c` katılma denemesini `HK_NET_RETRY_LIMIT` (5) sonra `error = true` ile bırakıyor (77, 294-304) ve `hk_main`'in `on_network_status()`'u bunu politikaya çevirmiyor. Dolayısıyla PoP kalktığında "yönlendiriciyi kapatıp cihazı ele geçirme yolu" **açılmıyor** — ama kullanıcı için de "ağ değişince kendiliğinden kurulum penceresi" **yok**: yönlendirici değişirse yol butondur. Bu ADR politikayı değiştirmez, yalnız kaydeder; dalı bağlamak ya da silmek ayrı bir karardır ve [[../01-planning/risk-register|risk kaydı]] ile `TODO.md`'de durur.

### Kayıt

ADR-0014 tümüyle aşıldı (`superseded`). ADR-0015 ve ADR-0016'ya bu ADR'yi gösteren tarihli notlar eklendi; ADR-0016 `accepted` kalır, çünkü yürürlükteki taşıma kuralı ondadır; ADR-0015'in durumu ADR-0016'nın bıraktığı gibidir. ADR-0001 iki kurulum yüzeyinin adını aldı. `AGENTS.md`'nin kilitli kararlarına provisioning satırı eklendi ve kimlik bilgisi paragrafı artık var olmayan bir sırrı anmıyor; `docs/credentials/README.md` düzeltildi. Plan, PRD, risk kaydı, firmware planı, `TODO.md`, beceri ve ajan tanımları ve `scripts/check_docs.py`'nin `provisioning-pop` sapma kuralı aynı işin parçasıdır; günlük `docs/08-development-log/2026-09-13-pinless-provisioning.md`. Tarihli kayıtlar (2026-08-31, 09-05, 09-08 günlükleri; iki bring-up kaydı) yeniden yazılmadı.

Değişmeyenler: Wi-Fi parolası hiçbir log'a düşmez (`check_no_credential_logs.py`, CI); `docs/credentials/` yasağı aynen — kullanıcının ev parolası kurulum açıkken de sırdır; ses izni bu karardan bağımsızdır ve `schema` + `profile` + geçerli karar ister (ADR-0022).

## Doğrulama / geri dönüş

**Bu ADR'nin hiçbir maddesi bir telefonda ya da kartta denenmedi.** Kayıttaki her provisioning PASS'i Security 2 ve WPA2 ile ölçüldü ve bu tasarımı anlatmaz. Bir PASS ancak operatör kaydıyla yazılır.

Otomatik kapılar (birleşmeden önce): ürün, geliştirme kartı ve tezgâh derlemeleri; host takımı (`test_provision`, `test_identity` yeni adlarla, `test_portal` yalnız form ayrıştırıcısı — sayfa metnini üretmediği için cümleyi denetlemez, cümle `hk_portal.c`'de sayfaya derlenir); `test_provision_credentials.py` ve `test_write_profile.py`; `check_no_credential_logs.py`; `scripts/check_docs.py` sıfır hata. Bunlar kodun kayıtla tutarlı olduğunu kanıtlar, kurulumun çalıştığını değil.

Operatör ölçümleri (`TODO.md`; her biri kaydedilmeden PASS yazılamaz):

- Geliştirme kartında ve ürün kartında `esp_prov.py --transport ble --verbose`: `proto-ver`'de `sec_ver 1` ve `cap [no_pop, wifi_scan]`, `Security Scheme: 1`, PoP sorusu yok.
- İki uygulamanın da (iOS ve Android ESP BLE Provisioning) `PROV_` cihazı **QR'sız** listelemesi; kurulumun PIN sorulmadan bitmesi.
- Telefonun ağ listesinde açık SSID'nin kilitsiz görünmesi; portal sayfasının açılması ve cümleyi göstermesi.

Geri dönüş: bu ADR'yi aşan bir ADR ile D seçeneği (WPA2 + QR `password`) ya da Security 2; kod `git`'te (`ef0e501` ve öncesi). Bir Kconfig anahtarı ya da "sessiz sertleştirme" ile değil.
