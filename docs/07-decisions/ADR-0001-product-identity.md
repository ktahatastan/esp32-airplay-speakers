---
status: accepted
decision: accepted
owner: orchestrator
updated: 2026-08-30
tags: [adr, identity]
---

# ADR-0001: Ürün kimliği

Proje ve provisioning ürün ailesi adı **Merzarkabul Airplay Speakers** olacaktır. Ürün tek bir hoparlördür: tek kabin, sekiz sürücü, ağda tek cihaz ([[ADR-0021-single-cabinet|ADR-0021]]). Cihaz yüzeylerinde kısa ad `Merzarkabul` kullanılır: AirPlay `Merzarkabul XXXX`, BLE `Merzarkabul-XXXX`, kurulum ağı `Merzarkabul-Setup-XXXX`, mDNS `merzarkabul-xxxx`. Cihaz yüzeyleri `XXXX` benzersiz son ekiyle [[../controls-and-provisioning-plan]] tablosunu izler; son ek MAC türevidir, çünkü bir ad hangi ağa takılırsa takılsın benzersiz olmalıdır. Kullanıcı AirPlay adını değiştirebilir; ürün ailesi kimliği kalır.
