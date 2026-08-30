---
status: draft
owner: firmware-engineer
reviewers: [qa-engineer]
updated: 2026-08-30
tags: [security, ota, provisioning]
---

# Güvenlik ve recovery

- Her cihaz benzersiz Security 2 PoP/QR taşır; ortak fabrika parolası yoktur.
- Wi-Fi parolası log, crash dump veya portal yanıtında gösterilmez.
- Provisioning ilk açılışta veya fiziksel butonla zaman sınırlı açılır.
- Kullanıcı reseti fabrika kalibrasyonunu silemez.
- OTA düşük bataryada başlamaz; güç kaybında önceki çalışan imaja döner.
- OTA yalnız HTTPS ve imzalı image ile; idle audio, güvenli batarya/sıcaklık ve eşleşen donanım manifesti koşullarında başlar.
- GitHub tokenı veya firmware signing özel anahtarı uygulama imajına gömülmez.
- Yeni image ilk-boot sağlık kontrolü geçmeden valid işaretlenmez; ayrıntı [[ota-and-release-plan]].
- Kurtarma yolu USB/UART ve belgelenmiş boot prosedürü içerir.
- Kimlik/QR üretimi, yedekleme ve seri eşlemesi kayıt altına alınır.
