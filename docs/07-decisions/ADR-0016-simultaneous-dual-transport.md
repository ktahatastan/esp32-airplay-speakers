---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-13
supersedes: ADR-0015
tags: [adr, provisioning, ble, softap, transport]
---

# ADR-0016: İki taşıma aynı anda açılır

> [!note] 2026-09-13: Aşağıdaki "WPA2 kurulum ağı … aynen geçerlidir" ve "Uygulama yolu, Security 2'si … olduğu gibi kalır" cümleleri [[ADR-0023-pinless-provisioning|ADR-0023]] ile aşıldı — kurulum ağı açık, yönetici Security 1 ile ve sahiplik kanıtı olmadan başlıyor. Bu kararın konusu olan taşıma kuralı aynen geçerlidir ve durum `accepted` kalır.

[[ADR-0015-softap-captive-portal|ADR-0015]]'in **taşıma kuralının** yerine geçer. O kararın WPA2 kurulum ağı, `hk_portal` ve `wifi_prov_mgr_configure_sta()` seçimleri aynen geçerlidir — değişen, hangi taşımanın ne zaman açıldığıdır.

## Bağlam

[[ADR-0005-dual-provisioning|ADR-0005]] iki taşımayı **sırayla** sunmaya karar vermişti ve gerekçesi tek cümleydi: `wifi_prov_mgr` tek bir statik bağlam tutar ve yapılandırmasında tek bir scheme alır. Bu doğruydu ve hâlâ doğrudur.

Yanlış olan, ondan çıkarılan sonuçtu. ADR-0015 portalı protocomm'a değil `wifi_prov_mgr_configure_sta()`'ya bağladı — ESP-IDF'in tam bu iş için belgelediği API'ye. O günden beri **SoftAP ayağı yöneticiden geçmiyor**: ne scheme'e, ne protocomm örneğine, ne de ikinci bir yöneticiye ihtiyacı var. Yani tek-scheme tekilliği **tek** taşımaya hizmet etmek zorunda, ikisine değil.

Kısıt kalkmadı; ADR-0015 onu farkında olmadan aşmıştı. ADR-0005'in "ikisi bir oturumda birlikte açılamaz" cümlesi bugün fazla geniştir ve bu ADR onu düzeltir.

Sahibin gereksinimi de netti: telefonunda BLE varsa BLE ile, yoksa SoftAP ile kurmak istiyor, ve seçimi cihazın değil kendisinin yapmasını istiyor.

## Karar

**Kimlik bilgisi olmayan bir cihaz, açılışın ilk saniyesinden itibaren iki taşımayı birden sunar.** Telefon hangisini destekliyorsa onu kullanır.

- Yöneticiyi **BLE** sürer. Uygulama yolu, Security 2'si ve uç noktalarıyla birlikte olduğu gibi kalır.
- **Erişim noktasını ve portalı `hk_network` kaldırır.** `start_setup_ap()`, ESP-IDF'in `scheme_softap.c`'sinden birebir aktarılmıştır; yaptığı iş bir arayüz ve bir sunucudur, provisioning şeması değil.
- `hk_network_scheme_for()` artık bir şey türetmiyor: yalnız **yöneticinin** hangi taşımayı sürdüğünü söylüyor, ve o her zaman BLE.

### Yöneticinin geri almasını engelleyen tek satır

BLE şeması `WIFI_MODE_STA` ilan eder, ve yönetici kimlik bilgisi geldiğinde bu ilanı **yeniden uygular** — yani erişim noktasını bizim sahibi olmadığımız bir kod yolundan, kurulumun ortasında düşürürdü.

Şema yöneticinin yapılandırmasına **değer olarak** kopyalanıyor, o yüzden kopyayı değiştirmek yetiyor:

```c
config.scheme.wifi_mode = WIFI_MODE_APSTA;
```

Alternatif, modu her seferinde geri almaktı; o bir yarıştır ve bazen kaybedilir.

## Sonuçlar

### BLE açılış başına bir kez

`WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM`, pencere kapanınca Bluetooth yığınını serbest bırakıyor ve **bu geri alınamıyor**. Yani dürüst ürün cümlesi şudur: *ilk açılışta ikisi de; BLE o pencere kapanana kadar.* Pencere kapandıktan sonra yeniden açılan bir kurulum yalnız SoftAP sunar.

Alternatif `CONFIG_WIFI_PROV_KEEP_BLE_ON_AFTER_PROV` ile BT belleğini tüm oturum boyunca tutmaktı — ADR-0005'in bilerek reddettiği şey, ve reddi hâlâ geçerli: BLE normal çalışmada ayakta kalmaz.

### Ölçülen bellek

Ürün kartında, 2026-09-08:

| An | Dahili boş | En büyük blok |
|---|---:|---:|
| Radyolardan önce | 285.923 B | 188.416 B |
| **İki taşıma da ayakta** | **148.007 B** | 73.728 B |

İkisi birlikte ~138 KB tutuyor. Bu bir kabul değil, bir başlangıç noktası: penceredeyken AirPlay alıcısı çalışmıyor, ve pencere kapandığında BT belleğinin gerçekten geri geldiği **henüz ölçülmedi**.

### Tarama çakışması

Portal kendi taramasını yapıyor, yöneticinin `prov-scan` uç noktası kendi taramasını. İki telefon aynı anda ağ listesi isterse biri boş liste alır. Kod bunu zaten öngörüyor ve boş liste + log satırı olarak bitiriyor; yanlış ya da eksik bir SSID üretmiyor. Kabul edilen bir bozulma, engelleyici değil.

### Yol boyunca kapatılan bir kusur

`wifi_prov_mgr_start_provisioning()` istasyon yapılandırmasını RAM'de siliyor ve yalnız **başarısız** olduğunda geri koyuyor. Başarı yolunda boş kalıyor, `wifi_prov_mgr_is_provisioned()` o canlı yapılandırmayı okuduğu için açılışın geri kalanında "kurulmamış" diyor, ve pencere kapanınca yapılacak yeniden bağlanmanın bağlanacağı bir şey kalmıyor. Yapılandırma artık başlatmadan sonra geri konuyor. Bu kusur çift taşımadan önce de vardı.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| İki protocomm örneği (ADR-0005'in "özel protocomm katmanı"ı) | Mümkün — protocomm'un iki taşıması birbirinden habersiz. Ama yüzlerce satır ve `prov-ctrl` gibi bazı işleyiciler yalnız özel başlıklarda. Seçilen yol uygulama yolunun tamamını yöneticiden bedavaya alıyor; bu seçenek onun sağladığı hiçbir şeyi eklemiyor. |
| Kısa basışla taşıma değiştirmek | Kullanıcı seçiyor ama **bir kez ve tek yönde**: `FREE_BTDM`'den sonra SoftAP'tan BLE'ye dönüş yok. Ayrıca bağlı bir telefonu oturumun ortasında düşürüyor. |
| Sıralı kalmak (bugünkü hâl) | Gereksinimi karşılamıyor. Üstelik `CONFIG_HK_FIRST_BOOT_BLE` ile yeni bir cihazda uygulamasız yol **hiç erişilebilir değildi**; çift taşıma bunu da kapatıyor. |

## Doğrulama

Ürün kartında ölçülen (2026-09-08): tek açılış, panik yok, ve tek pencerede

```text
NimBLE: GAP procedure initiated: advertise
wifi_prov_mgr: Provisioning started with service name : Merzarkabul-932C
wifi:mode : sta + softAP
esp_netif_lwip: DHCP server started ... 192.168.4.1
hk_portal: setup page open at http://192.168.4.1/
hk_net: provisioning open on both: ble "Merzarkabul-932C" and softap "Merzarkabul-Setup-932C"
```

**Kapanmayanlar** ve hepsi operatör kaydı ister:

- İki taşımadan **her biriyle ayrı ayrı** kurulumun tamamlanması, diğeri o sırada ayaktayken.
- Pencere kapandıktan sonra dahili belleğin katılmış hâline dönmesi.
- Pencere kapanıp yeniden açıldığında yalnız SoftAP sunulması, ve logun BLE iddia etmemesi.
- İki telefon aynı anda tarama istediğinde davranış.
