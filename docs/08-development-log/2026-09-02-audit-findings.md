---
title: Denetim bulguları ve düzeltmeleri
status: partial
owner: orchestrator
reviewers: [firmware-engineer, verifier]
updated: 2026-09-02
tags: [development-log, verification, audit, firmware, ci]
---

# 2026-09-02 — Bağımsız denetimin bulduğu on beş şey

Bu oturumda yazılan her şeye karşı çok açılı, çürütmeli bir denetim çalıştırıldı: hata avı, test kalitesi, belge doğruluğu, CI bütünlüğü ve modüller arası tutarlılık. Otuz beş ajanın yirmi dokuzu tamamlandı; **`verify:claims` doğrulayıcılarının altısı da oturum limitine takıldı**, o yüzden belge bulguları doğrulanmamış geldi ve tek tek elle teyit edildi. On beş bulgu onaylandı, dokuzu çürütüldü.

## Kodda gerçek kusurlar

**Limiter sonsuz bir örnekte susuyordu.** Koruma yalnız `isnan` bakıyordu. `fabsf(INFINITY)` her tavanı aştığı için indirim çalışıyor ve `gain = tavan / INFINITY = 0` oluyordu; hold onu orada tutuyordu. Tek bozuk örnek çıkışı **hold süresi boyunca susturuyordu** — bu ayarlarda ~10 ms. Bir koruma katının vaadinin tam tersi. Üretip gördüm: `gain=0, held=478`. `isfinite`'a çevrildi.

**Biquad sonsuz Q kabul ediyordu.** `sinf(w0)/(2*INFINITY)` sıfır, yani `alpha` yok oluyor ve `a2` tam olarak 1 çıkıyordu — kutupları birim çemberin üstünde bir bölüm, ki aynı dosyadaki kararlılık denetimi onu reddediyor. Modülün kendi kararsız dediği katsayıları döndürmek, isteği reddetmekten kötü.

**Sıcaklıkta histerezis yoktu.** Gerilimde vardı, sıcaklıkta yoktu — hem de "kötüleşme anında, iyileşme hak edilir" diye yazan modülde. Sınırda duran bir NTC her okumada `OVERHEAT`/`NORMAL` arasında gidip geliyordu ve her geçiş susturma sıralayıcısını çalıştırıyordu. Ölçtüm: 10 okumada 10 geçiş. `recover_cell_c` eklendi; aynı salınım artık **tek** geçiş üretiyor. NTC susarsa da paket sıcakken `OVERHEAT` bırakılıyor: raporlamayı kesmesi soğuduğunun kanıtı değil.

**Provisioning durumu iki görevden kilitsiz değiştiriliyordu.** `on_button` `hk_ui` görevinde, tick `app_main`'de, iki çekirdek açık. `hk_prov_t` birlikte değişmesi gereken birkaç alan. Kilit `hk_main`'e kondu — `hk_provision` bilerek RTOS'suz, host'ta test edilebilmesi için.

## CI: bir blocker

`firmware-ci.yml`'de `cmake --build ... --target manifest_e2e` satırı `export.sh` kaynaklanmayan bir adımdaydı. IDF konteynerinde `cmake` ancak `export.sh` sonrası PATH'te — yani o adım **her çalışmada** "command not found" ile düşerdi. Üstelik gereksizdi: bir önceki adım zaten tüm hedefleri kuruyor. Silindi.

## Yayın hattı: iki major

**Checkout'lar etikete sabitlenmemişti.** `workflow_dispatch` varsayılan olarak ana dalı seçer, ama sürüm adı serbest metin `inputs.tag`'ten geliyordu. Yani hat, ana dalda ne varsa onu doğrulayıp derleyip **bir etiketin adıyla** yayımlardı. Üç checkout da etikete sabitlendi.

**Etiket girdisi doğrulanmıyordu.** Serbest metin doğrudan kabuk satırına genişletiliyordu. Artık ortam değişkeninden geçiyor ve `vMAJOR.MINOR.PATCH` kalıbına uymayan reddediliyor.

**Rollback koruması düz yazıya takılıyordu.** `grep -rq 'esp_ota_mark_app_valid_cancel_rollback'` `hk_health.h` içindeki iki açıklama satırıyla eşleşiyordu, yani koruma hiçbir çağrı yokken de geçerdi. Artık yalnız `.c` dosyalarında gerçek çağrı aranıyor.

## Testler: 330 mutasyonun 76'sı hayatta kalmıştı

Denetimin en değerli tespiti kontrol sayısının yanıltıcı olduğuydu: `test_biquad` ve `test_limiter` tüm kontrollerin **%94,1**'i, geri kalan on dört test dosyası **%0,8**. Ve 252 bin kontrolün hedefi olan `hk_biquad` ağaçtaki **en düşük dal kapsamına** sahip (%67,57).

Kapatılanlar:

- `hk_audio_outputs()` yalnız tek yönlü çıkarımlarla test ediliyordu; bir çıkışı yanlışlıkla **açan** mutasyon hepsini sağlayabiliyordu. Beş durumun üçlüsü tek tabloda sabitlendi.
- `test_sched.c`'deki bir iddia yanlış sebeple geçiyordu: seçilen `0xFFFFFFFF` damgası, modülün kendi sarma aritmetiğinde `due_ms`'ten **1001 ms önce**. Karar verilebilir damgalar eklendi.
- `hk_ota`'nın authority ayrıştırıcısında `?` ve `#` ayraçları test edilmiyordu.
- `hk_sched_limits_sane` `interval + jitter` taşmasını denetliyordu ama aynı toplamı kuran `backoff_max + jitter` yolunu denetlemiyordu.

Dördü de artık mutasyonla yakalanıyor.

## Belgede yanlış kalanlar

`hk_main.c` başlığı hâlâ "F0 ... ses çalmaz, ağa bağlanmaz, GPIO sürmez" diyordu ve ADR-0007'yi "hâlâ açık" sayıyordu; üçü de yanlış. `on_button` yorumu yıkıcı dalların "niyetini logladığını" söylüyordu — gerçekte siliyorlar. Kablolama planı `.kicad_sch`'in Git'te olduğunu yazıyordu; kaldırılmıştı. OTA planı "korumalı `release` ortamı" diyordu — ortam kapsamı gerçek ama **zorunlu inceleyici yok**. Ve iki belge yayın korumasını dosya konumuna dayalı anlatıyordu; oysa parmak izi listesine geçmişti.

## Doğrulama

393223 host kontrolü / 0 hata · ASan+UBSan temiz · fuzz tüm değişmezleri koruyor · her `firmware-ci` adımı yerelde geçti · iki iş akışı da ayrıştı · beş kusurun beşi de geri alındığında test düşüyor.

## Denetimin çürüttükleri

Dokuz bulgu çürütüldü. Biri kayda değer: `check_generated_kicad.py`'nin şema yokken sessizce geçtiği iddiası. Betik bir **bayatlık** denetleyicisi, varlık denetleyicisi değil; yokluk durumu açık bir dal ve mesaj basıyor.
