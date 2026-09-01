---
status: pending
owner: acoustics-engineer
updated: 2026-08-30
tags: [acoustics, crossover, limiter]
---

# Ölçüm ve DSP planı

1. DC direnç, empedans ve rezonans.
2. Dummy-load üzerinde iki kanal gain/faz.
3. Tweeter bağlı değilken HPF ve boot/mute.
4. Çok düşük seviyede tek tek sürücü taraması.
5. Crossover frekansı/eğim, polarite ve delay.
6. Woofer excursion/tweeter gücüne göre RMS/peak limiter.
7. Kabin içinde yakın alan + dinleme ekseni ölçümü.
8. Dört ünitede tolerans/kalibrasyon karşılaştırması.

Her profil kaynak ölçüm, firmware sürümü, tarih ve rollback değeriyle saklanır.

## Limiter: yazıldı, ayarlanmadı

Peak limiter `firmware/components/hk_audio/hk_limiter.c` içinde yazıldı ve host'ta test edildi. Tavan, release ve hold **enjekte ediliyor**; gerçek değerler `G2`'den, ölçülmüş sürücü davranışından gelecek. Kodda hiçbir varsayılan tavan yok — verilmemiş bir yapılandırma çalıştırılmıyor, reddediliyor.

İki tasarım kararı kayda değer, çünkü ikisi de alışılmış limiter tercihlerinin tersi.

**Attack yok, lookahead yok.** Bir örneği tavanın altına indiren kazanç, o örnekten hesaplanıp o örneğe uygulanıyor; yani `|çıkış|` tavanı **hiçbir zaman** aşmıyor. Ders kitabı alternatifi kazanç indirimini bir attack süresine yayar: daha yumuşak duyulur ve kazanç düşerken tepelerin geçmesine izin verir. Tepe geçiren şey koruma değildir. Diğer alternatif lookahead gecikme hattıdır; bozulmayı kaldırır ama **gecikme ekler**, ve bu cihazın harcayamayacağı bir gecikme bütçesi var: [[../07-decisions/ADR-0007-airplay-stack|ADR-0007]] odalar arası senkrona ≤1 ms veriyor. Nadiren devreye giren bir koruma katının, devreye girdiği anda daha hoş duyulması için o bütçeyi harcamak yanlış takas.

Bedeli dürüstçe: anlık kazanç değişimi bozulmadır. Normal kullanımda duyulmaması ve yalnız bir şey zaten ters gittiğinde devreye girmesi gereken bir katta, takas bu yönde doğru.

**Release yumuşak, üstüne hold var.** Toparlanma ters durum: kazancın bir anda geri gelmesi pompalama üretir; iki gürültülü geçiş arasındaki kısa boşlukta toparlanması ise ikinci geçişin tam kazançla gelmesi demektir — yani yakalanması gereken geçişin.

`G2` bu sayıları ürettiğinde değişecek tek şey yapılandırma olacak; mantık değişmeyecek.

### Henüz yazılmayan

Crossover ve HPF katsayı matematiği (biquad, Linkwitz-Riley) hâlâ yok. Köşe frekansları `G0`/`G2`'yi bekliyor ama matematiğin kendisi beklemiyor; sıradaki iş bu.
