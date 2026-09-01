---
status: planned
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [firmware, recovery, usb, ota, safety]
---

# USB/UART kurtarma prosedürü

## Ne zaman gerekir

- OTA sonrası cihaz açılmıyor ve geri alma da kurtarmadı.
- İmzalama anahtarı değişti; sahadaki cihazlar yeni anahtarla imzalanmış imajı reddediyor (bkz. [[../credentials/signing-keys|imzalama anahtarları]]).
- Kalibrasyon yazılacak: yeni bir cihaz ilk kez hazırlanıyor.
- Geliştirme sırasında bilinen iyi bir sürüme dönmek gerekiyor.

[[ota-and-release-plan|OTA planı]] bu yolun **her sürümde** korunmasını şart koşuyor. Bu yüzden `GPIO19/20` (native USB) `hk_pins.h` içinde derleme zamanında yasaklı: onları başka bir işe vermek, elde kalan tek kurtarma yolunu harcamak olurdu.

## Tehlikeli komut

```bash
esptool erase_flash      # BUNU YAPMAYIN
```

Bu, `factory_cal`'ı da siler. O bölüm sürücü koruma ölçümlerini tutuyor: sürücülerle, bir amfiyle ve bir öğleden sonrayla elde edilmiş sayılar. Hiçbir yazılım onları yeniden üretemez ve kalibrasyonu gitmiş bir hoparlör **hiç çalmayı reddeder** — tasarım gereği, çünkü koruma profili uydurmak bu projenin yasakladığı şey.

Doğru kurtarma cerrahi olmalı: yalnız açılışı sağlayan dört bölge yazılır, veri bölümlerine dokunulmaz.

## Prosedür

### 1. Bilinen iyi bir imaj hazırlayın

```bash
idf.py -C firmware build
```

Yayımlanmış bir sürüme dönüyorsanız release asset'ini indirin; o zaten imzalıdır.

### 2. Cihazı indirme moduna alın

USB-C kabloyu takın. ESP32-S3 kendi USB-Serial-JTAG'ini kullanır, yani ek bir dönüştürücü gerekmez. Cihaz açılmıyorsa:

- `GPIO0`'ı GND'ye kısa devre yapın (buton varsa basılı tutun),
- `EN`/reset'e kısa basın,
- `GPIO0`'ı bırakın.

### 3. Yazın

```bash
python3 firmware/tools/recover.py --port /dev/cu.usbmodemXXXX
```

Betik önce neyi **yazmayacağını** listeler, sonra komutu gösterir. Ne yapacağını görmek için:

```bash
python3 firmware/tools/recover.py --dry-run
```

İmzalı bir sürüm asset'i yazacaksanız:

```bash
python3 firmware/tools/recover.py --port ... --build build-release --image harman-kardom-signed.bin
```

### 4. Doğrulayın

Açılış raporu ürünü, sürümü, donanım kimliğini, pin tablosunu ve iki deponun durumunu basar. Beklenen satırlar:

```
storage     user=... calibration=use
```

`calibration=fail_safe` görüyorsanız kalibrasyon gitmiş demektir ve cihaz ses vermeyecektir. Bu bir hata değil, koruma.

## Betiğin yaptığı ve yapmadığı

| Ofset | Ne yazılır |
|---|---|
| `0x00000` | bootloader |
| `0x08000` | bölüm tablosu |
| `0x0f000` | `otadata` — A/B seçiciyi 0. yuvaya döndürür |
| `0x20000` | uygulama (`ota_0`) |

Dokunulmayanlar: `nvs`, `nvs_keys`, `factory_cal`, `phy_init`, `storage`.

Ofsetler `partitions.csv`'den okunur, betiğe yazılmaz — yeniden bölümleme yapıldığında betiğin sessizce yanlış yere yazması böyle önlenir. Ayrıca yazılacak her bölgenin korunan bir bölüme taşmadığı denetlenir; taşarsa betik durur. Bu denetim hiç tetiklenmemeli, ama tetiklenip fark edilmemesinin bedeli geri getirilemeyen bir kalibrasyon.

## Henüz doğrulanmamış

Bu prosedür **hiç çalıştırılmadı**: donanım elde değil. `recover.py` mantığı host'ta test ediliyor (ofsetlerin tabloyla eşleşmesi, taşma denetiminin gerçekten durdurması, tam silme komutunun betikte hiç bulunmaması), ama gerçek bir cihaza yazma `G6` kabul matrisinin son satırıdır ve operatör kaydı ister.
