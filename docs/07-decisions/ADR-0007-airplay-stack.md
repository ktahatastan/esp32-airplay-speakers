---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-08
tags: [adr, airplay, license]
---

# ADR-0007: AirPlay yığını

## Karar

Alıcı yığın olarak [`rbouteiller/airplay-esp32`](https://github.com/rbouteiller/airplay-esp32) (CMake proje adı `airplay2-receiver`) seçilmiştir. Depoya sabit bir commit'e sabitlenerek vendor edilir.

> **2026-08-31 notu — vendor'lama henüz yapılmadı ve "bileşen olarak vendor et" ifadesi göründüğü kadar basit değil.**
>
> Yukarı akış incelendi (`v0.2.0`, commit `38027441ff43`, Non-Commercial License © 2026 Remi Bouteiller). Bulgular:
>
> - Yığın bir kütüphane değil, **kendi `app_main`'i olan tam bir ESP-IDF uygulaması** (`main/main.c:209`, 374 satır; AirPlay çekirdeği toplam 23.5k satır). İki `app_main` bir arada bulunamaz.
> - Onların `app_main`'i bizim kabul edilmiş kararlarımızla doğrudan çakışıyor: sürüm uyuşmazlığında `nvs_flash_erase()` çağırıyor (kalibrasyon duvarı, PRD-008), `led_init()` `hk_ui` ile, `iot_board_init()` `hk_pins` ile çakışıyor.
> - `components/` 18 MB ve bunun 17.3 MB'ı kullanmadığımız TI DSP amfilerinin blob'ları (`dac_tas58xx` 11 MB, `dac_tas57xx` 6.3 MB). Bizim DAC'ımız PCM5102A.
>
> Yani entegrasyon, kopyalama değil mimari bir karar: ya onların `app_main`'i `hk_airplay_start()`'a çevrilir (vendor'lanmış kodda kalıcı yama), ya da bizim katmanlarımız onların uygulamasına taşınır (birkaç ADR'yi supersede etmeyi gerektirir). Kullanıcı 2026-08-31'de bu düğümü **donanım gelene kadar ertelemeyi** seçti; entegrasyon gerçekten test edilebildiği zaman kararlaştırılacak. Bu ADR'nin yığın seçimi değişmedi.

Bu karar **yığın seçimidir**. Alıcının bir telefon tarafından bulunup eşleşip akış aldığı iddiası ayrı bir ölçümdür ([[../01-architecture/audio-network-feasibility|fizibilite]], PRD-009); kabul ölçütü aşağıdadır.

## Kanıt

Bulgular birincil kaynaklardan (kaynak kodu, `LICENSE` dosyaları, Espressif'in kendi issue kayıtları) doğrulandı; pazarlama sayfaları kanıt sayılmadı.

### Aday gerçekten AirPlay 2 alıcısıdır

RAOP'un üzerine giydirilmiş bir AirPlay 1 değildir. Kaynakta:

- HomeKit `pair-setup` / `pair-verify`, gerçek SRP-6a ile (`main/hap/srp.c`: RFC 5054 3072-bit asal, üreteç 5, mbedTLS üzerinde SHA-512).
- FairPlay `/fp-setup` işleyicisi (`main/rtsp/rtsp_fairplay.c`).
- IEEE-1588 **PTP dinleyicisi** (`main/network/ptp_clock.c`, çok noktaya yayın `224.0.1.129`, UDP `319/320`). AirPlay 2'nin saat mekanizması budur; alıcı akış sırasında bu saate kilitlenir.
- RTSP gönderim tablosunda AirPlay 2'ye özgü `SETPEERS`, `SETPEERSX`, `SETRATEANCHORTIME`, `FLUSHBUFFERED` ve `SETRATE` metotları.

AirPlay 1 (RAOP) yolu paralel olarak korunur; `CONFIG_AIRPLAY_FORCE_V1` ile AP2 yolu derleme dışı bırakılabilir. Bu, AirPlay 2 yolu bir iOS güncellemesiyle kırılırsa geri çekilecek yolu verir.

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
- ESP32'de çalışan diğer her şey **yalnız AirPlay 1 (RAOP)**: `squeezelite-esp32` (ESP-IDF v4.3.x'e sabit), `esp-airsync`/`esp-raop-receiver` (GPL-3.0, ESP-IDF v5.5.1, ESP32-S3 N16R8 + PCM5102A), `conduit-stream` (lisanssız).
- **Espressif'in AirPlay'i yoktur.** ESP-ADF'in 15 etiketi ve 3 dalının tamamında `airplay`/`raop` eşleşmesi sıfırdır. 2019 basın bülteni "Integrates ... Airplay" dese de Espressif kendi issue #291'de "it is not included in the latest plan" diyor. Bileşen kayıt defteri her iki terim için de "Components not found" döndürüyor.
- Espressif'in kendi grup çalma ürünü **ESP MRM**'dir: tescilli master/slave çok noktaya yayın (grup `239.255.255.252`, port 1900). ESP32 master olur ve bir URL çalar; kaynak bir iPhone olamaz. Bu ürün bir telefonun akış gönderdiği AirPlay hedefidir, o yüzden aday değildir.

## Kabul edilen riskler

| Risk | Neden kabul ediliyor |
|---|---|
| **FairPlay yanıtları sabit kodludur.** `/fp-setup` dört önceden hesaplanmış "FPLY" bloğunu tekrar oynatır. Bu bir MFi/kriptografik uygulama değil, protokol analizidir. Depo README'si de "Not guaranteed to work with future iOS or macOS versions" diyor. | Alternatif yok. MFi bireylere kapalı. Bir iOS güncellemesi bunu kırabilir; risk kaydına kalıcı satır olarak girdi. |
| **Apple AirPlay 2 spesifikasyonunu hiç yayımlamadı.** Spesifikasyon yalnız MFi lisanslılarına NDA altında dağıtılıyor. Her açık uygulama gözlemlenen davranıştan türetilmiştir. | Aynı sebep. Projede protokol taklidi değil, protokol uygulaması yapılıyor; HomeKit/MFi kimliği taklit edilmiyor (ADR-0005). |
| **Proje genç.** Depo 2026-01-22'de açıldı, v0.2.0 güncel sürüm. | Kod okunabilir durumda ve sabit commit'e sabitlenecek. Yukarı akış kaybolursa vendor edilmiş kopya çalışmaya devam eder. |
| **Lisans OSI açık kaynağı değil.** Özel bir "Non-Commercial License" (© 2026 Remi Bouteiller): kullanma, kopyalama, değiştirme ve dağıtma yalnız **ticari olmayan** amaçlarla serbest; ticari kullanım yazılı izin ister. | Bu proje bir kişinin kendi evi için bir hoparlör yapmasıdır. Kalıcı sonucu aşağıda. |

### Lisansın kalıcı sonucu

Bu seçim projeyi **ticari olmayan** kullanıma bağlar. Merzarkabul Airplay Speakers firmware'i satılamaz veya ticari bir üründe kullanılamaz; bunun için yazarın yazılı izni gerekir.

[[ADR-0008-github-releases-ota|ADR-0008]] release hattı için somut zorunluluk: yayımlanan her firmware asset'i, vendor edilmiş kaynağın lisans metnini ve telif bildirimini taşımalıdır. Bu, `F7` release iş listesine girer.

## Kabul ölçütü

Yığın seçiminin ürün için anlamı tek cümledir: **bir Apple cihazı alıcıyı bulur, eşleşir ve ses akışı gönderir** (PRD-009). Ölçüm üç adımdır ve "kulağa çalışıyor gibi" ile geçilmez:

1. mDNS/AirPlay keşfi: alıcı telefonun AirPlay listesinde `Merzarkabul XXXX` olarak görünür.
2. `pair-setup` / `pair-verify` tamamlanır ve RTSP oturumu kurulur; `ptp_clock` kilitlenir.
3. Ses akışı gelir ve duyulur.

Geliştirme kartında 2026-09-05'te ölçüldü: sahibinin iPhone'undan gerçek bir oturum, DMAP meta verisi, `ptp_clock: LOCKED` (`dev=973672 ns`, `samples=62`), ses tezgâh S/PDIF çıkışından duyuldu ([[../06-testing/devkit-bring-up|bring-up kaydı]]). Ürün kartında 2026-09-08'de ürünün kendi yolu çaldı: AirPlay → I²S → PCM5102A → XH-A232 → ses, mono ve temiz, tek amfiyle tezgâhta ([[../06-testing/bench-measurement-order|tezgâh ölçüm sırası]]). İkisi de **dinleme** kaydıdır; seviye, clipping ve pop `G1`'de dummy-load üzerinde ölçülür ve hiçbir kapı bu dinlemeyle açılmaz.

## Hâlâ kanıtlanmamış olan

Bu ADR yığını seçer. Aşağıdakiler **ölçülmedi** ve donanım gerektirir:

- Çalışma zamanı heap/PSRAM kullanımı ve CPU yükü **akış sırasında** ölçülmedi; yukarıdaki rakamlar statik derleme çıktısıdır.
- Ses yolu duyuldu ama ölçülmedi: seviye, clipping, DC offset ve pop `G1`'in konusudur; paket kaybı ve yeniden bağlanma davranışı denenmedi.

Ayrıntı: [[../01-architecture/audio-network-feasibility|AirPlay fizibilitesi]].
