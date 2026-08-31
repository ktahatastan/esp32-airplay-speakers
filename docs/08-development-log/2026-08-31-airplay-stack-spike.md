---
title: F1 AirPlay yığını fizibilite spike'ı
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, airplay, f1, adr]
---

# 2026-08-31 — F1 AirPlay yığını spike'ı

## Amaç

Projenin en büyük teknik riskini kapatmaya çalışmak: ESP32-S3 üzerinde senkron AirPlay multiroom mümkün mü, mümkünse hangi yığınla? [[../07-decisions/ADR-0007-airplay-stack|ADR-0007]] Ağustos başından beri boş duruyordu.

Araştırma birincil kaynaklarla yapıldı: kaynak kodu, `LICENSE` dosyaları, Espressif'in kendi issue kayıtları. Pazarlama sayfaları kanıt sayılmadı; nitekim biri yanlış çıktı.

## Sonuç

**Yığın seçildi:** [`rbouteiller/airplay-esp32`](https://github.com/rbouteiller/airplay-esp32), sabit commit'e vendor edilecek. ADR-0007 `accepted`.

Bu bir yığın seçimidir, senkron iddiası değildir.

### Aday gerçekten AirPlay 2 alıcısı

Kaynakta HomeKit `pair-setup`/`pair-verify` (gerçek SRP-6a, RFC 5054 3072-bit, mbedTLS üzerinde SHA-512), FairPlay `/fp-setup`, IEEE-1588 **PTP dinleyicisi** (`224.0.1.129`, UDP 319/320) ve AirPlay 2'ye özgü `SETPEERS` / `SETPEERSX` / `SETRATEANCHORTIME` / `FLUSHBUFFERED` / `SETRATE` metotları var. PTP, multiroom gruplamanın saat mekanizmasıdır — yani gruplanabilirliğin altyapısı kodda mevcut.

AirPlay 1 (RAOP) yolu paralel korunuyor ve `CONFIG_AIRPLAY_FORCE_V1` ile AP2 derleme dışı bırakılabiliyor. Bu, G7 başarısız olursa geri çekilme yolunu veriyor.

### Yer sorun değil — derlenerek gösterildi

Yığın, kurulu ESP-IDF v5.5.1 ile ESP32-S3 için temiz bir dizinde derlendi:

| | Değer | Bütçedeki yeri |
|---|---:|---|
| Uygulama imajı | 1.460.192 bayt | Bir OTA slotunun **%20,3'ü** |
| Statik DIRAM | 136.495 bayt | 341.760'ın %39,9'u |
| `F0` iskeletimiz (karşılaştırma) | 232.576 bayt / 56.531 bayt | — |

Derleme başarılı ama uyarısız değil: iki uyarı, biri `-Wshift-count-negative`.

### Alternatif yok

- ESP32 için **başka açık AirPlay 2 alıcısı bulunmuyor**; GitHub'daki diğer "ESP32 AirPlay 2" depoları bunun kopyaları.
- Diğer her şey AirPlay 1: `squeezelite-esp32` (ESP-IDF v4.3.x'e sabit), `esp-airsync` (GPL-3.0, bizimle aynı donanım), `conduit-stream` (lisanssız, multiroom kapsam dışı).
- **Espressif'in AirPlay'i yok.** ESP-ADF'in 15 etiketi ve 3 dalında sıfır eşleşme. 2019 basın bülteni "Integrates ... Airplay" diyor ama Espressif kendi issue #291'inde "it is not included in the latest plan" yanıtını vermiş. Bileşen kayıt defteri iki terim için de boş.
- Espressif'in çoklu-oda ürünü ESP MRM tescilli master/slave; ESP32 master olup URL çalıyor, kaynak iPhone olamıyor — PRD-002'yi karşılamıyor.

### Tavan gruplamayı içeriyor

`shairport-sync`'in kendi "What Does Not Work" listesi altı madde sayıyor (HD lossless, Dolby Atmos, Windows iTunes, uzaktan kumanda, eski macOS, aynı makinede çoklu örnek) ve **multiroom bu listede yok**. Yani lisanssız bir AP2 alıcısının gruplanamayacağına dair bir engel bilinmiyor.

Bu, bizim donanımımızda çalışacağının kanıtı değil; protokol düzeyinde imkânsız olmadığının kanıtı.

## G7 eşikleri kilitlendi

Ölçümün **elektriksel** olması gerektiği kararlaştırıldı. Akustik ölçüm yapılmıyor: sesin yayılma gecikmesi `1 ms ≈ 34 cm` ve mikrofon yerleşimi hatası ölçülecek büyüklüğü aşıyor. Yöntem: iki cihazın `TP14` DAC çıkışından eşzamanlı iki kanallı kayıt, çapraz korelasyonla `Δt`, 2 saat boyunca 5 dakikada bir.

| Ölçüm | Eşik |
|---|---|
| Kararlı durum `abs(Δt)` | ≤ 1 ms (farklı oda kullanımı) |
| 2 saatte kayma | Eşiği aşacak şekilde büyümemeli |
| Yeniden bağlanma | ≤ 30 s |

Stereo çift seçilirse eşik **≤ 100 µs**'ye iniyor, çünkü aynı odada ilişkili içerikte `Δt` tarak filtrelemesi üretir ve `1 ms`'de ilk çentik `500 Hz`'e düşer — duyulur. Bunun Wi-Fi üzerinde ulaşılabilir olduğu varsayılmıyor; ölçüm gösterirse stereo çift ürün seçeneği düşer.

## Kabul edilen riskler

- **FairPlay yanıtları sabit kodlu.** `/fp-setup` dört önceden hesaplanmış blok tekrar oynatıyor; MFi değil, protokol analizi. Depo README'si "Not guaranteed to work with future iOS or macOS versions" diyor. Bir iOS güncellemesi kırabilir.
- **Apple spesifikasyonu hiç yayımlamadı**; MFi bireylere kapalı. Her açık uygulama gözlemlenen davranıştan türetilmiş.
- **Lisans OSI açık kaynağı değil.** Özel "Non-Commercial License": ticari olmayan kullanım/değiştirme/dağıtım serbest, ticari kullanım yazılı izin ister. **Bu projeyi kalıcı olarak ticari olmayan kullanıma bağlıyor** ve ADR-0008 release hattına somut bir zorunluluk getiriyor: yayımlanan her firmware asset'i lisans metnini ve telif bildirimini taşımalı.

Üçü de risk kaydına girdi.

## Doğrulama

Araştırma altı bağımsız açıdan yapıldı ve her taşıyıcı iddia ayrı bir doğrulama turundan geçirildi: **36 iddia kontrol edildi, 23'ü doğrulandı, 13'ü çürütüldü veya düzeltildi.** Düzeltilenler arasında dikkat çekenler:

- Bir araştırmacı `shairport-sync`'in multiroom'u desteklemediğini ima etmişti; doğrulayıcı "What Does Not Work" listesini açıp multiroom'un orada olmadığını gösterdi. Sonuç tersine döndü.
- AP2'ye özgü metot kümesinin "dört metot" olduğu iddiası eksikti; beşincisi (`SETRATE`) atlanmıştı.
- Espressif basın bülteninin AirPlay desteği iddiası, Espressif'in kendi issue yanıtıyla çürütüldü.
- İki iddia bu depoya ait olmayan dosyaları (`main/network/ptp_clock.c`) bizim depomuzdaymış gibi göstermişti; doğrulayıcılar bunların üçüncü taraf projeye ait olduğunu ayırdı.

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 90 dosya, 0 hata |
| Yığının ESP32-S3 + ESP-IDF v5.5.1 derlemesi | **PASS** — bağımsız olarak temiz dizinde yeniden üretildi |
| Dört cihazda senkron ölçümü | **YAPILMADI** — donanım yok |
| Çalışma zamanı kaynak kullanımı | **YAPILMADI** — donanım yok |

## Sonraki adım

`F1`'in kalan yarısı donanım bekliyor: yığını `firmware/components/hk_airplay/` altına sabit commit'le vendor et, karta yükle, dört hedefin Apple cihazında birlikte göründüğünü ve çalışma zamanı kaynak kullanımını ölç.

Donanım gelene kadar sırada `F4` (ağ ve provisioning) ile `F5` (buton ve LED) var; ikisi de `F2`/`F3`'ten bağımsız çalışabilir ve donanım kapısı gerektirmez.
