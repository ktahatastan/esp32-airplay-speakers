# Ek kök sertifikalar

Bu dizindeki her PEM, ESP-IDF'in varsayılan paketine **eklenir**
(`CONFIG_MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE`, Kconfig başlığı "Add custom
certificates to the default bundle"). Varsayılan paketi değiştirmez.

## isrg-root-yr.pem — ISRG Root YR

GitHub iki ayrı sertifika hiyerarşisi sunar:

| Sunucu | Zincir | ESP-IDF v5.5.1 paketinde kök var mı |
|---|---|---|
| `api.github.com` | leaf -> Sectigo E36 -> Sectigo Root E46 | evet |
| `release-assets.githubusercontent.com` | leaf -> `YR1` -> `ISRG Root YR` | **hayır** |

ESP-IDF v5.5.1 paketindeki 150 sertifika arasında yalnız `ISRG Root X1` ve
`ISRG Root X2` var; `ISRG Root YR` ve `YR1` yok. Varlık sunucusu bugün
doğrulanıyor, çünkü GitHub zincirin içinde `ISRG Root YR`'nin X1 tarafından
çapraz imzalanmış kopyasını da gönderiyor.

2026-08-31'de canlı zincirle ölçülen davranış:

```
capraz imza DAHIL  -> OK
capraz imza HARIC  -> verification failed (error 20, unable to get local issuer)
Root YR guvenilir  -> OK
```

Çapraz imza geçicidir. GitHub onu zincirden çıkardığı gün — ki bunu duyurmak
zorunda değil — sahadaki her hoparlör OTA'yı sessizce kaybeder ve geri dönüş
yolu USB'den yeniden flash'tır. Kökü şimdi eklemek 606 bayt tutar ve olayı
olaysızlaştırır.

Dosya, çapraz imzalanmış kopyadır. `gen_crt_bundle.py` yalnız konu adını ve
açık anahtarı sakladığı için güven çıpası olarak kendinden imzalı kopyayla
aynı işi görür.

Kaynak: `openssl s_client -showcerts -connect release-assets.githubusercontent.com:443`
Geçerlilik: `notAfter=Sep  2 23:59:59 2032 GMT`. O tarihten önce yenilenmeli.
