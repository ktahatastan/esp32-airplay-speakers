---
status: active
owner: firmware-engineer
updated: 2026-09-05
tags: [controls, provisioning, firmware]
---

# Merzarkabul Airplay Speakers kontroller, LED ve Wi-Fi provisioning planı

Güncelleme: 2026-09-05

## Karar özeti

Hoparlörde iki kullanıcı arayüzü öğesi bulunacak:

1. Bir adet çok-fonksiyonlu anlık buton: provisioning, ağ sıfırlama ve fabrika sıfırlama.
2. Bir adet RGB durum LED'i: açılış, provisioning, Wi-Fi, AirPlay ve hata durumları.

V1'de fiziksel güç anahtarı yoktur: cihaz 24 V adaptörü çekilerek kapatılır, gerisini firmware'in boşta bekleme durumu karşılar ([[07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]).

Wi-Fi kurulumu iki yöntemle sunulacak:

- Uygulamasız: WPA2 korumalı SoftAP + kendi captive portal'ımız ([[07-decisions/ADR-0015-softap-captive-portal|ADR-0015]]). İkisi **aynı anda** açılır ve seçimi telefon yapar ([[07-decisions/ADR-0016-simultaneous-dual-transport|ADR-0016]]).
- Bluetooth LE: ESP-IDF Unified Provisioning. Espressif uygulaması veya ileride hazırlanacak özel iOS/Android uygulaması kullanılacak.

> [!note] İkisi sırayla sunulur, aynı anda değil
> ESP-IDF'in provisioning yöneticisi tek bir statik bağlam tutar ve yapılandırmasında **tek bir scheme** alır. `scheme_ble` Wi-Fi'yi `WIFI_MODE_STA`'ya, `scheme_softap` ise `WIFI_MODE_APSTA`'ya alır; ikisi bir oturumda birlikte çalışamaz. Kaynak: `components/wifi_provisioning/src/manager.c` (tek `prov_ctx`), `scheme_ble.c:337` ve `scheme_softap.c:185`, ESP-IDF v5.5.1.
>
> [[07-decisions/ADR-0005-dual-provisioning|ADR-0005]] bunu sırayla sunmaya karar verdi. Hangisinin açılacağını provisioning'e nasıl girildiği belirler: **kayıtlı kimlik bilgisi yoksa SoftAP**, **yapılandırılmış cihazda butonla açılırsa BLE**. Uygulamasız yol her zaman erişilebilir kalır, çünkü 5 saniyelik basış Wi-Fi'yi silip cihazı ilk duruma döndürür.

ESP32-S3'te Bluetooth Classic/A2DP bulunmaması BLE provisioning'i engellemez. Provisioning tamamlanınca BLE servisi durdurulacak ve ayrılan bellek serbest bırakılacak; normal AirPlay çalışmasında BLE açık tutulmayacak.

## Plan ile bugün arasındaki fark (2026-09-05)

Plan duruyor. Değişen, ilk kez bir geliştirme kartında denenmiş olması — ürün kartında değil. Bazı satırların karşılığı çıktı, bir tanesinin arkasında hiçbir şey olmadığı görüldü. Bu fark hiçbir yerde yazılı değildi ve zor yoldan yeniden keşfedildi; bölüm bunun için var. Ham kayıt [[06-testing/devkit-bring-up|geliştirme kartı bring-up kaydındadır]], burada yalnız planı bağlayan sonuç var.

| Plandaki söz | 2026-09-05'te ne var |
|---|---|
| SoftAP açılıyor, adı `Merzarkabul-Setup-XXXX` | **Var.** Donanımda açıldı; kimlik bilgileri `factory_cal`'dan yükleniyor (salt 16 B, verifier 384 B). |
| SoftAP'a bağlanan telefonda kurulum sayfası açılıyor | **O gün yoktu.** 2026-09-08'de yazıldı (ADR-0015, `hk_portal`) ve **hiçbir donanımda denenmedi**. |
| BLE yayını, Security 2 / SRP6a, QR ile kurulum | **Var ve uçtan uca çalışıyor.** Bir iPhone'dan ağa katılındı. |
| LED desenleri | **Kısmen görüldü.** Kartta harici RGB LED yok, ama `hk_ui` aynı render geçişini kartın kendi adreslenebilir LED'ine aynalıyor; `ready` (yeşil) ve `playing` (mor nefes) sahibi tarafından doğrulandı. `ota` ve buton geri sayımları görülmedi. |
| Butonla açılan pencere | **Kısa basış ve 5 sn doğrulandı** (`GPIO7`-`GND` köprüsüyle), çökme yok. 12 sn fabrika sıfırlaması henüz üretilmedi. Pencerenin 10 dakikada kendi kendine kapanması ölçülmedi. |

### Uygulamasız yol: 2026-09-05'te yoktu, 2026-09-08'de yazıldı

O gün görülen şuydu: `wifi_prov_scheme_softap` bir web sayfası sunmuyor, `192.168.4.1` üzerinde yalnız protocomm uç noktaları açıyor. SoftAP'a katılmak hiçbir şey açmıyordu ve captive portal algılaması açacak bir sayfa bulamıyordu. ADR-0005'in gerekçesi — "uygulamasız yol her zaman erişilebilir olmalı" — yazılmış, karşılığı yazılmamıştı.

Karşılığı [[07-decisions/ADR-0015-softap-captive-portal|ADR-0015]] ile yazıldı, ve kararın belirleyici noktası sayfanın kendisi değil, **gizliliğin nereden geldiği**:

- Kurulum ağı artık **açık değil, WPA2**. Anahtarı cihaz başına kurulum parolasıdır — etiketteki parolanın aynısı. Ağa katılabilen kişi zaten parolayı bilen kişidir ve bağlantı 802.11 katmanında şifrelidir.
- Sayfa bu yüzden **düz bir form** olabiliyor. Alternatifi, tarayıcı düz HTTP'de kendi kriptosunu (`crypto.subtle`) vermediği için, SHA-512/AES-GCM/3072-bit modexp'i elle yazıp flash'tan servis etmekti.
- Sayfa ikinci bir kurulum yolu değil: aldığı bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye veriyor, yani bağlanma ve pencere kapatma mantığı tek yerde kalıyor.
- Bedeli kayda geçti: WPA2 anahtarı cihazda düz metin durmak zorunda. Flash'ı okuyabilen biri kullanıcının ev Wi-Fi parolasını zaten `nvs`'ten alıyor, o yüzden bu yeni bir maruziyet açmıyor — ayrıntı ADR-0015'te.

**Hiçbiri donanımda denenmedi.** Bugün kanıtlanan, iki profilin de derlendiği ve form ayrıştırıcısının host'ta test edildiğidir. Telefonun kurulum sayfasını kendiliğinden açtığı bir tezgâh iddiasıdır ve henüz yapılmadı.

### BLE çalışıyor, ve hangi uygulamayla çalıştığı önemli

Espressif'in iki ayrı provisioning uygulaması var ve **yalnız biri çalışıyor**. Adları benzediği için aynı sanılması kolay; yanlış olanla denemek "cihaz bozuk" sonucunu verir.

- **ESP BLE Provisioning** — 2026-09-05'te bir iPhone'dan uçtan uca doğrulandı: QR tarandı, ağ seçildi, cihaz katıldı. Cihazla birlikte adı yazılacak uygulama budur.
- **ESP SoftAP Prov** — çalışmıyor. 2026-09-03 tezgâh notu şunu kaydetti: SRP6a el sıkışması **başarıyla bittikten sonra** AES-GCM katmanında düşüyor (`mbedtls_gcm_auth_decrypt : -18`). Aynı cihaza, aynı kimlik bilgileriyle ESP-IDF'in kendi `esp_prov.py`'si bağlanıyor. Yani kusur firmware'de değil, o uygulamada. Bu ayrım kayda geçiyor, çünkü aksi hâlde çalışan bir firmware "düzeltilmeye" oturulur.

QR yükü SRP6a kullanıcı adını da taşıyor. Sonucu şu: QR ile kurulan bir cihazda kullanıcı adının ekosistem varsayılanı olması **gerekmiyor**, çünkü istemci adı QR'dan okuyor. Kullanıcı adı ancak QR'sız, listeden kurulumda bir uyumluluk konusu olur.

### Bugün kapatılan iki kusur: BLE hiç yayında değildi

İkisi de donanım olmadan görünmüyordu. Derleme, testler ve cihazın kendi logu hep doğru diyordu.

1. **BLE hiç yayın yapmıyordu.** `hk_network_start()`, "bu cihaz kurulmuş mu?" sorusunu sorabilmek için provisioning yöneticisini SoftAP şemasıyla kuruyor, sonra gerçek transport'u seçiyor ve **yöneticiyi bir daha kurmuyordu** — yanındaki yorum kurduğunu söylediği hâlde. `wifi_prov_mgr_init()` taşımayı bağlar; sonrasında başlayan şey seçilen değil, bağlanmış olandır. Cihaz `provisioning open over ble` yazarken SoftAP yayınlıyordu. Log dışında her yüzey tutarlıydı, o yüzden kusur yalnız radyoya bakınca görülüyordu. Artık yeniden kuruluyor ve NimBLE gerçekten yayın yapıyor.
2. **BLE'nin yayınladığı ad SoftAP SSID'siydi.** Yayın `Merzarkabul-Setup-XXXX` derken cihazla gelen QR `Merzarkabul-XXXX` arıyordu; yani QR'lı kurulum hiçbir zaman eşleşemezdi. Aşağıdaki kimlik tablosundaki iki satır zaten ayrıydı, karışan kod tarafıydı.

### Provisioning'de seçilen ağ ürünü belirliyor

Kart geliştirme sırasında yönlendiricinin misafir ağındaydı ve o ağ tasarımı gereği yalıtık olduğu için ana ağdaki bir Mac cihazı hiç göremedi. AirPlay keşfi mDNS çoklu yayınıyla, AirPlay 2'nin saati PTP çoklu yayınıyla çalışır ve ikisi de yalıtılmış bir misafir ağını aşmaz: hoparlör ile telefon aynı L2 ağında olmak zorundadır. Bu, provisioning'i doğrudan ilgilendiriyor, çünkü ağı seçen adım burasıdır. Ölçüm ve ayrıntı [[06-testing/devkit-bring-up|bring-up kaydında]].

## Otomatik tanıma için gerçekçi platform sınırı

### Uygulamasız deneyim

- Cihaz ilk açılışta veya provisioning tuşuna basılınca `Merzarkabul-Setup-XXXX` isimli 2,4 GHz SoftAP açar.
- Kullanıcı telefonun Wi-Fi listesinden bu ağı seçer ve etiketteki kurulum parolasını girer; ağ WPA2 korumalıdır (ADR-0015).
- iOS/Android captive portal algılaması kurulum sayfasını otomatik açmayı dener.
- Portal otomatik açılmazsa sabit adres `192.168.4.1` kullanılır.

> [!warning] 2026-09-08 — yazıldı, ölçülmedi
> Sayfa ve captive DNS artık var (`hk_portal`, ADR-0015) ve kurulum ağı WPA2 olduğu için listedeki ikinci madde de değişti: kullanıcı ağı seçerken etiketteki parolayı girer. Hiçbiri bir kartta denenmedi.

Bu yöntem özel uygulama istemez, ancak telefonun BLE yayınını görür görmez ana ekranda Apple/Android sistem kartı açması garanti edilemez.

### BLE ile uygulamalı deneyim

- Cihaz `Merzarkabul-XXXX` adı ve üretici servis UUID'si ile BLE yayını yapar.
- ESP-IDF Unified Provisioning, Security 2 / SRP6a ve cihaza özel proof-of-possession kullanır.
- QR kod cihaz adı, transport, güvenlik sürümü ve benzersiz PoP bilgisini taşır.
- İlk prototip Espressif Provisioning iOS/Android uygulamalarıyla kurulabilir.
- Nihai özel uygulama yapılırsa iOS'ta AccessorySetupKit, Android'de Companion Device Manager kullanılarak sistem aksesuar seçicisi gösterilebilir.

> [!note] 2026-09-05 — bu yol donanımda çalıştı, ama tek bir uygulamayla
> Dördüncü madde doğrulandı: **ESP BLE Provisioning**. Espressif'in "ESP SoftAP Prov" uygulaması ayrı bir uygulamadır ve çalışmıyor; hangisinin neden çalışmadığı yukarıdaki "Plan ile bugün arasındaki fark" bölümünde.

iOS AccessorySetupKit ve Android Companion Device Manager bir uygulama tarafından çağrılan API'lerdir. Uygulama olmadan özel ürün görseli ve sistem eşleştirme kartı açma kapsam dışıdır. Apple HomeKit/MFi veya Google Fast Pair kimliği taklit edilmeyecektir.

## Merzarkabul Airplay Speakers ürün kimliği

| Yüzey | Varsayılan ad |
|---|---|
| Proje/ürün ailesi | `Merzarkabul Airplay Speakers` |
| AirPlay görünen adı | `Merzarkabul XXXX` |
| BLE provisioning yayını | `Merzarkabul-XXXX` |
| SoftAP SSID | `Merzarkabul-Setup-XXXX` |
| mDNS/yerel ağ adı | `merzarkabul-xxxx.local` |
| Captive portal başlığı | `Merzarkabul Kurulum` |
| QR ürün etiketi | `Merzarkabul Airplay Speakers` |

`XXXX`, MAC adresinden türetilen kısa benzersiz cihaz kimliğidir. Kullanıcı AirPlay adını değiştirebilir; BLE ve SoftAP adlarında benzersiz son ek korunur. Son ek cihazı aynı ağdaki başka her AirPlay/BLE hedefinden ayırır: bir ad herhangi bir ağda benzersiz olmalıdır.

## Provisioning durum makinesi

```text
İlk açılış / kayıtlı Wi-Fi yok
              |
              v
BLE yayını + SoftAP captive portal
              |
              v
Güvenli kimlik doğrulama ve ağ seçimi
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
- Art arda bağlantı hatasında cihaz kontrollü olarak tekrar provisioning moduna döner.
- Wi-Fi parolası hiçbir log, web sayfası geri cevabı veya seri telemetride gösterilmez.
- 2026-09-05/08: Şemadaki "BLE yayını + SoftAP captive portal" kutusunun SoftAP yarısı donanımda gerçek; captive portal yarısı 2026-09-08'de yazıldı ama denenmedi. Kutu ayrıca ikisini yan yana gösteriyor; ADR-0015 (ADR-0005'ten devralarak) sırayla sunuyor.

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

- PoP cihaza özgü üretilir; ürün genelinde sabit bir parola yoktur.
- SoftAP mümkünse cihaza özel parola ile korunacak; QR kod bu bilgiyi taşıyacak.
- BLE provisioning yalnız ilk kurulumda veya fiziksel butonla zaman sınırlı olarak açılacak.
- Provisioning istekleri hız sınırlı olacak ve başarısız kimlik doğrulamalar loglarda parola içermeyecek.
- Saklanan Wi-Fi kimlik bilgileri için ESP-IDF NVS encryption seçeneği değerlendirilecek.

## Teknik kaynaklar

- ESP-IDF Unified Provisioning: https://github.com/espressif/esp-idf/blob/master/docs/en/api-reference/provisioning/provisioning.rst
- Espressif iOS provisioning: https://github.com/espressif/esp-idf-provisioning-ios
- Espressif Android provisioning: https://github.com/espressif/esp-idf-provisioning-android
- Apple AccessorySetupKit: https://developer.apple.com/documentation/AccessorySetupKit
- Android Companion Device Pairing: https://developer.android.com/develop/connectivity/bluetooth/companion-device-pairing
- AirPlay ESP32 ilk açılış: https://rbouteiller.github.io/airplay-esp32/getting-started/first-boot/
- AirPlay ESP32 buton altyapısı: https://rbouteiller.github.io/airplay-esp32/features/buttons/
