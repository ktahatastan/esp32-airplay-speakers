---
status: active
owner: orchestrator
updated: 2026-08-31
tags: [risk, fmea]
---

# Risk kaydı

| Risk | Etki | Olasılık | Azaltma | Sahip |
|---|---|---|---|---|
| AirPlay multiroom senkronu ölçülmedi | Kritik | Orta | Yığın seçildi ve AP2/PTP yeteneği kaynakta doğrulandı (ADR-0007); kalan risk gerçek cihazlarda ölçüm. G7 elektriksel ölçümü; başarısızsa AirPlay 1'e geri çekilme | firmware |
| FairPlay yanıtları sabit kodlu; iOS/macOS güncellemesi eşleşmeyi kırabilir | Yüksek | Orta | Sürüm sabitleme, vendor edilmiş kopya, kırılırsa AirPlay 1 yoluna geçiş | firmware |
| Seçilen yığının lisansı ticari olmayan kullanımla sınırlı | Orta | Kesin | Kabul edildi ve ADR-0007'ye yazıldı; release asset'leri lisans metnini taşır | orchestrator |
| Sürücü empedansı/rezonansı bilinmiyor | Kritik | Yüksek | G0 ve düşük seviyeli G2 | acoustics |
| Tweeter'a DC/düşük frekans gider | Kritik | Orta | HPF, limiter, mute | hardware |
| BMS ilan akımı gerçek değil | Kritik | Orta | Teknik doğrulama, yük testi, sigorta | power |
| Hücre termal kaçağı | Kritik | Düşük-Orta | Eşleme, NTC, profesyonel puntalama, G4/G5 | power |
| Buck/LED/Wi-Fi ses gürültüsü | Yüksek | Orta | Yerleşim, yıldız toprak, filtre, G3 | hardware |
| Seçilen KM103 güç anahtarının 16,8 V DC kontak değeri belgesiz | Kritik | Orta | Satıcıdan yazılı değer, LED pinini ayırma, akım sınırlı G3 yük/ark testi; başarısızsa DC-rated alternatif | hardware |
| OTA sırasında brick | Yüksek | Orta | A/B OTA, düşük batarya kilidi, G6 | firmware |
| XL4015 şarj sonlandırma garantisi yok | Kritik | Yüksek | G4'te CV akım düşüşü ve sonlandırma ölçümü; sonlandırma yoksa ADR ile alternatif; ölçümden önce gözetimsiz şarj yasak (ADR-0009) | power |
| XL4015'te ters polarite koruması yok | Yüksek | Orta | Kutup etiketleme, bağlantı sırası prosedürü, ilk enerjilendirmede akım sınırlı kaynak | power |
| BMS pasif balans akımı şarj akımından çok küçük | Orta | Yüksek | Balans akımını/eşiğini satıcıdan yazılı al; G4'te hücre sapmasını çok çevrimde ölç | power |
| Paralel agent çakışması | Orta | Orta | Tek orkestratör ve tek yazma sahibi | orchestrator |
| Belge sapması: kanonik plan ile şema/BOM ayrışması | Yüksek | Orta | `scripts/check_docs.py` birleşme öncesi zorunlu; mimari değişiklik ADR'siz birleşmez | orchestrator |
