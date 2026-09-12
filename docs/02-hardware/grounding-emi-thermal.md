---
status: draft
owner: hardware-engineer
updated: 2026-09-12
tags: [emi, thermal, grounding]
---

# Topraklama, EMI ve termal

- İki buck (A: ESP32-S3, B: PCM5102A) ve Wi-Fi anteni analog DAC/amfi girişinden uzak tutulur; buck B'nin dönüşü DAC `AGND`'ye tek noktadan gider.
- Analog/dijital/güç dönüş akımları planlı yıldız noktada birleşir; dört amfinin dönüşü `POWER_GND` yıldızına.
- BTL amfi hoparlör eksileri şaseye bağlanmaz.
- PWM LED, I2S ve Wi-Fi yayınında gürültü spektrumu ölçülür.
- 24 V adaptör beslemesinde uzun süreli yükte dört amfi ve iki buck indüktörünün sıcaklığı, sekiz kanal 4 Ω sınıfı yüke adaptörün 2,9 A noktasında — besleme bütçesi katının (`supply_budget_sq` / `supply_window_ms`) G1 S7'de alındığı nokta — sürülürken kaydedilir (G1 dummy-load, G8 kapalı kabin).
- Elektronik bölme akustik hacimden ayrılır; adaptör kabinin dışındadır. Havalandırma testle kararlaştırılır, rastgele menfez açılmaz.
