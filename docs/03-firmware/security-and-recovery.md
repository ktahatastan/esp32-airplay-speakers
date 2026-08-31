---
status: draft
owner: firmware-engineer
reviewers: [qa-engineer]
updated: 2026-08-31
tags: [security, ota, provisioning]
---

# Güvenlik ve recovery

- Her cihaz benzersiz Security 2 parolası ve QR'ı taşır; ortak fabrika parolası yoktur. Üretim aracı: `firmware/tools/provision_credentials.py`.
- Cihaz parolayı **saklamaz**. Yalnız SRP6a salt ve verifier tutulur; flash'ı okumak kimlik bilgisini vermez.
- Kimlik bilgisi yoksa firmware provisioning'i açmayı reddeder ve daha zayıf bir güvenlik moduna düşmez.
- Wi-Fi parolası log, crash dump veya portal yanıtında gösterilmez. `firmware/tools/check_no_credential_logs.py` bunu kaynak üzerinde denetler ve CI'da çalışır: gizli bir değerin **uzunluğunu** loglamak serbest, **değerini** loglamak değil.
- Provisioning ilk açılışta veya fiziksel butonla zaman sınırlı açılır.
- Kullanıcı reseti fabrika kalibrasyonunu silemez.
- OTA düşük bataryada başlamaz; güç kaybında önceki çalışan imaja döner.
- OTA yalnız HTTPS ve imzalı image ile; idle audio, güvenli batarya/sıcaklık ve eşleşen donanım manifesti koşullarında başlar.
- GitHub tokenı veya firmware signing özel anahtarı uygulama imajına gömülmez.
- Yeni image ilk-boot sağlık kontrolü geçmeden valid işaretlenmez; ayrıntı [[ota-and-release-plan]].
- Kurtarma yolu USB/UART ve belgelenmiş boot prosedürü içerir.
- Kimlik/QR üretimi, yedekleme ve seri eşlemesi kayıt altına alınır.
