---
status: draft
owner: orchestrator
updated: 2026-08-30
tags: [requirements]
---

# Gereksinimler

| Kimlik | Gereksinim | Kabul kanıtı |
|---|---|---|
| PRD-001 | Dört cihaz ağda ayrı adlarla görünmeli | mDNS/AirPlay keşif kaydı |
| PRD-002 | Dört cihaz hedef modda duyulabilir drift olmadan senkron kalmalı | 2 saatlik G7 ölçümü |
| PRD-003 | Her kutu woofer ve tweeter'ı ayrı korumalı kanaldan sürmeli | Şema, crossover ve limiter testi |
| PRD-004 | Wi-Fi kurulumu BLE veya captive portal ile yapılabilmeli | iOS/Android test raporu |
| PRD-005 | 5 sn ağ reseti, 12 sn kullanıcı reseti çalışmalı | Durum makinesi testi |
| PRD-007 | Besleme kesilirken veya brownout'ta pop üretmeden güvenli kapanmalı | G8 raporu |
| PRD-008 | Kullanıcı reseti fabrika DSP kalibrasyonunu silmemeli | NVS testi |

Bir agent tek başına gereksinim silemez; değişiklik kabul edilmiş ADR gerektirir.
