---
status: accepted
decision: accepted
owner: firmware-engineer
reviewers: [orchestrator, verifier]
updated: 2026-09-08
extends: ADR-0017
tags: [adr, firmware, display, ui, provisioning, security]
---

# ADR-0019: Ekran kendi durumunu tutmaz, ve eşleşme kodunu kendisi üretir

## Bağlam

[[ADR-0017-round-display|ADR-0017]] paneli seçti ve pinleri bağladı; ne göstereceğini karara bağlamadı. Panel `2026-09-08` günü ilk kez doğru görüntü bastı, ve o an iki soru açıldı: ekran hangi kaynaktan sürülecek, ve BLE eşleşme karekodu ekrana nasıl gelecek.

İkincisi göründüğünden zor. [[ADR-0014-srp6a-username|ADR-0014]] şunu kayda geçirdi: Security 2'de cihaz parolayı saklamaz — elinde yalnız `salt` ve `verifier` vardır, ve verifier'dan parola geri üretilemez. Bu tek başına doğru olsaydı cihazın çizecek bir şeyi olmazdı.

## Karar

### 1. Ekranın kendi durumu yoktur

Gösterilecek ekran her karede `hk_screen_choose()` ile **girdilerden türetilir**. Bu saf bir fonksiyondur; kimsenin çağırmayı unutabileceği bir komut, kimsenin yanlış sıralayabileceği bir geçiş yoktur. LED zaten aynı girdilerden çözülüyor (`hk_led_resolve`), ekran da o çözümü kullanıyor — yani ikisi cihazın ne yaptığı konusunda anlaşmazlığa düşemez.

Bunun asıl kazancı sıfırlama yollarında: ağ sıfırlama ve fabrika sıfırlama ekranlarına, biri onları çağırdığı için değil, **durum öyle çözüldüğü için** varılır. Tablo eksiksizdir: `hk_led_state_t`'nin her değeri bir ekrana düşer, "hiçbirine uymayan" hâl yoktur. `firmware/test/test_screen.c` bunu 4096 girdi bileşimi × 5 tuş seviyesi üzerinde yürüyerek doğruluyor, ve ayrıca ulaşılamayan ekran kalmadığını — tasarlanıp öksüz bırakılmış ekran olmadığını — kontrol ediyor.

### 2. Eşleşme karekodunu cihaz kendisi üretir

[[ADR-0015-softap-captive-portal|ADR-0015]]'in bir sonucu bunu mümkün kılıyor ve bu sonucu açıkça yazmak gerekiyor: **WPA2 anahtarı ile SRP6a proof-of-possession değeri aynı sırdır.** Kurulum ağının ihtiyaç duyduğu `ap_pass` bloğu, eşleşme uygulamasının sorduğu `pop` değerinin ta kendisidir. Yani ADR-0014'ün "cihaz parolayı bilmez" ifadesi Security 2 tek başına ele alındığında doğrudur, bu projede ise değildir.

Karekod bu yüzden `hk_network.c` içinde, pencere açılırken üretilir; formatı ve alan sırası `firmware/tools/provision_credentials.py`'yi izler, çünkü etiketten bir anahtarı farklı olan karekod sahibin elinde başarısız olan karekoddur.

**Bu bilinçli bir açığa çıkarmadır.** Pencere açıkken paneli gören herkes eşleşebilir. Kabul edilme gerekçesi: basılı etiketin açığa çıkardığından fazlası değildir, yalnız pencere boyunca sürer, ve `WIFI_PROV_END` anında `hk_view_clear_setup()` sırları RAM'den siler. Alternatif — etiketi bulmadan eşleşemeyen sahip — bu ekranın ortadan kaldırmak için var olduğu sürtünmenin kendisidir.

### 3. Geçiş iris'tir, çapraz geçiş değil

İki aydınlık ekranın karışımı ikisinden de parlak bir ortadan geçer ve bu boyutta bir panelde flaş gibi okunur. Giden ekran içe göçer, gelen dışa açılır; ikisi hiçbir anda birlikte görünmez, dolayısıyla yanlış olabilecek bir orta yoktur.

### 4. Ölçülmemiş sayı ekrana çıkmaz

Her `have_*` bayrağı bir alanın **ölçülmüş olup olmadığını** taşır. Şarj ekranı yalnız ölçülen alanı çizer; `have_battery` yokken şarj ekranı seçilmez bile. Hiç ölçülmediği için %0 gösteren bir ekran yalan söyler, ve bu projede ölçülmemiş bir sayının ölçülmüş gibi görünmesi kabul edilmez.

## Sonuçlar

- `hk_view_t` ekranın bilmesine izin verilen her şeyi tek bir yapıda toplar; her alanın tek yazarı vardır.
- Yazı tipi Inter'dir (SIL OFL); lisans metni `firmware/components/hk_display/tools/Inter-OFL.txt` içindedir ve üreteç `gen_font.py` ile yeniden çalıştırılabilir.
- Karekod kodlayıcı depoya girdi. Doğruluğu OpenCV çözücüsüyle, gerçek eşleşme yükü dâhil dört yük üzerinde doğrulandı; ikinci bir kodlayıcıyla bit karşılaştırması **yanlış testtir**, çünkü maske seçimi standardın serbest bıraktığı bir aramadır ve iki doğru kodlayıcı farklı maske seçebilir.
- Kare bütçesi bağlayıcıdır: 240×240×16 bit = 115.200 bayt, 80 MHz'de 11,5 ms, ve bu kısaltılamaz. Ölçülen kare süresi `hk_lcd` günlüğünde raporlanır.
- Dört haneli PIN **hâlâ açık**: WPA2 parolası 8 karakterden kısa olamaz, dolayısıyla dört hane istenmesi WPA2 anahtarı ile `pop` değerinin ayrılmasını gerektirir. Bu ayrı bir karardır ve bu ADR'de verilmemiştir; ekran şimdilik uzun anahtarı tek çerçevede, kısa PIN'i haneli hücrelerde gösterecek şekilde ikisini de kaldırıyor.
