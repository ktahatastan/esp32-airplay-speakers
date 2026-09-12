---
title: Dört kutu tek kabine indi, ve amfide susturma hattı yok
status: done
owner: orchestrator
reviewers: [hardware-engineer, firmware-engineer, acoustics-engineer, verifier]
updated: 2026-09-12
tags: [development-log, architecture, hardware, power, acoustics, firmware]
---

# 2026-09-12 — Dört kutu tek kabine indi

Sahibin kararı kısa ve kesin: ayrı ayrı dört kutu değil, **tek bir kabin**. İçinde bir ESP32-S3, bir PCM5102A, dört XH-A232 amfi; dört woofer, dört tweeter; 24 V / 2,9 A masaüstü adaptör; iki buck. Kararın kendisi [[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]'de, besleme tarafı [[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]'de, sinyal zinciri [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002]]'de. Bu kayıt, o üç kararın projenin geri kalanını nasıl değiştirdiğini tutar.

## Ne değişti

**Sinyal zinciri.** DAC'ın sol kanalı dört amfinin sol girişine, sağ kanalı dört amfinin sağ girişine paralel gider. Her amfi bir woofer ve bir tweeter sürer. DAC'ın iki kanalı **bant** taşır, kanal değil: sol woofer bandı, sağ tweeter bandı. Stereo kapsam dışı; kutu AirPlay akışının L+R toplamını çalar. Sağ/sol/mono kanal ayarı bu yüzden silindi — tek programı olan bir kutunun seçeceği bir kanal yoktur.

**DSP zinciri aynı kaldı.** Mono toplam, kullanıcı EQ'su, 55 Hz subsonic, LR4 ayrım, dal başına kazanç ve limiter: `hk_dsp`, `hk_biquad`, `hk_limiter`, `hk_profile` dokunulmadı. Sayılar `G0` kapanana kadar yer tutucudur; bu da değişmedi.

**Besleme.** 24 V / 2,9 A adaptör; amfi aralığının üst ucu, dört kart tek rayı paylaşırken en fazla headroom. `VIN` jaktan dört amfiye doğrudan gider. Lojik tarafı **iki** MP1584: biri ESP32-S3'e, biri PCM5102A'ya. İkiye ayrılmalarının sebebi tezgâhta duyulan bir şeydir — paylaşılan tek buck DAC'a hışırtı verdi. `CONFIG_HK_SUPPLY_MV` varsayılanı `24000`; tavan ölçekleme 12 V tezgâh referansından aşağı doğru çalışır, koruyucu yöndedir.

**Kabin.** Pasif radyatörlü kapalı kabin; Nova'nın kendi radyatörleri elde. Başlangıç varsayımı tek hava hacmi; woofer'lara ayrı hacim `G0` (`Fs`, `Vas`) sonrasının sorusu. Yeni not: [[../04-acoustics/cabinet-plan|kabin planı]].

**Silinenler.** Cihazlar arası senkron gereksinimi ve kapısı — ölçülemeyen ve artık gerekmeyen bir tıkayıcıydı. Numaraları boş kalır, yeniden verilmez; hangileri olduğu gereksinim tablosunda ve ADR dizininde yazılıdır.

**Çizim ve BOM.** SVG paftası dört amfiyi (`U7`-`U10`), iki buck'ı (`U3` ESP, `U4` DAC), `VIN`'i ve `TP0`-`TP34` test noktalarını çizer; KiCad üreteci aynı ağı üretir. BOM'da adaptör satırı 24 V / 2,9 A, dört `C_SAFE`, iki buck, bir `C_A` (gerilim sınıfı adaptörün boşta çıkışının üstünde), tek kasa.

**Kapılar.** `G1`'e giren satırlar: adaptör boşta çıkışı < 25,5 V, jak polaritesi, tek amfi dummy-load, dört amfi tam yükte `VIN` çökmesi, iki buck'ın gürültüsü, açma/kapama pop. Aşamalandırma kuralı: ilk enerjilenen yol **bir** amfi, bir woofer, bir tweeter; diğer üçü ancak o çift `G0`-`G2`'yi geçince sürücüye bağlanır.

## Nasıl yapıldı

İş altı yazıcıya bölündü — kararlar, donanım, diğer dokümanlar, firmware çekirdeği, firmware kalanı, araçlar — her birinin dosya kümesi kesişmeyecek biçimde, ve her alanın arkasında bağımsız bir doğrulayıcı. Altı alandan beşi ilk geçişte temiz çıktı; kararlar alanı bir şeyi eksik bırakmıştı: ADR-0013'ün "kanıtlanmamış" listesi hâlâ "hiçbir Apple cihazı bağlanmadı" diyordu, oysa 2026-09-05 ve 2026-09-08 kayıtları tersini söylüyor. Doğrulayıcıların on yedi bulgusu tek tek işlendi; ikisi karar olarak kabul edildi (kanıt tarihinden sonraya çekilen üç `updated` alanı), geri kalanı düzeltildi.

Drift kuralı olarak `scripts/check_docs.py`'ye iki desen girdi: çoklu cihaz sözlüğü ve eski besleme rakamı. İkincisinin izin verilen tek yeri ADR-0020'nin reddedilen seçenekler satırıdır.

## Doğrulama

- `python3 scripts/check_docs.py`: 0 hata, 0 uyarı.
- Ürün, geliştirme kartı ve tezgâh yapıları derleniyor; üçünde de `CONFIG_HK_SUPPLY_MV=24000`.
- Host testleri: 540 679 kontrol, 0 hata.
- `git diff --check` temiz; `check_no_private_keys.py` 0 sorun.
- KiCad üreteci yalnız ayrıştırıldı; `--validate` bu makinede koşamaz (KiCad yok). Şema durumu `candidate` kalır.

## Aynı gün, ikinci düzeltme: amfide susturma hattı yoktur

Sahibi kartı eline alıp baktı: XH-A232'de güç girişi, ses girişi ve hoparlör çıkışları dışında hiçbir bağlantı yok. TPA3110'un `SD` bacağı kart üzerinde dışarı çıkarılmamış. Yukarıdaki iş bunu bilmeden yazılmıştı — bir `GPIO21` hattının dört `SD` pad'ine paralel gittiğini, her amfide bir pull-down (`R7`-`R10`) olduğunu, beş direnç ve `TP34` diye bir prob noktası olduğunu varsayıyordu; bunların hiçbiri var olmayan bir pad'e bağlanamaz. `hk_pins.h` bunu baştan beri bir **rezervasyon** olarak işaretlemişti ve kablolama planı "pad bulunmazsa firmware kontrollü amfi susturması yoktur" cümlesini o gün için hazır tutmuştu. O gün geldi.

Sökülen: `HK_PIN_AMP_MUTE`, `amp_enabled`, sıralayıcıdaki `DAC_LIVE` durumu ve `dac_settle_ms`; şemadaki kesikli bus, dört direnç ve `TP34`; BOM'daki koşullu satır. Pin tablosu sekiz GPIO'dur.

Kalan sıralayıcı iki hat sürer ve dört durumu vardır: `SILENT → CLOCKING → PLAYING`, inişte `MUTING`. Açarken saat oturmadan DAC açılmaz — o adım aynı, sebebi daha ağır: DAC çıkışındaki adımı yakalayacak bir amfi susturması artık yok. Kapatırken **önce DAC susturulur ve saat tutulur**: `XSMT` yumuşak susturmadır, PCM5102A çıkışı bit saatine karşı rampalar; saati önce kesmek rampayı yarıda bırakır ve amfiye tam da rampanın önlemek için var olduğu geçişi verir. `MUTING` durumunun anlamı bu yüzden değişti — eskiden "amfi indi, DAC hâlâ açık" idi, şimdi "DAC indi, saat hâlâ açık". Host testi bu asimetriyi dört durumun tamamında sabitler.

Bedeli açık yazıldı: TPA3110'un kendi açılış/kapanış geçişi firmware'in erişemediği bir şeydir. Adaptör takılırken ve çekilirken dört amfinin pop'u `G1`'de, DAC susturuluyken, olduğu gibi kaydedilir; bu bir `G1` satırıdır ve `test-strategy`'nin kapanış pop satırı da buna göre yeniden yazıldı. Duyulur bir pop `VIN` tarafında bir donanım önlemi isterse o yeni bir ADR'dir.

Tezgâh yapısı için de bir cümle değişti: "DAC'ı açıp amfiyi kapalı tutan bir ayar yok" cümlesinin sebebi artık "aynı sembol ikisini birden bırakıyor" değil, "bırakılacak bir amfi hattı yok". Sonuç aynı — amfiyi devre dışı bırakmak kabloyla olur — ama gerekçe artık doğru.

`check_docs.py`'ye bir drift kuralı daha girdi: `AMP_MUTE`, `amp_enabled`, `GPIO21` ve `R7`-`R10` yalnız tarihli kayıtlarda (test kayıtları, günlükler, risk kütüğü) ve ADR-0011'de geçebilir.

Doğrulama: üç yapı derleniyor; host testleri 540 620 kontrol, 0 hata (sıralayıcı testi üç hatlıdan iki hatlıya yeniden yazıldı); `check_docs` 0 hata; SVG yeniden üretildi (`R6` tek pull-down, `TP0-TP33`); KiCad üreteci yalnız ayrıştırıldı.

## Açık olanlar

- `G0` hâlâ açık: empedans eğrisi ve `Fs`. Crossover köşesi, limiter tavanı ve kabin hacmi bunu bekliyor.
- `G1`'e giren yeni satır: amfinin kendi açılış/kapanış pop'u, DAC susturuluyken, adaptör takılıp çekilirken; ve kapanışta `XSMT`'nin `BCLK` durmadan önce düştüğünün kaydı (`TP6` ↔ `TP33`).
- Ürün yapısı `CONFIG_HK_AIRPLAY_OUTPUT_I2S` ile derleniyor, DSP zinciri yalnız tezgâh yapısında. Ürün profili `G0` olmadan sesi zaten reddettiği için bugün bir güvenlik açığı değil; ama `G0` kapandığında ürün varsayılanının DSP arka ucuna çekilmesi gerekir. Karar sahibindir.
- KiCad olan bir makinede `generate_merzarkabul.py --validate` ve `check_generated_kicad.py --record`.
- Kabin mekaniği: sahibi ölçüleri ve fotoğrafları verdiğinde parametrik, 3D yazıcıya uygun bir kabin çizimi (`hardware/cabinet/`).
