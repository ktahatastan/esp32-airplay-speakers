---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, acoustics-engineer, verifier]
updated: 2026-09-12
tags: [adr, audio, firmware, dsp]
---

# ADR-0022: DSP zinciri ürünün çıkış arka ucudur

## Bağlam

Alıcının çıkış katı bir Kconfig seçimidir (`HK_AIRPLAY_OUTPUT`) ve bugüne kadar varsayılanı, vendor edilmiş yığının kendi geçiş (passthrough) çıkışı `CONFIG_HK_AIRPLAY_OUTPUT_I2S` idi. O kat programı olduğu gibi DAC'a yazar: mono toplam yok, crossover yok, subsonic yok, limiter yok. [[ADR-0002-biamp-signal-chain|ADR-0002]]'nin tanımladığı zincir — `LOUT` woofer bandı, `ROUT` tweeter bandı — ise üçüncü bir arka uçta, `output/hk_airplay_output_i2s.c` içinde yaşıyordu ve yalnız tezgâh fragmanı `sdkconfig.bench` onu seçiyordu. Yani ürün imajı, tweeter'a tam bant program gönderen bir çıkış katıyla derleniyordu; tek koruması, ses izni olmadan alıcının hiç başlamamasıydı.

Bu arka uç 2026-09-08'de (`86f629c`) ADR'siz girdi; gerekçesi commit mesajında, Kconfig yardım metninde ve `scripts/check_vendor_output_shadow.py`'nin açıklamasında dağınık duruyordu. Aynı gün ürün profiline `CONFIG_HK_AIRPLAY=y` yazıldı (`7f2df34`) ve [[ADR-0013-airplay-integration-shape|ADR-0013]]'ün "ürün yapısı değişmedi, `CONFIG_HK_AIRPLAY` varsayılan kapalı" satırı o günden beri yanlıştı. 2026-09-12'de tek kabin kararı ([[ADR-0021-single-cabinet|ADR-0021]]) alınırken günlük bu soruyu sahibine bıraktı: "Ürün yapısı `CONFIG_HK_AIRPLAY_OUTPUT_I2S` ile derleniyor, DSP zinciri yalnız tezgâh yapısında. [...] `G0` kapandığında ürün varsayılanının DSP arka ucuna çekilmesi gerekir. Karar sahibindir."

Sahibi aynı gün karar verdi: **"Önceki dsp yi yeni yapıya uyarla sonra ölçümlerle düzenleriz DSP olsun."** Ve önceki zincir için: "fena değildi, eksikleri vardı, iyileştirme gerektiren aynısını uyarlayabilirsin."

Ses kapısı da yarımdı. `hk_storage_audio_permitted()` profilin **varlığına** bakıyordu, geçerliliğine değil; `hk_profile_valid()` çağrısı "`G0` verisi geldiğinde `hk_main`'e eklenecek" diye bekliyordu. Var olan ama bozuk bir blob, arka ucun dijital sıfır yazma yoluna kadar gelebiliyordu — arka ucun kendi yorumunun "olmaz" dediği şey.

## Seçenekler

| Seçenek | Ne yapar | Sonuç |
|---|---|---|
| A. Geçiş katı `G0`'a kadar varsayılan kalsın | Bugünkü durum | Reddedildi. Ürün imajında tweeter'ın önünde koruması olmayan bir kat durur; onu çalıştırmayan tek şey ses iznidir. Zincirin sayıları ölçülmemiş olsa da **biçimi** üründe olmalı, yoksa `G0` kapandığı gün ürün imajı hâlâ yanlış katı taşır. |
| B. Kconfig seçiminin varsayılanı DSP olsun | `firmware/components/hk_airplay/Kconfig`'te `default HK_AIRPLAY_OUTPUT_DSP`; geçiş üyesi yalnız geliştirme kartında (ADR-0012) veya tezgâh istisnası açıkken seçilebilir | **Seçildi.** İki satır (varsayılan ve geçiş üyesinin `depends on`'u), tek kaynak: hiçbir fragman seçimi ezmez, yardım metni gerçeği söyler, geliştirme kartı SPDIF'te kalır (`sdkconfig.devkit` açıkça seçer), tezgâh fragmanının kendi satırı gereksizleşir. |
| C. `sdkconfig.defaults`'a bir `CONFIG_HK_AIRPLAY_OUTPUT_DSP=y` satırı | Fragman seçimi ezer | Reddedildi. Çalışır — daha sonraki fragmanın seçimi kazanır — ama Kconfig'in kendi varsayılanı ve yardım metni tersini söylemeye devam eder. `CONFIG_HK_AIRPLAY` 2026-09-08'de tam bu tuzağa düştü: fragman açtı, yardım metni "kapalı" dedi, ADR-0013 dört gün yanlış kaldı. |

## Karar

### Çıkış arka ucu

**DSP zinciri (`hk_dsp`, `output/hk_airplay_output_i2s.c`) ürünün ve sürümün çıkış arka ucudur.** `CONFIG_HK_AIRPLAY_OUTPUT_DSP` Kconfig seçiminin varsayılanıdır; ürün ve sürüm yapıları bununla derlenir ve CI bunu iddia eder.

Vendor'un geçiş katı `CONFIG_HK_AIRPLAY_OUTPUT_I2S` silinmez ama **yalnız geliştirme kartında (ADR-0012) veya bir tezgâh istisnası (`CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE`) açıkken seçilebilir**; Kconfig'te `depends on` bunu söyler. Sebebi basit: o kat tweeter dalına tam bant program koyar ve üründe tweeter dalı diye bir şey ancak crossover'dan sonra vardır. Sürüm işi (`release.yml`) geçiş katı seçilmiş bir yapıyı ve her `CONFIG_HK_BENCH_*` bool'unu reddeder; `firmware/CMakeLists.txt` imzalı-uygulama ayarıyla bir tezgâh sembolünü aynı yapıda görürse yapılandırmayı durdurur, geliştirme kartı imzalama reddiyle aynı biçimde.

Arka uç vendor dosyasının **gölgesidir**, yaması değil: ADR-0013 vendor ağacında tek satır değişikliği yasaklar, bu yüzden aynı DMA geometrisi, aynı çalma imleci, aynı gecikme modeli ve aynı görev önceliği ayrı bir dosyada tutulur ve `scripts/check_vendor_output_shadow.py` vendor dosyasının özetini gölgenin yanında saklar. Yukarı akış güncellemesi artık bir kopyalama işi **artı bir gölge incelemesidir**; betik, inceleme yapılmadan sessizleştirilemez, çünkü özeti alınan dosya vendor'unkidir, gölge değil.

### Tek yargıç

Profili **bir** fonksiyon yargılar: `hk_profile_load(blob, uzunluk, fs, besleme, profil, zincir)`. Bloğu okur, yapısal olarak doğrular, örnekleme hızında kurar ve ilk hatada her iki çıktıyı sıfırlar. Arka uç zincirini bununla kurar; `hk_main` açılışta depo ayağa kalkar kalkmaz aynı fonksiyonla bloğu yargılar (`CONFIG_HK_AIRPLAY` kapalı bir yapıda yalnız yapısal olarak, `hk_profile_from_blob` ile) ve hükmü `hk_storage_profile_judged(geçerli)` ile depoya iter. `hk_storage_audio_permitted()` artık üç şey ister: şema uyumlu, profil **var** ve profil **geçerli bulundu**. Tezgâh istisnası dalı değişmez: yalnız **yokluk** reddini kaldırır; var ama reddedilmiş bir profil tezgâhta da reddedilir (`hk_storage.c`).

Böylece "var ama bozuk" durumu kapanır: bozuk blob açılış raporunda hükmünün adıyla görünür ("present:ceiling" gibi; `hk_profile_verdict_name()`'in tek sözcüklük adı), ses izni verilmez, alıcı hiç başlamaz. Arka ucun dijital sıfır yolu, kimse ona ulaşamadığı hâlde durur — ulaşılırsa yazdığı şey sıfırdır, ses değil.

### Profil şeması 2

Blob bir tel biçimidir: alan sırası korunur, yeni alanlar sona eklenir, şema `1`'den `2`'ye çıkar ve şema 1 blob'u `schema` hükmüyle reddedilir (eski bir profil yoktur; hiçbir cihaza yazılmadı). Her alanın yanına onu **kimin** dolduracağı yazılır, çünkü bu ADR'nin tek sayısı yoktur ve olamaz: sürücülerin empedans eğrisi ve `Fs`'si ölçülmedi.

| Alan | Anlamı | Dolduran |
|---|---|---|
| `schema`, `reserved`, `measured_yyyymmdd`, `source` | Sürüm ve kaynak: profilin türetildiği ölçüm kaydının adı. Kaynağı olmayan profil reddedilir. | Tezgâh kaydı |
| `woofer_dcr_ohm`, `tweeter_dcr_ohm` | Ölçülen DC dirençler; taşınır, hesaba girmez | `G0` (birer sürücü 2026-09-08'de okundu: 4,0 / 3,5 Ω; diğer altısı bekliyor) |
| `woofer_hpf_hz` | Subsonic yüksek geçiren; artık **dördüncü derece Butterworth**, iki bölüm (`Q` 0,5412 ve 1,3066), köşe −3 dB noktasıdır. Pasif radyatörlü kabin için ikinci derece yetmiyordu ve kayıt bunu açık madde olarak taşıyordu | `G0` + kabin akordu |
| `crossover_hz` | LR4 köşesi; iki dal aynı fazda, düz toplanır | `G0` (tweeter `Fs`) → `G2` |
| `woofer_gain`, `tweeter_gain` | Dal seviyeleri, (0, 1] | `G2` |
| `reference_supply_mv`, `woofer_ceiling`, `tweeter_ceiling` | Tepe limiter tavanları ve dinlendikleri besleme | `G2`, `G1` bütçesinin altında |
| `woofer_release_ms`, `woofer_hold_ms`, `tweeter_release_ms`, `tweeter_hold_ms` | Dal başına limiter zamanlaması. Eski `release_ms`/`hold_ms` woofer'ınki olarak adlandı, tweeter'ınki eklendi: 25 mm bir dome ile bir koni aynı toparlanmayı istemez | `G2` |
| `woofer_delay_samples`, `tweeter_delay_samples` | Akustik merkez hizalama; en çok 64 örnek (44,1 kHz'de ~1,5 ms), **en fazla biri** sıfırdan farklı. İki dalı birden geciktirmek hizalama değil gecikmedir ve zamanlama motoruna bildirilen gecikmeyi yalan yapar | `G2`, kabindeki yerleşimde mikrofonla |
| `tweeter_polarity` | 0 veya 1. LR4 dalları aynı fazdadır ve çevrilmez; bu alan, kabindeki fiziksel yerleşim köşede çentik gösterirse **ölçümle** ters çevirmek içindir | `G2`, mikrofonla; varsayılan 0 |
| `supply_budget_sq`, `supply_window_ms` | Adaptör bütçesi, DSP'nin tek birimiyle: iki dalın **toplam ortalama karesinin** üst sınırı (0 < x ≤ 2) ve ortalamanın alındığı pencere (≥ 1 ms). **Hiçbir zaman varsayılan almaz** | `G1`, `S7` adımı, 4 Ω sınıfı yüklerde |
| `amp_gain_db` | Amfi kazanç köprüsü: 0 = okunmadı, yoksa 20/26/32/36 (TPA3110D2 Tablo 2). Taşınır, hesaba girmez; bir tutarlılık kontrolünün geleceği yer | `C3` tezgâh maddesi |

Yeni retler `delay`, `polarity`, `budget` ve `amp-gain`'dir (`hk_profile_verdict_t`'de `HK_PROFILE_BAD_DELAY` … `HK_PROFILE_BAD_AMP_GAIN`); log satırında `hk_profile_verdict_name()`'in bu tek sözcüklük adıyla görünürler. Okunmamış kazanç (`0`) **reddedilmez**: okunmamış kazancın ilk `factory_cal` yazımını engelleyip engellemeyeceği sahibinin kararıdır ve sorulur, alınmaz.

Zincirin sırası: mono toplam → kullanıcı EQ → subsonic (iki bölüm) → LR4 bölme → dal kazançları → dal gecikmesi → tweeter işareti → ortak besleme kazancı → **en sonda** dal başına tepe limiter. Koruyucu biquad sayısı 6'dır (EQ hariç); `hk_biquad_stable()` beş katsayının da sonlu olmasını ister ve NaN taşıyan bir zincir `HK_DSP_BAD_FILTER` ile reddedilir.

### Besleme bütçesi katı

[[ADR-0020-dc-adapter-power|ADR-0020]]'nin 31. satırı, özgün hâliyle, limiter tavanının 2,9 A adaptör bütçesinden türetilmesini istiyordu. Tepe tavanları bunu **taşıyamaz**, iki sebeple: bütçe bir **ortalama** kısıtıdır (adaptörün 2,9 A'sı ısıl ve aşırı akım korumasının zaman sabitinde bir ortalamadır, bir örnek süresinde bir tepe değil; tavanda sürekli duran sıkıştırılmış bir program, aynı tavana bir kez dokunan bir geçişten kat kat fazla çeker), ve bütçe **toplamdır** (dört woofer ve dört tweeter tek rayı paylaşır; her dal kendi tavanının altında kalırken toplam yine bütçeyi aşabilir). Tepe ve ortalama, dal ve toplam: iki eksende de farklı büyüklükler.

Bu yüzden zincire ayrı bir kat girer, `hk_supply_limiter`: iki dalın kare toplamının `supply_window_ms` penceresindeki üstel ortalamasını **girişten** (ileri beslemeli) tutar; blok başında bir kez hedef kazancı hesaplar (`min(1, sqrt(bütçe / ortalama))`) ve örnek başına doğrusal olarak ona kayar. Karekök ve bölme blokta bir kezdir; örnek başına maliyet veriden bağımsızdır ve F3'ün "deterministik süre" ölçütü bu yüzden savunulabilir. Sonlu olmayan bir örnek dedektöre girmez. Tepe limiterlar bundan **sonra** gelir ve `|çıkış| ≤ tavan` garantisi bozulmaz.

İki sayısı `G1`'in `S7` adımında, kablolama planının kademeli kurulum tablosuna göre alınır: dört amfi `VIN`'de, adaptörden, sekiz kanalda **4 Ω sınıfı** dummy-load — çünkü sabit kazançlı bir amfide aynı dijital seviyede 4 Ω, 8 Ω'un iki katı akım çeker ve 8 Ω'da alınan bir bütçe 4 Ω'da iki kat cömert olur. Bilinen dijital ortalama karede sinyal sürülür, `VIN` akımı ve çöküşü kaydedilir, adaptör bütçesine dayanan dijital ortalama kare payıyla `supply_budget_sq`'ya, adaptörün bir tepeyi ne kadar süre tolere ettiği `supply_window_ms`'ye yazılır. Tezgâh profilindeki bütçe ve pencere bunun yerini tutan **yer tutuculardır** ve açılışta kendilerini böyle ilan eder; yazılı bütçe 1,0'dır — tam ölçekli tek dalın ortalama karesi kadar, doğrulayıcının 2,0 sınırının yarısı, `G1`'in `S7` adımının yerine gerçeğini yazacağı bir yer tutucu — ve tezgâh kazançlarıyla (0,25 / 0,18; en çok w² + t² ≈ 0,095) ancak kullanıcı EQ'su yükseltirse devreye girer, yani tezgâhta kat sessizce çalışır ve yalnız kısar.

### Tavan ölçekleme tek yönlüdür ve "sürücüdeki volt sabit kalır" iddiası geri çekilir

`hk_profile_ceiling_at()` tavanı `tavan × min(1, referans / besleme)` ile taşır. Besleme referansın üstündeyse tavan iner; **altındaysa tavan olduğu gibi kalır**, yükselmez. Eskiden yükseliyordu (1,0'da kırpılarak) ve testler bunu iddia ediyordu; ikisi de çevrildi.

Sebep veri sayfasındadır. TPA3110D2 (SLOS528F) **sabit kazançlı** bir amfidir: kazanç iki pinle 20/26/32/36 dB'den birine köprülenir (Tablo 2) ve çıkış, ray kırpana kadar `kazanç × giriş`tir — beslemeyi izlemez. Tablo 3 bunu sayıyla gösterir: 24 V'ta 1 Vrms giriş ve 20 dB ile çıkış 27,7 Vpp (kırpılmamış, ≈ 10 Vrms), 12 V'ta aynı giriş 23,5 Vpp'de **raya çarpar**. Yani 12 V'ta dinlenen bir dijital tavan 24 V'ta yarıya indirilirse sürücüdeki volt aynı kalmaz: tezgâh seviyesi 12 V rayını kırpmadıysa **yarıya iner**; kırptıysa 24 V rayı daha geç kırpar ve yarıya inen tavan sürücüye tezgâhın duyduğundan fazlasını verebilir — hangisinin geçerli olduğunu `C3` (amfinin kazanç köprüsü) söyler (`hk_profile.h`). Ölçekleme kısıcı yöndedir, yazıldığı durumda sessize doğru hata yapar ve bu yüzden kalır. Ama bir garanti değildir ve kayıt onu artık öyle anlatmaz: `hk_profile.h`, Kconfig yardımları, arka uç yorumları, ölçüm ve DSP planı ve ADR-0020'nin 37. satırı bu ADR'ye işaretle düzeltildi. Ters yön — 24 V referanslı bir `G2` tavanının 12 V'luk bir yapıda iki katına çıkması — sabit kazançlı amfide sürücüye iki kat volt demektir; tek güvensiz yön oydu ve artık kodda yoktur.

### `C3` 24 V'ta sürücüye dinlemenin önkoşuludur

Amfi kartlarının kazanç köprüsü hiç okunmadı (tezgâh maddesi `C3`). 36 dB'de zincir amfinin istediğinden yaklaşık 26 dB sıcaktır — tezgâh kaydı bunu 2026-09-08'de kaydırıcının üst kısmının kullanılamamasının **muhtemel** sebebi olarak yazıyor, okunmuş sebebi olarak değil — ve 24 V rayı aynı kaydırıcı konumunda tezgâhın 12 V'ta duyduğunun iki katına kadar volta izin verir. Kazanç okunmadan hangi seviyede hangi rayın kırptığı hesaplanamaz. Bu yüzden **`C3` okunmadan hiçbir sürücü 24 V'ta dinlenmez**; firmware, derlendiği besleme tezgâh referansından farklıysa bunu açılışta uyarı olarak basar ve okunan kazanç `amp_gain_db`'ye yazılır.

### Dokunulmayanlar

- **Tepe limiter olduğu gibi kalır**: sıfır attack, lookahead yok. Ölçüm ve DSP planı bunu bilinçli bir karar olarak kaydediyor — tepe geçiren şey koruma değildir, lookahead her örnekte ödenen gecikmedir — ve bu ADR onu yeniden açmaz. `hk_limiter.c` değişmedi; başlığındaki anlatım hizalandı.
- **Tezgâh yapısı tek dinleme yoludur.** `sdkconfig.bench` iki istisnayı (`CONFIG_HK_BENCH_AUDIO_WITHOUT_PROFILE`, `CONFIG_HK_BENCH_PROVISIONAL_PROFILE`) açar ve yer tutucu profili verir; artık arka ucu seçmez, çünkü arka uç zaten üründür. *2026-09-13 eki:* ikinci bir dinleme yolu eklendi ve sahibinin isteğidir — kaynağı (`source`) `provisional` ile başlayan bir profil, `firmware/tools/write_profile.py` ile `factory_cal`'a **sahibin eliyle** flaşlanır ve ürün yapısını kademeli çift üzerinde (tek amfi, bir woofer, `C_SAFE`'li bir tweeter, kaynak ≤ 15 V, düşük seviye) dinletir. Bu bir kalibrasyon değildir: `hk_main` böyle bir profili kabul eder ama açılış raporunda `PROVISIONAL` uyarısıyla ilan eder, ve dosyanın kendisi her alanın ölçüldü / yer tutucu durumunu taşır ([[../04-acoustics/measurement-and-dsp-plan#Profili yazmak — değerler dosyası, araç, flaş|DSP planı]], [[../06-testing/bench-measurement-order#E — Oynatma testi (ürün yolu, sonraki oturum)|tezgâh sırası §E]]). Sekiz sürücüye bağlı bir karta yazılmaz; `G0`/`G2` kapandığında aynı dosya ölçülmüş sayılarla yeniden yazılır ve `provisional` adı düşer.
- **Zincir bitmiş sayılmaz.** Sayıları `G0`, `G1`, `G2`'nindir. Bu ADR biçimi üründe konumlandırır; hiçbir köşe, tavan veya bütçe yazmaz.

## Sonuçlar

- **Profil yoksa ürün hiçbir şey çalmaz.** Alıcı başlamaz; başlasaydı arka uç dijital sıfır yazardı. Bu bir eksik değil, ayarın kendisidir: sessiz bir ürün, ölçülmemiş bir tweeter'a tam bant program göndermeyen bir üründür.
- **Yer tutucular kendilerini ilan eder.** Tezgâh profilinin köşeleri, tavanları ve bütçesi ölçülmüş değildir; bench yapısı açılışta bunu uyarı olarak basar, ürün imajında o dize hiç yoktur ve CI bunu imajı metin olarak tarayarak (`grep -a`) denetler; tezgâh imajında aynı dizenin varlığı da denetlenir. `factory_cal`'a yazılmış `provisional` bir profil için aynı ilan ürün yapısında `hk_main`'den gelir: kaynak adı `provisional` ile başlıyorsa açılış raporu bunun bir dinleme profili olduğunu, kalibrasyon olmadığını uyarı olarak basar (2026-09-13). Tezgâh profilinin tweeter DC direnci operatör kaydını izler (kayıt 3,5 Ω yazıyor, firmware 3,7 taşıyordu; kayıt kazanır, operatör doğrular); ayrıntı günlüktedir.
- **Kayıt düzeltmeleri.** ADR-0013'ün 36, 53 ve 55. satırları yerinde ve tarihli işaretle düzeltildi; ADR-0002'nin DSP durumu ve giriş yükü aritmetiği (yalnız 36 dB'de geçerli olduğu; kazanç okunmadan paralel yük tipik 2,25–15 kΩ, uçlarda 1,8–18 kΩ) düzeltildi; ADR-0020'nin 31, 37 ve 61. satırları bu ADR'ye işaretle düzeltildi. Hiçbir kilitli karar sessizce açılmadı: besleme topolojisi, sinyal zinciri ve tek kabin olduğu gibi durur.
- **Amfi 24 V / 4 Ω çalışma noktasında karakterize edilmemiştir.** TPA3110D2'nin mutlak azami değer tablosu `PVCC > 15 V` için asgari BTL yükünü 4,8 Ω verir; iki Nova sürücüsü de 4 Ω sınıfıdır. Bu, `AGENTS.md`'de bir tıkayıcı ve [[../01-planning/risk-register|risk kaydında]] `Kritik` bir satırdır: `G0` `Z_min`'i verene ve `G1` amfinin 24 V'ta 4 Ω sınıfı **dummy-load**a ısıl ve koruma davranışını kaydedene kadar hiçbir sürücü 24 V'ta bağlanmaz. Veri sayfasının aracı `PLIMIT` (sanal düşük ray) bir adaydır; XH-A232'nin `PLIMIT` bağlantısı doğrulanmadı ve karar `G1`'in ölçümünündür.
- **Bekçiler.** `firmware-ci.yml` ürün işi `CONFIG_HK_AIRPLAY_OUTPUT_DSP=y`'yi ve imajda tezgâh dizesinin yokluğunu iddia eder; ayrı bir tezgâh yapısı adımı iki `HK_BENCH_*` sembolünü ve bölüm sığmasını denetler; belge işi gölge betiğini çalıştırır. `release.yml` geçiş katını ve her tezgâh bool'unu tek bir düzenli ifadeyle reddeder. `.githooks/pre-commit` gölge betiğini de çalıştırır. `scripts/check_docs.py` iki kural kazanır: geçiş katının adı yalnız Kconfig, CMake, iş akışları, günlük, `firmware/README.md`, bu ADR ve kuralın kendisinde yazılabilir; bir yer tutucu sayının "ölçüldü / doğrulandı / kesin" ile aynı cümlede anılması hatadır ("kesin değildir" yazılabilir).
- **Çekilen ağaç tuzağı.** `idf.py`'nin ürettiği izlenmeyen `sdkconfig` dosyaları fragmanları yener. Ürün yapısında geçiş katı seçilemez olduğundan eski bir `firmware/sdkconfig` onu taşıyamaz: kconfgen görünmeyen seçimi düşürür ve yapı DSP'ye döner. Geliştirme kartı ve tezgâh istisnası yapılarında ise geçiş katı seçilebilir kalır, yani bu ADR'den önce üretilmiş bir `firmware/build-bench/sdkconfig` (veya `build-devkit/sdkconfig`) `CONFIG_HK_AIRPLAY_OUTPUT_I2S=y`'yi korur — ve amfilere ulaşan yapı tezgâh yapısıdır. Çektikten sonra ikisi de silinir (`firmware/README.md`); aksi hâlde yerel tezgâh yapısı CI'ın reddettiği katı sessizce derler.
- `AGENTS.md` kilitli kararlar listesi bu ADR'yi gösterir; çıkış arka ucu yalnız supersede eden bir ADR ile değişir.
- **Fiziksel hiçbir test geçmedi.** Bu ADR'nin kanıtı tamamen otomatiktir ve [[../08-development-log/2026-09-12-dsp-product-output|2026-09-12 günlüğünde]] kayıtlıdır; `G0`, `G1`, `G2` açıktır.

## Doğrulama / geri dönüş

**Doğrulama** — hepsi tezgâhsız, sayılar günlükte:

- Host paketi (`firmware/test`): profil şeması 2'nin her reddi, `hk_profile_load` mutlu yolu ve retleri, tek yönlü ölçekleme, dördüncü derece subsonic'in eğimi (24 dB/oktav, köşede −3 dB, 4·fc'de düz), gecikmenin örnek-kesinliği, polarite, besleme katının gürültülü iki tonda devreye girip sessizde girmemesi, besleme katı açıkken `|çıkış| ≤ tavan`, NaN katsayılı zincirin reddi.
- Üç derleme: ürün (`firmware/build`, DSP=y, tezgâh dizesi yok), geliştirme kartı (SPDIF), tezgâh (`firmware/build-bench`, DSP=y, iki `HK_BENCH_*` sembolü, `tools/check_partitions.py`).
- `firmware/test/nvs_host`: var + yargılanmamış → izin yok; geçerli bulundu → izin; geçersiz bulundu → izin yok; kullanıcı sıfırlaması blob'u ve hükmü korur.
- `python3 scripts/check_docs.py` sıfır hata; `scripts/check_vendor_output_shadow.py` sessiz.

**Geri dönüş** iki Kconfig satırıdır: seçimin `default` satırı (`default HK_AIRPLAY_OUTPUT_DSP` → `default HK_AIRPLAY_OUTPUT_I2S`) ve geçiş üyesinin `depends on` satırı. Yalnız `default` çevrilirse Kconfig görünmeyen varsayılanı atlar ve ürün ilk görünür üyede, DSP'de kalır; ürün imajı için geri dönüş hiçbir şey yapmaz. Sessiz de değildir: tezgâh istisnası üyeyi görünür kıldığından tezgâh yapısı geçiş katına döner ve `firmware-ci.yml`'nin tezgâh adımının DSP=y iddiası kırılır. İki satır birlikte çevrildiğinde ürün ve sürüm de döner ve gürültü `firmware-ci.yml`'nin ürün işi ile `release.yml`'nin DSP=y iddiasındandır; yani geri dönüş iki hâlde de fark edilmeden birleşemez. `check_docs`'un geçiş katı kuralı bu gürültünün parçası değildir: Kconfig'i izinli sayar; o kural bir fragmanın ya da notun geçiş katını seçmesini yakalar, bu geri dönüşü değil. Tek yargıç kapısı ve şema 2 geri dönüşten bağımsızdır ve kalır.
