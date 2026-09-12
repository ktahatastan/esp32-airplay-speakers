---
status: pending
owner: acoustics-engineer
updated: 2026-08-30
tags: [acoustics, dsp, moc]
---

# Akustik ve DSP

- [[cabinet-plan|Kabin planı]]
- [[measurement-and-dsp-plan|Ölçüm, crossover ve limiter]]
- [[../02-hardware/driver-measurements|Sürücü ölçümleri]]

Akustik ürün tek bir kabindir: Nova'nın kendi pasif radyatörleriyle akortlanmış refleks kabin, dört woofer + dört tweeter, mono program ([[../07-decisions/ADR-0021-single-cabinet|ADR-0021]]). Sürücü dizilimi ve pasif radyatör akordu `G0` (`Fs`, `Vas`) sonrasında kararlaşır.

Nova'nın orijinal DSP eğrisi bilinmediği için ayarlar dinleyerek tahmin edilmez; ölçülür, sürümlenir ve güvenli sınırlar fabrika kalibrasyonunda tutulur.
