---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [adr, ota, github-releases, security]
---

# ADR-0008: Otomatik OTA ve GitHub Releases dağıtımı

## Karar

Harman Kardom firmware sürümleri SemVer Git etiketleriyle oluşturulacak ve GitHub Actions tarafından test/build sonrası GitHub Release asset'i olarak yayımlanacaktır. Cihazlar uygun güç, sıcaklık, ağ ve idle-audio koşullarında daha yeni stable sürümü HTTPS üzerinden otomatik olarak alacaktır.

OTA; çift uygulama slotu, imzalı image, ilk-açılış sağlık kontrolü, rollback ve USB/UART recovery içerir. Dört cihazın tamamına doğrudan aynı anda dağıtım yapılmaz; bir canary cihazdan sonra stable terfisi uygulanır.

2026-08-31'de F7 kapsamında beş alt karar netleşti. Hepsi bu ADR'nin ilk sürümünün açık bıraktığı yerleri doldurur.

### 1. İstemci: proje manifest istemcisi. `esp_ghota` reddedildi

ADR'nin ilk sürümü `esp_ghota`'yı aday sayıp "spike geçmezse ESP HTTPS OTA ve proje manifest istemcisi" demişti. Değerlendirme yapıldı ve aday elendi:

- Depo fiilen bakımsız. Son commit `335b330`, 2024-02-17 — bugünden 2,5 yıl önce. Üç açık issue var, en eskisi 2023-08-11, ve **üçüne de hiç yanıt verilmemiş**.
- CI matrisi hiçbir zaman ESP-IDF v5.2'den yenisini denemedi (`espidf: [v4.4.6, v5.0.5, v5.1.2, v5.2]`). Bu proje v5.5.1'de.
- README'nin kendi kurulum komutu çalışmıyor: `idf.py add-dependency Fishwaldo/ghota^1.0.0` diyor ama kayıtta `fishwaldo/ghota` diye bir bileşen yok, olan `fishwaldo/esp_ghota` ise yalnız 0.0.1 yayımlanmış. Açık issue #10 (2024-07-31, yanıtsız) tam olarak bunu bildiriyor.
- Asıl mimari çelişki: donanım eşleşmesini `fnmatch(config.filenamematch, asset.name)` ile, yani **dosya adından** yapıyor ve manifest kavramı yok. Bu ADR'nin kuralı bunun tersi — istemci hedefi ve donanımı manifest'ten doğrular. G6 kabul satırlarının dördü, kütüphanenin çekirdeği yeniden yazılmadan sağlanamıyor.

Yerine `esp_https_ota` üstünde küçük bir istemci yazıldı: `firmware/components/hk_ota/hk_ota.c` (saf C, ana makinede test edilir) ve `firmware/components/hk_ota/hk_ota_client.c` (ESP-IDF katmanı).

### 2. Manifest, ikilinin kendisinden üretilir

`firmware/tools/make_manifest.py` `product`, `version` ve `secure_version` alanlarını komut satırından almaz; **imzalanmış ikilinin uygulama tanımlayıcısından okur**. Böylece manifest ile görüntünün çelişmesi ifade edilemez hale gelir. Etiket ise doğrulanır: `v0.3.0` etiketiyle 0.2.0 derlemesi yayımlanamaz.

Bu gereklidir çünkü ESP-IDF v5.5.1 kendisi bu kontrolü yapmaz. `esp_https_ota_get_img_desc` yalnızca `magic_word` denetler (`esp_https_ota.c:589-594`) ve `project_name` OTA yolunda **hiçbir yerde** karşılaştırılmaz. Yani başka bir projenin doğru derlenmiş ESP32-S3 görüntüsü, IDF'in tüm denetimlerinden geçer. `hk_ota_image_check()` bunu yakalayan tek yerdir.

### 3. İmzalama: `CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`, hiçbir eFuse yakılmadan

Sürüm profili `firmware/sdkconfig.release` bu seçeneği açar. ESP32-S3'te:

- **Hiçbir eFuse yakılmaz** ve karar geri alınabilir: `sdkconfig` düzeltilip USB'den yeniden flash'lanır.
- Kapsam dürüstçe sınırlıdır: imza **yalnız OTA anında** doğrulanır, açılışta değil. Açılışta doğrulama `SECURE_SIGNED_ON_BOOT_NO_SECURE_BOOT`'a, o da `SECURE_BOOT_V1_SUPPORTED` üzerinden `SOC_SECURE_BOOT_V1`'e bağlıdır; ESP32-S3'te bu yetenek yoktur (`soc_caps.h` yalnız `SOC_SECURE_BOOT_V2_RSA` tanımlar). Yani bu ayar **uzaktan sahte güncellemeye karşı korur, fiziksel erişime karşı korumaz**. Dört cihaz kullanıcının evinde olduğu için kabul edilen takas budur.
- Güven çıpası çalışan uygulamanın kendi imza bloğudur. Bu yüzden `sdkconfig.release` ile derlenen bir görüntü **imzalanmadan cihaza yazılamaz**: ESP-IDF açılışta `esp_efuse_startup.c:103` → `esp_secure_boot_init_checks()` → `check_signature_on_update_check()` yolunu işletir ve imzasız uygulamada `abort()` eder. Bu nedenle imzalama ayarı `sdkconfig.defaults` içinde **değildir**; düz bir `idf.py flash` geliştirme yapısı boot döngüsüne girerdi.
- Şema RSA-3072'dir; ESP32-S3'te başka seçenek yoktur.

Anahtar **çevrimdışı, bir kez** üretilir ve CI'da asla üretilmez:

```bash
idf.py secure-generate-signing-key --version 2 --scheme rsa3072 hk_signing_key.pem
```

Kaybı felaket değildir: dört cihaz USB'den yeniden flash'lanır. Bu, dört prototip için doğru takastır; bir ürün için olmazdı.

### 4. Geri alma koruması (anti-rollback) bilerek KAPALI

`CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` bir yazılım ayarı değildir. Bootloader geçerli **her açılışta** `secure_version` alanını eFuse'a işler (`bootloader_utility.c:453-457`); `esp_ota_mark_app_valid_cancel_rollback` de yakar (`esp_ota_ops.c:918-920`). ESP32-S3'te alan 16 bitlik unary, yani ömür boyu 16 artış hakkı vardır ve geri alınamaz. Bir kez `secure_version=12` yayımlamak o yonganın 12 hakkını kalıcı harcar **ve o yongayı daha önce derlenmiş her görüntü için kalıcı olarak açılamaz yapar**.

Manifest'teki `secure_version` alanı ayrılmış olarak durur ve `hk_manifest` içinde yalnız **yazılımsal** karşılaştırılır. Bir gün açılması düşünülürse önce `CONFIG_BOOTLOADER_EFUSE_SECURE_VERSION_EMULATE=y` ile kanıtlanmalıdır.

### 5. Sertifika paketi: ISRG Root YR eklendi

GitHub iki ayrı hiyerarşi sunar. `api.github.com` Sectigo'ya, sürüm varlık sunucusu `release-assets.githubusercontent.com` ise Let's Encrypt'e zincirlenir. ESP-IDF v5.5.1 paketindeki 150 sertifika arasında `ISRG Root X1` ve `X2` var, **`ISRG Root YR` ve `YR1` yok**. Varlık sunucusu bugün yalnızca GitHub zincirin içinde Root YR'nin X1 tarafından çapraz imzalanmış kopyasını gönderdiği için doğrulanıyor.

2026-08-31'de canlı zincirle ölçüldü: çapraz imza zincirden çıkarıldığında doğrulama `error 20, unable to get local issuer certificate` ile başarısız oluyor. GitHub o kopyayı kırptığı gün — ki bunu duyurmak zorunda değil — sahadaki her hoparlör OTA'yı sessizce kaybeder. Kök `firmware/certs/isrg-root-yr.pem` olarak eklendi (606 bayt) ve üretilen pakette doğrulandı.

### 6. Hat iki işe bölündü

`.github/workflows/release.yml`: `verify` → `build` → `publish`. Derleme işi imzalama anahtarını **hiç görmez**; `CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=n` ile imzasız derlenir. Anahtarı yalnız `publish` işi okur.

**Bu bölümün ilk hâli fazla iddialıydı, düzeltildi ve sonra gerçekten kuruldu.** İlk yazımda "korumalı `release` ortamına bağlı" demiştim; ölçüldüğünde ortam mevcut değildi ve oluşturacak yetki yoktu. Depo 2026-08-31'de `ktahatastan`'a taşınıp **private** yapıldıktan sonra ortam oluşturuldu ve `HK_SIGNING_KEY` **ortam** secret'ı olarak saklandı. Depo secret'ı sayısı **0**: yani `build` işi anahtarı gerçekten okuyamıyor ve bölünme artık bir tasarım niyeti değil, doğrulanmış bir özellik.

**Zorunlu inceleyici de eklendi** (2026-09-02, koruma kuralı sayısı 1). Private'ken API `HTTP 422` ile reddediyordu; koruma kuralları public depolarda ücretsiz. Bir etiket push'u artık onay bekliyor.

Bunun etrafından `HK_SIGNING_KEY`'i **depo** secret'ı yaparak dolaşmak yasaktır. Hattı çalıştırırdı, ama anahtarı `build` dahil her işe okutarak — yani bölmenin tek sebebini ortadan kaldırarak. Doğrusu `release` ortamına bağlı **ortam** secret'ıdır.

Ayrıca `publish` işi iki şeyi imzalamadan önce denetler: anahtarın açık yarısının parmak izi `docs/credentials/burned-keys.txt` içindeyse reddeder — liste konuma değil parmak izine bakar, çünkü bir anahtar dosyasını silmek onu ifşa edilmemiş yapmaz; ve `firmware/certs/hk-signing-key.pub.bin` ile sabitlenmiş açık anahtarla eşleşmiyorsa reddeder. İkincisi olmadan, geçerli ama **yanlış** bir RSA-3072 anahtarı her denetimden geçer ve dört hoparlörün de sessizce reddedeceği bir sürüm yayımlanırdı; kurtarma yolu dört cihazı USB'den yeniden yazmaktır.

### 7. Dağıtım: depo public

2026-09-02'de ölçüldü ve karara bağlandı. Depo private'ken sürüm varlıkları kimlik doğrulamasız erişilemiyordu:

| Hedef | Tokensiz sonuç |
|---|---|
| Herhangi bir public depo, `releases/latest` | `HTTP 200` |
| Bu depo, private iken | `HTTP 404` |
| Aynısı API üzerinden | `HTTP 404` |

Bu ADR varlıkların düz HTTPS ile çekilmesini varsayıyor ve [[../03-firmware/ota-and-release-plan|OTA planı]] cihazların token taşımasını yasaklıyor. İkisi birlikte private bir depodan OTA'yı imkânsız kılıyordu.

Depo yeniden **public** yapıldı. Private olmasının sebebi imzalama anahtarının depoda olmasıydı; o sebep ortadan kalkmıştı — anahtar `release` ortam secret'ında ve depoda hiçbir özel anahtar yok. Public yapmak üç şeyi birden çözdü: varlıklar `HTTP 200` döndü, ortam koruma kuralları ücretsiz hâle geldiği için zorunlu inceleyici eklenebildi, ve gizlilik dağıtımın işi olmadığı için kaybedilen bir şey olmadı — özgünlüğü imza sağlıyor.

Yanmış anahtarın commit'i geçmişte duruyor ve yine herkese açık. Bu kabul edildi: anahtar zaten ifşa olmuştu, sonradan private yapmak onu geri çağırmamıştı ve geçmişi yeniden yazmak da çağırmaz. Bir daha imzalayamaması konumla değil parmak iziyle sağlanıyor.

**Kanal adresleri.** `releases/latest` prerelease'i atladığı için yalnız stable sunuyordu; canary'ye ayarlı bir cihaz onu çeker, takip etmediği kanalı görür ve doğru şekilde reddederdi — güvenli ama işe yaramaz.

Her kanalın kendi **sabit etiketli işaretçi sürümü** var: `channel-stable` ve `channel-canary`. Cihazın adresi hiç değişmiyor; yayımlama, o etiketin **ne taşıdığını** değiştiriyor. Adres kanal ayarından `hk_ota_manifest_url()` ile kuruluyor ve kırpılma sessizce geçmiyor, hata olarak dönüyor — kısalmış bir adres, cihazın var olmayan bir yeri nazikçe kontrol etmesi demek.

**Yeni sürüm aday olarak doğar.** `release.yml` sürümü prerelease yayımlıyor, manifest'i `canary` kanalıyla üretiyor ve `channel-canary` işaretçisini taşıyor. Diğer üç hoparlöre hiçbir şey ulaşmıyor. Varsayılanın tersi olması, canary adımını pratikte isteğe bağlı yapardı.

**Terfi yeniden derlemez.** `promote.yml` yayımlanmış ikiliyi indiriyor, manifest'i ondan `stable` kanalıyla yeniden üretiyor ve yalnız manifest'i yayımlıyor. Yeniden derleme farklı bir imaj üretirdi — tek başına derleme zamanı bile baytları değiştirir — ve canary hoparlörün gerçekten çalıştırdığı şey kimseye ulaşmazdı. Ölçüldü: canary ve stable manifest'leri arasında **yalnız `channel` alanı** farklı; `sha256`, `size` ve `asset` aynı.

## Gerekçe

- Kullanıcı müdahalesi olmadan güvenilir sürüm dağıtımı.
- Release binary, manifest, checksum ve release notes için tek izlenebilir kaynak.
- Enerji kesintisi veya bozuk sürümde dört hoparlörü aynı anda kullanılmaz hale getirmeme.
- Firmware signing anahtarını cihaz, kaynak kod veya derleme işi içinde dağıtmama.
- Geri alınamaz donanım kararlarından (eFuse) dört prototip uğruna kaçınma.

## Sonuçlar

- G6, enerji kesintisi ve rollback testleriyle zorunlu kapıdır ve **hiçbiri henüz çalıştırılmadı**; donanım elde değil.
- OTA partition boyutu firmware özellik bütçesini etkiler. Sürüm profili görüntüsü 0x120000 bayt, 0x6E0000 yuvanın %16'sı.
- Bootloader/partition table V1'de uzaktan güncellenmez.
- İmza yalnız OTA anında doğrulanır; fiziksel erişime karşı koruma yoktur ve bu bilinçli kabuldür.
- `ISRG Root YR` sertifikası 2032-09-02'de dolar; o tarihten önce yenilenmelidir.
- Ayrıntılı plan: [[../03-firmware/ota-and-release-plan]].
