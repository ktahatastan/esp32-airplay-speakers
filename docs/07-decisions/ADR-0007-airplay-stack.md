---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [adr, airplay, sync, license]
---

# ADR-0007: AirPlay yığını ve senkron kriteri

## Karar

Alıcı yığın olarak [`rbouteiller/airplay-esp32`](https://github.com/rbouteiller/airplay-esp32) (CMake proje adı `airplay2-receiver`) seçilmiştir. Depoya sabit bir commit'e sabitlenerek vendor edilir.

Bu karar **yığın seçimidir**. Dört cihazın gerçekten senkron çaldığı iddiası değildir; o iddia yalnız aşağıdaki `G7` ölçümüyle kurulabilir.

## Kanıt

Bulgular birincil kaynaklardan (kaynak kodu, `LICENSE` dosyaları, Espressif'in kendi issue kayıtları) doğrulandı; pazarlama sayfaları kanıt sayılmadı.

### Aday gerçekten AirPlay 2 alıcısıdır

RAOP'un üzerine giydirilmiş bir AirPlay 1 değildir. Kaynakta:

- HomeKit `pair-setup` / `pair-verify`, gerçek SRP-6a ile (`main/hap/srp.c`: RFC 5054 3072-bit asal, üreteç 5, mbedTLS üzerinde SHA-512).
- FairPlay `/fp-setup` işleyicisi (`main/rtsp/rtsp_fairplay.c`).
- IEEE-1588 **PTP dinleyicisi** (`main/network/ptp_clock.c`, çok noktaya yayın `224.0.1.129`, UDP `319/320`). Multiroom gruplamanın saat mekanizması tam olarak budur.
- RTSP gönderim tablosunda AirPlay 2'ye özgü `SETPEERS`, `SETPEERSX`, `SETRATEANCHORTIME`, `FLUSHBUFFERED` ve `SETRATE` metotları.

AirPlay 1 (RAOP) yolu paralel olarak korunur; `CONFIG_AIRPLAY_FORCE_V1` ile AP2 yolu derleme dışı bırakılabilir. Bu, `G7` başarısız olursa geri çekilecek yolu verir.

### ESP32-S3 üzerinde ESP-IDF v5.5.1 ile derleniyor

Bağımsız olarak, temiz bir dizinde yeniden üretildi:

| Ölçü | Değer | Bizim bütçemiz |
|---|---:|---|
| Uygulama imajı | 1.460.192 bayt (1,39 MiB) | Bir OTA slotunun **%20,3'ü** (`0x6E0000`) |
| Statik DIRAM | 136.495 bayt | 341.760 baytın %39,9'u |
| Karşılaştırma: `F0` iskeletimiz | 232.576 bayt / 56.531 bayt DIRAM | — |

Yer sorun değildir. Derleme başarılı ama **uyarısız değil**: iki uyarı, biri `components/boards/esp32s3-generic/board.c:25` içinde `-Wshift-count-negative`.

### Alternatif yok

- ESP32 için **başka bir açık AirPlay 2 alıcısı bulunmuyor.** GitHub'daki diğer "ESP32 AirPlay 2" depoları bu projenin kopyalarıdır ve aynı lisansı taşır.
- ESP32'de çalışan diğer her şey **yalnız AirPlay 1 (RAOP)**: `squeezelite-esp32` (ESP-IDF v4.3.x'e sabit), `esp-airsync`/`esp-raop-receiver` (GPL-3.0, ESP-IDF v5.5.1, ESP32-S3 N16R8 + PCM5102A), `conduit-stream` (lisanssız, multiroom kapsam dışı).
- **Espressif'in AirPlay'i yoktur.** ESP-ADF'in 15 etiketi ve 3 dalının tamamında `airplay`/`raop` eşleşmesi sıfırdır. 2019 basın bülteni "Integrates ... Airplay" dese de Espressif kendi issue #291'de "it is not included in the latest plan" diyor. Bileşen kayıt defteri her iki terim için de "Components not found" döndürüyor.
- Espressif'in gerçek çoklu-oda ürünü **ESP MRM**'dir: tescilli master/slave çok noktaya yayın (grup `239.255.255.252`, port 1900). ESP32 master olur ve bir URL çalar; kaynak bir iPhone olamaz. Bu haliyle **PRD-002'yi karşılamaz**.

### Lisanssız bir alıcının tavanı gruplamayı içeriyor

Olgun açık alıcı `shairport-sync`, `AIRPLAY2.md` içindeki "What Does Not Work" listesinde altı madde sayar: HD lossless (96/192 kHz), Dolby Atmos, Windows iTunes, uzaktan kumanda, macOS 10.15 öncesi ve aynı ana makinede birden çok AP2 örneği. **Multiroom veya gruplama bu listede yoktur.** Yani lisanssız bir AirPlay 2 alıcısının gruplanabilmesi, bilinen sınırlamaların dışındadır.

Bu, bizim donanımımızda çalışacağının kanıtı değildir; yalnız protokol düzeyinde imkânsız olmadığının kanıtıdır.

## Kabul edilen riskler

| Risk | Neden kabul ediliyor |
|---|---|
| **FairPlay yanıtları sabit kodludur.** `/fp-setup` dört önceden hesaplanmış "FPLY" bloğunu tekrar oynatır. Bu bir MFi/kriptografik uygulama değil, protokol analizidir. Depo README'si de "Not guaranteed to work with future iOS or macOS versions" diyor. | Alternatif yok. MFi bireylere kapalı. Bir iOS güncellemesi bunu kırabilir; risk kaydına kalıcı satır olarak girdi. |
| **Apple AirPlay 2 spesifikasyonunu hiç yayımlamadı.** Spesifikasyon yalnız MFi lisanslılarına NDA altında dağıtılıyor. Her açık uygulama gözlemlenen davranıştan türetilmiştir. | Aynı sebep. Projede protokol taklidi değil, protokol uygulaması yapılıyor; HomeKit/MFi kimliği taklit edilmiyor (ADR-0005). |
| **Proje genç.** Depo 2026-01-22'de açıldı, v0.2.0 güncel sürüm. | Kod okunabilir durumda ve sabit commit'e sabitlenecek. Yukarı akış kaybolursa vendor edilmiş kopya çalışmaya devam eder. |
| **Lisans OSI açık kaynağı değil.** Özel bir "Non-Commercial License" (© 2026 Remi Bouteiller): kullanma, kopyalama, değiştirme ve dağıtma yalnız **ticari olmayan** amaçlarla serbest; ticari kullanım yazılı izin ister. | Bu proje bir kişinin kendi evi için dört hoparlör yapmasıdır. Kalıcı sonucu aşağıda. |

### Lisansın kalıcı sonucu

Bu seçim projeyi **ticari olmayan** kullanıma bağlar. Harman Kardom firmware'i satılamaz veya ticari bir üründe kullanılamaz; bunun için yazarın yazılı izni gerekir.

[[ADR-0008-github-releases-ota|ADR-0008]] release hattı için somut zorunluluk: yayımlanan her firmware asset'i, vendor edilmiş kaynağın lisans metnini ve telif bildirimini taşımalıdır. Bu, `F7` release iş listesine girer.

## G7 senkron kabul kriteri

`G7`, "kulağa senkron geliyor" ile geçilemez. Ölçüm elektrikseldir; akustik dinleme ayrı ve ikincil bir kontroldür.

### Ölçüm yöntemi

Havadan ölçüm yapılmaz: sesin yayılma gecikmesi `1 ms ≈ 34 cm`'dir ve mikrofon yerleşimi hatası ölçülecek büyüklüğü aşar.

1. Dört cihaz aynı AirPlay grubuna alınır ve aynı test sinyali çalınır: `1 kHz` tek darbe treni, tercihen 10 s aralıklı.
2. İki cihazın **DAC analog çıkışından** (`TP14`) eşzamanlı iki kanallı kayıt alınır. Amfi çıkışı kullanılmaz: Class-D anahtarlama ve BTL topolojisi ölçümü bozar.
3. Kanallar arası gecikme `Δt`, çapraz korelasyonun tepe noktasından hesaplanır.
4. Kayıt 2 saat boyunca 5 dakikada bir tekrarlanır. Her cihaz çifti en az bir kez ölçülür.
5. Aynı düzende paket kaybı, yeniden bağlanma ve tek cihazın kapatılması senaryoları çalıştırılır.

### Eşikler

| Ölçüm | Eşik | Gerekçe |
|---|---|---|
| Kararlı durumda cihazlar arası `abs(Δt)` | **≤ 1 ms** | Farklı odalarda kullanım. Bu aralıkta öncelik etkisi (Haas) tek bir kaynak izlenimini korur; ayrı yankı duyulmaz. |
| `Δt`'nin 2 saatteki değişimi | Eşiği aşacak şekilde **büyümemeli** | Saat kayması varsa süre uzadıkça duyulur hale gelir. Tek bir anlık ölçüm bunu göstermez. |
| Yeniden bağlanma | Kesintiden sonra **≤ 30 s** içinde gruba geri dönmeli ve eşiğe oturmalı | Kullanıcı müdahalesi gerekmemeli. |
| Tek cihaz kapatma | Kalan cihazlar eşik içinde kalmalı | Bir hoparlörün kapanması grubu bozmamalı. |

**Stereo çift seçilirse eşik değişir.** İki hoparlör aynı odada ilişkili içerik çalıyorsa `Δt` tarak filtrelemesi üretir; ilk çentik `1/(2Δt)` frekansındadır, yani `1 ms` gecikmede `500 Hz`. Bu duyulur. Stereo çift yapılandırması için hedef **≤ 100 µs**'dir ve bunun Wi-Fi üzerinde ulaşılabilir olduğu **ölçülmeden varsayılmaz**. Ölçüm gösterirse bu ürün seçeneği düşer; bu bir ürün kararıdır ve TODO P0'daki "dört bağımsız mono kutu mu, iki stereo çift mi" sorusuna bağlıdır.

Eşikler ilk `G7` ölçümünden sonra gözden geçirilebilir. Değişirse bu ADR'yi supersede eden yeni bir ADR gerekir.

## Hâlâ kanıtlanmamış olan

Bu ADR yığını seçer. Aşağıdakiler **ölçülmedi** ve donanım gerektirir:

- Dört hedefin bir Apple cihazında **birlikte seçilebildiği** gösterilmedi.
- Çalışma zamanı heap/PSRAM kullanımı ve CPU yükü ölçülmedi; yukarıdaki rakamlar statik derleme çıktısıdır.
- Gerçek senkron davranışı ölçülmedi.

PRD-002 bu ölçümler yapılana kadar tamamlanmış sayılmaz. Ayrıntı: [[../01-architecture/audio-network-feasibility|AirPlay ve senkron fizibilitesi]].
