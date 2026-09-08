---
title: Kayıt tutarlılığı, uygulamasız kurulum yolu ve kalibrasyon profili
status: done
owner: orchestrator
reviewers: [verifier]
updated: 2026-09-08
tags: [development-log, docs, provisioning, portal, security, dsp, acoustics]
---

# 2026-09-08 — Dört iş: kaydı düzeltmek, iki karar, iki modül

Donanım hâlâ yok ve bu oturumun tamamı donanım gerektirmeyen işti. Dördü de kullanıcı tarafından seçildi ve bu sırayla yapıldı, çünkü ilki diğer üçünün üstüne yazacağı zemindi.

## 1. Kayıt, 5 Eylül'ün gerisinde kalmıştı

Depo kendini projenin kaydı olarak tanımlıyor. Yedi yerde o kayıt, kendisinden önceki bir depoyu anlatıyordu:

- `firmware/README.md` "plays no audio, joins no network and drives no GPIO" diye açılıyor ve AirPlay yığınını "not chosen" diye listeliyordu. Üçü de yanlıştı; yığın vendor edilmişti.
- Risk kaydı yığını hâlâ vendor'lanmamış sayıyordu.
- Bring-up kaydının kendi sonuç tablosunda "Bir Apple cihazı bağlandı — YAPILMADI" yazıyordu; **aynı gün, birkaç saat sonra** bir iPhone bağlandı ve ses duyuldu.
- 5 Eylül günlüğünün "Ne yapılmadı" bölümü, aynı dosyanın üst yarısıyla çelişiyordu.
- `TODO.md` aynı akşam bitmiş işleri istiyordu.
- Risk kaydının alıntıladığı PTP kilit rakamı (`dev=973672 ns`) bağlantı verdiği belgede yoktu — fizibilite sayfasındaydı.

Bunların hiçbiri kozmetik değil. Her biri bir sonraki kişiyi ya var olanı yeniden yapmaya ya da açık bir kapıya güvenmeye gönderirdi.

Düzeltirken tek kural: **yalnız deponun başka bir yerde zaten kanıtladığı şey yazıldı.** PTP rakamının bağlantısı, rakamın gerçekten durduğu yere çevrildi; bring-up kaydına, fizibilite sayfasının "ham günlük oradadır" dediği dış doğrulama eklendi. Günlükteki yanlış satır silinmedi, **üstü çizildi**: günlük tarihli bir kayıttır ve neyin ne zaman yanlış durduğu da kayıttır.

## 2. SRP6a kullanıcı adı: `harmankardom` -> `wifiprov` (ADR-0014)

Üretici `harmankardom` üretiyordu, ekosistemin her istemcisi `wifiprov` varsayıyor. İsim tercihi gibi görünüyor ve değil: ad verifier'ın içine giriyor, yani yanlış tahmin eden istemci kanıt adımında düşer.

Varsaymak yerine ESP-IDF `v5.5.1` okundu ve riskin **nerede** olduğu daraldı:

1. Cihaz adı saklamıyor — `security2.c` adı istemcinin kendi mesajından alıp SRP değişimine veriyor.
2. Yanlış ad sessizce geçmiyor — `esp_srp.c` istemci kanıtını `memcmp` ile denetliyor.
3. **QR yükü adı taşıyor.** Yani QR ile kurulumda özel bir ad çalışırdı.

Kırılan tek yol QR'sız kurulum: listeden seçip parolayı yazan kullanıcının uygulamasına adı öğretecek hiçbir şey yok, ve o uygulama `wifiprov` gönderir.

Ad **sır değil** — QR'da, etikette ve telde açık. Koruma tümüyle parolada. Yani özel ad hiçbir güvenlik kazandırmıyor, karşılığında bir kurulum yolunu tamamen kapatıyordu.

Bu karar, Espressif SoftAP uygulamasının `mbedtls_gcm_auth_decrypt : -18` hatasını **açıklamıyor** ve ADR-0014 bunu açıkça yazıyor: o arıza kanıt adımı geçtikten sonra oluyor, yani adlar zaten uyuşmuştu. İki ayrı sorun; biri diğerini çözmüyor.

Eski adla üretilmiş kimlik bilgileri geçersiz. Bugünkü maliyeti sıfır: sahada cihaz yok.

## 3. Uygulamasız kurulum yolu artık var (ADR-0015)

ADR-0005 "SoftAP + captive portal" sözü vermiş, portal hiç yazılmamıştı. 5 Eylül'de sonucu görüldü: kullanıcı ağa katıldı, hiçbir şey açılmadı.

Portalı yazmanın gerçek sorusu sayfa değil: **sayfa ev Wi-Fi parolasını nasıl alacak?** Kaynaktan üç kısıt çıktı ve karar bunlardan doğdu:

1. **Tarayıcı kriptosu düz HTTP'de yok** (`crypto.subtle` yalnız güvenli bağlamda). Security 2'yi sayfada konuşmak, elle yazılmış SHA-512/AES-GCM/3072-bit modexp'i flash'tan servis etmek demekti — üstelik sayfanın kendisinin düz HTTP'den indiği bir yolda.
2. **`service_key` SoftAP'ın Wi-Fi parolasıdır.** Bugüne kadar `NULL` geçiliyordu, yani kurulum ağı açıktı.
3. **`wifi_prov_scheme_softap_set_httpd_handle()`** protocomm'u bizim sunucumuza bindiriyor.

Karar: gizlilik bir katman aşağı iniyor. Kurulum ağı **WPA2** olur, anahtarı etiketteki cihaz başına parola. Sunucuya ulaşan kişi parolayı zaten kanıtlamıştır ve bağlantı şifrelidir — sayfa bu yüzden **düz bir form** olabilir.

Bedeli açıkça yazıldı: cihaz o parolayı düz metin saklamak zorunda. Bu yeni bir maruziyet açmıyor ve iddia ölçülü — flash'ı okuyabilen biri kullanıcının ev Wi-Fi parolasını zaten `nvs`'ten alıyor; 5 Eylül oturumu tam olarak bunu yaparak (24 KB `nvs` geri yazarak) ağa döndü.

Portal **ikinci bir kurulum yolu değil**: aldığı bilgiyi `wifi_prov_mgr_configure_sta()` ile yöneticiye veriyor — ESP-IDF'in tam bu iş için belgelediği API — yani bağlanma, yeniden deneme, başarı olayı ve pencerenin kapanması BLE yolunun sürdüğü tek durum makinesinde kalıyor.

Düşmanca girdinin indiği yer form ayrıştırıcısı, o yüzden altında ESP-IDF olmayan saf C ve ayrı testli: kaçış dizileri, sınırlar, araya sıkışmış NUL, tekrarlanan anahtar (çözülmüyor, **reddediliyor** — "hangi ağ" sorusuna iki cevap arasından galip seçilmez) ve geride kullanılabilir parça bırakmayan ret.

Captive DNS her adı cihaza çözüyor; yoklama adresleri joker yerine tek tek listelendi, çünkü aynı sunucuda protocomm'un uç noktaları duruyor ve bir joker `/prov-session`'a HTML sayfası döndürüp çalışan yolu bozardı.

## 4. Kalibrasyon profili: `G0` gelince doldurulacak struct (F3 hazırlığı)

Filtreler ve limiter aylar önce yazılmıştı ve ikisi de frekans/tavan uydurmayı reddediyor. Eksik olan, onlara gerçek sayıları verecek şeydi. `hk_profile` o biçimi tanımlıyor: alanlar, reddettikleri ve zincire dönüşmesi. İçinde tek bir sürücü değeri yok.

İki nokta struct'ın kendisinden değerli:

**Profil kendi kaynağını taşıyor.** Ölçülen DC dirençler saklanıyor ama çalışma zamanı onlarla hesap yapmıyor — crossover'ı türetmek tezgâhta insanın işi, cihaz sonucu taşır. Sonucun neyden türetildiğini kaydetmek profili ölçüme bağlanabilir kılar; ölçümünü adlandıramayan profil reddediliyor.

**Tavanın yanında bir gerilim var.** Tavan dijital, sürücüye ulaşan volt, ve class-D bir amfide volt beslemeyi izler — burada besleme 16,8 V'tan 12,0 V'a düşen bir batarya. Tek bir saklanmış tavan sürücüyü tek bir şarj durumunda korur. Profil bu yüzden tavanı ölçüldüğü gerilimle saklıyor, `hk_profile_ceiling_at()` o anki gerilime taşıyor. Yön testte ayrıca iddia ediliyor: paket **doluyken** dijital tavan **aşağı** inmeli. Ters yazılsa sonuç "biraz kısık hoparlör" gibi değil, yalnız batarya doluyken ölen bir tweeter gibi görünürdü.

Bu, F3'ün "limiter tam dolu ve düşük bataryada ayrı ayrı doğrulandı" ölçütünün firmware yarısıdır.

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | 115 dosya, 0 hata |
| Host testleri | 393.388 kontrol, 0 hata |
| `check_no_credential_logs.py` | 0 sorun (bir bulgusu düzeltilerek) |
| `check_storage_isolation.py` | 0 sorun |
| Ürün profili derlemesi (ESP-IDF v5.5.1) | geçti; 1.324.336 B, slotun %81,6'sı boş |
| Geliştirme profili derlemesi | geçti; 1.674.592 B, slotun %44,5'i boş |
| `check_partitions.py` (iki tablo) | 0 sorun |

Portal, geliştirme imajını 31 KB büyüttü.

Kimlik bilgisi günlüğü denetleyicisi bir satırımı yakaladı ve haklıydı: kuralı gevşetmek yerine satır, üstündeki salt/verifier satırıyla aynı biçime çevrildi — anahtarın kendisi değil **uzunluğu** basılıyor.

## Açık riskler ve sonraki adım

- **Portalın hiçbir telefonda denenmediği.** Bugün kanıtlanan: iki profil derleniyor, ayrıştırıcı testli. Captive portal algılamasının iOS ve Android'de gerçekten tetiklendiği ölçülene kadar PRD-004 kapanmaz.
- **WPA2 kurulum ağının BLE yolunu bozmadığı** varsayılıyor (BLE'de `service_key` anlamsız), ama doğrulanmadı.
- **Verifier değişti** (ADR-0014), yani 5 Eylül'de çalışan QR'lı BLE kurulumu tekrar doğrulanmalı.
- **Profil boş.** `G0` iki DC direnç, subsonic köşe, crossover köşesi ve iki dal kazancı; `G2` iki tavan ve ölçüldükleri paket gerilimi verecek. Kod tarafında değişecek bir şey yok.
- Hiçbir fiziksel kapı açılmadı. `G0`-`G8` duruyor.
