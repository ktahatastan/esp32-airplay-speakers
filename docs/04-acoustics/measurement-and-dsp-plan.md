---
status: pending
owner: acoustics-engineer
updated: 2026-09-12
tags: [acoustics, crossover, limiter]
---

# Ölçüm ve DSP planı

1. DC direnç, empedans ve rezonans.
2. Dummy-load üzerinde dört amfinin sekiz kanalı: gain/faz ve amfiler arası eşleşme.
3. Tweeter bağlı değilken HPF ve boot/mute.
4. Çok düşük seviyede tek tek sürücü taraması.
5. Crossover frekansı/eğim, polarite ve delay.
6. Woofer excursion/tweeter gücüne göre RMS/peak limiter.
7. Kabin içinde yakın alan + dinleme ekseni ölçümü.
8. Aynı kabindeki dört woofer ve dört tweeter arasında tolerans; dört amfi kazanç eşleşmesi.
9. Dört amfi birlikte **4 Ω sınıfı** (ya da `G0`'ın ölçtüğü `Z_min`) dummy-load'a sürülürken adaptörün 2,9 A noktasında DSP'nin besleme dedektörü okunur: log'daki en yüksek ortalama-kare `supply_budget_sq`, `VIN` çökmesinin zaman sabiti `supply_window_ms` olur (`G1` S7; sırada 2'den sonra gelir, madde numaraları yukarıdan atıfla bağlı olduğu için sona yazıldı).

Her profil kaynak ölçüm, firmware sürümü, tarih ve rollback değeriyle saklanır. Bu cümlenin karşılığı 2026-09-08'de yazıldı: aşağıdaki "Profil" bölümüne bakın.

## Limiter: yazıldı, ayarlanmadı

Peak limiter `firmware/components/hk_audio/hk_limiter.c` içinde yazıldı ve host'ta test edildi. Tavan, release ve hold **enjekte ediliyor**, ve 2026-09-12'den beri release/hold **dal başına** ayrıdır (`woofer_release_ms`/`woofer_hold_ms`, `tweeter_release_ms`/`tweeter_hold_ms`): bir kubbe ile bir koninin toparlanma süresi aynı olmak zorunda değildir ve `G2` ikisini ayrı ölçer. Gerçek değerler `G2`'den, ölçülmüş sürücü davranışından gelecek. Kodda hiçbir varsayılan tavan yok — verilmemiş bir yapılandırma çalıştırılmıyor, reddediliyor.

İki tasarım kararı kayda değer, çünkü ikisi de alışılmış limiter tercihlerinin tersi.

**Attack yok, lookahead yok.** Bir örneği tavanın altına indiren kazanç, o örnekten hesaplanıp o örneğe uygulanıyor; yani `|çıkış|` tavanı **hiçbir zaman** aşmıyor. Ders kitabı alternatifi kazanç indirimini bir attack süresine yayar: daha yumuşak duyulur ve kazanç düşerken tepelerin geçmesine izin verir. Tepe geçiren şey koruma değildir. Diğer alternatif lookahead gecikme hattıdır; bozulmayı kaldırır ama **gecikme ekler**: çıkış, gönderenin verdiği AirPlay sunum zamanına göre geç kalır ve o gecikme her örnekte ödenir. Nadiren devreye giren bir koruma katının, devreye girdiği anda daha hoş duyulması için bütün akışı geciktirmek yanlış takas.

Bedeli dürüstçe: anlık kazanç değişimi bozulmadır. Normal kullanımda duyulmaması ve yalnız bir şey zaten ters gittiğinde devreye girmesi gereken bir katta, takas bu yönde doğru.

**Release yumuşak, üstüne hold var.** Toparlanma ters durum: kazancın bir anda geri gelmesi pompalama üretir; iki gürültülü geçiş arasındaki kısa boşlukta toparlanması ise ikinci geçişin tam kazançla gelmesi demektir — yani yakalanması gereken geçişin.

`G2` bu sayıları ürettiğinde tepe katında değişecek tek şey yapılandırma olacak; mantık değişmeyecek.

**Hizalama gecikmesi lookahead değildir.** Şema 2 dal başına bir gecikme alanı taşıyor (`woofer_delay_samples`, `tweeter_delay_samples`; en çok 64 örnek ve yalnız **biri** sıfırdan farklı olabilir). Bu, iki sürücünün akustik merkezlerini hizalamak içindir (plan maddesi 5) ve gecikmesiz dal hattın derinliğini tanımlar: alıcıya bildirilen gecikme değişmez, çünkü bir dalı ötekine göre kaydırmak akışı sunum zamanına göre geciktirmek değildir. Sayı `G2`'nin baffle üzerindeki ölçümünden gelir; bugün ikisi de sıfırdır. Aynı yerde bir de tweeter polaritesi alanı var (`tweeter_polarity`, 0/1): LR4'te iki dal aynı fazdadır ve çevrilmez, alan yalnız `G2` bir kablolama hatasını ölçüp kayda geçirdiğinde kullanılır.

## Besleme bütçesi katı: yazıldı, iki sayısı G1'in

Tepe limiter'ın taşıyamadığı bir sınır var. Adaptör 24 V / 2,9 A'dır ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]) ve bu bir **ortalama** kısıtıdır, tepe değil: aynı tepe seviyesinde bir sinüs ile 15 dB crest'li müzik adaptörden yaklaşık on altı kat farklı akım çeker. Üstelik dört woofer ve dört tweeter tek rayı paylaştığı için sınır dalların **toplamı** üzerindedir, dal başına değil. Tepe tavanına katlanmış bir bütçe ya müziği gereksiz kısar ya da sürekli bir sinüste bütçeyi aşar; ikisi de yanlış.

Bu yüzden `hk_supply_limiter` ayrı bir kattır (`firmware/components/hk_audio/hk_supply_limiter.c`, host'ta testli): dal kazançlarından sonra, tepe limiter'lardan **önce**, iki dalın örneklerinin kare toplamını üstel bir pencereyle ortalar ve ortalama bütçeyi aşınca iki dala birden **ortak** bir kazanç uygular (`sqrt(bütçe / ortalama)`; hedef blok başına bir kez hesaplanır, örnek başına doğrusal yaklaşılır, yani örnek başına maliyet veriden bağımsızdır). Dedektör ileri beslemelidir, girişe bakar; kazancın kendisini ölçmez, dolayısıyla salınmaz. Tepe limiter'lar ondan sonra geldiği için `|çıkış| ≤ tavan` garantisi olduğu gibi durur.

İki sayısı vardır ve ikisi de **hiç varsayılmaz**: `supply_budget_sq` (ortalama-kare eşiği, 0 < x ≤ 2) ve `supply_window_ms` (pencere, ≥ 1 ms). Doğrulayıcı ikisinden biri yoksa ya da aralık dışındaysa profili `budget` adıyla reddeder. Nereden geleceği plan maddesi 9: `G1` S7'de dört amfi 4 Ω sınıfı yüke sürülürken adaptörün 2,9 A noktasında dedektörün log'a bastığı en yüksek ortalama-kare bütçe olur, `VIN` çökmesinin zaman sabiti pencere olur. Bunlar 8 Ω'da alınamaz: 8 Ω'luk bir geçiş gerçek akımın yaklaşık yarısını çeker. Tezgâh profilinin yer tutucusu (1,0, 100 ms) bir tam ölçekli dalın ortalama-karesidir — doğrulayıcının sınırının (2,0) yarısı ve tezgâh kazançlarının programdan üretebileceğinin (0,25² + 0,18² ≈ 0,095) on katı — `G1` S7'nin yerine koyacağı bir yer tutucudur ve tezgâh kazançlarıyla (0,25 / 0,18) ancak kullanıcı EQ'su yükseltirse devreye girebilir; açılışta bunu söyler.

## Crossover: matematik yazıldı, köşeler bekliyor

`firmware/components/hk_audio/hk_biquad.c`: ikinci derece bölümler ve onlardan kurulan 4. derece Linkwitz-Riley çifti. Köşe frekansı her fonksiyonda **argüman**; `G0`/`G2` gelmeden hiçbir sayı gömülmedi.

**Direct Form II transposed, Direct Form I değil.** Kâğıt üstünde aynı filtre; tek duyarlıklı `float`'ta değil. DF2T durumunu sinyalle aynı büyüklük aralığında tutar; DF1 birbirine çok yakın iki büyük sayının farkını biriktirir ve düşük köşe frekanslarında bit kaybeder — yani tam olarak tweeter'ın koruma yüksek-geçireninin yaşadığı yerde.

**Subsonic filtre için tersi: Butterworth, LR4 değil.** 2026-09-12'den beri woofer dalının subsonic yüksek-geçireni dördüncü derecedir (iki bölüm, `Q` 0,5412 ve 1,3066, aynı köşede) ve `woofer_hpf_hz` köşede **−3 dB** demeye devam eder. LR4 aynı köşeyi −6 dB'ye koyardı; subsonic filtrenin toplanacağı bir eşi yok, dolayısıyla LR4'ün düz-toplam özelliğine ihtiyacı yok, ama köşesinin anlamını korumaya ihtiyacı var — 55 Hz yazan bir alan sessizce 45 Hz gibi davranmamalı. Şema 2 bunun için gerekliydi: bir alanın anlamı değişince sürüm atlar.

**LR4, Butterworth çifti değil.** Sebep tek bir özellik: iki dal **düz toplanır**. Butterworth çifti köşede 3 dB yukarı çıkar; LR4'te her dal 6 dB aşağıdadır ve toplamları bire döner. Ayrıca LR2'nin aksine LR4'ün iki dalı aynı fazdadır, yani hiçbirinin polaritesi ters çevrilmez. Bu bir tuzak: yine de çevrilirse köşede derin bir çentik oluşur ve bu, kablolama hatası gibi değil "crossover sorunu" gibi ölçülür.

Bir sınır fuzz'lamayla bulundu: `fc/fs` oranı yaklaşık **4,83e-5**'in altında katsayılar tek duyarlıkta kararlı bir filtre tanımlamayı bırakıyor — `a2` bire yaklaşıyor ve yuvarlama kutupları birim çemberin üstüne ya da dışına itiyor. Katsayılar yine de üretiliyor ve yine filtre gibi görünüyor. Modül artık bu oranın altını **reddediyor** (sınır ölçülenin iki katı, 48 kHz'de 4,8 Hz). Projenin ihtiyacı olan her değer çok yukarıda: 2 kHz crossover 0,042; tweeter'ın 80 Hz koruma yüksek-geçireni 0,0017; 20 Hz subsonic 4,2e-4.

Doğrulama katsayı tablosuna değil **frekans yanıtına** bakıyor — yanlış formül tutarlı biçimde yazıldığında bir tablo mutlu mutlu geçerdi, yanıt geçmez. İki dalın toplamının düzlüğü tüm spektrum boyunca 2000 noktada denetleniyor, ve eğim ayrıca gerçek işleme yolundan sinüsle ölçülüyor.

## Profil: biçim yazıldı, sayılar bekliyor

`firmware/components/hk_audio/hk_profile.c`. `G0` ve `G2` sayıları ürettiğinde yapılacak iş **bir struct doldurmak** olsun diye, o struct'ın kendisi ve reddettiği şeyler şimdiden tanımlandı. İçinde tek bir sürücü değeri yok ve olamaz: Nova woofer/tweeter'ın empedans eğrisi ve `Fs`'si ölçülmedi (DC dirençler ölçüldü, ikisi de 4 Ω sınıfı), bugün yazılacak herhangi bir köşe yarın ölçülmüş olandan ayırt edilemezdi.

**Profil kendi kaynağını taşıyor.** Ölçülen DC dirençler saklanıyor, ama çalışma zamanı onlarla hiçbir hesap yapmıyor. Crossover'ı türetmek tezgâhta bir insanın işi; cihaz yalnız sonucu taşır. Sonucun **neyden** türetildiğini kaydetmek, profili bir ölçüme bağlanabilir kılan şeydir — ve ölçümünü adlandıramayan bir profil, bu projenin çalıştırmayı reddettiği tahminin kendisidir. Doğrulayıcı bu yüzden kaynağı olmayan profili reddediyor.

**Tavanın yanında bir gerilim var, ve olmak zorunda.** Limiter tavanı dijital bir sayı; sürücüye ulaşan şey volt. Bu bölüm 2026-09-12'ye kadar "sabit bir dijital seviyede class-D bir amfinin çıkışı besleme ile ölçeklenir" diyordu. **Bu amfi için doğru değil ve geri çekildi.** TPA3110D2 sabit kazançlı bir amfidir: çıkış gerilimi, ray kırpana kadar kazanç × giriştir (SLOS528F: kazanç ayarları Tablo 2, sabit kazancın kanıtı Tablo 3 — aynı 1 Vrms giriş 24 V'ta 27,7 Vpp, 12 V'ta ray kırpmasıyla 23,5 Vpp), beslemeyle değil. Besleme yalnız kırpma noktasını, yani sürücünün görebileceği **en yüksek** gerilimi taşır. Yani sürücüyü tek bir besleme geriliminde koruyan şey tavan değil, tavan ile amfinin o beslemede kırptığı seviyenin birlikte okunmasıdır; ve buradaki besleme kabine takılan DC adaptördür ([[../07-decisions/ADR-0020-dc-adapter-power|ADR-0020]]): nominali 24 V, amfinin penceresi 8-26 V, "24 V" bir etikettir, ölçüm değil.

Profil tavanı yine ölçüldüğü besleme gerilimiyle **birlikte** saklıyor ve `hk_profile_ceiling_at()` onu firmware'in bildiği nominal beslemeye (`CONFIG_HK_SUPPLY_MV`, varsayılan 24 V) taşıyor — ama artık **tek yönde**:

```text
tavan(V) = tavan_ref x min(1, V_ref / V_besleme)
```

Referansın üstündeki bir besleme tavanı aşağı çeker; referansın **altındaki** bir besleme onu **yükseltmez**, saklanan değer olduğu gibi kalır. Eski hâli iki yöne de gidiyordu ve yükselen yön bu amfide tek tehlikeli yöndü: 24 V referansla `G2`'de kaydedilmiş bir tavan, 12 V'a yapılandırılmış bir yapıda iki katına çıkar, ve sabit kazançlı amfi 12 V'ta o iki katı kırpana kadar sürücüye aynen taşırdı. Testler artık bu yönü de iddia ediyor.

Aşağı yönün ne olduğu da açıkça söylenmeli. Tezgâhtaki tavanlar 12 V'ta alındı (`HK_BENCH_REFERENCE_SUPPLY_MV`); 24 V'luk üründe 12/24 ile aşağı ölçeklenmeleri **dijital tavanı yarıya indirir**, ve bunun sürücüde ne yaptığı tezgâha bağlıdır. Tezgâh seviyesi 12 V rayını kırpmadıysa, sabit kazançta aynı dijital seviye aynı voltu verdiği için yarıya inen tavan sürücüdeki voltu da yarıya indirir. Kırptıysa — ve firmware'in kendi notuna göre (`hk_airplay_output_i2s.c`, tezgâh profili) 2026-09-08 dinlemesi kırpıyordu: seviye kazançla değil **ray kırpmasıyla** sınırlıydı, kaydırıcının yaklaşık %80'inde amfi bitiyordu — 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye tezgâhın duyduğundan **fazlasını** verebilir; aynı kaydırıcı konumunda iki katına kadar, ve bunu ne çarpım ne tavan engeller. Hangisinin geçerli olduğunu `C3`, amfinin kazanç strap'i söyler. Engelleyen şey sıradır: amfinin kazanç strap'i ([[../06-testing/bench-measurement-order|tezgâh sırası]], `C3`) okunmadan sürücüde 24 V'ta dinleme yoktur, ve dinleme kademeli, tek çift, düşük seviyededir. `G2` tavanları kullanılacakları beslemede kaydeder, yani üründe çarpan tam olarak 1'dir. Ölçekleme modelinin kendisi (DAC'a göre tavan + ray kırpma reddi + saklanan amfi kazancı) `C3` sayısı gelene kadar açık bir ADR maddesidir; şema 2 bu yüzden `amp_gain_db` alanını taşır — okunan değer oraya yazılır, hesapta kullanılmaz. Yanlış adaptörü ürüne sokmayan şey de çarpım değil, ADR-0020'nin `G1` kuralıdır: yüksüz çıkış bağlanmadan önce ölçülür ve 25,5 V'un altında değilse adaptör reddedilir. Aynı profil iki besleme değerinde inşa ediliyor, yalnız tavanlar değişiyor, filtre katsayıları aynı kalıyor; F3'ün limiter ölçütü bunun firmware tarafıdır.

**Besleme bütçesi tavana katlanmaz.** Adaptör 24 V / 2,9 A (yaklaşık 70 W) ile verilidir ve sekiz BTL kanal bunun çok üstünü çekebilir; çöken `VIN` ESP32-S3'ü şarkı ortasında sıfırlar. Bu bölüm 2026-09-12'ye kadar `G2`'nin tavanı sürücü koruması ile besleme bütçesinden düşük olanına koymasını istiyordu; bütçe artık kendi alanında ve kendi katındadır (yukarıda: `supply_budget_sq`, `supply_window_ms`), `G1` S7 doldurur. Tepe tavanları yalnız sürücü korumasınındır ve `G2`'nindir; bütçenin altında kalırlar, onu taşımazlar (ADR-0020).

**Reddetmek, düzeltmekten iyidir.** Subsonic filtre crossover'ın üstündeyse woofer dalında hiçbir şey kalmaz; bu iki sayının yer değiştirmesidir ve düzeltilmez, reddedilir. Aynı şekilde bu örnekleme hızında kurulamayan bir köşe sessizce aşağı çekilmez: sonucu kimsenin seçmediği bir crossover olurdu. Her ret kendi adını veriyor (`schema`, `source`, `frequency`, `ceiling`, …), yani tezgâhta bir profil reddedildiğinde hangi alanın sorunlu olduğu log satırında yazıyor.

**Kalan iş ölçümdür — ve kod tarafı 2026-09-12'de bir kez daha değişti.** Bu cümle 2026-09-08'de "kod tarafında değişecek bir şey yok" diyordu; doğru değildi, çünkü kayıt kendi açık maddelerini sayıyordu (dördüncü derece subsonic, profilin açılışta yargılanması, delay/polarite, besleme bütçesi). Onlar kapandı ve şema 2 oldu: `hk_profile_load()` blob'u tek bir çağrıda okuyup zincire çeviriyor, `hk_main` aynı yargıyı açılışta veriyor ve `hk_storage` ses izni için o kararı istiyor; geçerli olmayan bir profil artık DAC'ı sıfır yazan bir zincire açmıyor, adıyla reddediliyor. Zincir ürünün çıkış arka ucudur ([[../07-decisions/ADR-0022-dsp-product-output-backend|ADR-0022]]).

Şimdi hangi kapı hangi alanı dolduruyor:

| kapı | doldurduğu alanlar |
|---|---|
| `G0` | iki DC direnç (tweeter'ınki kayıtta 3,5 Ω, firmware'in tezgâh profili 3,7 taşıyordu; kayıt kazandı, operatör teyit eder), `woofer_hpf_hz` (kabin içi empedansın iki tepesi arasındaki `Fb`), `crossover_hz` (tweeter `Fs`'sinin en az iki katı) |
| `C3` (tezgâh sırası) | `amp_gain_db` — dört kartta okunan TPA3110D2 kazanç ayarı; taşınır, hesapta kullanılmaz; 24 V'ta sürücüde dinlemeden önce okunur |
| `G1` S7 | `supply_budget_sq`, `supply_window_ms` — dört amfi 4 Ω sınıfı yükte, adaptörün 2,9 A noktasında |
| `G2` | iki dal kazancı, iki tepe tavanı ve kaydedildikleri besleme (`reference_supply_mv`, üründe 24000), dal başına release/hold, tek daldaki hizalama gecikmesi, tweeter polaritesi |

Hiçbiri kodda varsayılmaz: eksik olan reddedilir, adı log'a yazılır. Kalan kod işi de var ve bilinerek sonraya bırakıldı: sürücü başına termal (RMS) limiter (`G2`'nin güç dayanımı ölçümünü bekler; dedektör mekanizması `hk_supply_limiter`'da hazır), `C_SAFE` ile crossover köşesinin etkileşimi için bir telafi (tweeter `Fs` ölçülmeden karar verilemez), 24 bit çıkış ve dither, çalışma zamanında EQ değişikliği (bir ayar yazıcısı var olduğunda). Bunlar `TODO.md`'de.
