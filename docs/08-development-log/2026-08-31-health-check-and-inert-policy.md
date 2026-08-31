---
title: İlk-boot sağlık kontrolü ve atıl kalmış iki politikanın işletilmesi
status: partial
owner: firmware-engineer
reviewers: [orchestrator]
updated: 2026-08-31
tags: [development-log, firmware, f4, f5, f7, ota]
---

# 2026-08-31 — Sağlık kontrolü, ve yazılmış ama çalışmayan iki politika

## Neden bu iş

"Kart geldiğinde yazılım hazır beklesin." Donanım gerektirmeyen işlerin envanterini çıkarırken iki şey ortaya çıktı: eksik olan bir modül, ve **yazılıp test edilmiş ama hiçbir şey yapmayan** iki politika.

## Eksik olan: `hk_health`

Yeni bir imaj ESP-IDF'in "pending verify" durumunda açılır. Uygulama `esp_ota_mark_app_valid_cancel_rollback()` çağırırsa kalıcı olur, çağırmazsa bootloader bir sonraki açılışta eski yuvaya döner. Üçüncü seçenek yok ve iki hata da pahalı, ama **zıt yönlerde**:

- Erken onaylamak bozuk imajı kalıcı yapar; çıkış yolu USB kablosu.
- Hiç onaylamamak sağlam imajı geri aldırır. Bu "güncellemeler bir türlü tutmuyor" gibi görünür ve teşhisi çok zordur, çünkü hiçbir yerde hata yoktur.

Bu yüzden modül boolean değil **üç** sonuç döndürür ve üçüncüsü — beklemeye devam — ilk yarım dakikanın normal cevabıdır. Erken karar vermek burada erdem değil.

Her şeyi şekillendiren kural: henüz rapor vermemiş bir alt sistem `UNKNOWN`'dır, başarısız değil. Sessizliği başarısızlık saymak, yalnızca yavaş açılan her imajı geri aldırırdı. Sessizlik ancak **süre dolduğunda** hükme dönüşür.

İki sertleştirme:

- `deadline <= settle` ise karar verilmez. Aksi hâlde her imaj onaylanmaya hak kazanmadan süreye takılır ve günlük, gerçek kusur yapılandırmadayken en yavaş alt sistemi suçlar.
- `storage` ve `network` **atlanamaz**. Her ölçütü `SKIP` yapmak, her imajı koşulsuz onaylayan bir sağlık kontrolü demek: kapatılmış ama hâlâ çalışıyor görünen bir geri alma mekanizması. `SKIP`, henüz var olmayan `hk_audio` ve `hk_power` içindir.

On mutasyonun dokuzu yakalandı (onuncusu geçersiz mutasyondu, derlenmedi).

## Atıl olan bir: provisioning politikası hiç işlemiyordu

F4'te yazılıp test ettiğim sınırlı pencere — yapılandırılmış bir cihazda butonla açılan setup oturumunun on dakika sonra kendini kapatması — **hiçbir zaman çalışamazdı**:

- `hk_prov_handle` üç çağrı yerinin üçünde de `now_ms = 0` alıyordu.
- `HK_PROV_EV_TICK` hiç gönderilmiyordu.
- `hk_prov_radios()` hiç okunmuyordu.

Yani zaman hiç geçmiyordu, pencere hiç dolmuyordu ve hangi radyoların açık kalacağını kimse sormuyordu. Testler geçiyordu; kod hiçbir şey yapmıyordu.

Üstelik kapatacak bir şey de yoktu: `hk_network` içinde `close_provisioning` diye bir fonksiyon yoktu. Politika "kapat" diyordu, aktüatör mevcut değildi.

Düzeltme: ana döngü artık saniyede bir tick veriyor, gerçek saat geçiriliyor, pencere kapandığında `hk_network_close_provisioning()` çağrılıyor. Bu, ESP-IDF'in kendi uyarısına uyarak `wifi_prov_mgr_stop_provisioning()`'i `deinit()`ten önce çağırıyor.

## Atıl olan iki: LED durumu birbirini eziyordu

`hk_ui_set_status()` tüm yapıyı değiştiriyordu (`s_status = *status;`) ve iki çağıranın ikisi de alanların yalnız bir kısmını doldurulmuş taze bir yapı gönderiyordu. Sonuç: bir OTA görevi `.ota = true` dediği an, sıradaki rutin Wi-Fi olayı onu **sessizce siliyordu**. "Flash yazılıyor, gücü kesme" göstergesi tam güncelleme sırasında sönerdi ve hiçbir yerde iz kalmazdı.

Bugün gizli bir kusur, çünkü henüz OTA görevi yok — ama tam da yazmak üzere olduğum koda kurulmuş bir tuzaktı.

Çözüm yorum değil, yapı: tüm-yapı ataması kaldırıldı ve her üretici yalnız kendi alanını yazıyor. `error` bayrağı için de kaynak maskesi kondu (`HK_UI_FAULT_NETWORK`, `_AUDIO`, `_POWER`, `_UPDATE`), yoksa aynı sorun bir seviye aşağıda tekrarlanırdı: bir ağ hatasının temizlenmesi, sesteki bir hatayı da sessizce temizlerdi.

## Belge tarafında kendi hatam

`AGENTS.md`, risk kaydındaki "burada tekrarlanmayan **iki** `Kritik` satır"dan söz ediyordu. Bugün imzalama anahtarı satırını eklediğimde sayı 7'den 8'e çıktı ve cümle geçersizleşti. Sayıyı güncellemek yerine cümleyi sayıdan kurtardım: kendi kaynağını yanlış tarif eden bir sözleşme, ona işaret eden bir sözleşmeden kötüdür.

Ayrıca bayat bir risk satırı gerçekleştirildi: provisioning kimlik üretim aracı artık **var**; kalan risk hiçbir cihaza yazılmamış olması.

## Doğrulama

| Ne | Sonuç |
|---|---|
| Ana makine testleri | 10373 kontrol, 0 hata |
| `hk_health` mutasyonu | 10 mutasyonun 9'u yakalandı, 1'i geçersiz |
| `check_docs` | 100 dosya, 0 hata |
| Firmware derlemesi | 0x111590 bayt, uyarısız |
| Anahtar konum denetimi | 0 sorun |

## Açık kalanlar

- `esp_ota_mark_app_valid_cancel_rollback()` çağrısı ve alt sistem raporlayıcıları. Bunlar olmadan `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` kapalı kalır; yayın hattı eşleşmeyi zaten denetliyor.
- `hk_audio` ve `hk_power` hâlâ yok; sağlık kontrolünün iki ölçütü bu yüzden `SKIP` ile geçilecek.
- ADR-0007 vendor'lanmış AirPlay yığınını şart koşuyor ama yığın **depoda değil**; F1 kanıtı bugün yeniden üretilemez durumda.
- `hk_pins.h` ses tarafında yalnız I2S'i tanımlıyor: amfi mute/standby, PCM5102A `XSMT`, şarj algılama ve DC sezme için GPIO **yok**. Pin tablosu hâlâ `candidate` ve hiçbir şey lehimlenmedi, yani şimdi eklemek bedava.
