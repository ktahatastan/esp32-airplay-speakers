---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, qa-engineer]
updated: 2026-08-31
tags: [adr, charging, power, usb-c, battery]
---

# ADR-0009: USB-C PD tabanlı 16,8 V CC/CV şarj zinciri

## Bağlam

Proje başlangıcında şarj kaynağı "hazır 16,8 V CC/CV adaptör" olarak tarif edilmişti. 2026-08-30 tedarik araştırmasında zincir USB-C PD tabanlı bir çözüme çevrildi ve maliyet buna göre hesaplandı, KiCad paftası buna göre çizildi. Ancak kanonik güç ve devre belgeleri eski adaptör mimarisini anlatmaya devam etti; değişiklik için ADR açılmadı. Bu, `AGENTS.md` değişiklik disiplinine aykırıydı ve bir agent'ın çizilenden farklı bir devre kurmasına yol açabilirdi.

## Karar

V1 şarj yolu şu zincirdir:

```text
USB-C PD adaptör (65 W sınıfı)
        |
   Type-C kablo (5 A / e-marker doğrulanacak)
        |
   PD tetikleyici modülü -> sabit 20 V profili
        |
   XL4015 CC/CV katı -> 16,80 V / 2,00 A'e kalibre
        |
   4S balanslı BMS (koruma + balans)
        |
   4S1P hücre paketi
```

Bağlayıcı kurallar:

- USB-C soketi 4S pakete doğrudan bağlanmaz. PD tetikleyici yalnız gerilim profilini anlaşır, şarj profilini sağlamaz.
- Şarj profilini **XL4015 CC/CV katı** sağlar. BMS bir şarj cihazı değildir; yalnız koruma ve balans katmanıdır.
- PD tetikleyici 20 V DIP seçimi **batarya bağlı değilken** ölçülerek doğrulanır.
- XL4015 çıkışı **batarya bağlı değilken** 16,80 V'a ve akım sınırı 2,00 A'e kalibre edilir; ardından elektronik yükle doğrulanır.
- Zincir [[ADR-0004-v1-charge-policy|ADR-0004]] ile birlikte uygulanır: V1'de şarj sırasında amfi kapalıdır.
- Cihaz kapalıyken şarj gereksinimi (PRD-006) korunur; şarj katı ana güç anahtarının batarya tarafında kalır.

## Bilinen açık risk: şarj sonlandırma

XL4015 jenerik bir ayarlanabilir CC/CV buck'tır. Amaca özel Li-ion şarj entegresi değildir ve **şarj sonlandırma (termination) garantisi yoktur**: CV aşamasında akım düşse de çıkış 16,80 V'ta süresiz kalabilir. Hücreleri 4,2 V'ta sürekli tutmak ömür ve güvenlik açısından istenmez.

BMS'in aşırı şarj koruması bir sonlandırma algoritması değildir; koruma eşiği normal şart altında tetiklenmemelidir.

Bu nedenle:

- Sonlandırma davranışı **G4 kapısında zorunlu ölçümdür**: CV akımının zamanla düşüşü, çıkış geriliminin kararlılığı ve akım sonlandırma eşiği kaydedilir.
- Ölçüm sonlandırma olmadığını gösterirse şu seçenekler değerlendirilir ve yeni ADR ile karara bağlanır: (a) kullanıcı tarafından sonlandırılan gözetimli şarj + zaman aşımı, (b) ESP32 telemetrisi ile şarj katını kesen düşük tarafta yük anahtarı, (c) amaca özel 4S şarj entegresi.
- Sonlandırma davranışı belgelenmeden **gözetimsiz veya gece boyu şarj yapılmaz**.

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| Hazır 16,8 V CC/CV adaptör (WEKO/KA sınıfı) | Reddedilmedi, ikincil yedek olarak korunur. Cihaz başına ayrı adaptör gerektirir; USB-C tek kaynak esnekliği kaybolur. Sonlandırma riski daha düşük olduğu için G4 başarısız olursa ilk alternatiftir. |
| USB-C PD doğrudan pakete | Yasak. PD tetikleyici sabit gerilim verir, akım sınırlaması ve CV profili yoktur. |
| Rastgele "boost şarj" modülü | Yasak. Bkz. [[../power-and-battery-plan]]. |

## Sonuçlar

- [[../power-and-battery-plan|Güç ve batarya planı]], [[../02-hardware/circuit-and-wiring-plan|devre planı]] ve şema bu zinciri gösterir.
- G4 kabul matrisine şarj sonlandırma ölçümü eklenir.
- Risk kaydına "XL4015 şarj sonlandırma garantisi yok" satırı eklenir.
- Ters polarite koruması XL4015 modülünde yoktur; bağlantı sırası ve kutup etiketleme G4 öncesi zorunludur.
