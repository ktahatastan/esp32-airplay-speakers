---
title: F7 yazma yarısı — OTA istemcisi, imzalama ve yayın hattı
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, f7, ota, security, releases]
---

# 2026-08-31 — OTA istemcisi, imzalama ve yayın hattı

## Amaç

`F7`'nin donanım gerektirmeyen tamamı: yayımlanan bir sürümün bu cihaza ait olup olmadığına karar veren mantık, güncellemenin ne zaman başlayabileceği, inen görüntünün doğrulanması, imzalama yapılandırması ve etiketten yayına giden GitHub Actions hattı.

## Dört karar, dördü de ölçümle

### 1. `esp_ghota` reddedildi

ADR-0008 bu kütüphaneyi aday saymış, "spike geçmezse kendi istemcimiz" demişti. Spike yapıldı, aday elendi. Belirleyici olan bakımsızlık değil, mimari çelişkiydi: `esp_ghota` donanım eşleşmesini `fnmatch(config.filenamematch, asset.name)` ile, yani **dosya adından** yapıyor ve manifest kavramı yok. ADR-0008'in kuralı bunun tam tersi. G6 kabul matrisinin dört satırı kütüphanenin çekirdeği yeniden yazılmadan sağlanamıyordu.

Yan bulgular: son commit 2024-02-17 (2,5 yıl), üç açık issue'nun üçü de yanıtsız, CI hiç v5.2'den yenisini denememiş (biz v5.5.1'deyiz) ve README'nin kendi kurulum komutu çalışmıyor — `fishwaldo/ghota` diye bir bileşen kayıtta yok.

### 2. Anti-rollback açılmayacak

`CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK` bir yazılım ayarı sanılıyor; değil. Bootloader geçerli **her açılışta** `secure_version`'ı eFuse'a işliyor (`bootloader_utility.c:453-457`). ESP32-S3'te alan 16 bitlik unary: ömür boyu 16 artış, geri dönüş yok. Bir kez `secure_version=12` yayımlamak o yonganın 12 hakkını kalıcı harcar **ve o yongayı daha önce derlenmiş her görüntü için kalıcı olarak açılamaz yapar**.

Dört prototip için bu takas kötü. `secure_version` manifest'te duruyor ve yalnız yazılımsal karşılaştırılıyor.

### 3. İmzalama açık, ama `sdkconfig.defaults` içinde değil

`CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT` ESP32-S3'te hiçbir eFuse yakmıyor ve geri alınabiliyor. Kapsamı dürüstçe sınırlı: imza **yalnız OTA anında** doğrulanıyor, açılışta değil — açılış doğrulaması `SOC_SECURE_BOOT_V1` gerektiriyor ve S3'te o yetenek yok. Yani uzaktan sahte güncellemeye karşı koruyor, fiziksel erişime karşı korumuyor.

Bu ayarı varsayılan derlemeye koymadım ve sebebi kaynakta duruyor: açıkken ESP-IDF açılışta `esp_efuse_startup.c:103` → `esp_secure_boot_init_checks()` → `check_signature_on_update_check()` yolunu işletiyor ve **imzasız uygulamada `abort()` ediyor**. Düz bir `idf.py flash` ile derlenen geliştirme yapısı boot döngüsüne girerdi. Ayrı bir `sdkconfig.release` profili yaptım; onu yalnız imzalama adımının kesin çalıştığı yayın işi kullanıyor.

Anahtarsız derlemenin çalıştığını ölçtüm — ESP-IDF'in kendisi şunu basıyor:

```
App built but not signed. Sign app before flashing
```

Bu, hattı iki işe bölmeyi mümkün kılan şey: derleme işi anahtarı hiç görmüyor.

### 4. Sertifika paketinde tarihli bir mayın vardı

GitHub iki ayrı hiyerarşi sunuyor. `api.github.com` Sectigo'ya zincirleniyor ve o kök pakette var. Sürüm varlık sunucusu `release-assets.githubusercontent.com` ise Let's Encrypt'e zincirleniyor ve zincir `leaf → YR1 → ISRG Root YR`. ESP-IDF v5.5.1 paketindeki 150 sertifikayı tek tek ayrıştırdım: `ISRG Root X1` ve `X2` var, **`Root YR` ve `YR1` yok**.

Bugün çalışıyor olmasının tek sebebi GitHub'ın zincirin içinde Root YR'nin X1 tarafından çapraz imzalanmış kopyasını da göndermesi. Canlı zincirle üç yönlü ölçtüm:

```
capraz imza DAHIL  -> OK
capraz imza HARIC  -> verification failed (error 20, unable to get local issuer)
Root YR guvenilir  -> OK
```

Çapraz imza geçici. GitHub onu kırptığı gün — duyurmak zorunda değil — sahadaki dört hoparlör de OTA'yı sessizce kaybederdi ve geri dönüş USB'den yeniden flash olurdu. Kökü `firmware/certs/isrg-root-yr.pem` olarak ekledim (606 bayt) ve **üretilen paket içinde** doğruladım: ISRG kaydı 2'den 3'e çıktı.

## ESP-IDF'in yapmadığı kontrol

Araştırmanın en değerli çıktısı buydu. `esp_https_ota_get_img_desc` uygulama tanımlayıcısı hakkında **tek bir şey** denetliyor: `magic_word` (`esp_https_ota.c:589-594`). `project_name` ise `app_update`, `esp_https_ota` ve `bootloader_support` içinde **hiçbir yerde** karşılaştırılmıyor.

Sonucu şu: başka bir projenin doğru derlenmiş ESP32-S3 görüntüsü, IDF'in tüm denetimlerinden geçer, pasif slota yazılır ve açılır. `hk_ota_image_check()` bunu yakalayan tek yer. İnen görüntünün `project_name`, `version` ve `secure_version` alanları manifest'in vaadiyle **birebir** eşleşmek zorunda; daha yeni bir sürüm bile reddediliyor, çünkü iki kaynak çelişiyorsa hangisinin yanlış olduğunu buradan bilmenin yolu yok.

## Manifest, ikilinin kendisinden üretiliyor

`make_manifest.py` `product`, `version` ve `secure_version`'ı komut satırından almıyor; imzalı ikilinin uygulama tanımlayıcısından okuyor. Böylece manifest ile görüntünün çelişmesi **ifade edilemez** hale geliyor. Etiket ise doğrulanıyor: `v0.3.0` etiketiyle 0.2.0 derlemesi yayımlanamıyor.

Bir hata yaptım ve testler yakaladı: tanımlayıcıyı 24. bayttan okumaya çalıştım. Doğrusu 32 — arada 8 baytlık segment başlığı var. Yanlış ofsette okumak çökme vermiyor, **makul görünen yanlış değerler** veriyor; `magic_word` denetimi olmasa fark edilmezdi.

## Doğrulama

| Ne | Sonuç |
|---|---|
| Ana makine birim testleri | 10310 kontrol, 0 hata |
| `hk_ota` mutasyon testi | 11 mutasyonun 11'i yakalandı |
| `make_manifest` testleri | 14 test, alan adı bozulunca 4'ü, etiket kontrolü kaldırılınca 1'i düşüyor |
| `check_docs.py` | 96 dosya, 0 hata, 0 uyarı |
| Firmware derlemesi | 0x1113e0 bayt, yuvanın %16'sı, uyarısız |
| Sürüm profili derlemesi | Anahtarsız başarılı, IDF "not signed" diyor |
| İmzalama + doğrulama | Tek kullanımlık anahtarla uçtan uca geçti, anahtar silindi |
| Yayın işinin geri-okuma adımı | Doğru ikilide geçiyor, takas edilmiş ikilide yakalıyor |
| Sertifika paketi | `Root YR` üretilen pakette bulundu |

Mutasyon testi bir kez gerçek bir kusur gösterdi. URL doğrulayıcıda `@` ve `:` reddi hiçbir sonucu değiştirmiyordu — ana makineyi *yanlış biçimlendiği için* eleyip geçiyordum, yani iki kural ölü daldı. Doğru yapı önce ayrıştırıp sonra karar vermek: artık kullanıcı bilgisi ve port ayrı ayrı ayıklanıyor ve her kural bağımsız gözlemlenebiliyor. Aynı sweep `https://` şartının da test edilmediğini gösterdi; `httpx://github.com/x` kabul ediliyordu, çünkü tek test ettiğim `http://` şeması sekiz karakterden kısa olduğu için ofset kayması sayesinde zaten düşüyordu.

## Açık kalanlar

- İlk-boot sağlık kontrolü ve `esp_ota_mark_app_valid_cancel_rollback()` çağrısı yazılmadı, bu yüzden `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` sürüm profilinde de **kapalı**. Bunu ilk yazışımda açmıştım; günlüğü yazarken fark ettim ki çağrı olmadan yayımlanan her imaj ilk yeniden başlatmada geri döner, yani güncelleme hiç tutmazdı — `sdkconfig.defaults`'un zaten uyardığı hata, başka dosyada. Yorumla bırakmak yerine yayın işine bir denetim koydum: bayrak açıksa kaynakta o çağrı yoksa hat durur. İkisi aynı değişiklikte açılacak.
- Güncelleme zamanlayıcısı (rastgele gecikme, günde bir kontrol, backoff) ve LED entegrasyonu yok.
- USB/UART recovery prosedürü yazılmadı.
- `HK_SIGNING_KEY` üretilmedi ve `release` ortamı korumaya alınmadı. Bu sahibinin işi; anahtar çevrimdışı üretilmeli ve CI'da asla üretilmemeli.
- `G6` matrisinin **on iki satırının hiçbiri çalıştırılmadı**. Donanım elde değil. Bu aşama "yazıldı", "kanıtlandı" değil.

## Sıradaki

Donanım gerektirmeyen aşamaların sonu burası. Kart geldiğinde ilk sıra: F0 yapısını flash'layıp açılış raporunu şemayla karşılaştırmak, sonra G0 sürücü ölçümleri.
