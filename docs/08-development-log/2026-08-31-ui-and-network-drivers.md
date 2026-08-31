---
title: F5 UI sürücüsü ve F4 ağ katmanı
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, f4, f5, wifi, provisioning]
---

# 2026-08-31 — UI sürücüsü ve ağ katmanı

## Amaç

Donanım gelene kadar sürücü katmanlarını yazmak; doğrulama karta kavuşunca tek tek yapılacak. Bu kayıt **yazılanı** ve **neyin kanıtlanmadığını** ayırır.

## Eklenenler

| Bileşen | İş |
|---|---|
| `hk_ui` | Buton GPIO'su ve RGB PWM'i. Test edilmiş `hk_button` ve `hk_led` politikalarını donanıma bağlar. |
| `hk_network` | Wi-Fi istasyonu, yeniden bağlanma, mDNS ve provisioning transport'u. `hk_provision` politikasını donanıma bağlar. |

`hk_ui` kendi düşük öncelikli görevinde çalışıyor. Kontrol planı LED animasyonunun ses göreviyle aynı görevde çalışmamasını açıkça istiyor: oradaki uzun bir adım I2S underrun olarak görünür.

LED PWM taşıyıcısı **25 kHz** seçildi — işitme bandının üstünde, çünkü LED hatları analog ses yolu ve Class-D amfiyle aynı kutuda. Bu bir gerekçe, ölçüm değil; nihai değeri G3 gürültü ölçümü belirler.

Parlaklık gama düzeltmeli: algılanan parlaklık ışığın karekökü gibi davrandığından doğrusal duty rampası "parlak flaş sonra hiçlik" gibi okunur. Kareleme, nefes efektini nefes gibi gösteriyor.

## İki kasıtlı ret

**Provisioning kimlik bilgisi yoksa açılmıyor.** Security 2 (SRP6a) salt ve verifier'ı cihaza özeldir ve `factory_cal` alanından okunur; parolanın kendisi cihazda hiç saklanmaz. Yoksa firmware provisioning'i **açmayı reddediyor** ve daha zayıf bir moda düşmüyor. Wi-Fi parolası kabul eden bir cihazda ortak veya eksik bir kimlik bilgisi, hiç provisioning olmamasından kötüdür.

Bunları yazan üretim aracı henüz yok; risk kaydına girdi.

**BLE transport'u `CONFIG_BT_ENABLED` istiyor** ve bu derlemede kapalı. Örtük bırakmak yerine açıkça koşullu yapıldı: BLE istenirse `ESP_ERR_NOT_SUPPORTED` dönüyor, sessizce başka bir transport kullanmıyor.

## ADR-0005 kararı: seçenek C

`F4`'ü yazarken ortaya çıktı: **ESP-IDF'in provisioning yöneticisi BLE ve SoftAP'ı aynı anda açamıyor.** Tek statik bağlam tutuyor ve yapılandırmasında tek bir scheme alıyor; `scheme_ble` Wi-Fi'yi `WIFI_MODE_STA`'ya, `scheme_softap` `WIFI_MODE_APSTA`'ya alıyor.

Kaynak: ESP-IDF v5.5.1 `components/wifi_provisioning/src/manager.c` (tek `prov_ctx`), `scheme_ble.c:337`, `scheme_softap.c:185`.

[[../07-decisions/ADR-0005-dual-provisioning|ADR-0005]] "birlikte sunulur" diyordu; bu haliyle uygulanamıyordu. Dört seçenek gerekçeleriyle sunuldu ve **sırayla sunma (seçenek C)** seçildi.

Hangi transport'un açılacağını çağıran değil, provisioning'e nasıl girildiği belirliyor:

| Giriş | Transport | Neden |
|---|---|---|
| Kayıtlı kimlik bilgisi yok | **SoftAP + captive portal** | Uygulamasız yol her zaman erişilebilir olmalı; ilk kurulumu yapanda uygulama olmayabilir ve zaten çalışan bir ağ yok |
| Yapılandırılmış cihazda buton | **BLE** | Cihaz çalışıyor; SoftAP açmak kullanıcının telefonunu bağlı olduğu ağdan koparır, BLE koparmaz |

Uygulamasız yol yapılandırılmış cihazda da erişilebilir kalıyor: 5 saniyelik basış Wi-Fi'yi siler ve cihazı ilk satıra döndürür.

Kodda scheme bir parametre değil; `hk_network_scheme_for()` durumdan türetiyor, böylece bir çağrı yeri yanlışlıkla farklı transport seçemiyor. BLE için NimBLE etkinleştirildi (bu ürün yalnız BLE kullanıyor ve NimBLE belirgin biçimde küçük); yığın provisioning bitince `FREE_BTDM` ile serbest bırakılıyor.

## Diğer kararlar

- `dependencies.lock` artık Git'te. Yönetilen bileşenlerin (mDNS) sürümünü sabitliyor; tekrar üretilemeyen bir derleme derleme sayılmaz.
- `scripts/check_docs.py` artık `managed_components/` ve `build/` altını taramıyor — üçüncü taraf belgeleri bizim denetleyeceğimiz şey değil.
- **ESP-IDF tuzağı bulundu ve belgelendi:** `sdkconfig.defaults` değişikliği mevcut `sdkconfig`'e uygulanmıyor. ESP-IDF varsayılanları yalnız dosyayı ilk ürettiğinde okuyor, dolayısıyla yerel bir derleme yeni açtığınız ayarı sessizce yok sayıyor. NimBLE'ı etkinleştirdiğimde imaj hiç büyümediği için fark edildi. CI etkilenmiyor (temiz checkout'tan başlıyor); `firmware/README.md`'ye uyarı eklendi.
- Butonun yıkıcı dalları hâlâ yalnız log basıyor. Depolama katmanı `F6`; yarı kurulu bir silme yolunu fiziksel bir butona bağlamak, kullanıcının kalibrasyonunu kazayla kaybetmesinin yoludur.

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 92 dosya, 0 hata |
| Host birim testleri | **PASS** — 10.178 kontrol, 0 hata |
| Partition kapısı ve kendi testleri | **PASS** |
| `idf.py -C firmware build` (sıfırdan, `sdkconfig` dahil) | **PASS** — 0 uyarı; imaj 1.117.152 bayt, slotun **%84,5'i boş** |
| **Donanımda çalıştırma** | **YAPILMADI** |

İmaj 246 KB'dan 1,12 MB'a çıktı: Wi-Fi, provisioning, mDNS ve mbedtls 922 KB'a, NimBLE de 1,12 MB'a taşıdı. Bütçe yine de rahat — AirPlay yığını için ölçülen 1,46 MB eklense bile bir slotun yarısından azı dolar.

## Kanıtlanmamış olanlar

Politika modüllerinin altındaki her şey **yazıldı ama çalıştırılmadı**. Kart geldiğinde tek tek doğrulanacaklar:

- Buton GPIO'su gerçekten okunuyor mu; debounce sahada nasıl davranıyor.
- RGB PWM'i doğru renkleri veriyor mu; 25 kHz taşıyıcı ses tabanına giriyor mu (G3).
- Wi-Fi katılımı, yeniden bağlanma ve mDNS keşfi.
- SoftAP provisioning akışı iOS ve Android'de.
- BLE provisioning akışı Espressif uygulamasıyla; ve provisioning bitince NimBLE yığınının gerçekten serbest bırakıldığı.
- Provisioning sonrası belleğin gerçekten geri kazanıldığı.
