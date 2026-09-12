---
status: active
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-09-12
tags: [power, hardware]
---

# Güç planı

Merzarkabul Airplay Speakers'ın her hoparlörü prizden beslenen bir masaüstü hoparlördür. Bu belge o beslemenin kutuya nasıl girdiğini, iki raya nasıl dağıldığını ve hangi ölçümlerle kabul edildiğini toplar.

Kanonik kararlar: [[07-decisions/ADR-0020-dc-adapter-power|ADR-0020 (19 V DC adaptörle besleme)]], [[07-decisions/ADR-0010-esp32-s3-n16r8-board|ADR-0010 (N16R8 kart)]].

## Karar özeti

- Her hoparlör **19 V DC masaüstü adaptörle** beslenir. Adaptör kutuya **5,5 × 2,1 mm, merkez pozitif** barrel jak üzerinden girer; jaktan gelen ray şemada `VIN`'dir.
- `VIN`, XH-A232 / TPA3110 amfiyi **doğrudan** besler. Kartın giriş aralığı 8-26 V'tur ve 19 V bu pencerenin içindedir; arada regülatör yoktur.
- ESP32-S3 ve PCM5102A'yı ayrı bir 5 V buck (MP1584EN sınıfı) besler: girişi `VIN`, çıkışı yüksüz `5,10 V`'a ayarlanır.
- **V1'de güç anahtarı yoktur.** Cihaz adaptörü çekilerek kapatılır; boşta beklemeyi firmware'in idle standby'ı (`standby_min`) karşılar.
- Firmware nominal beslemeyi tek bir yerden bilir: `CONFIG_HK_SUPPLY_MV`, varsayılan `19000`, aralık `8000-26000`. Ses profilindeki limiter tavanı, ölçüldüğü besleme gerilimiyle birlikte saklanır ve bu değere ölçeklenir. Tezgâh referansı `HK_BENCH_REFERENCE_SUPPLY_MV = 12000`'dir; 19 V'ta tavanın aşağı inmesi doğru yöndür ve biri 24 V'luk bir adaptör takarsa tweeter'ı koruyan şey yine bu tek çarpımdır.

Çalışma noktası veri sayfasından değil ölçümden gelir: TPA3110D2'nin 19 V'ta 4 Ω sınıfı Nova sürücülere vereceği güç `G1`'de dummy-load üzerinde kaydedilir. Seviye tavanını besleme gerilimi değil, `G1`/`G2` ölçümleri ve limiter belirler. Sürücülerin empedans eğrisi ve `Fs` ölçülmeden amfi seviyesi ve limiter kilitlenmez.

## Blok şema

```text
19 V DC masaüstü adaptör
             |
     5,5 × 2,1 mm jak (merkez pozitif) -- TP0 DC_IN
             |
     D2 seri Schottky / ideal-diyot (ADAY; yoksa 0 Ω köprü)
             |
            VIN -- TP1 ------------------> XH-A232 / TPA3110 (8-26 V)
             |
             +--> MP1584 5,10 V -- TP3 --> ESP32-S3 + PCM5102A
```

Kablo, konnektör ve test noktası ayrıntıları [[02-hardware/circuit-and-wiring-plan|devre ve bağlantı planında]] tutulur; bu belge onları tekrarlamaz.

## Polarite ve ters bağlantı

Ters polarite koruması iki katmandır ve ikisi de `G1` satırıdır:

1. **Ölçüm.** İlk enerjilendirmeden önce satın alınan adaptörün ucu ve kutudaki jak ölçü aletiyle doğrulanır: merkez pozitif. Etiket okumak ölçüm yerine geçmez; 5,5 × 2,1 mm uçlu merkez negatif adaptörler de satılır.
2. **Parça.** `VIN` üzerinde seri bir Schottky veya ideal-diyot modülü (`D2`) kablolama planında **aday** olarak durur. Kabulü `G1`'de ölçülen ileri düşüme ve ısınmaya bağlıdır: düşüm amfinin headroom'undan ve buck'ın girişinden gider. Takılmazsa yerine 0 Ω köprü gelir; `DC_IN` ile `VIN` o zaman tek nettir.

## G1 satırları

Adaptör beslemesinin `G1`'e (amfi + dummy-load) eklediği ölçümler:

| Ölçüm | Nerede | Kabul |
|---|---|---|
| Jak polaritesi | TP0/TP2, enerjisiz adaptör ucu | Merkez pozitif, kayıtlı |
| `D2` düşümü ve ısısı | TP0 − TP1, yükte | Kabul/ret kararı kayıtlı; ret ise 0 Ω köprü |
| Besleme çöküşü | TP1 bas tepesinde, TP3 Wi-Fi sıçramasında | `VIN` adaptör akım sınırına girmiyor; 5 V hattı 4,75 V altına düşmüyor |
| Brownout | TP4 | Reset üreten çökme yok |
| Açma/kapama pop | TP1 + TP20/TP21 + TP8/TP9 single-shot | Kapanış adaptör çekilerek kaydedilir; pop yok, susturma hatları açılış penceresi boyunca LOW |
| Adaptör gürültüsü | Dip gürültü, cızırtı, ground-loop; adaptör bağlıyken | Lab kaynağıyla alınan tabana göre fark kayıtlı |

Adaptörün akım değeri de buradan çıkar: seçilen seviyede amfinin sürekli çektiği akım artı dijital tarafın payı ölçülür, BOM'daki adaptör satırı o rakama göre kilitlenir. Bir sayı varsayılmaz.

## Hoparlör başına güç BOM'u

| Kalem | Adet | Asgari özellik | Durum / aday |
|---|---:|---|---|
| 19 V DC masaüstü adaptör | 1 | 5,5 × 2,1 mm uç, merkez pozitif; sürekli akım en az `G1` tepe akımı | **Aday, marka/model ve fiyat yok.** Çıkışın PE'ye bağlı olup olmadığı sorulur |
| DC giriş jakı | 1 | 5,5 × 2,1 mm panel tipi; kontak akımı en az ölçülen tepe akım + %50 | Direnc.net DC-005 adayı ([[05-procurement/suppliers|satıcılar]]) |
| `D2` ters polarite | 1 koşullu | Seri Schottky veya ideal-diyot; `G1` tepe akımını taşır | Aday; ret ise 0 Ω köprü |
| `C_A` bulk kondansatör | 1 | 470-1.000 µF / 25 V, 105 °C düşük-ESR hedef | Aday; değer `G1` ripple ölçümüyle |
| 5 V regülatör | 1 | 4,5-28 V giriş, 5 V / en az 2 A | MP1584EN 3 A modül; 5,10 V'a ayarlanıp yük altında test edilecek |
| Fonksiyon butonu | 1 | Anlık, normalde açık | Provisioning ve reset; aktif-low GPIO |
| RGB durum LED'i | 1 | Ortak katot, 3 kanal | Her renge seri direnç ve PWM GPIO |

## Ses ve EMI kuralları

- MP1584, PCM5102A'nın analog çıkışından ve amfi giriş kablolarından uzakta konumlandırılacak.
- 5 V hattında buck çıkışına yakın düşük ESR kapasitör ve gerekirse ferrit/LC filtre denenecek.
- Güç ve analog ses toprakları yıldız noktada birleştirilecek; amfi hoparlör eksi uçları hiçbir zaman şaseye bağlanmayacak.
- Wi-Fi yayın akımı sıçramalarında ESP32 brownout testi yapılacak.
- Adaptör bağlıyken dip gürültüsü, cızırtı ve ground-loop ölçülecek; adaptör bu cihazın olağan besleme kaynağıdır, istisnası değil.

## Mekanik ve güvenlik

- Adaptör kabinin dışındadır; kabine yalnız jak girer.
- Elektronik bölme akustik hacimden ayrılır; kablolar pasif radyatör ve woofer hareket alanına girmez.
- İlk enerjilendirmeler tezgâhta, adaptörle değil akım sınırlı laboratuvar kaynağıyla yapılır.
- Adaptörle osiloskop kullanmadan önce adaptör çıkışının koruma toprağı ve DUT ile izolasyon ilişkisi ölçülür; belirsizse adaptörle scope bağlanmaz.
- Kabin kapatılmadan önce polarite ve kısa devre kontrolü yapılır.

## Hâlâ aday olanlar

- Adaptörün markası, modeli, akım sınıfı ve fiyatı (`G1` tepe akımından sonra).
- `D2`'nin tipi, ya da hiç olmaması.
- `C_A` değeri ve sınıfı.
- Sonraki sürüm için DC hattında güç anahtarı: ADR-0020 bunu V1 dışında bırakır; istenirse belgeli DC kontak değeri ve `G1` ark/yük testiyle yeni bir ADR açılır.

## Teknik kaynaklar

- TPA3110D2: https://www.ti.com/lit/ds/symlink/tpa3110d2.pdf
- MP1584: https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MP1584/document_id/204/
