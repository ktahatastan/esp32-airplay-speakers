---
status: blocked
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-30
tags: [airplay, feasibility, gate]
---

# AirPlay ve senkron fizibilitesi

`airplay-esp32` yaklaşımının AirPlay 1 alıcısı mı yoksa AirPlay 2 multiroom/grup alıcısı mı sunduğu, lisansı ve dört cihazdaki saat davranışı kaynak kodu + gerçek testle doğrulanmalıdır.

## Karar kapısı

- Desteklenen AirPlay sürümünü birincil kaynakla kaydet.
- Apple cihazında dört hedefin birlikte seçilebildiğini kanıtla.
- Aynı işareti dört cihazda kaydedip başlangıç farkı ve uzun dönem drift ölç.
- Paket kaybı, yeniden bağlanma ve tek cihaz kapanması senaryolarını çalıştır.
- Açık kaynak lisansını hedef kullanımla karşılaştır.

## G7 ilk hedefi

2 saat kesintisiz oynatma, duyulabilir yankı oluşturmayan başlangıç farkı, zamanla büyümeyen drift ve kontrollü yeniden bağlanma. Sayısal eşikler ölçüm düzeni kurulduğunda ADR-0007 ile kesinleştirilir.
