---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, acoustics-engineer]
updated: 2026-09-12
tags: [adr, product, enclosure, audio]
---

# ADR-0021: Tek kabin, sekiz sürücü, tek program

## Bağlam

Elde bir Harman Kardon Nova'dan sökülmüş sekiz sürücü var: dört woofer, dört tweeter, ve Nova'nın kendi pasif radyatörleri (operatör kaydı, 2026-09-08). Sürücülerin DC dirençleri ölçüldü, empedans eğrileri ve `Fs` henüz değil. Soru, bu sürücü setinin kaç kutuya ve kaç ağ cihazına bölüneceğidir; cevap, projenin en büyük riskini ve en büyük iş yükünü belirler.

## Karar

Ürün **tek bir kabindir**: dört Nova woofer ve dört Nova tweeter tek kutuda, tek bir ESP32-S3 + tek bir PCM5102A + dört özdeş XH-A232 ile sürülür ([[ADR-0002-biamp-signal-chain|ADR-0002]]), tek bir 24 V adaptörle beslenir ([[ADR-0020-dc-adapter-power|ADR-0020]]).

- **Tek program.** Kutu, AirPlay akışının L+R toplamını çalar; sekiz sürücünün hepsi aynı programın kendi bandını alır. Stereo kapsam dışıdır.
- **Ağda tek cihaz.** AirPlay adı `Merzarkabul XXXX`'tir ([[ADR-0001-product-identity|ADR-0001]]); `XXXX` MAC türevidir, çünkü bir ad hangi ağa takılırsa takılsın benzersiz olmalıdır.
- **Pasif radyatörlü kapalı kabin.** Kabin, Nova'nın kendi pasif radyatörleriyle akortlanan bir refleks hizalamadır. Başlangıç varsayımı tek paylaşılan hava hacmidir; woofer'ların ayrı hacim alıp almayacağı `G0` (`Fs`, `Vas`) sonrasında kararlaşır. Sürücü yerleşimi henüz belirlenmedi. Ayrıntı [[../04-acoustics/cabinet-plan|kabin planındadır]].

## Gerekçe

- **Çoklu cihaz senkron riski yok.** Birden fazla kutu, cihazlar arası zamanlama ölçümü ve o ölçümün geçemeyeceği bir ürün gereksinimi ister; lisanssız bir AirPlay 2 alıcısında bu, projenin kritik tıkayıcısı olurdu. Tek kutuda saat tektir: alıcı gönderenin PTP saatine kilitlenir ve sekiz kanal aynı I2S çerçevesinden çıkar.
- **Tek besleme, tek kurulum, tek OTA hedefi, tek tezgâh.** Bir adaptör, bir provisioning akışı, bir sürüm kanalı, bir `G1` düzeni.
- **Sekiz sürücü tek kutuda tam sürücü setini kullanır.** Dört woofer paralel değil, her biri kendi BTL kanalında; dört tweeter her biri kendi `C_SAFE`'i ile.
- **Kurulacak tek kabin.** Mekanik iş dörde bölünmez; pasif radyatörler elde ve tek kutuya yeter.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| Dört ayrı kutu, her biri ağda kendi cihazı | Cihazlar arası senkron ölçülmemiş ve tek kartla ölçülemez; dört adaptör, dört kurulum, dört OTA hedefi, dört kabin. Projenin kritik tıkayıcısı bu ölçüm olurdu. |
| İki kutu, stereo çift | Stereo kapsam dışı. Aynı odada ilişkili içerik çalan iki kutu ≤100 µs hizalama ister (tarak filtresi: 1 ms gecikmede ilk çentik 500 Hz'de); kimse Wi-Fi üzerinde bunu ölçmedi. |
| Tek kutu, sürücü yarıları sol/sağ | Tek program tasarım kararıdır: DAC'ın iki kanalı bant taşır, kanal değil (ADR-0002). Sol/sağ bölme, sekiz sürücüye tek DSP geçişinden bant vermeyi imkânsız kılar. |
| Tek kutu, bass-reflex kanallı kabin | Pasif radyatörler elde ve Nova bu sürücüler için onlarla akortlanmıştı; ölçülmemiş bir sürücüye kanal tasarlamak tahmindir. |

## Sonuçlar

- Gereksinim tablosunda çoklu cihaz satırı yoktur; kapı listesinde çoklu cihaz kapısı yoktur.
- Kabin işi tek kutudur: `docs/04-acoustics/cabinet-plan.md` sürücü yerleşimini, hacim kararını ve pasif radyatör akordunu `G0` sonrasında taşır.
- PRD-001 (keşif) ve PRD-009 (bir telefon bulur, eşleşir, akış gönderir) tek cihaz için geçerlidir.
- `hk_sched`'in rastgele ilk gecikmesi kalır: sebebi her güç kesintisinden sonra GitHub'a sabit bir anda vurmamaktır; rastgele seçilen gecikme, bir gün daha fazlası yapılırsa bir kişilik filoyu aynı saniyeye vuran bir filoya çevirmez.
- `AGENTS.md` kilitli kararlar listesi bu ADR'yi gösterir; tek kutu, sekiz sürücü ve tek program yalnız supersede eden bir ADR ile değişir.
