---
status: active
owner: firmware-engineer
updated: 2026-09-13
tags: [controls, provisioning, firmware]
---

# Merzarkabul Airplay Speakers kontroller, LED ve Wi-Fi provisioning planı

Güncelleme: 2026-09-13

## Karar özeti

Hoparlörde iki kullanıcı arayüzü öğesi bulunacak:

1. Bir adet çok-fonksiyonlu anlık buton: provisioning, ağ sıfırlama ve fabrika sıfırlama.
2. Bir adet RGB durum LED'i: açılış, provisioning, Wi-Fi, AirPlay ve hata durumları.

V1'de fiziksel güç anahtarı yoktur: cihaz 24 V adaptörü çekilerek kapatılır, gerisini firmware'in boşta bekleme durumu karşılar ([[07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]).

Wi-Fi kurulumu **PIN'siz** ve iki yöntemle sunulur ([[07-decisions/ADR-0023-pinless-provisioning|ADR-0023]]):

- Uygulamasız: **açık** SoftAP (`PROV_Merzarkabul-XXXX`, kilit yok) + kendi captive portal'ımız ([[07-decisions/ADR-0015-softap-captive-portal|ADR-0015]]'in portal seçimi). Sayfa düz bir formdur ve ağın açık olduğunu, formun şifrelenmeden gittiğini bir cümleyle söyler.
- Bluetooth LE: ESP-IDF Unified Provisioning, protocomm Security 1, **sahiplik kanıtı yok** — cihaz `no_pop` ilan eder, uygulama PIN sormaz. Espressif'in stok **ESP BLE Provisioning** uygulaması ya da ileride hazırlanacak özel iOS/Android uygulaması.

İkisi **aynı anda** açılır ve seçimi telefon yapar ([[07-decisions/ADR-0016-simultaneous-dual-transport|ADR-0016]]): yöneticiyi BLE sürer, erişim noktasını ve portalı `hk_network` kendisi kaldırır.

> [!note] Tek scheme kısıtı hâlâ doğru; onu aşan şey portalın protocomm dışında olması
> ESP-IDF'in provisioning yöneticisi tek bir statik bağlam tutar ve yapılandırmasında **tek bir scheme** alır (`components/wifi_provisioning/src/manager.c`, tek `prov_ctx`, ESP-IDF v5.5.1). [[07-decisions/ADR-0005-dual-provisioning|ADR-0005]] bundan "ikisi sırayla" sonucunu çıkarmıştı. ADR-0015 portalı protocomm'a değil `wifi_prov_mgr_configure_sta()`'ya bağlayınca SoftAP ayağının yöneticiye ihtiyacı kalmadı ve ADR-0016 ikisini birlikte açtı. Bu notun 2026-09-05 tarihli "sırayla sunulur" hâli o günkü kararı anlatıyordu; bugünkü kural yukarıdadır.
>
> BLE açılış başına bir kez: pencere kapanınca Bluetooth belleği serbest bırakılır ve geri alınamaz. Pencere kapandıktan sonra yeniden açılan bir kurulum yalnız SoftAP sunar; uygulama yolu o yeniden açılışta SoftAP üzerinden gider (ADR-0016). Uygulamasız yol her zaman erişilebilir kalır, çünkü 5 saniyelik basış Wi-Fi'yi silip cihazı ilk duruma döndürür.

ESP32-S3'te Bluetooth Classic/A2DP bulunmaması BLE provisioning'i engellemez. Provisioning tamamlanınca BLE servisi durdurulacak ve ayrılan bellek serbest bırakılacak; normal AirPlay çalışmasında BLE açık tutulmayacak.

## Kurulum akışı: kullanıcı ne görür (2026-09-13, ADR-0023)

Sahibin isteği olduğu gibi: *"Wifi Prov ve BLE prov aşamasından pin olayını kaldıralım wifi açık olsun evde kullanacağım sorun yok kolay olsun kurulumu."* Akış buna göre yeniden yazıldı. **Aşağıdakilerin hiçbiri bir telefonda denenmedi**: uygulama davranışı Espressif kütüphanelerinin kaynağından okundu (GitHub `master`; mağaza ikilileri doğrulanmadı), donanım kaydı ise Security 2 ve WPA2 ile alınmış eski sonuçlardır.

### Espressif ESP BLE Provisioning ile (iOS / Android)

1. Uygulama açılır, BLE seçilir. İki stok uygulama da BLE listesini varsayılan olarak `PROV_` önekiyle süzer; cihaz bu yüzden `PROV_Merzarkabul-XXXX` adıyla yayın yapar ve **önek ayarı değiştirilmeden, QR'sız** listede görünür.
2. Cihaz listeden seçilir ya da etiketteki QR taranır. QR isteğe bağlıdır ve sır taşımaz: `{"ver":"v1","name":"PROV_Merzarkabul-XXXX","transport":"ble","security":1}`. `security` alanı, iki kütüphanenin de eksik alanı 2 varsayması yüzünden yazılır; oturumda geçerli olan cihazın ilan ettiği sürümdür.
3. Uygulama `proto-ver`'i okur: `sec_ver 1`, `cap [no_pop, wifi_scan]`. **PIN sorulmaz.** Oturum X25519 + AES-CTR ile açılır; anahtar yalnız anahtar değişiminden türer.
4. Ağ listesi gelir, ev ağı seçilir, ev Wi-Fi parolası yazılır. Parola BLE'de şifreli gider.
5. "Provisioning succeeded" → pencere kapanır, BLE belleği serbest bırakılır, hoparlör ağa katılır.

### Espressif ESP SoftAP Prov ile

Yalnız BLE serbest bırakıldıktan sonraki yeniden açılışta erişilebilir: çift taşıma penceresinde protocomm portalın sunucusuna bağlı değildir, yöneticiyi BLE sürer (ADR-0016). Telefon açık `PROV_Merzarkabul-XXXX` ağına **kendi Wi-Fi ayarlarından** katılır, sonra uygulama cihazı bulur ve PIN sormaz. QR'la otomatik katılma bu yolda güvenilmez: Android örnek uygulamasının Android 10+ yolu QR'daki ağa boş bir WPA2 parolasıyla bağlanmayı ister (`ESPDevice.java:197-199`) ve açık bir ağla eşleşmez; iOS QR'daki açık SSID'ye katılır (`transport: softap`, parola alanı yok). Elle katılma iki platformda da uygulamadan bağımsızdır. 2026-09-03'te görülen `mbedtls_gcm_auth_decrypt : -18` Security 2'nin AES-GCM katmanındaydı; Security 1 (AES-CTR) bu uygulamayla hiç denenmedi — **çalışır denemez**.

### Uygulamasız: telefonun Wi-Fi listesi + captive portal

1. Telefonun Wi-Fi listesinde `PROV_Merzarkabul-XXXX`, **kilit simgesiz**. Katılınca parola sorulmaz.
2. iOS/Android captive portal algılaması `http://192.168.4.1/` sayfasını açar; açmazsa adres elle yazılır.
3. Sayfa düz bir formdur: ağ listesi ve parola alanı. Sayfada bir cümle ağın açık olduğunu ve formun şifrelenmeden gittiğini söyler: buraya yalnız ev Wi-Fi parolası, yalnız hoparlör kurulumdayken yazılır.
4. "Katıl" → sayfa bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye verir; bağlanma ve pencere kapatma mantığı tek yerde kalır.

Bu yolda ev parolası açık ağ üzerinde düz HTTP ile gider; menzildeki pasif bir dinleyici okuyabilir. Kabulün gerekçesi ve sınırı ADR-0023'te ve [[01-planning/risk-register|risk kaydında]]: hoparlör evde kurulur; ortak alan, ofis ya da konuk ağı bu kabulün dışındadır.

### Referans istemci

`esp_prov.py --transport ble --service_name PROV_Merzarkabul-XXXX --verbose`: `--sec_ver` verilmeden 1'i seçer, `no_pop` görünce PIN sormaz. Tezgâhta ilk kanıt bu satırdır: `proto-ver` çıktısında `sec_ver 1` ve `cap [no_pop, wifi_scan]`.

### Kaldırılanlar

Security 2 ve cihaz başına kurulum parolası firmware'den silindi, bir Kconfig arkasında tutulmadı. `factory_cal` provisioning sırrı taşımaz; ADR-0023'ten önce kimlik bilgisi yazılmış kartlarda (ürün kartı 2026-09-08'de, geliştirme kartı daha önce) eski satırlar yerinde durur, firmware onları hiç okumaz, yeniden flaşlama gerekmez. Etiket sır içermez: cihaz adı ve iki QR. Kaybolursa hiçbir şey kaybolmaz.

## Plan ile bugün arasındaki fark (2026-09-05)

> [!note] 2026-09-13 — bu bölüm Security 2 ve WPA2 günlerinin kaydıdır
> Aşağıdaki tablo ve iki alt bölüm 2026-09-05/08'de ölçüleni anlatır ve olduğu gibi bırakıldı. ADR-0023 o tasarımı geri aldı: BLE Security 1 ve sahiplik kanıtsız, kurulum ağı açık, adlar `PROV_Merzarkabul-XXXX`. Buradaki hiçbir "var" ya da "çalışıyor" yeni yolun kanıtı değildir; yeni yol hiçbir telefonda denenmedi.

Plan duruyor. Değişen, ilk kez bir geliştirme kartında denenmiş olması — ürün kartında değil. Bazı satırların karşılığı çıktı, bir tanesinin arkasında hiçbir şey olmadığı görüldü. Bu fark hiçbir yerde yazılı değildi ve zor yoldan yeniden keşfedildi; bölüm bunun için var. Ham kayıt [[06-testing/devkit-bring-up|geliştirme kartı bring-up kaydındadır]], burada yalnız planı bağlayan sonuç var.

| Plandaki söz | 2026-09-05'te ne var |
|---|---|
| SoftAP açılıyor, adı `Merzarkabul-Setup-XXXX` (o günkü ad; ADR-0023 ile `PROV_Merzarkabul-XXXX`) | **Var.** Donanımda açıldı; o günkü Security 2 kimlik bilgileri `factory_cal`'dan yükleniyordu (salt 16 B, verifier 384 B). ADR-0023 sonrası firmware bu anahtarları okumaz. |
| SoftAP'a bağlanan telefonda kurulum sayfası açılıyor | **O gün yoktu.** 2026-09-08'de yazıldı (ADR-0015, `hk_portal`) ve **hiçbir donanımda denenmedi**. |
| BLE yayını, Security 2, QR ile kurulum | **Var ve uçtan uca çalışıyordu** — Security 2 ile. Bir iPhone'dan ağa katılındı. Security 1 / sahiplik kanıtsız yol bu sonucun kapsamı dışındadır. |
| LED desenleri | **Kısmen görüldü.** Kartta harici RGB LED yok, ama `hk_ui` aynı render geçişini kartın kendi adreslenebilir LED'ine aynalıyor; `ready` (yeşil) ve `playing` (mor nefes) sahibi tarafından doğrulandı. `ota` ve buton geri sayımları görülmedi. |
| Butonla açılan pencere | **Kısa basış ve 5 sn doğrulandı** (`GPIO7`-`GND` köprüsüyle), çökme yok. 12 sn fabrika sıfırlaması henüz üretilmedi. Pencerenin 10 dakikada kendi kendine kapanması ölçülmedi. |

### Uygulamasız yol: 2026-09-05'te yoktu, 2026-09-08'de yazıldı

O gün görülen şuydu: `wifi_prov_scheme_softap` bir web sayfası sunmuyor, `192.168.4.1` üzerinde yalnız protocomm uç noktaları açıyor. SoftAP'a katılmak hiçbir şey açmıyordu ve captive portal algılaması açacak bir sayfa bulamıyordu. ADR-0005'in gerekçesi — "uygulamasız yol her zaman erişilebilir olmalı" — yazılmış, karşılığı yazılmamıştı.

Karşılığı [[07-decisions/ADR-0015-softap-captive-portal|ADR-0015]] ile yazıldı, ve o gün kararın belirleyici noktası sayfanın kendisi değil, **gizliliğin nereden geldiği** idi:

- Kurulum ağı o gün **WPA2** yapıldı; anahtarı cihaz başına kurulum parolasıydı, etiketteki parolanın aynısı. Bu kısım ADR-0023 ile geri alındı: ağ artık **açık**, etikette parola yok.
- Sayfa **düz bir form** — bu kısım kaldı. Alternatifi, tarayıcı düz HTTP'de kendi kriptosunu (`crypto.subtle`) vermediği için, SHA-512/AES-GCM/3072-bit modexp'i elle yazıp flash'tan servis etmekti; o alternatif bugün de reddedilmiş durumda. Ağ açık olunca formun gizliliğini artık hiçbir katman sağlamaz; sayfa bunu söyler ve ADR-0023 bunu ev kullanımı için kabul eder.
- Sayfa ikinci bir kurulum yolu değil: aldığı bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye veriyor, yani bağlanma ve pencere kapatma mantığı tek yerde kalıyor. Bu kısım da kaldı.
- O günkü bedel — WPA2 anahtarının cihazda düz metin durması — anahtarla birlikte ortadan kalktı.

**Hiçbiri donanımda denenmedi.** Kanıtlanan, profillerin derlendiği ve form ayrıştırıcısının host'ta test edildiğidir. Telefonun kurulum sayfasını kendiliğinden açtığı bir tezgâh iddiasıdır ve henüz yapılmadı — ne WPA2'li hâliyle, ne açık hâliyle.

### BLE çalışıyordu, ve hangi uygulamayla çalıştığı önemli

Espressif'in iki ayrı provisioning uygulaması var ve 2026-09-05'te **yalnız biri çalışıyordu**. Adları benzediği için aynı sanılması kolay; yanlış olanla denemek "cihaz bozuk" sonucunu verir.

- **ESP BLE Provisioning** — 2026-09-05'te bir iPhone'dan uçtan uca doğrulandı (Security 2 ile): QR tarandı, ağ seçildi, cihaz katıldı. Cihazla birlikte adı yazılacak uygulama budur. Security 1 / `no_pop` ile aynı uygulama henüz denenmedi.
- **ESP SoftAP Prov** — o gün çalışmıyordu. 2026-09-03 tezgâh notu şunu kaydetti: Security 2 el sıkışması **başarıyla bittikten sonra** AES-GCM katmanında düşüyor (`mbedtls_gcm_auth_decrypt : -18`). Aynı cihaza, aynı kimlik bilgileriyle ESP-IDF'in kendi `esp_prov.py`'si bağlanıyor. Yani kusur firmware'de değil, o uygulamada. Bu ayrım kayda geçiyor, çünkü aksi hâlde çalışan bir firmware "düzeltilmeye" oturulur. Security 1'in AES-CTR katmanı o hatanın dışındadır; ama bu, uygulamanın Security 1'de çalıştığı anlamına gelmez — denenmedi.

O gün QR yükü Security 2 kullanıcı adını da taşıyordu ve ADR-0014 o adı ekosistem varsayılanına çekmişti. ADR-0023 ile Security 2 gidince kullanıcı adı diye bir şey kalmadı; ADR-0014 tümüyle aşıldı.

### Bugün kapatılan iki kusur: BLE hiç yayında değildi

İkisi de donanım olmadan görünmüyordu. Derleme, testler ve cihazın kendi logu hep doğru diyordu.

1. **BLE hiç yayın yapmıyordu.** `hk_network_start()`, "bu cihaz kurulmuş mu?" sorusunu sorabilmek için provisioning yöneticisini SoftAP şemasıyla kuruyor, sonra gerçek transport'u seçiyor ve **yöneticiyi bir daha kurmuyordu** — yanındaki yorum kurduğunu söylediği hâlde. `wifi_prov_mgr_init()` taşımayı bağlar; sonrasında başlayan şey seçilen değil, bağlanmış olandır. Cihaz `provisioning open over ble` yazarken SoftAP yayınlıyordu. Log dışında her yüzey tutarlıydı, o yüzden kusur yalnız radyoya bakınca görülüyordu. Artık yeniden kuruluyor ve NimBLE gerçekten yayın yapıyor.
2. **BLE'nin yayınladığı ad SoftAP SSID'siydi.** Yayın `Merzarkabul-Setup-XXXX` derken cihazla gelen QR `Merzarkabul-XXXX` arıyordu; yani QR'lı kurulum hiçbir zaman eşleşemezdi. Aşağıdaki kimlik tablosundaki iki satır zaten ayrıydı, karışan kod tarafıydı.

### Provisioning'de seçilen ağ ürünü belirliyor

Kart geliştirme sırasında yönlendiricinin misafir ağındaydı ve o ağ tasarımı gereği yalıtık olduğu için ana ağdaki bir Mac cihazı hiç göremedi. AirPlay keşfi mDNS çoklu yayınıyla, AirPlay 2'nin saati PTP çoklu yayınıyla çalışır ve ikisi de yalıtılmış bir misafir ağını aşmaz: hoparlör ile telefon aynı L2 ağında olmak zorundadır. Bu, provisioning'i doğrudan ilgilendiriyor, çünkü ağı seçen adım burasıdır. Ölçüm ve ayrıntı [[06-testing/devkit-bring-up|bring-up kaydında]].

## Otomatik tanıma için gerçekçi platform sınırı

### Uygulamasız deneyim

- Cihaz ilk açılışta veya provisioning tuşuna basılınca `PROV_Merzarkabul-XXXX` isimli 2,4 GHz SoftAP açar; ağ **açıktır**, parola yoktur (ADR-0023).
- Kullanıcı telefonun Wi-Fi listesinden bu ağı seçer; kilit simgesi görmez, parola girmez.
- iOS/Android captive portal algılaması kurulum sayfasını otomatik açmayı dener.
- Portal otomatik açılmazsa sabit adres `192.168.4.1` kullanılır.
- Sayfa, ağın açık olduğunu ve formun şifrelenmeden gittiğini söyler; kullanıcı ev ağını seçer, ev parolasını yazar.

> [!warning] 2026-09-13 — yazıldı, ölçülmedi
> Sayfa ve captive DNS 2026-09-08'den beri var (`hk_portal`, ADR-0015); ağ 2026-09-13'te açıldı (ADR-0023). Hiçbiri bir telefonda denenmedi: ne portalın kendiliğinden açılması, ne kilitsiz ağın listede görünmesi, ne sayfadaki cümle.

Bu yöntem özel uygulama istemez, ancak telefonun BLE yayınını görür görmez ana ekranda Apple/Android sistem kartı açması garanti edilemez.

### BLE ile uygulamalı deneyim

- Cihaz `PROV_Merzarkabul-XXXX` adı ve üretici servis UUID'si ile BLE yayını yapar. `PROV_` öneki, Espressif'in iki stok uygulamasının listeyi varsayılan olarak o önekle süzmesi içindir: cihaz uygulamanın ayarına dokunmadan listede çıkar.
- ESP-IDF Unified Provisioning, protocomm Security 1, sahiplik kanıtı yok: cihaz `no_pop` ilan eder, uygulama PIN sormaz (ADR-0023). Oturum X25519 + AES-CTR ile şifrelidir; pasif dinleyici bir şey öğrenmez, kimse doğrulanmaz.
- QR isteğe bağlıdır ve cihaz adı, taşıma ve güvenlik sürümünü taşır; sır taşımaz.
- İlk prototip Espressif Provisioning iOS/Android uygulamalarıyla kurulabilir.
- Nihai özel uygulama yapılırsa iOS'ta AccessorySetupKit, Android'de Companion Device Manager kullanılarak sistem aksesuar seçicisi gösterilebilir.

> [!note] 2026-09-05 — bu yol donanımda Security 2 ile çalıştı, ama tek bir uygulamayla
> Dördüncü madde o gün doğrulandı: **ESP BLE Provisioning**, Security 2 ve QR ile. Espressif'in "ESP SoftAP Prov" uygulaması ayrı bir uygulamadır ve o gün çalışmıyordu; hangisinin neden çalışmadığı yukarıdaki "Plan ile bugün arasındaki fark" bölümünde. ADR-0023'ün Security 1 / `no_pop` akışı iki uygulamayla da henüz denenmedi.

iOS AccessorySetupKit ve Android Companion Device Manager bir uygulama tarafından çağrılan API'lerdir. Uygulama olmadan özel ürün görseli ve sistem eşleştirme kartı açma kapsam dışıdır. Apple HomeKit/MFi veya Google Fast Pair kimliği taklit edilmeyecektir.

## Merzarkabul Airplay Speakers ürün kimliği

| Yüzey | Varsayılan ad |
|---|---|
| Proje/ürün ailesi | `Merzarkabul Airplay Speakers` |
| AirPlay görünen adı | `Merzarkabul XXXX` |
| BLE provisioning yayını | `PROV_Merzarkabul-XXXX` |
| SoftAP SSID (açık) | `PROV_Merzarkabul-XXXX` |
| mDNS/yerel ağ adı | `merzarkabul-xxxx.local` |
| Captive portal başlığı | `Merzarkabul Kurulum` |
| QR ürün etiketi | `Merzarkabul Airplay Speakers` |

`XXXX`, MAC adresinden türetilen kısa benzersiz cihaz kimliğidir. Kullanıcı AirPlay adını değiştirebilir; kurulum adlarında benzersiz son ek korunur. Son ek cihazı aynı ağdaki başka her AirPlay/BLE hedefinden ayırır: bir ad herhangi bir ağda benzersiz olmalıdır. BLE yayını ve kurulum ağı 2026-09-13'ten beri **tek** adı paylaşır ve `PROV_` önekini taşır (ADR-0023, ADR-0001): önek stok Espressif uygulamalarının varsayılan liste süzgecidir, kullanıcıya gösterilen bir marka değil. 2026-09-05/08 kayıtlarındaki `Merzarkabul-XXXX` ve `Merzarkabul-Setup-XXXX` o günlerin adlarıdır.

## Provisioning durum makinesi

```text
İlk açılış / kayıtlı Wi-Fi yok
              |
              v
BLE yayını + açık SoftAP captive portal (aynı anda)
              |
              v
Şifreli oturum (BLE, PIN'siz) ya da düz form (portal); ağ seçimi
              |
              v
Wi-Fi bağlantı testi
       | başarılı       | başarısız
       v                v
BLE/AP kapat         provisioning açık kalır
mDNS + AirPlay       hata LED'i + yeniden dene
```

- Provisioning penceresi ilk açılışta kurulum tamamlanana kadar açık kalır.
- Kayıtlı bir cihazda butonla açılan provisioning 10 dakika sonra otomatik kapanır.
- Başarılı bağlantıdan sonra BLE ve SoftAP tamamen kapatılır.
- Art arda bağlantı hatasında cihazın tekrar provisioning moduna dönmesi **politikada yazılı, cihazda bağlı değil**: `hk_provision`'ın üç ardışık başarısız katılımdan sonra sınırsız pencere açan dalı host'ta testlidir, ama `hk_main` politikaya yalnız tick, buton ve sıfırlama olaylarını verir; bağlantı sonucu olayları hiçbir yerden beslenmez ve `hk_network` beş denemeden sonra hata bayrağıyla durur. Yani bugün o dal erişilemez; PIN kalktıktan sonra bu bilerek kayda geçti (ADR-0023). Bağlanması ya da silinmesi ayrı bir karardır.
- Wi-Fi parolası hiçbir log, web sayfası geri cevabı veya seri telemetride gösterilmez.
- 2026-09-05/08: Şemadaki kutunun SoftAP yarısı donanımda gerçek; captive portal yarısı 2026-09-08'de yazıldı ama denenmedi. İki taşıma 2026-09-08'den beri birlikte açılıyor (ADR-0016), 2026-09-13'ten beri PIN'siz ve açık ağla (ADR-0023) — ikincisi hiçbir kartta denenmedi.

## Çok-fonksiyonlu buton davranışı

Buton aktif-low çalışacak; seçilecek normal GPIO ile GND arasına bağlanacak ve dahili pull-up kullanılacak. GPIO numarası kesin kart ve I2S pinleri seçildikten sonra belirlenecek; boot/strapping pinleri kullanılmayacak.

| Hareket | İşlev | LED geri bildirimi |
|---|---|---|
| Kısa basış, 0,1-1,5 sn | 10 dakikalık BLE + SoftAP provisioning başlat | Mavi nefes |
| 5 sn basılı tut | Yalnız Wi-Fi kimlik bilgilerini sil ve provisioning'e yeniden başlat | Sarı geri sayım, sonra mavi |
| 12 sn basılı tut | Kullanıcı ayarlarını fabrika değerine döndür | Kırmızı hızlı yanıp sönme, bırakınca beyaz |

Fabrika sıfırlama sürücü koruma profili, maksimum güvenli limiter, crossover güvenlik sınırları veya donanım kalibrasyonunu silmeyecek. NVS alanları `factory_cal` ve `user_settings` olarak ayrılacak.

Buton yazılım gereksinimleri:

- 50 ms debounce.
- Basılı tutma eşiklerinde anlık LED geri bildirimi.
- 12 saniyelik işlem yalnız buton bırakıldığında onaylanır; eşik geçilirken veri silinmez.
- Açılış sırasında buton basılıysa kurtarma provisioning modu başlatılır.
- Yanlışlıkla kısa dokunmada kayıtlı Wi-Fi silinmez.

## RGB LED durumları

Tek gövdeli, ortak katot RGB LED ve her renk için ayrı seri direnç kullanılacak. Üç PWM GPIO gerekir. Parlaklık gece kullanımında rahatsız etmeyecek şekilde sınırlandırılacak.

| Renk/desen | Durum |
|---|---|
| Kapalı | Adaptör takılı değil veya boşta bekleme |
| Beyaz nefes | Açılış ve donanım testi |
| Mavi nefes | BLE/SoftAP provisioning aktif |
| Sarı yavaş yanıp sönme | Wi-Fi ağına bağlanıyor |
| Yeşil 3 sn, sonra sönük | Wi-Fi bağlı ve AirPlay hazır |
| Mor nefes, düşük parlaklık | Aktif AirPlay oynatma |
| Camgöbeği yanıp sönme | OTA güncelleme; güç kesilmemeli |
| Kırmızı hızlı | Wi-Fi veya ses hatası |

> **2026-09-05 — hangi satırlar gerçekten sürülüyor.** Tablo baştan beri eksiksizdi ve `hk_led` her satırı uygulamıştı, ama bir durumu hiçbir yer set etmiyordu: `playing`. Oynatma durumu bu tarihte bağlandı — AirPlay yığınının kendi RTSP olayları `hk_main` üzerinden `hk_ui`'ya taşınıyor, LED'in tek sahibi `hk_ui` kalmaya devam ediyor. Sahibi "mor nefes" istediği için desen `SOLID`'den `BREATHE`'e alındı ve satır buna göre güncellendi.
>
> Aynı turda nefes efektinin kendisi de düzeltildi ve bu tablodaki üç "nefes" satırının hepsini etkiliyor: zarf üçgendi, kosinüs oldu (üçgen iki uçta da anında döndüğü için göz onu nefes değil sıçrama olarak görüyor), ve artık sıfıra inmiyor — sıfıra inen bir nefes yavaş yanıp sönmedir, renk kaybolur ve göz ritmi değil kaybolmayı fark eder.

LED animasyonları audio task üzerinde çalışmayacak; düşük öncelikli ayrı görev/timer kullanılacak. PWM veya GPIO güncellemelerinin I2S zamanlamasına ve analog dip gürültüsüne etkisi ölçülecek.

## Güç: açma ve kapatma

- V1'de güç anahtarı yoktur ([[07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]). Cihaz 24 V adaptörü takılınca açılır, çekilince kapanır; adaptör dört amfiyi doğrudan, ESP32-S3'ü buck A ve DAC'ı buck B üzerinden (iki ayrı 5 V) besler.
- Boşta bekleme kullanıcı ayarı `standby_min` ile zamanlanır (varsayılan 30 dk, `0` kapatır). Davranışın kendisi henüz yazılmadı; yazıldığında amfi mute sıralayıcısı üzerinden geçecek ve AirPlay hedefi görünür kalacak.
- Adaptör takılırken ve çalarken çekilirken DAC susturma sıralaması ve pop sesi G1'de dummy-load üzerinde ölçülür; amfinin susturma girişi olmadığından kendi açılış/kapanış geçişi olduğu gibi kaydedilir. Kapanış pop'u `PRD-007`'nin konusudur.

## Güvenlik ve gizlilik

- Kurulumda sahiplik kanıtı yoktur (ADR-0023): BLE'de protocomm Security 1, PIN'siz; kurulum ağı açık. Cihaz başına üretilen bir parola, sabit bir fabrika parolası ya da etikette bir sır yoktur.
- Uygulama yolu pasif dinleyiciye karşı şifrelidir (X25519 + AES-CTR); portal yolu açık ağda düz HTTP'dir. Pencere açıkken menzildeki herkes hoparlörü kendi ağına kurabilir; sahte bir cihaz sahibin telefonundan ev parolasını alabilir; portal yolunda ev parolası menzildeki bir dinleyici tarafından okunabilir. Kabulün gerekçesi sahibin sözü — evde kullanılacak — ve sınırı [[01-planning/risk-register|risk kaydındadır]].
- BLE provisioning yalnız ilk kurulumda veya fiziksel butonla zaman sınırlı olarak açılır; ilk açılış penceresi kurulana kadar sınırsızdır.
- Wi-Fi parolası loglarda görünmez; `firmware/tools/check_no_credential_logs.py` bunu CI'da denetler.
- Saklanan Wi-Fi kimlik bilgileri için ESP-IDF NVS encryption seçeneği değerlendirilecek.

## Teknik kaynaklar

- ESP-IDF Unified Provisioning: https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/provisioning/provisioning.rst
- Espressif iOS provisioning: https://github.com/espressif/esp-idf-provisioning-ios
- Espressif Android provisioning: https://github.com/espressif/esp-idf-provisioning-android
- Apple AccessorySetupKit: https://developer.apple.com/documentation/AccessorySetupKit
- Android Companion Device Pairing: https://developer.android.com/develop/connectivity/bluetooth/companion-device-pairing
- AirPlay ESP32 ilk açılış: https://rbouteiller.github.io/airplay-esp32/getting-started/first-boot/
- AirPlay ESP32 buton altyapısı: https://rbouteiller.github.io/airplay-esp32/features/buttons/
