---
status: pending
owner: acoustics-engineer
reviewers: [orchestrator, hardware-engineer]
updated: 2026-09-12
tags: [acoustics, cabinet, enclosure, passive-radiator]
---

# Kabin planı

Merzarkabul Airplay Speakers **tek bir kabindir**: dört Nova woofer, dört Nova tweeter ve Nova'nın kendi pasif radyatörleri, tek mono program, ağda tek cihaz ([[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]). Bu not kabinin akustik kararlarını ve her kararın hangi ölçümü beklediğini toplar. Elektrik tarafı [[../02-hardware/circuit-and-wiring-plan|devre ve bağlantı planında]], sürücü ölçümü [[../02-hardware/driver-measurements|sürücü ölçüm planında]].

## Hizalama: pasif radyatörlü refleks

Kabin, pasif radyatörle akortlanmış bir refleks hizalamadır; kanal yoktur. Sebebi eldeki parçadır: Nova'nın pasif radyatörleri operatörün elinde (operatör kaydı, 2026-09-08) ve Nova bu sürücüler için onlarla akortlanmıştı. Ölçülmemiş bir sürücüye kanal tasarlamak tahmin olurdu; pasif radyatör ise kutu bittiğinde ölçülür ve gerekirse kütle eklenerek akort aşağı çekilir.

Akort frekansı `Fb` kabin hacmine bağlıdır ve **ölçülür**: woofer'lar kutunun içindeyken alınan empedans eğrisi iki tepe verir, aradaki çukur `Fb`'dir. Perakende/inceleme kaynakları orijinal Nova için 60-65 Hz veriyor; yeni kabinin hacmi farklıysa akort da farklıdır ve o sayı yalnız bir başlangıç beklentisidir.

`Fb`'nin altında pasif radyatör akustik yükü bırakır ve woofer havasız kalır: koni serbest salınır, eksürsiyon hızla artar, ses üretilmez. DSP'deki subsonic yüksek geçiren bunun için vardır ve köşesi `Fb`'den türetilir; 55 Hz, Nova'nın 60-65 Hz akordunun hemen altında bir yer tutucudur ve kabin bittiğinde ölçülen `Fb` ile değişir. Woofer'ın serbest hava `Fs`'si 2026-09-12 kaba taramasında **≈ 50 Hz** çıktı (±%13, [[../02-hardware/driver-measurements#11. Sonuç|ölçüm kaydı]]); kutu içinde rezonans yukarı çıkar, akort bu sayıya göre yapılır ([[measurement-and-dsp-plan|ölçüm ve DSP planı]]).

## Hava hacmi: başlangıçta ortak, karar G0 sonrası

Başlangıç varsayımı **tek paylaşılan hava hacmidir**: dört woofer ve pasif radyatörler aynı hacmi görür. Aynı programı çalan dört özdeş woofer için bu en az bölme, en az sızdırmazlık yüzeyi ve tek bir `Fb` demektir.

Woofer'ların ayrı hacim alıp almayacağı `G0` verisiyle karar verilir: `Fs` ve `Vas` ölçülmeden hacim hesabı yapılamaz, ve dört woofer'ın `Fs`/`Re` eşleşmesi kötüyse ortak hacim eşleşmeyen sürücüleri birbirinin yükü yapar. Karar ölçütü ve sonucu bu nota yazılır; o zamana kadar mekanik çizim bölme duvarı olmadan, ama bölme eklenebilecek biçimde planlanır.

Woofer'ın arkasındaki ek mıknatıs `Bl`'yi büyütüp `Qts`'yi düşürür; bu da hacim hesabını fabrika değerinden uzaklaştırır ve ölçümü kaçınılmaz kılar.

## Sürücü dizilimi: belirlenmedi

Dört woofer ve dört tweeter'ın kabin üzerindeki yerleşimi henüz belirlenmedi. Tek program çalındığı için dizilim stereo görüntü değil, dağılım ve ortak-hacim davranışı sorusudur. Adaylar karşılaştırılırken bakılacak şeyler:

- Woofer'lar arası mesafe ve crossover köşesindeki tarak etkisi (tweeter'a geçişte dört kaynak, sonra dört başka kaynak).
- Tweeter'ların birbirine ve dinleme eksenine göre konumu; dört tweeter'ın toplamı tek bir tweeter gibi ölçülmeyecektir.
- Pasif radyatörlerin woofer'lardan ve elektronik bölmeden uzaklığı; kablolar pasif radyatör ve woofer hareket alanına girmez.
- Elektronik bölme akustik hacimden ayrıdır; dört amfi ve iki buck'ın ısısı kapalı kabinde `G8` ile ölçülür.

Karar yakın alan ve dinleme ekseni ölçümüyle verilir (ölçüm ve DSP planı, 7. madde), dinleyerek tahmin edilmez.

## Kabin: bu notun beklediği ölçümler

| Karar | Bekleyen ölçüm | Nerede |
|---|---|---|
| Hacim ve ortak/ayrı hacim | Woofer `Fs` (kaba: ≈ 50 Hz, 2026-09-12), `Vas`, `Qts`; dört woofer arası eşleşme | `G0`, sürücü ölçüm planı |
| Pasif radyatör akordu `Fb` ve gerekirse ek kütle | Kutu içi empedans eğrisi (iki tepe) | Kabin bittiğinde, sürücü ölçüm planı |
| Subsonic köşe | `Fb` | DSP planı, profil |
| Dizilim | Yakın alan + dinleme ekseni ölçümü | Ölçüm ve DSP planı, 7. madde |
| Kapalı kabin termal | Dört amfi ve iki buck sıcaklığı soak boyunca | `G8`, test stratejisi |

Hiçbir satır burada ölçülmüş olarak kayıtlı değildir; sayıyı tezgâhtaki operatör yazar.

## İlgili belgeler

- [[../07-decisions/ADR-0021-single-cabinet|ADR-0021 — Tek kabin, sekiz sürücü, tek program]]
- [[../07-decisions/ADR-0002-biamp-signal-chain|ADR-0002 — Tek DAC, dört özdeş amfi]]
- [[../02-hardware/driver-measurements|Sürücü ölçüm planı]]
- [[measurement-and-dsp-plan|Ölçüm ve DSP planı]]
- [[../02-hardware/circuit-and-wiring-plan|Devre ve bağlantı planı]]
