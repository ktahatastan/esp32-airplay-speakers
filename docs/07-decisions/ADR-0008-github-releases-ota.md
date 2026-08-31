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

`.github/workflows/release.yml`: `verify` → `build` → `publish`. Derleme işi imzalama anahtarını **hiç görmez**; `CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=n` ile imzasız derlenir. Yalnız korumalı `release` ortamına bağlı `publish` işi anahtarı okur. Böylece CI'ya dokunan bir değişiklik anahtara ulaşamaz.

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
