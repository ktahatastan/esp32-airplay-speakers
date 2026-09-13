---
status: accepted
decision: accepted
owner: orchestrator
updated: 2026-09-13
tags: [adr, identity]
---

# ADR-0001: Ürün kimliği

Proje ve provisioning ürün ailesi adı **Merzarkabul Airplay Speakers** olacaktır. Ürün tek bir hoparlördür: tek kabin, sekiz sürücü, ağda tek cihaz ([[ADR-0021-single-cabinet|ADR-0021]]). Cihaz yüzeylerinde kısa ad `Merzarkabul` kullanılır: AirPlay `Merzarkabul XXXX`, mDNS `merzarkabul-xxxx`; iki kurulum yüzeyi — BLE yayını ve kurulum ağının SSID'si — tek bir ad taşır, `PROV_Merzarkabul-XXXX` ([[ADR-0023-pinless-provisioning|ADR-0023]]: `PROV_` öneki, Espressif'in stok kurulum uygulamalarının cihazı QR'sız ve ayar değiştirmeden listelemesi içindir; kısa ad onun ardında durur). Cihaz yüzeyleri `XXXX` benzersiz son ekiyle [[../controls-and-provisioning-plan]] tablosunu izler; son ek MAC türevidir, çünkü bir ad hangi ağa takılırsa takılsın benzersiz olmalıdır. Kullanıcı AirPlay adını değiştirebilir; ürün ailesi kimliği kalır.
