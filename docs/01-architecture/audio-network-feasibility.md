---
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [airplay, feasibility, gate]
---

# AirPlay ve senkron fizibilitesi

Yığın seçildi ve gerekçesi [[../07-decisions/ADR-0007-airplay-stack|ADR-0007]]'de kayıtlıdır: `rbouteiller/airplay-esp32`, sabit commit'e sabitlenerek vendor edilir.

Bu sayfa neyin kanıtlandığını ve neyin hâlâ ölçülmediğini ayırır.

## Kanıtlanan

Birincil kaynaklardan (kaynak kodu, lisans dosyaları, Espressif'in kendi kayıtları) doğrulandı:

- Seçilen yığın **gerçek bir AirPlay 2 alıcısıdır**. HomeKit SRP-6a eşleşmesi, FairPlay `/fp-setup`, IEEE-1588 PTP dinleyicisi ve `SETPEERS` / `SETRATEANCHORTIME` sınıfı AirPlay 2 metotları kaynakta mevcuttur. PTP, multiroom gruplamanın saat mekanizmasıdır.
- Yığın **ESP32-S3 üzerinde ESP-IDF v5.5.1 ile derleniyor**: 1.460.192 baytlık imaj, bir OTA slotumuzun %20,3'ü; 136.495 bayt statik DIRAM.
- **Lisanssız bir alıcı için gruplama bilinen sınırlamalar arasında değildir.** `shairport-sync`'in kendi "What Does Not Work" listesi multiroom içermiyor.
- ESP32 için **başka açık AirPlay 2 alıcısı yok**; diğer her şey AirPlay 1 (RAOP). Espressif'in AirPlay'i yok, çoklu-oda çözümü (ESP MRM) iPhone'u kaynak olarak kabul etmiyor.

## Kanıtlanmayan

Aşağıdakiler donanım gerektirir ve **yapılmadı**:

- Dört hedefin bir Apple cihazında birlikte seçilebilmesi.
- Cihazlar arası gerçek gecikme ve 2 saatlik kayma.
- Çalışma zamanı heap/PSRAM ve CPU yükü. Yukarıdaki rakamlar statik derleme çıktısıdır; ses akarken durum farklıdır.
- Paket kaybı, yeniden bağlanma ve tek cihazın kapanması davranışı.

Bunlar `G7` kapısıdır. Ölçüm yöntemi ve sayısal eşikler [[../07-decisions/ADR-0007-airplay-stack#G7 senkron kabul kriteri|ADR-0007'de]] kilitlenmiştir; özet olarak ölçüm elektrikseldir (DAC çıkışından çapraz korelasyon), akustik değildir, çünkü `1 ms ≈ 34 cm` yayılma gecikmesi mikrofon yerleşimi hatasının altında kalmaz.

## Geri çekilme yolu

Seçilen yığın AirPlay 1 (RAOP) yolunu paralel olarak korur ve `CONFIG_AIRPLAY_FORCE_V1` ile AirPlay 2 derleme dışı bırakılabilir. `G7` başarısız olursa cihazlar tek tek AirPlay 1 alıcısı olarak çalışmayı sürdürür; senkron çoklu-oda özelliği düşer ve PRD-002 yeniden müzakere edilir.

## Duran riskler

- FairPlay yanıtları sabit kodludur; bir iOS/macOS güncellemesi eşleşmeyi kırabilir.
- Apple AirPlay 2 spesifikasyonunu yayımlamamıştır; her açık uygulama gözlemlenen davranıştan türetilmiştir.
- Yığının lisansı ticari olmayan kullanımla sınırlıdır ve bu projeyi kalıcı olarak bağlar.
