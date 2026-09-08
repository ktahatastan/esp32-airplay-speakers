---
status: pending
owner: acoustics-engineer
updated: 2026-08-30
tags: [drivers, measurements, gate]
---

# Sürücü ölçüm planı — G0

## Eldeki sürücüler (operatör kaydı, 2026-09-08)

Harman Kardon Nova'dan sökülmüş. Etiketlerde yalnız üretici iç kodları var;
bunlar veri sayfasına çevrilmiyor, dolayısıyla **ölçüm tek yol**.

| sürücü | etiketteki kodlar |
|---|---|
| woofer/mid | `66057-0001006` · `110066` · `06801` · `2313351` |
| tweeter | `660056-0001004` · `310013` · `01007` · `2113356` |

**Woofer'ın arkasına ek mıknatıs yapıştırılmış.** Bu bir not değil, ölçümü
etkileyen bir gerçek: ek mıknatıs motor kuvvetini (`Bl`) değiştirir, dolayısıyla
`Fs`, `Qts` ve `Qes` fabrika değerlerinden sapar. Aynı modelin başka bir yerde
yayınlanmış parametresi bulunsa bile bu sürücüye uymaz. Aşağıdaki ölçümler bu
yüzden isteğe bağlı değil.

## Ölçüldü — DC direnç (operatör, 2026-09-08)

| sürücü | okunan `Re` | çıkarım |
|---|---|---|
| woofer/mid | **4,0 Ω** | nominal **4 Ω** |
| tweeter | **3,5 Ω** | nominal **4 Ω** |

Kural: `Re` genelde nominal empedansın 0,75–0,85 katıdır. 4 Ω'luk bir sürücü
tipik olarak 3,0–3,6 Ω okur, 6 Ω'luk 4,4–5,0 Ω. Woofer'ın 4,0'ı iki aralığın
sınırında duruyor; **prob direnci çıkarılmadıysa** gerçek değer ~3,7 Ω olur ve
tereddüt kalmaz. Tweeter'ın 3,5'i zaten net biçimde 4 Ω sınıfı.

### Bu neyi açıyor, neyi açmıyor

**Açtığı:** her iki sürücü de 4 Ω sınıfı. TPA3110 BTL için tipik asgari yük
4 Ω'dur, yani zincir sınırın içinde — ama sınırın *üstünde* değil, tam üstünde.
Bu, güvenli amfi seviyesi ve termal bütçe için ilk somut girdi.

**Açmadığı:** `Re` bir sayıdır, empedans eğrisi değildir. Hâlâ eksik olanlar:

- **Tweeter `Fs`.** Yüksek geçiren filtrenin en düşük güvenli kesim frekansını bu
  belirler (kural: `Fs`'nin en az iki katı). Bu ölçülmeden crossover köşesi
  muhafazakâr bir tahmindir, ölçüm değil.
- **Empedans minimumu.** Müzikte gerçek yük `Re`'nin altına inebilir; amfinin
  akım sınırı buna bakar.
- **Woofer `Fs`/`Qts`.** Arkasına ek mıknatıs yapıştırıldığı için yayınlanmış
  hiçbir değer geçerli değil.

Yani `AGENTS.md`'deki "sürücü empedansı doğrulanmadı" tıkayıcısı **kısmen**
kapandı: nominal empedans biliniyor, koruyucu filtrenin köşesi hâlâ bilinmiyor.

## Tweeter seri kondansatörü `C_SAFE` (2026-09-08)

**Seçilen değer: 10 µF, kutupsuz film, ≥ 50 V.**

Bu bir crossover değil, **emniyet supabı**: asıl filtreleme DSP'de olacak
(LR4, 24 dB/oktav). Bunun işi firmware çökerse, DSP yanlış yüklenirse veya biri
tam bantlı sinyal gönderirse tweeter'ı hayatta tutmak.

Değeri seçen şey `Fs`'ye olan mesafe. 4 Ω'da `C = 1/(2π·R·Fc)`:

| C | köşe | `Fs`=1200 | `Fs`=1500 | `Fs`=2000 |
|---:|---:|---:|---:|---:|
| **10 µF** | 3980 Hz | 3,3× | 2,7× | **2,0×** |
| 15 µF | 2650 Hz | 2,2× | 1,8× | 1,3× |
| 22 µF | 1810 Hz | 1,5× | 1,2× | 0,9× |

25 mm kubbe için makul `Fs` aralığı 1200–2000 Hz. 10 µF bu aralığın **tamamının**
en az iki katı üstünde kalıyor. Daha büyük bir kondansatörün köşesi rezonansa
yaklaşır, ve orada seri kondansatör koruma sağlamaz — empedans tepesiyle birlikte
rezonans devresi kurar ve yanıtı tepelendirir, yani korumak istediği yerde
eksürsiyonu artırır.

**Bedeli:** 3,5 kHz'de −3,6 dB, yani DSP kesimiyle üst üste biniyor ve akustik
geçiş noktasını yukarı itiyor. `Fs` ölçülene kadar bu kabul ediliyor: şu anda
DSP crossover'ı yok, dolayısıyla bu kondansatör tweeter'ın tek koruması, ve
bilinmeyen bir `Fs`'ye karşı sağlamlık geçiş bandındaki 3 dB'den önce gelir.

**Kutupsuz olması şart, sebebi BTL:** amfi köprülü çıkışlı, hoparlörün eksi ucu
toprak değil, o da salınıyor. Kutuplu bir kondansatör orada ters gerilim görür.

`Fs` ölçüldükten sonra yeniden değerlendirilir; değiştirmek tek lehim noktasıdır.

## Bilinenler

- Nova'dan **60 mm woofer** ve **25 mm kubbe tweeter** çıktı (operatör ölçümü,
  2026-09-08). Önceki "yaklaşık 63 mm / 35 mm" notu tahmindi ve tweeter'da
  yanlıştı; 25 mm, kondansatör seçimini değiştirdiği için önemli.
- Woofer'ın arkasında **çift mıknatıs yığını** var (fotoğrafla doğrulandı):
  mıknatıs, çelik plaka, ikinci mıknatıs. 60 mm'lik bir sürücü için alışılmadık
  derecede güçlü bir motor — `Bl` yüksek, dolayısıyla `Qts` düşük ve `Fs`
  fabrika değerinden sapmış olmalı.
- Orijinal sistem bi-amp/DSP kullandığı için tek tek sürücü ohm değeri sistem ilanından çıkarılamaz.
- Kesin DC direnç ve nominal empedans henüz doğrulanmadı.

## Her sürücü için

- Ön/arka/etiket/mıknatıs fotoğrafı ve benzersiz kimlik.
- Multimetreyle DC direnç; prob direnci dahil.
- Polarite ve terminal işareti.
- Empedans eğrisi ve rezonans bölgesi.
- Düşük seviyeli tarama; sürtünme/bozulma kontrolü.

Ham veriler `docs/assets/measurements/drivers/` altında tutulur. Tüm sürücüler ölçülmeden nominal ohm veya güvenli crossover kilitlenmez.
