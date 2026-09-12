---
title: Tarama modu ilk kez tezgâhta — woofer 50 Hz dedi, tweeter bir aralık verdi
status: done
owner: orchestrator
reviewers: [acoustics-engineer, hardware-engineer]
updated: 2026-09-12
tags: [development-log, bench, drivers, g0, measurement]
---

# 2026-09-12 — Tarama modu ilk kez tezgâhta

Gece oturumu, sahibi tezgâhta, ben seri hattı dinleyip adım saatlerini yazarak. Amaç `G0`'ın açık yarısı: sürücülerin `Fs`'si, bu kez kartın kendi tarama moduyla (`sdkconfig.bench` + `sdkconfig.sweep`, sonra `FINE_HZ=50`). Ölçüm kaydı [[../02-hardware/driver-measurements#11. Sonuç|sürücü ölçüm kaydında]]; burada yalnız ne öğrenildiği var.

## Düzenek üç kez bizi yanılttı, hepsi tel

Firmware her koşuda doğru şeyi söyledi: saat, veri, `XSMT` bırakıldı. DAC yine de susuyordu. Sırayla bulunanlar:

1. **`SCK` ESP'ye bağlıymış.** PCM5102A o pinde bir şey görünce harici saat sanıyor; 3-telli mod için `SCK` toprakta olmalı. Kablolama planı §3.3 bunu yazıyordu; tel yine de oradaydı. Kesildi, GND'ye alındı.
2. **DAC ile ESP toprağı ayrıydı.** 8 Eylül'de toprak hışırtı yapınca "toprağı ayır" öğrenilmişti — yanlış ders. I2S ortak referans olmadan çalışmaz. Doğru ders yıldız toprak: ESP `GND` ↔ DAC `GND` kısa doğrudan tel, besleme toprağı ayrı.
3. **Seri direnç 680 kΩ çıktı** (mavi-gri-sarı). Sürücüdeki sinyal 18 µV oldu, hiçbir alet görmedi. 680 Ω ile değiştirildi.

Ve bir dördüncüsü: `GPIO13 → XSMT` jumper'ı üç kez çıktı. Aynı arıza 8 Eylül'de de vardı. Bu tel lehimlenmeli.

## Modülün dahili direnci

`LOUT` yüksüz 3,0 Vpp, 680 Ω + sürücü ile 1,82 Vpp, 220 Ω + sürücü ile 0,999 Vpp: üçü de modülün çıkışında **~470 Ω seri direnç** varmış gibi. Bu, sürücüdeki sinyali dış direnç ne olursa olsun ~6 mV rms'te tutuyor ve ölçüm yordamının "470 yerine 680 Ω" hesabını geçersiz kılıyor. Çıkarım, ölçüm değil; yordam buna göre yeniden yazılacak (ya modülün çıkış direnci ölçülür ya da oran yöntemi standart olur).

## Sonuç

- **Woofer `Fs` ≈ 50 Hz** (kaba tarama: 40 Hz 40 mVpp, 50 Hz 98, 63 Hz 38; ±%13). 8 Eylül'ün ~100 Hz tahmininin yarısı — o tahmin tepenin üst yamacını tepe sanmıştı. 2–4,9 kHz'de yükselen kuyruk bobin endüktansı. Sonuç: yer tutucu subsonic köşesi (55 Hz) serbest hava rezonansının tam üstünde; kalıcı karar kabin akordunun (`Fb`).
- **Tweeter:** 1–4,9 kHz'de düz ~20 mVpp, tepe 10 mV/div çözünürlüğünde yok. Ağır sönümlü (ferro-sıvı) küçük kubbe; `Fs` nokta değil **1–3 kHz aralığı**. Crossover kararı için yeterli değil; oran yöntemi ya da mV çözünürlüklü alet gerekiyor.
- İnce tarama (31,5–79,3 Hz) denendi, iki yöntemle de okunamadı; açık.

## Aletler

DMM'in 6 V AC kademesi 10 mV'u göstermiyor; osiloskopun otomatik `Vpp` ölçümü 20 mVpp'nin altında tutunamıyor, kare saymak gerekti. Sonraki tur için: sürücü kanalı 5–10 mV/div, tetik `LOUT` kanalından, imleç; ya da mV kademeli bir metre. Kart bu gece ikinci bir N16R8'di (kimlik `056C`), 8 Eylül'ün kartı değil.

## Aynı gün, tezgâh dışında

Adaptör ölçüldü ve kayda girdi: yüksüz 24,49 V (< 25,5 V, geçti), etiket 24 V / 2,91 A / 70 W, polarite iç `+`. Şebeke tarafı topraklı; DC `−` ↔ PE ilişkisi ve fiş iç pim çapı açık. `D2` için elde `DR6A01` (6A01 sınıfı standart doğrultucu, `Uf` 595 mV test cihazında — Schottky değil), `C_A` için 1000 µF / 50 V; ikisi de tezgâh/G1 adayı olarak yazıldı.

Hiçbir kapı açılmadı; `G0` kısmi kayıttır.
