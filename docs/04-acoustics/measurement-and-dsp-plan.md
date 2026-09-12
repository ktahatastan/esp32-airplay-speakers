---
status: pending
owner: acoustics-engineer
updated: 2026-09-08
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

Her profil kaynak ölçüm, firmware sürümü, tarih ve rollback değeriyle saklanır. Bu cümlenin karşılığı 2026-09-08'de yazıldı: aşağıdaki "Profil" bölümüne bakın.

## Limiter: yazıldı, ayarlanmadı

Peak limiter `firmware/components/hk_audio/hk_limiter.c` içinde yazıldı ve host'ta test edildi. Tavan, release ve hold **enjekte ediliyor**; gerçek değerler `G2`'den, ölçülmüş sürücü davranışından gelecek. Kodda hiçbir varsayılan tavan yok — verilmemiş bir yapılandırma çalıştırılmıyor, reddediliyor.

İki tasarım kararı kayda değer, çünkü ikisi de alışılmış limiter tercihlerinin tersi.

**Attack yok, lookahead yok.** Bir örneği tavanın altına indiren kazanç, o örnekten hesaplanıp o örneğe uygulanıyor; yani `|çıkış|` tavanı **hiçbir zaman** aşmıyor. Ders kitabı alternatifi kazanç indirimini bir attack süresine yayar: daha yumuşak duyulur ve kazanç düşerken tepelerin geçmesine izin verir. Tepe geçiren şey koruma değildir. Diğer alternatif lookahead gecikme hattıdır; bozulmayı kaldırır ama **gecikme ekler**, ve bu cihazın harcayamayacağı bir gecikme bütçesi var: [[../07-decisions/ADR-0007-airplay-stack|ADR-0007]] odalar arası senkrona ≤1 ms veriyor. Nadiren devreye giren bir koruma katının, devreye girdiği anda daha hoş duyulması için o bütçeyi harcamak yanlış takas.

Bedeli dürüstçe: anlık kazanç değişimi bozulmadır. Normal kullanımda duyulmaması ve yalnız bir şey zaten ters gittiğinde devreye girmesi gereken bir katta, takas bu yönde doğru.

**Release yumuşak, üstüne hold var.** Toparlanma ters durum: kazancın bir anda geri gelmesi pompalama üretir; iki gürültülü geçiş arasındaki kısa boşlukta toparlanması ise ikinci geçişin tam kazançla gelmesi demektir — yani yakalanması gereken geçişin.

`G2` bu sayıları ürettiğinde değişecek tek şey yapılandırma olacak; mantık değişmeyecek.

## Crossover: matematik yazıldı, köşeler bekliyor

`firmware/components/hk_audio/hk_biquad.c`: ikinci derece bölümler ve onlardan kurulan 4. derece Linkwitz-Riley çifti. Köşe frekansı her fonksiyonda **argüman**; `G0`/`G2` gelmeden hiçbir sayı gömülmedi.

**Direct Form II transposed, Direct Form I değil.** Kâğıt üstünde aynı filtre; tek duyarlıklı `float`'ta değil. DF2T durumunu sinyalle aynı büyüklük aralığında tutar; DF1 birbirine çok yakın iki büyük sayının farkını biriktirir ve düşük köşe frekanslarında bit kaybeder — yani tam olarak tweeter'ın koruma yüksek-geçireninin yaşadığı yerde.

**LR4, Butterworth çifti değil.** Sebep tek bir özellik: iki dal **düz toplanır**. Butterworth çifti köşede 3 dB yukarı çıkar; LR4'te her dal 6 dB aşağıdadır ve toplamları bire döner. Ayrıca LR2'nin aksine LR4'ün iki dalı aynı fazdadır, yani hiçbirinin polaritesi ters çevrilmez. Bu bir tuzak: yine de çevrilirse köşede derin bir çentik oluşur ve bu, kablolama hatası gibi değil "crossover sorunu" gibi ölçülür.

Bir sınır fuzz'lamayla bulundu: `fc/fs` oranı yaklaşık **4,83e-5**'in altında katsayılar tek duyarlıkta kararlı bir filtre tanımlamayı bırakıyor — `a2` bire yaklaşıyor ve yuvarlama kutupları birim çemberin üstüne ya da dışına itiyor. Katsayılar yine de üretiliyor ve yine filtre gibi görünüyor. Modül artık bu oranın altını **reddediyor** (sınır ölçülenin iki katı, 48 kHz'de 4,8 Hz). Projenin ihtiyacı olan her değer çok yukarıda: 2 kHz crossover 0,042; tweeter'ın 80 Hz koruma yüksek-geçireni 0,0017; 20 Hz subsonic 4,2e-4.

Doğrulama katsayı tablosuna değil **frekans yanıtına** bakıyor — yanlış formül tutarlı biçimde yazıldığında bir tablo mutlu mutlu geçerdi, yanıt geçmez. İki dalın toplamının düzlüğü tüm spektrum boyunca 2000 noktada denetleniyor, ve eğim ayrıca gerçek işleme yolundan sinüsle ölçülüyor.

## Profil: biçim yazıldı, sayılar bekliyor

`firmware/components/hk_audio/hk_profile.c`. `G0` ve `G2` sayıları ürettiğinde yapılacak iş **bir struct doldurmak** olsun diye, o struct'ın kendisi ve reddettiği şeyler şimdiden tanımlandı. İçinde tek bir sürücü değeri yok ve olamaz: Nova woofer/tweeter ölçülmedi, bugün yazılacak herhangi bir sayı yarın ölçülmüş olandan ayırt edilemezdi.

**Profil kendi kaynağını taşıyor.** Ölçülen DC dirençler saklanıyor, ama çalışma zamanı onlarla hiçbir hesap yapmıyor. Crossover'ı türetmek tezgâhta bir insanın işi; cihaz yalnız sonucu taşır. Sonucun **neyden** türetildiğini kaydetmek, profili bir ölçüme bağlanabilir kılan şeydir — ve ölçümünü adlandıramayan bir profil, bu projenin çalıştırmayı reddettiği tahminin kendisidir. Doğrulayıcı bu yüzden kaynağı olmayan profili reddediyor.

**Tavanın yanında bir gerilim var, ve olmak zorunda.** Limiter tavanı dijital bir sayı; sürücüye ulaşan şey volt. Sabit bir dijital seviyede class-D bir amfinin çıkışı besleme ile ölçeklenir, ve buradaki besleme kutuya takılan DC adaptördür ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]): nominali 19 V, ama amfinin kabul ettiği pencere 8-26 V ve "19 V" bir adaptörün etiketidir, bir ölçüm değil. Yani tek bir saklanmış tavan sürücüyü **tek bir besleme geriliminde** korur; başka bir adaptörde ya güvensiz ya gereksiz kısıktır.

Profil bu yüzden tavanı, ölçüldüğü besleme gerilimiyle **birlikte** saklıyor ve `hk_profile_ceiling_at()` onu firmware'in bildiği nominal beslemeye (`CONFIG_HK_SUPPLY_MV`, varsayılan 19 V) taşıyor:

```text
tavan(V) = min(1, tavan_ref x V_ref / V_besleme)
```

Yön önemli ve tersi hayat pahasına yanlış: besleme **yüksekken** dijital birim başına daha çok volt düşer, yani dijital tavan **aşağı** inmelidir. Besleme düştükçe aynı volt için daha çok dijital seviye kalır, ve üst sınır tam ölçektir — orada artık gönderilecek sinyal kalmadığı için, güvenlik kararı değil aritmetik. Bu yön testte ayrıca iddia ediliyor, çünkü sessizce ters yazılırsa sonuç "biraz kısık bir hoparlör" gibi değil, 24 V adaptör takılan gün ölen bir tweeter gibi görünür.

Tezgâhtaki tavanlar 12 V'ta alındı (`HK_BENCH_REFERENCE_SUPPLY_MV`); 19 V'luk üründe aynı tavanın aşağı ölçeklenmesi tam da istenen yöndür. Bir alan ve bir çarpım: bedeli yok, ve yanlış adaptörü ölü bir tweeter yerine kısık bir hoparlör yapıyor. Aynı profil iki besleme değerinde inşa ediliyor, yalnız tavanlar değişiyor, filtre katsayıları aynı kalıyor; F3'ün limiter ölçütü bunun firmware tarafıdır.

**Reddetmek, düzeltmekten iyidir.** Subsonic filtre crossover'ın üstündeyse woofer dalında hiçbir şey kalmaz; bu iki sayının yer değiştirmesidir ve düzeltilmez, reddedilir. Aynı şekilde bu örnekleme hızında kurulamayan bir köşe sessizce aşağı çekilmez: sonucu kimsenin seçmediği bir crossover olurdu. Her ret kendi adını veriyor (`schema`, `source`, `frequency`, `ceiling`, …), yani tezgâhta bir profil reddedildiğinde hangi alanın sorunlu olduğu log satırında yazıyor.

**Kalan iş ölçümdür.** `G0` kapandığında doldurulacak alanlar: iki DC direnç, subsonic köşe, crossover köşesi, iki dal kazancı. `G2` kapandığında: iki tavan ve ölçüldükleri besleme gerilimi. Kod tarafında değişecek bir şey yok.
