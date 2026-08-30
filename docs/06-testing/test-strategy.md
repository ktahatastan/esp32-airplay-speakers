---
status: active
owner: qa-engineer
updated: 2026-08-30
tags: [testing, gates]
---

# Test stratejisi ve kabul kapıları

Her kayıt; seri no, firmware/PCB sürümü, ölçüm cihazı, ortam, önkoşul, adımlar, ham veri yolu, pass/fail ve sorumlu içerir.

| Gate | Kapsam | Geçiş koşulu |
|---|---|---|
| G0 | Sürücü | DC/empedans/polarite verisi kayıtlı |
| G1 | Amfi/dummy-load | Güç, clipping, DC offset ve termal güvenli |
| G2 | Sürücü bring-up | HPF/crossover/limiter doğrulandı |
| G3 | Güç/EMI | Brownout, dip gürültü, pop ve batarya aralığı geçti |
| G4 | Batarya/şarj | BMS, sigorta, NTC, CC/CV ve korumalar geçti |
| G5 | Kapalı kabin | Sürekli sıcaklık ve mekanik güvenlik geçti |
| G6 | OTA/recovery | İmza/manifest, düşük güç erteleme, enerji kesintisi, ilk-boot sağlık kontrolü, rollback ve USB/UART recovery geçti |
| G7 | Dört cihaz | 2+ saat drift/jitter ve yeniden bağlanma geçti |
| G8 | Dayanıklılık | 24 saat soak ve düşük batarya kapanışı geçti |

G0-G5 geçmeden dört batarya paketine çoğaltma yoktur. Manuel dinleme/ürün kabulü kullanıcıya aittir; agent ölçüm ve otomatik test kanıtını raporlar.

Elektriksel bring-up sırasında kullanılacak `TP0-TP27` test noktaları, beklenen gerilim/dalga şekilleri, BTL çıkış ölçüm yöntemi ve osiloskop kanal planı [[../02-hardware/circuit-and-wiring-plan#7. Test noktaları ve osiloskop planı|devre ve bağlantı planında]] tanımlıdır.

G6 senaryoları ve release kabul sözleşmesi [[../03-firmware/ota-and-release-plan#G6 kabul matrisi|OTA ve sürüm yönetimi planında]] tanımlıdır.
