---
status: complete
owner: procurement-researcher
reviewers: [hardware-engineer, orchestrator]
updated: 2026-08-30
tags: [development-log, procurement, battery, usb-c]
---

# 2026-08-30 — Aspilsan hücre ve düşük maliyetli şarj seti

## Yapılan değişiklik

- Molicel P28A adayı, kullanıcının seçtiği Pilpaketi Aspilsan `INR18650A28` ile değiştirildi.
- Üretici veri sayfasındaki 14 A sürekli deşarj sınırı kanonik kabul edildi; satıcının 25 A başlığı koşullu tepe değer olarak işaretlendi.
- Ayrı Baseus adaptör+kablo yerine, kablo dahil Syrox GAN65T 65 W set düşük maliyetli aday yapıldı.
- KiCad hücre değerleri yanlış `21700` etiketinden `ASPILSAN A28 18650` olarak düzeltildi.

> [!note] Kısmen üzerine yazıldı
> Bu kayıttaki **10.757,36 TL** toplamı, sonraki [[2026-08-30-km103-power-switch|KM103 güç anahtarı seçimiyle]] 140,00 TL düşerek **10.617,36 TL** olmuştur. Güncel tablo: [[../05-procurement/turkey-shopping-list-2026-08-30#Maliyet özeti|Türkiye satın alma listesi]]. Hücre ve adaptör kararları geçerliliğini korur.

## Maliyet etkisi

- Hücre: 594,00 TL/adet yerine 119,36 TL/adet.
- Ortak adaptör+kablo: 2.698,00 TL yerine 865,00 TL.
- Dört 4S1P hoparlör + ortak şarj seti + sarf tahmini: 20.184,60 TL yerine **10.757,36 TL**.
- Toplam tahmini azalma: **9.427,24 TL**.

## Doğrulama ve açık kapılar

- Aspilsan üretici veri sayfası: 3,65 V nominal, 2.800 mAh, 4,2 V şarj sonu, 14 A sürekli deşarj.
- Syrox üretici kataloğu: tek Type-C çıkışında 20 V / 3,25 A ve set içinde 1 m Type-C kablo.
- Kablo e-marker/5 A bilgisi katalogda açık olmadığı için set yalnız `candidate`; PD/e-marker test cihazı ve 40 W elektronik yük testi gerekir.
- Hücreler aynı lot, kapasite ve iç direnç bakımından eşlenmeden; nokta kaynak, BMS, NTC, sigorta ve G4 testi tamamlanmadan paket onaylanmaz.

İlgili belgeler: [[../05-procurement/turkey-shopping-list-2026-08-30|Türkiye satın alma listesi]], [[../power-and-battery-plan|güç ve batarya planı]].
