---
title: Ses açılış/kapanış sıralayıcısı
status: partial
owner: firmware-engineer
reviewers: [orchestrator, acoustics-worker]
updated: 2026-08-31
tags: [development-log, firmware, f2, audio, safety]
---

# 2026-08-31 — Susturma sıralayıcısı

## Neden bu modül

ADR-0011 susturma hatlarına gerçek aktüatör verdi (`GPIO21` amfi `SD`, `GPIO13` DAC `XSMT`). Aktüatör olunca sıralama yazılabilir hale geldi — ve sıralama, yazılımda onarılamayacak iki şeyi koruyor: empedansı henüz ölçülmemiş bir tweeter, ve dinleyicinin kulağı.

## Sıra iki yönde de önemli

**Açarken** saat -> DAC -> amfi. Saat oturmadan DAC'ı açmak, çıkışına bir basamak koyar; DAC oturmadan amfiyi bırakmak, DAC'ın ilk örneklerinde ne varsa onu amfinin kazancıyla çarpıp tweeter'a gönderir.

**Kapatırken sıra tersine döner ve amfi önce gider.** TPA3110 veri sayfası kapanış pop'u için shutdown'ın güçten önce verilmesini açıkça söylüyor. Önce DAC'ı susturup amfiyi sonra kapatmak, DAC'ın kendi geçişini canlı bir amfiden geçirirdi — yani tam da bu sıralamanın önlemek için var olduğu gümbürtü.

Bu yüzden ayrı bir `MUTING` durumu var: amfi kapalı ama DAC ve saatler hâlâ açık. Bu asimetri durumun tek varlık sebebi.

## Testler sıraya bakıyor, zamanlamaya değil

Yanlış sırada hızlı bir açılış, doğru sırada yavaş olandan kötüdür. O yüzden üç değişmez, **var olan her durum üzerinde** tek tek doğrulanıyor:

- Amfi açıkken DAC hep açık.
- DAC açıkken saat hep çalışıyor.
- Amfi yalnız tek bir durumda açık.

Sonradan eklenen bir durum sıralamayı bozarsa, kimsenin sebebini hatırlamasına gerek kalmadan burada düşer.

## Bilinçli bir karar: geri dönüş yok

Kapanış başladıysa, izin hemen geri gelse bile sonuna kadar gidiyor. Yarıda dönmek, DAC kendi geçişinin ortasındayken amfiyi serbest bırakırdı. Baştan başlamak bir oturma süresi tutuyor ve her zaman temiz.

## Doğrulama

11110 ana makine kontrolü, 0 hata. On bir mutasyonun **on biri** yakalandı — dördü doğrudan sıralamayı bozan mutasyonlardı (`MUTING`'de amfiyi açık bırak, `DAC_LIVE`'da amfiyi erken aç, `CLOCKING`'de DAC'ı erken aç, `SILENT`'ta DAC'ı saatsiz aç), üçü geçişleri (`PLAYING`'den doğrudan sessizliğe, unwind ortasında dön, izin kontrolünü kaldır).

Sayaç sarmasi da test edildi: 49,7 gün açık kalan bir hoparlör, sayaç döndü diye geçişin ortasında takılmamalı.

## Açık kalanlar

- Sürücü katmanı yok: `GPIO21` ve `GPIO13`'ü gerçekten süren kod `F2`'de, amfi kontrolüyle birlikte gelecek.
- Oturma süreleri (`clock_settle_ms`, `dac_settle_ms`, `mute_settle_ms`) enjekte ediliyor ve gerçek değerleri `G1`/`G3`'te osiloskopla bakılarak belirlenecek. Testlerdeki sayılar yer tutucu.
- Limiter ve crossover matematiği hâlâ yazılmadı.
