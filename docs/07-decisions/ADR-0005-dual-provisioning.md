---
status: accepted
decision: accepted
owner: firmware-engineer
updated: 2026-08-31
tags: [adr, provisioning]
---

# ADR-0005: Çift provisioning

Uygulamasız SoftAP/captive portal ile güvenli BLE Unified Provisioning birlikte sunulur. Özel Apple/Android sistem kartı ancak companion app ile sağlanır; platform kimliği taklit edilmez.

## Karar: iki yol sırayla sunulur (seçenek C)

ESP-IDF'in provisioning yöneticisi tek statik bağlam tutar ve yapılandırmasında tek bir scheme alır. `scheme_ble` Wi-Fi'yi `WIFI_MODE_STA`'ya, `scheme_softap` `WIFI_MODE_APSTA`'ya alır; ikisi bir oturumda birlikte açılamaz. Kaynak: ESP-IDF v5.5.1 `components/wifi_provisioning/src/manager.c` (tek `prov_ctx`), `scheme_ble.c:337`, `scheme_softap.c:185`.

Bu nedenle iki yol **aynı anda** değil, **sırayla** sunulur. Hangisinin açılacağını çağıran değil, provisioning'e nasıl girildiği belirler:

| Giriş | Transport | Neden |
|---|---|---|
| Kayıtlı kimlik bilgisi yok (ilk açılış, ağ sıfırlama sonrası, fabrika sıfırlama sonrası) | **SoftAP + captive portal** | Uygulamasız yol her zaman erişilebilir olmalı. Hoparlörü ilk kez kuran kişide uygulama olmayabilir. Zaten çalışan bir ağ da yoktur. |
| Yapılandırılmış cihazda butonla açılan pencere | **BLE** | Cihaz çalışıyor ve kullanıcının telefonu bir ağa bağlı. SoftAP açmak telefonu o ağdan koparır; BLE koparmaz. |

Uygulamasız yol yapılandırılmış bir cihazda da erişilebilir kalır: butonu 5 saniye basılı tutmak Wi-Fi kimlik bilgilerini siler ve cihazı yukarıdaki ilk satıra döndürür, yani SoftAP'a.

### Sonuçlar

- Firmware'de scheme bir parametre değildir; `hk_network_scheme_for()` durumdan türetir. Böylece bir çağrı yeri yanlışlıkla farklı bir transport seçemez.
- BLE için `CONFIG_BT_ENABLED` ve `CONFIG_BT_NIMBLE_ENABLED` açıldı. NimBLE seçildi: bu ürün yalnız BLE kullanıyor ve NimBLE belirgin biçimde küçük. Yığın provisioning bittiğinde `FREE_BTDM` ile serbest bırakılır, yani flash maliyeti kalıcı, RAM maliyeti değil.
- Kullanıcı, yapılandırılmış bir cihazda uygulamasız kuruluma geçmek için önce Wi-Fi'yi silmek zorundadır. Bu bilinçli bir takas: alternatifi, çalışan bir hoparlörün butona her basıldığında telefonları ağdan koparan bir erişim noktası açmasıydı.

### Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| Yalnız SoftAP | Uygulamalı kurulumun kolaylığı tümüyle kaybolur; çalışan bir cihazda her yeniden yapılandırma telefonu ağdan koparır. |
| Yalnız BLE | Uygulama kuramayan veya kurmak istemeyen kullanıcı cihazı hiç kuramaz. Uygulamasız yol ürün gereksinimidir (PRD-004). |
| Özel protocomm katmanı | Tek protocomm örneğine iki transport bağlamak mümkün olabilir ama ESP-IDF'in desteklemediği bir yol; bakım yükü, kazancı olan kolaylığa değmiyor. |
