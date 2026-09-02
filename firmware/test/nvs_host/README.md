# `factory_cal` duvarının çalıştırılmış kanıtı

`PRD-008` kullanıcı resetinin kalibrasyonu silmemesini şart koşuyor. Bu proje şimdiye kadar bunu **yapısal** olarak savundu: ayrı partition, salt-okunur açılış, ve `tools/check_storage_isolation.py`'nin CI'da kaynağı tarayarak yıkıcı çağrıların kalibrasyon partition'ını adlandırmadığını doğrulaması.

Yapısal argüman iyidir ama çalıştırılmış bir test değildir. Bu proje o boşluğu kapatır: **gerçek NVS**, **gerçek bölüm tablosu** ve **gerçek `hk_storage.c`** ile, kart olmadan.

Bunu mümkün kılan ESP-IDF'in `linux` hedefi. `esp_partition`'ın linux uygulaması `partitions.csv`'den üretilmiş ikili bir bölüm tablosunu bir dosyaya eşliyor, `nvs_flash` da onun üstünde normal çalışıyor.

## Çalıştırma

```bash
idf.py -C firmware/test/nvs_host --preview set-target linux
idf.py -C firmware/test/nvs_host build
firmware/test/nvs_host/build/hk_nvs_host.elf
```

macOS'ta da çalışır. `COMPONENTS main` ile bileşen kümesi daraltılmıştır: aksi hâlde derlemeye `vfs` giriyor ve o da `sys/eventfd.h` istiyor, ki macOS'ta yok.

`-Wno-error` bilinçlidir. Projenin sıkı uyarı bayrakları ESP-IDF'in vendor'ladığı kaynağa da uygulanıyor ve clang, GCC'nin görmediği uyarılar üretiyor. Bu bir host testidir, sevk edilen bir imaj değil.
