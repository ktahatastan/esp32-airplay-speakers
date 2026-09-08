---
status: superseded
decision: superseded
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-08
supersedes: ADR-0005
superseded_by: ADR-0016
tags: [adr, provisioning, softap, captive-portal, security, wpa2]
---

# ADR-0015: Kurulum ağı WPA2 olur ve portalı kendimiz sunarız

> [!warning] Taşıma kuralı [[ADR-0016-simultaneous-dual-transport|ADR-0016]] ile aşıldı.
> WPA2 kurulum ağı, `hk_portal` ve `wifi_prov_mgr_configure_sta()` seçimleri aynen geçerlidir. Aşılan tek şey "hangi taşıma ne zaman açılır" sorusudur: ikisi artık birlikte açılıyor. İronik olan, bunu mümkün kılanın bu kararın kendisi olması — portalı protocomm'dan çıkarmak, tek-scheme sınırını SoftAP ayağının üstünden kaldırdı.

[[ADR-0005-dual-provisioning|ADR-0005]]'in yerine geçer. Onun **taşıma seçimi** aynen korunur; değişen, SoftAP ayağının içinin ne olduğudur.

## Bağlam

ADR-0005 "uygulamasız SoftAP/captive portal" sözü verdi ve o portal hiç yazılmadı. 2026-09-05'te sonucu görüldü: kullanıcı kurulum ağına katıldı ve **hiçbir şey açılmadı**. Bu bir hata değildi — `wifi_prov_scheme_softap` sayfa sunmaz, `192.168.4.1` üzerinde yalnız protocomm uç noktaları açar. Yani PRD-004'ün uygulamasız kurulum gereksinimi, yazıldığı günden beri karşılanmıyordu; eksikliği gizleyen şey, o kod yolunun zaten hep bir uygulamayla denenmiş olmasıydı.

Portalı yazmanın önündeki asıl soru şudur: sayfa, kullanıcının **ev Wi-Fi parolasını** nasıl alacak?

Kaynaktan doğrulanan üç kısıt (ESP-IDF `v5.5.1`, bu makinedeki ağaç):

1. **Tarayıcı kriptosu düz HTTP'de yoktur.** `crypto.subtle` yalnız güvenli bağlamda tanımlıdır ve `http://192.168.4.1` güvenli bağlam değildir. Yani Security 2'yi sayfada konuşmak, SHA-512'yi, 3072-bit modexp'i ve AES-GCM'i **elle yazılmış** JavaScript olarak flash'tan servis etmek demektir.
2. **Kurulum ağı parolalı olabilir.** `wifi_prov_mgr_start_provisioning()`'in `service_key` parametresi, SoftAP taşımasında doğrudan Wi-Fi parolasıdır (`manager.h:354`). Bugün `NULL` geçiyoruz; kurulum ağı bu yüzden açık.
3. **HTTP sunucusu paylaşılabilir.** `wifi_prov_scheme_softap_set_httpd_handle()` protocomm'u bizim başlattığımız sunucuya bindiriyor, yani portal sayfası ile uygulama yolu aynı sunucuda yan yana durabilir.

## Karar

**Kurulum ağı WPA2 olur, anahtarı cihaz başına kurulum parolasıdır, ve portalı kendimiz sunarız.**

- SoftAP `service_key` ile açılır. Kurulum ağına katılabilen kişi, parolayı zaten bilen kişidir, ve bağlantı 802.11 katmanında şifrelenir.
- Portal sayfası, captive DNS ve işletim sistemi yoklama adresleri `hk_portal` bileşenindedir; protocomm ile aynı `httpd` örneğine kayıtlanır.
- Sayfa düz bir formdur: ağ listesi ve parola alanı. Gizliliği **bağlantı** sağlar, sayfanın kendi kriptosu değil.
- Alınan kimlik bilgileri `wifi_prov_mgr_configure_sta()` ile yöneticiye verilir. Böylece bağlanma, yeniden deneme, başarı olayı ve pencerenin kapanması **tek** durum makinesinde kalır; portal ikinci bir yol açmaz, mevcut yolun ön yüzü olur.
- **Uygulama yolu değişmez.** BLE + Security 2 aynen durur; protocomm uç noktaları aynı sunucuda yayınlanmaya devam eder.

ADR-0005'ten korunanlar: taşıma çağıranın seçtiği bir parametre değildir, durumdan türetilir (`hk_network_scheme_for()`); kimlik bilgisi yoksa SoftAP, yapılandırılmış cihazda butonla BLE; 5 saniyelik basış cihazı ilk satıra döndürür; BLE yığını NimBLE'dır ve pencere kapanınca serbest bırakılır.

## Sonuçlar

### Cihaz artık bir parolayı düz metin saklıyor, ve bunun bedeli sanıldığından küçük

WPA2-PSK her iki uçta da anahtarı ister. Yani `factory_cal`, SRP6a salt/verifier'ının yanına düz metin bir `ap_pass` alır. Bu, "flash'ı okumak kimlik bilgisini vermez" özelliğini kurulum parolası için kaybettirir.

Kaybedilen şey, ölçüldüğünde zaten yoktu: **flash'ı okuyabilen biri kullanıcının ev Wi-Fi parolasını hâlihazırda alıyor.** ESP-IDF'in Wi-Fi sürücüsü onu `nvs` bölümünde tutar, ve bu bir varsayım değil — 2026-09-05 oturumu tam olarak bunu yaptı: yedekten yalnız `nvs` geri yazıldı ve kart kullanıcının ağına yeniden katıldı. Korunacak şeyin kendisi zaten oradayken, onun yanına bir kurulum PSK'sı koymak yeni bir maruziyet açmıyor.

Korunan şey duruyor: SRP6a parolası hâlâ saklanmıyor, ve **uzaktan** bir saldırgan için hiçbir şey değişmiyor. Değişen yalnız cihazı fiziksel olarak eline alan saldırganın hesabı, ki o hesap zaten kaybedilmişti.

### Uygulamanın SoftAP yolu artık elle katılma istiyor

Espressif'in QR biçiminde kurulum ağının parolası için alan yok (`ver`, `name`, `username`, `pop`, `transport`). Ağ parolalı olunca uygulama QR'dan okuyup kendiliğinden katılamaz.

Bu, bugün var olan bir şeyi kaybetmiyor: uygulamanın SoftAP yolu 2026-09-03'te zaten çalışmıyordu (`mbedtls_gcm_auth_decrypt : -18`, kusur uygulamada). Uygulamayla çalışan yol BLE'dir ve BLE'de `service_key` anlamsızdır — o yol bu karardan hiç etkilenmiyor.

### Portal ikinci bir kurulum yolu değil

`wifi_prov_mgr_configure_sta()` seçilmesinin sebebi budur. Kendi `esp_wifi_set_config()` + `esp_wifi_connect()` yolumuzu yazsaydık, zaman aşımı, yeniden deneme ve pencere kapatma mantığının **ikinci** bir kopyası olurdu; ikisinin ne zaman ayrışacağı da zaman meselesiydi.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| Portalda tam Security 2 (saf JS kripto) | Mimari olarak en tutarlısı, ama bedeli güvenlik-kritik el yazması SHA-512/AES-GCM/modexp'i flash'tan servis etmek. Üstelik sayfanın kendisi düz HTTP'den iniyor: aktif bir saldırgan sayfayı değiştirebiliyorsa, sayfanın içindeki kripto onu durdurmaz. Karmaşıklık gerçek, kazancı değil. |
| Açık SoftAP + düz form | Bu bir düşürmedir ve firmware'in açık özelliğiyle çelişir: kimlik bilgisi yoksa provisioning açılmaz, zayıf moda düşülmez. Açık bir ağda düz form, kullanıcının ev parolasını menzildeki herkese verir. |
| Uygulamasız yolu tümüyle bırakmak | PRD-004'ü düşürür. Kutuyu açan kişinin doğru uygulamayı kurmuş olmasını şart koşmak, bu üründe kabul edilmeyen bir kurulum koşuludur. |

## Doğrulama

Bu kararın hiçbir maddesi donanımda denenmedi. Kapanması için gerekenler:

- Kurulum ağına WPA2 ile katılma, portalın **kendiliğinden açılması** (iOS ve Android), ağ listesinin dolması, ve kimlik bilgisiyle katılma.
- Portalla kurulan cihazın `WIFI_PROV_CRED_SUCCESS` üretmesi ve pencerenin kapanması — yani portalın gerçekten yöneticinin ön yüzü olduğu.
- BLE + Security 2 yolunun bu değişiklikten sonra hâlâ çalışması.
