---
title: Ürün kartı geldi ve firmware ilk kez üstünde çalıştı
status: done
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-08
tags: [development-log, firmware, product-board, bring-up, flash]
---

# 2026-09-08 — Firmware ürün kartında

Kullanıcı ADR-0010'un kilitlediği kartı eline aldı ve iş tekti: firmware'i ona yükleyip gerçekten çalıştırmak. Kanıt ayrı duruyor: [[../06-testing/product-board-bring-up|ürün kartı bring-up kaydı]].

## Etikete değil çipe bakmak

İlk yapılan şey karta sormaktı. `esptool` cevabı: ESP32-S3 rev v0.2, **16 MB flash**, **8 MB gömülü PSRAM** (AP_3v3), MAC `68:ee:8f:4b:93:2c`. Bu, ADR-0010'un istediği `N16R8`. Ürün profilinin dört ayarı — flash boyutu, PSRAM modu, bölüm tablosu, OTA donanım sürümü — bununla eşleşiyor.

Bu sıranın önemi 5 Eylül'de öğrenilmişti: dört ayar yanlış eşleştiğinde arıza derleme hatası değil, **geç ve sessiz** bir çalışma hatasıdır.

## İlk port yanlış porttu, ve bu "kart bozuk" gibi görünüyor

Kart takıldığında bir seri port göründü ve `esptool` ona bağlanamadı: `No serial data received`. Hata mesajı sebebi söylemiyor.

Sebep USB kimliğinde yazıyordu: `0x303A:0x4001`. Bu çipin ROM'u değil, **karttaki uygulamanın** TinyUSB CDC portu. Üç yeniden başlatma yolu denendi (`default_reset`, `usb_reset`, 1200-baud dokunuşu) ve üçü de sökmedi, çünkü o uygulama host'tan yeniden başlatmayı uygulamıyor.

Kartın ikinci USB soketi bir WCH CH343 UART köprüsüne gidiyor ve esptool onu kendi sıfırlıyor. Düğmeye basmaya gerek kalmadı.

Bu bring-up kaydına girdi, çünkü belirti yanıltıcı: port görünür, cihaz cevapsızdır, ve bu "kart ölü" diye okunur. Bakılacak yer USB PID'idir.

## Kart boş değildi — yine

5 Eylül'ün dersi burada da geçerliydi. Kartta RGB LED döndüren bir üretici demosu vardı. Silmeden önce 16 MB'ın tamamı yedeklendi (sha256 kayıtlı, depo dışında).

Yedeğin çözümlenmesi bir soruyu kesin olarak kapattı: bizim tablomuzun `factory_cal` ofsetinde veri vardı, ama o **demo kodudur**. Bu kart hiç `G0`'dan geçmemiş, yani kaybedilecek ölçüm yok.

## Silme cerrahi yapıldı, ve sebebi ilkeden fazlası

Kaybedilecek kalibrasyon olmadığı **yedekten doğrulandığı hâlde** tam silme yapılmadı; yalnız veri bölgesi silindi (`0x9000`-`0x20000`).

Birinci sebep [[../03-firmware/usb-recovery|kurtarma notunun]] ilkesi: kurtarma cerrahi olmalı, çünkü tam silme `factory_cal`'ı da götürür ve oradaki ölçümleri hiçbir yazılım yeniden üretemez.

İkinci sebep daha somut: **raporun belirsiz olmaması.** Yabancı baytların üstüne yazılmış bir depo, ilk açılışta "boş mu, bozuk mu" ayrımını okunamaz hâle getirirdi.

Bu ayrım, bu oturumda çalıştırılan hazırlık workflow'unun 66 ajanı arasından çıktı: 60 iddia denetlendi, 18'i çürütüldü, ve çürütülenlerden biri tam da benim yapmak üzere olduğum tam silmeydi. Karşı iddia da düzeltildi — "hiç `erase_flash` yapma" mutlak bir kural değil; kural `factory_cal`'ı korumaktır, ve 5 Eylül'de o bölüm bilerek boş bırakıldığı için tam silme o gün doğruydu.

## Rapor

Dört bölge yazıldı, dördünde de `Hash of data verified`. Sonra iki kez açıldı ve iki rapor **birebir aynı** çıktı — boş heap baytına kadar.

Doğrulanan satırlar (tamamı kanıt kaydında):

- `octal_psram: vendor id 0x0d (AP)`, `density 64 Mbit` → PSRAM gerçekten **oktal**, ve bu geliştirme kartından devralınamayacak tek şeydi.
- `esp_psram: Found 8MB PSRAM device`, `Adding pool of 8192K` → 8.386.156 B boş PSRAM. Geliştirme kartında 2.094.848 B'ydi.
- `hk: slot ota_0 at 0x00020000, 7208960 bytes` → ürün slotu (`0x6e0000`), geliştirme kartının `0x2e0000`'ı değil.
- `hk: device id 932C` → MAC'in son iki baytıyla birebir; MAC'ten türeyen benzersiz son ek çalışıyor ve cihazı aynı ağdaki başka her AirPlay hedefinden ayırır.
- `storage user=use calibration=fail_safe`, `audio NOT permitted`, `output SILENT (i2s=0 dac=0 amp=0)`.

Son üç satır bu oturumun en önemli sonucudur ve **bir şeyin olmamasıdır**: kalibrasyon yokken cihaz ses çıkarmayı reddediyor, uydurma bir profil yazmıyor, ve çıkış hatları susturulu kalıyor. `G0` açıkken doğru davranış budur.

Aynı şekilde ağ da başlamadı: kimlik bilgisi olmadan provisioning açılmıyor ve zayıf moda düşülmüyor. Bu oturumda yazılan ADR-0015 satırı da ilk kez donanımda göründü: `no setup network key stored; the app-less setup path cannot open on this device`.

## Açılan ve açılmayan

`F0`'ın kalan işi ikiye ayrıldı ve **yarısı hâlâ açık**. Açılış raporu doğrulandı. GPIO tablosu `candidate` kaldı ve bilerek: açılan bir kart pin tablosunu kanıtlamaz — firmware o pinleri sürmedi, ve pinlerin ucunda ne olduğu kartın özelliğidir, imajın değil. ADR-0011 `accepted` için kartın kendi şemasını istiyor.

Hiçbir fiziksel kapı açılmadı. Kartta sürücü, amfi, DAC yok.

## Sonraki adım

1. Cihaz başına kimlik bilgilerini üret ve `factory_cal`'a yaz — ağ, provisioning ve bu oturumda yazılan portal ancak ondan sonra sınanabilir.
2. GPIO tablosunu kartın şemasıyla karşılaştır; `accepted` bunu bekliyor.

## Küçük not

Yüksek baud bu köprüde güvenilir değil: `read_flash` 921600 ve 460800'de `Invalid head of packet` ile düştü, 230400'de sorunsuz. Bir sonraki oturum bunu varsaymasın.
