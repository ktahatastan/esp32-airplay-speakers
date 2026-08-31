---
status: accepted
decision: accepted
owner: firmware-engineer
updated: 2026-08-31
tags: [adr, provisioning]
---

# ADR-0005: Çift provisioning

Uygulamasız SoftAP/captive portal ile güvenli BLE Unified Provisioning birlikte sunulur. Özel Apple/Android sistem kartı ancak companion app ile sağlanır; platform kimliği taklit edilmez.

## Açık soru: "birlikte" nasıl sunulacak

`F4` uygulaması sırasında bulundu: ESP-IDF'in provisioning yöneticisi tek statik bağlam tutar ve tek bir scheme alır. `scheme_ble` Wi-Fi'yi `WIFI_MODE_STA`'ya, `scheme_softap` `WIFI_MODE_APSTA`'ya alır — ikisi bir oturumda birlikte açılamaz. Kaynak: ESP-IDF v5.5.1 `components/wifi_provisioning/src/manager.c`, `scheme_ble.c:337`, `scheme_softap.c:185`.

Bu ADR'nin "birlikte sunulur" ifadesi bu haliyle uygulanamaz. Seçenekler:

| Seçenek | Ne demek | Bedeli |
|---|---|---|
| A. Yalnız SoftAP | Uygulamasız captive portal; BLE hiç kullanılmaz | Uygulamalı kurulum kolaylığı kaybolur. Bluetooth yığını hiç derlenmez: flash ve RAM tasarrufu. |
| B. Yalnız BLE | Espressif provisioning uygulamasıyla kurulum | Uygulamasız yol kaybolur; uygulama kuramayan kullanıcı cihazı kuramaz. |
| C. Sırayla | İlk açılış SoftAP, butonla açılan pencere BLE (veya tersi) | İki yol da var ama kullanıcı hangisinin ne zaman aktif olduğunu bilmek zorunda. |
| D. Özel protocomm katmanı | Tek protocomm örneğine iki transport bağlanır | En çok iş; ESP-IDF'in desteklemediği bir yolda bakım yükü. |

Firmware şimdilik scheme'i parametre olarak alıyor ve **SoftAP varsayılan**. Karar verilene kadar bu ADR'nin "birlikte" ifadesi uygulanmamış sayılır.
