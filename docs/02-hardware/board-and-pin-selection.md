---
status: pending
owner: hardware-engineer
updated: 2026-08-30
tags: [esp32, pins]
---

# ESP32-S3 kart ve pin seçimi

Kart gereksinimleri: Wi-Fi, BLE provisioning, yeterli PSRAM/flash, kararlı USB/UART recovery ve erişilebilir I2S/GPIO. N8R8/N16R8 sınıfı kartlar adaydır; kesin kart stack bellek ölçümü sonrası seçilir.

| İşlev | GPIO | Kural |
|---|---:|---|
| I2S BCLK/LRCLK/DATA | TBD | Strapping/USB ile çakışma yok |
| Buton | TBD | Active-low, boot pin değil |
| RGB R/G/B | TBD | PWM; ses gürültüsü ölçülür |
| I2C SDA/SCL | TBD | INA226/telemetri |
| Amp mute/enable | TBD | Pop önleyen sıra |
