---
title: F4 tamamlandı — cihaz başına kimlik ve log taraması
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, f4, security, provisioning]
---

# 2026-08-31 — Cihaz başına provisioning kimliği

## Amaç

`F4`'ün donanım gerektirmeyen son maddeleri: cihaz başına benzersiz kimlik üretimi, QR yükü ve parolanın loglarda görünmediğinin otomatik denetimi.

## Üretim aracı

`firmware/tools/provision_credentials.py` her cihaz için rastgele bir parola üretir ve **ESP-IDF'in kendi SRP6a uygulamasını** (`tools/esp_prov/security/srp6a.py`) kullanarak salt ve verifier hesaplar. İkinci bir uygulama yazmadım: bir hash farkı ancak provisioning anında, kutulanmış bir cihazda ortaya çıkardı.

Cihaz parolayı **saklamaz**. Security 2 SRP6a'dır: cihazda salt ve verifier durur, bunlardan parola geri elde edilemez. Bir hoparlörün flash'ını okumak kimlik bilgisini vermez.

Parola alfabesinden `O 0 I 1 L S 5 B 8` çıkarıldı. Bu etiketten okunup telefona yazılıyor ve karıştırılan her glif bir destek çağrısı demek. 30 sembolden 12 karakter ≈ 59 bit; hız sınırlı bir provisioning oturumu için fazlasıyla yeterli.

`label.txt` parolanın tek kopyası; sahibe özel izinle yazılıyor ve `.gitignore`'da.

Uçtan uca doğrulandı: üretilen CSV, `nvs_partition_gen.py` ile `partitions.csv`'deki `factory_cal` boyutuyla birebir 53.248 baytlık geçerli bir NVS imajı veriyor.

## İki gerçek kusur bulundu

### 1. Kimlik bilgileri yanlış partition'da duruyordu

`hk_network.c` `nvs_open("factory_cal", ...)` çağırıyordu. Bu, **varsayılan partition içinde** "factory_cal" adlı bir *namespace* açar — `factory_cal` partition'ını değil. Yani salt ve verifier kullanıcı ayarları partition'ında otururdu ve `init_user_store()`'un bozulma durumunda çalıştırdığı `nvs_flash_erase()` onları da silerdi. PRD-008 duvarının tam olarak engellemesi gereken şey.

Düzeltme: okuma artık `hk_storage_factory_get_blob()` üzerinden. Duvarı tek bir yer uyguluyor; ikinci bir açıcı, yanlış yapılacak ikinci bir yerdir.

### 2. Salt ve verifier her zaman aynı uzunlukta değil

Firmware tam 16 baytlık salt ve 384 baytlık verifier istiyordu. Araç iki cihaz üretti, biri 15 baytlık salt verdi.

Sebep: üreteç ikisini de büyük tam sayılardan türetip **minimum bayt sayısıyla** serileştiriyor, yani üst baytı sıfır olan bir değer bir bayt kısa çıkıyor. 600 üretim ölçtüm: salt bir kez 15 bayt, verifier dört kez 383 bayt — ikisi de aritmetiğin öngördüğü 1/256'ya yakın.

Bu, **256 cihazdan birinin hiç provisioning yapamaması** demekti. Dört cihazla muhtemelen hiç görülmez, görüldüğünde de rastgele bir donanım arızası gibi görünürdü.

Doldurmak çözüm değil: `calculate_x` salt'ı **ham bayt dizisi** olarak hashliyor, dolayısıyla cihazda sıfırla doldurmak el sıkışmayı bozardı. Firmware artık bir aralık kabul ediyor ve saklanan uzunluğu aynen kullanıyor.

## Log taraması

`firmware/tools/check_no_credential_logs.py` her `ESP_LOG` çağrısını inceliyor. Kural dar tutuldu, çünkü güvenilir olması gerekiyor: bir sırrın **uzunluğunu** loglamak serbest, **değerini** loglamak değil.

İlk sürüm 6 yanlış pozitif verdi — "credentials received" gibi *mesaj metinlerini* değer sanıyordu. Kural düzeltildi: önce string literaller çıkarılıyor, yalnız argümanlar taranıyor.

Kalan tek bulgu gerçekti ve kod tarafında düzeltildi: `esp_err_t credentials` adlı bir değişken aslında hata kodu tutuyordu. Denetimi gevşetmek yerine yanıltıcı ismi değiştirdim; bir log satırına ulaşan `credentials` adlı değişken tam olarak bu aracın araması gereken şekil.

Tarayıcının bir sınırı var ve belgelendi: bir literalin içindeki sırrı göremiyor. "password is hunter2" ile "waiting for password" arasındaki farkı bir düzenli ifade ayırt edemez ve tahmin etmek, kazandırdığından çok yanlış alarm maliyeti getirirdi.

## Doğrulama

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 94 dosya, 0 hata |
| Host birim testleri | **PASS** — 10.212 kontrol, 0 hata |
| `check_storage_isolation.py` | **PASS** |
| `check_no_credential_logs.py` | **PASS** — dört senaryo negatif test edildi |
| Partition kapısı ve kendi testleri | **PASS** |
| Sıfırdan `idf.py build` | **PASS** — 0 proje uyarısı |
| Üretim aracı uçtan uca | **PASS** — geçerli 53.248 baytlık NVS imajı |
| **Donanımda çalıştırma** | **YAPILMADI** |

## `F4`'ün kalanı

Yalnız donanım gerektirenler kaldı: iOS ve Android'de SoftAP ve BLE akışlarının test edilmesi, provisioning sonrası BLE heap'inin gerçekten geri kazanıldığının ölçülmesi.
