---
status: active
owner: orchestrator
updated: 2026-08-30
tags: [risk, fmea]
---

# Risk kaydı

| Risk | Etki | Olasılık | Azaltma | Sahip |
|---|---|---|---|---|
| AirPlay yığını multiroom desteklemiyor | Kritik | Yüksek | Erken G7 prototipi ve ADR-0007 | firmware |
| Sürücü empedansı/rezonansı bilinmiyor | Kritik | Yüksek | G0 ve düşük seviyeli G2 | acoustics |
| Tweeter'a DC/düşük frekans gider | Kritik | Orta | HPF, limiter, mute | hardware |
| BMS ilan akımı gerçek değil | Kritik | Orta | Teknik doğrulama, yük testi, sigorta | power |
| Hücre termal kaçağı | Kritik | Düşük-Orta | Eşleme, NTC, profesyonel puntalama, G4/G5 | power |
| Buck/LED/Wi-Fi ses gürültüsü | Yüksek | Orta | Yerleşim, yıldız toprak, filtre, G3 | hardware |
| Seçilen KM103 güç anahtarının 16,8 V DC kontak değeri belgesiz | Kritik | Orta | Satıcıdan yazılı değer, LED pinini ayırma, akım sınırlı G3 yük/ark testi; başarısızsa DC-rated alternatif | hardware |
| OTA sırasında brick | Yüksek | Orta | A/B OTA, düşük batarya kilidi, G6 | firmware |
| Paralel agent çakışması | Orta | Orta | Tek orkestratör ve tek yazma sahibi | orchestrator |
