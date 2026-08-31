---
title: F5 buton ve LED, F4 provisioning politikası
status: partial
owner: firmware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [development-log, firmware, f4, f5, ui, provisioning]
---

# 2026-08-31 — Buton, LED ve provisioning politikası

## Amaç

Donanım henüz elde olmadığı için kapı gerektirmeyen aşamalarla devam etmek. `F5`'in tamamı ve `F4`'ün politika yarısı saf mantıktır; donanımsız tam doğrulanabilir.

Üç modül eklendi. Hepsi ESP-IDF'ten bağımsız saf C:

| Modül | İş |
|---|---|
| `hk_button` | Debounce, basılı tutma seviyeleri ve neyin ne zaman onaylandığı |
| `hk_led` | Tek LED'i hangi durumun kazandığı ve nasıl göründüğü |
| `hk_provision` | Kurulum radyolarının ne zaman açık, ne zaman kapalı olduğu |

Altlarındaki sürücü katmanı (GPIO, PWM, Wi-Fi, BLE) **yazılmadı**. `app_main` bu politikaların ne karar verdiğini yazdırıyor, hiçbirini uygulamıyor.

## Tasarımın özü: ne yapmadığı

Buton, kullanıcının Wi-Fi kimlik bilgilerini silebilen ve ayarlarını sıfırlayabilen bir kontrol. Bu yüzden kodun ilginç kısmı yaptıkları değil, **yapmayı reddettikleri**:

- Eylem **bırakışta** karara bağlanır, eşik geçilirken değil. 12 saniyeyi geçip tutmaya devam etmek hiçbir şey silmez; kullanıcı tutmayı sürdürebilir ve yalnız bırakmak onaylar.
- Kısa basış ile ağ sıfırlama arasındaki aralık **bilerek ölüdür**. Orada bırakmak hiçbir şey yapmaz, böylece tereddütlü bir basış kazara yıkıcı bir eyleme düşemez.
- Açılışta basılı tutulan buton kurtarma ister ama bırakıldığında sıfırlama üretmez.

Provisioning tarafında da simetri kasıtlı olarak kırık: ilk açılışta pencere zaman aşımına uğramaz (dönülecek bir ağ yok), ama yapılandırılmış bir cihazda butonla açılan pencere 10 dakikada kapanır (açık bir erişim noktası ve BLE yayını kalıcı bırakılmaz).

## Commit öncesi bağımsız inceleme

Modüller dört boyutta incelendi ve her bulgu ayrı bir çürütme turundan geçti: **15 bulgu, 12'si çürütüldü, 3'ü doğrulandı ve düzeltildi.**

| Ağırlık | Bulgu | Düzeltme |
|---|---|---|
| Major | `hk_prov_init` kurtarma dalı, kimlik bilgisi **olan** bir cihazda da sınırsız pencere açıyordu. Sonuç: BLE ve SoftAP oturum boyunca yayında, BLE yığını hiç serbest bırakılmıyor (ADR-0005'e aykırı) ve cihaz hiç bağlanmayı denemediği için **hiç ses çalmıyor**. Kendi başlık dosyamın "yapılandırılmış cihazda açılan pencere zaman aşımına uğrar" kuralıyla çelişiyordu. | Kimlik bilgisi varsa pencere sınırlandırıldı. Süre dolunca `CONNECTING`'e düşüyor; ağ gerçekten yoksa üç-hata geri çekilmesi zaten sınırsız açıyor. Yanlış davranışı sabitleyen test tersine çevrildi. |
| Major | 50 ms debounce'un hiçbir test koruması yoktu: kontrolü tamamen silmek test paketini yeşil bırakıyordu. Somut sonuç: 12 saniyelik bir tutuşun altıncı saniyesindeki 20 ms'lik kontak sıçraması, kullanıcının yapmadığı bir hareketle Wi-Fi'yi siliyordu. | Tutuş ortasında sıçrama enjekte eden test eklendi; kısa basışı ikiye bölmemesi de test edildi. |
| Minor | `hk_button_hold` içindeki `latched` koruması test edilmiyordu. Onsuz, açılışta basılı tutulan buton `FACTORY_ARMED` bildiriyor ve LED, bırakınca gerçekleşmeyecek bir fabrika sıfırlama uyarısı yakıyordu. | Tek assertion eklendi. |

Çürütülenler arasında dikkat çekenler: LED'in "yeşil 3 sn sonra sönük" davranışının eksik olduğu iddiası (zamanlama sürücü katmanına ait, politika değil), `consecutive_failures` taşması (okunduğu tek yol o değeri sıfırlıyor) ve kurtarma modunun süresiz olması gerektiği iddiası (spesifikasyon böyle bir muafiyet vermiyor).

## Doğrulama

Düzeltmelerin gerçekten koruma sağladığını mutasyon testiyle gösterdim. Altı mutasyonun altısı da öldürüldü — daha önce hayatta kalan üçü dahil:

| Mutasyon | Sonuç |
|---|---|
| Debounce kontrolü silindi | öldürüldü |
| `latched` koruması silindi | öldürüldü |
| Kurtarma penceresi yine sınırsız | öldürüldü |
| Major sürüm karşılaştırma işareti ters | öldürüldü |
| LED önceliği: OTA hatanın önüne | öldürüldü |
| Pencere zaman aşımı devre dışı | öldürüldü |

| Kontrol | Sonuç |
|---|---|
| `python3 scripts/check_docs.py` | **PASS** — 91 dosya, 0 hata |
| Host birim testleri | **PASS** — 10.178 kontrol, 0 hata, 0 derleme uyarısı |
| Partition kapısı ve kendi testleri | **PASS** |
| `idf.py -C firmware build` | **PASS** — proje kaynaklarında 0 uyarı, 233.680 baytlık imaj, slotun %97'si boş |
| **Donanımda çalıştırma** | **YAPILMADI** — kart yok |

## Kalan iş

- `F4`: Wi-Fi, mDNS, SoftAP portal ve BLE Unified Provisioning sürücü katmanı; cihaz başına benzersiz PoP üretimi; parolanın loglarda görünmediğinin otomatik taranması.
- `F5`: GPIO ve PWM sürücü katmanı; LED PWM'inin I2S zamanlamasına etkisinin ölçülmesi (G3).
- Kullanıcı resetinin `factory_cal`'a dokunmadığı testi `F6` deposunu bekliyor.
