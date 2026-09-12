---
status: active
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-12
tags: [power, hardware]
---

# Güç planı

Merzarkabul Airplay Speakers prizden beslenen tek bir kabindir. Bu belge o beslemenin kabine nasıl girdiğini, üç raya (`VIN`, 5 V ESP, 5 V DAC) nasıl dağıldığını ve hangi ölçümlerle kabul edildiğini toplar.

Kanonik kararlar: [[07-decisions/ADR-0020-dc-adapter-power|ADR-0020 (24 V DC adaptörle besleme)]], [[07-decisions/ADR-0002-biamp-signal-chain|ADR-0002 (tek DAC, dört amfi)]], [[07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010 (N16R8 kart)]].

## Karar özeti

- Kabin **24 V / 2,9 A (yaklaşık 70 W) DC masaüstü adaptörle** beslenir. Adaptör kabine **5,5 × 2,1 mm, merkez pozitif** barrel jak üzerinden girer; jaktan gelen ray şemada `VIN`'dir.
- `VIN`, dört XH-A232 / TPA3110 amfiyi **doğrudan** besler. Kartın giriş aralığı 8-26 V'tur; 24 V bu pencerenin içindedir ve 26 V tavanına 2 V kalır. Arada regülatör yoktur.
- Lojik tarafını iki MP1584EN sınıfı buck besler, ikisinin de girişi `VIN`, çıkışı yüksüz `5,10 V`'a ayarlanır: **buck A** (`U3`) ESP32-S3'ü, **buck B** (`U4`) PCM5102A'yı. Ayrı olmalarının sebebi tezgâhta duyulan bir şeydir: paylaşılan tek buck DAC'a duyulur hışırtı verdi (sahibin gözlemi, 2026-09-12; düzenek ayrıntısı kaydedilmedi, bu bir kapı geçişi değildir). Buck B'de USB geri besleme jumperı yoktur; `JP1` yalnız buck A'nın ESP geliştirme kartı tarafındadır.
- **V1'de güç anahtarı yoktur.** Cihaz adaptörü çekilerek kapatılır; boşta beklemeyi firmware'in idle standby'ı (`standby_min`) karşılar.
- Firmware nominal beslemeyi tek bir yerden bilir: `CONFIG_HK_SUPPLY_MV`, varsayılan `24000`, aralık `8000-26000`. Ses profilindeki limiter tavanı, ölçüldüğü besleme gerilimiyle birlikte saklanır ve bu değere ölçeklenir. Tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV = 12000`'dir; 24 V'ta tavanın aşağı inmesi doğru yöndür. Etiketi 24 V olan ama yüksüz 26 V'a çıkan bir adaptörü ürünün dışında tutan şey ise aşağıdaki yüksüz ölçüm kuralıdır, çarpım değil.

İki `G1` kuralı adaptörün kendisine aittir:

- **Yüksüz çıkış, bağlanmadan önce ölçülür:** `< 25,5 V` değilse adaptör bağlanmaz, derate edilmez, reddedilir.
- **Limiter tavanı 2,9 A bütçesinden türetilir:** sekiz BTL kanal 70 W'ın çok üstünü çekebilir ve çöken `VIN` ESP32-S3'ü şarkı ortasında sıfırlar. Tavan, dört amfi birlikte sürülürken 2,9 A'yı aşmayacak biçimde belirlenir ve `G1`'de tam yükte `VIN` çöküşüyle doğrulanır.

Çalışma noktası veri sayfasından değil ölçümden gelir: TPA3110D2'nin 24 V'ta 4 Ω sınıfı Nova sürücülere vereceği güç `G1`'de dummy-load üzerinde kaydedilir. Seviye tavanını `G1`/`G2` ölçümleri ve limiter belirler; sürücülerin empedans eğrisi ve `Fs` ölçülmeden amfi seviyesi ve limiter kilitlenmez.

## Blok şema

```text
24 V / 2,9 A DC masaüstü adaptör  (yüksüz çıkış bağlanmadan önce ölçülür: < 25,5 V)
             |
     5,5 × 2,1 mm jak (merkez pozitif) -- TP0 DC_IN
             |
     D2 seri Schottky / ideal-diyot (ADAY; yoksa 0 Ω köprü)
             |
            VIN -- TP1 --+--> 4 × XH-A232 / TPA3110 (8-26 V), C_A tek, jak girişinde
                         |
                         +--> U3 MP1584 buck A 5,10 V -- TP3 --> ESP32-S3 (JP1 üzerinden)
                         |
                         `--> U4 MP1584 buck B 5,10 V -- TP4 --> PCM5102A
```

Kablo, konnektör ve test noktası ayrıntıları [[02-hardware/circuit-and-wiring-plan|devre ve bağlantı planında]] tutulur; bu belge onları tekrarlamaz.

## Polarite ve ters bağlantı

Ters polarite koruması iki katmandır ve ikisi de `G1` satırıdır:

1. **Ölçüm.** İlk enerjilendirmeden önce satın alınan adaptörün ucu ve kabindeki jak ölçü aletiyle doğrulanır: merkez pozitif. Etiket okumak ölçüm yerine geçmez; 5,5 × 2,1 mm uçlu merkez negatif adaptörler de satılır.
2. **Parça.** `VIN` üzerinde seri bir Schottky veya ideal-diyot modülü (`D2`) kablolama planında **aday** olarak durur. Kabulü `G1`'de ölçülen ileri düşüme ve ısınmaya bağlıdır: düşüm amfilerin headroom'undan ve buck'ların girişinden gider, ve 2,9 A'da bir Schottky yaklaşık 1 W veya üstü ısınır — bu, 0 Ω köprü sonucunu güçlendirir ama kararı ölçüm verir. Takılmazsa yerine 0 Ω köprü gelir; `DC_IN` ile `VIN` o zaman tek nettir.

## G1 satırları

Adaptör beslemesinin `G1`'e (amfi + dummy-load) eklediği ölçümler:

| Ölçüm | Nerede | Kabul |
|---|---|---|
| Adaptör yüksüz çıkışı | Adaptör ucu, bağlamadan önce, DMM | `< 25,5 V`, kayıtlı; değilse adaptör reddedilir |
| Jak polaritesi | TP0/TP2, enerjisiz adaptör ucu | Merkez pozitif, kayıtlı |
| `D2` düşümü ve ısısı | TP0 − TP1, 2,9 A'ya yakın yükte | Kabul/ret kararı kayıtlı; ret ise 0 Ω köprü |
| Besleme bütçesi ve çöküşü | TP1, dört amfi limiter tavanında birlikte sürülürken; TP3/TP4 Wi-Fi sıçramasında | Toplam akım 2,9 A'yı aşmıyor; `VIN` çökmüyor, ESP reset yok; iki 5 V hattı da 4,75 V altına düşmüyor |
| Brownout | TP5 | Reset üreten çökme yok |
| Buck gürültüsü | TP4 ve DAC çıkışı (TP9/TP10), amfi girişleri (TP11/TP12) | DAC hattında hışırtı yok; lab kaynağıyla alınan tabana göre fark kayıtlı |
| Açma/kapama pop | TP1 + TP33 + TP6 + TP9/TP10 single-shot | Kapanış adaptör çekilerek kaydedilir; `XSMT` açılış penceresi boyunca LOW ve kapanışta `BCLK` durmadan önce düşüyor; amfinin kendi pop'u (susturma girişi yok) olduğu gibi kaydedilir |
| Adaptör gürültüsü | Dip gürültü, cızırtı, ground-loop; adaptör bağlıyken | Lab kaynağıyla alınan tabana göre fark kayıtlı |

Adaptörün akım değeri sabittir: 2,9 A. Ondan türeyen şey limiter tavanıdır, tersi değil; `G1` bütçe satırı tavanın o akıma sığdığını gösterir.

## Güç BOM'u

| Kalem | Adet | Asgari özellik | Durum / aday |
|---|---:|---|---|
| 24 V / 2,9 A DC masaüstü adaptör | 1 | 5,5 × 2,1 mm uç, merkez pozitif; yüksüz çıkış < 25,5 V; 2,9 A sürekli | **Aday, marka/model ve fiyat yok.** Yüksüz çıkış ve çıkışın PE'ye bağlı olup olmadığı sorulur |
| DC giriş jakı | 1 | 5,5 × 2,1 mm panel tipi; kontak akımı 2,9 A sürekli, belgeli | Direnc.net DC-005 adayı ([[05-procurement/suppliers|satıcılar]]); kontak değeri tedarikçi sorusu |
| `D2` ters polarite | 1 koşullu | Seri Schottky veya ideal-diyot; 2,9 A sürekli taşır, ≈1 W ısı | Aday; ret ise 0 Ω köprü |
| `C_A` bulk kondansatör | 1 | 470-1.000 µF / **35 V**, 105 °C düşük-ESR hedef; tek, jak girişinde | Aday; değer `G1` ripple ölçümüyle; amfi başına ek bulk yalnız G1 isterse |
| 5 V regülatör, buck A (`U3`) | 1 | 4,5-28 V giriş, 5 V / en az 2 A | MP1584EN 3 A modül; ESP32-S3; 5,10 V'a ayarlanıp yük altında test edilecek |
| 5 V regülatör, buck B (`U4`) | 1 | 4,5-28 V giriş, 5 V / en az 1 A | MP1584EN 3 A modül; PCM5102A; ayrı buck çünkü paylaşılan buck DAC'a hışırtı verdi |
| Fonksiyon butonu | 1 | Anlık, normalde açık | Provisioning ve reset; aktif-low GPIO |
| RGB durum LED'i | 1 | Ortak katot, 3 kanal | Her renge seri direnç ve PWM GPIO |

## Ses ve EMI kuralları

- İki MP1584, PCM5102A'nın analog çıkışından ve amfi giriş kablolarından uzakta konumlandırılacak; buck B beslediği DAC'ın giriş ucuna yakın, analog çıkışından uzak durur.
- Her 5 V hattında buck çıkışına yakın düşük ESR kapasitör ve gerekirse ferrit/LC filtre denenecek.
- Güç ve analog ses toprakları yıldız noktada birleştirilecek; dört amfinin dönüşü `POWER_GND` yıldızına; hoparlör eksi uçları hiçbir zaman şaseye bağlanmayacak.
- Wi-Fi yayın akımı sıçramalarında ESP32 brownout testi yapılacak.
- Adaptör bağlıyken dip gürültüsü, cızırtı ve ground-loop ölçülecek; adaptör bu cihazın olağan besleme kaynağıdır, istisnası değil.

## Mekanik ve güvenlik

- Adaptör kabinin dışındadır; kabine yalnız jak girer.
- Elektronik bölme akustik hacimden ayrılır; kablolar pasif radyatör ve woofer hareket alanına girmez ([[04-acoustics/cabinet-plan|kabin planı]]).
- İlk enerjilendirmeler tezgâhta, adaptörle değil akım sınırlı laboratuvar kaynağıyla yapılır; ilk enerjilenen yol bir amfi, bir woofer ve bir tweeter'dır.
- Adaptörle osiloskop kullanmadan önce adaptör çıkışının koruma toprağı ve DUT ile izolasyon ilişkisi ölçülür; belirsizse adaptörle scope bağlanmaz.
- Kabin kapatılmadan önce polarite ve kısa devre kontrolü yapılır.

## Hâlâ aday olanlar

- Adaptörün markası, modeli ve fiyatı (değeri sabit: 24 V / 2,9 A; yüksüz çıkışı ölçülmeden alınmaz).
- `D2`'nin tipi, ya da hiç olmaması; 2,9 A'daki düşüm ve ısı.
- `C_A` değeri (35 V sınıfı).
- Jak kontağının 2,9 A sürekli akım değeri (tedarikçi sorusu).
- Sonraki sürüm için DC hattında güç anahtarı: ADR-0020 bunu V1 dışında bırakır; istenirse belgeli DC kontak değeri ve `G1` ark/yük testiyle yeni bir ADR açılır.

## Teknik kaynaklar

- TPA3110D2: https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf
- MP1584: https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584/document_id/204/
