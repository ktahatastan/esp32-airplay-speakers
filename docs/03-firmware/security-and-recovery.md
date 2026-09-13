---
status: draft
owner: firmware-engineer
reviewers: [qa-engineer]
updated: 2026-08-31
tags: [security, ota, provisioning]
---

# Güvenlik ve recovery

- Kurulumda sahiplik kanıtı (PIN) yoktur ([[../07-decisions/ADR-0023-pinless-provisioning|ADR-0023]]): BLE ve SoftAP'te protocomm Security 1, kanal X25519 + AES-CTR ile şifreli, kurulum ağı açık. Dinleyen ev parolasını alamaz; pencere açıkken menzildeki biri cihazı kendi ağına alabilir — ev için kabul edilen risk, pencere ilk açılış dışında yalnız butonla 10 dakika.
- Cihaz kurulum için hiçbir gizli değer **saklamaz**: `factory_cal`'da kurulumla ilgili satır yoktur; flash'ı okumak hiçbir kimlik bilgisi vermez. Eski kartlardaki `ADR-0014` kalıntıları okunmayan ölü veridir.
- Uygulamasız yol (captive portal) düz HTTP'dir ve sayfa bunu söyler; ev parolası orada yalnız kurulum penceresinde yazılır.
- Wi-Fi parolası log, crash dump veya portal yanıtında gösterilmez. `firmware/tools/check_no_credential_logs.py` bunu kaynak üzerinde denetler ve CI'da çalışır: gizli bir değerin **uzunluğunu** loglamak serbest, **değerini** loglamak değil.
- Provisioning ilk açılışta veya fiziksel butonla zaman sınırlı açılır.
- Kullanıcı reseti fabrika kalibrasyonunu silemez.
- OTA güç kaybında önceki çalışan imaja döner.
- OTA yalnız HTTPS ve imzalı image ile; idle audio ve eşleşen donanım manifesti koşullarında başlar.
- GitHub tokenı veya firmware signing özel anahtarı uygulama imajına gömülmez.
- Yeni image ilk-boot sağlık kontrolü geçmeden valid işaretlenmez; ayrıntı [[ota-and-release-plan]].
- Kurtarma yolu USB/UART ve belgelenmiş boot prosedürü içerir.
- Kimlik/QR üretimi, yedekleme ve seri eşlemesi kayıt altına alınır.
