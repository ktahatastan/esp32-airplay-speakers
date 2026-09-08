---
status: accepted
decision: accepted
owner: hardware-engineer
reviewers: [orchestrator, power-engineer, verifier]
updated: 2026-09-08
supersedes-part-of: ADR-0009
tags: [adr, power, telemetry, ina219, charge]
---

# ADR-0018: Akım sensörü INA219 olur, ve paket gerilimini ölçmez

## Bağlam

Depo baştan beri **INA226** yazıyordu (`bom.md`, devre planı, güç planı, KiCad üreticisi). Sahibi **INA219** aldı. İki parça aynı işi görmüyor ve farkları bu projenin tam da ölçmek istediği yerde önemli.

Ölçüm zaten zorunlu: [[ADR-0009-usb-c-pd-charge-chain|ADR-0009]] `G4`'te CV akımının zamanla düşüşünü görmeyi şart koşuyor, çünkü XL4015'in garantili bir şarj sonlandırması yok. Yani bu sensör bir konfor değil, bir kapının aleti.

## Karar

**INA219 kabul edilir**, şarj hattına yerleştirilir, ve **paket geriliminin kaynağı olmaz.**

### Nereye

```text
PAKET ─ BMS ─ TP5 (B+/P+) ─┬─ F_CHG 3A ─ [ŞÖNT 0,05 Ω] ─ TP26 CHG+ ← XL4015 OUT+
                           └─ F1 5A ─ TP7 ─ S1 ─ TP8 VBAT_SW ─ amfi + MP1584
```

Yüksek taraf, `F_CHG` ile `TP26 CHG+` arasında. 0,05 Ω %1 1 W: tam skala 6,4 A (sigortanın 3 A'sinin rahat üstünde), 2,00 A CC'de şönt üzerinde 100 mV ve 0,2 W, ve **200 µA çözünürlük** — CV kuyruğunu onlarca mA'e kadar okuyabilecek kadar ince. Adres `0x40` (A0, A1 → GND).

### Üç kısıt, ve neden burada yazıyorlar

**1. Ortak mod 26 V ile sınırlı** (INA226'da 40 V). Paketin 16,8 V'unda 9,2 V pay var, yeterli. Ama XL4015'in **20 V girişi tarafında** yalnız 6 V pay kalır, üstelik o buck'ın ters polarite koruması yok. Bu yüzden sensör ADR-0009 zincirinin **şarj girişi tarafına asla konmaz.**

**2. INA219'un VBUS pini yoktur.** Gerilimi şöntün *yük* tarafından, IN− ile GND arasından ölçer. Yüksek tarafta bu, paket gerilimi eksi kendi burden düşümü demektir.

> Bu maddenin tehlikesi diğerlerinden farklı. Yanlış okuma **makul** kalıyor — yüke bağlı olarak birkaç yüz mV aşağıda. `hk_power`'ın "imkânsız okuma sensör arızasıdır" tasarımı bunu yakalayamaz, çünkü sayı imkânsız değildir, yalnız yanlıştır. Ve o sayının beslediği şey düşük gerilim kapanma eşiğidir.
>
> Bu yüzden: **`pack_mv`'nin tek kaynağı `GPIO1`/`BATT_SENSE` bölücüsüdür.** INA219'un gerilim yazmacı akımı yorumlamak için okunur, paketi değil.

**3. `BRNG` biti (config 00h, bit 13)** 32 V / 16 V tam skala seçiyor. Dolu 4S paket 16,8 V, yani 16 V ayarının **üstünde**. POR değeri 0x399F ve `BRNG=1` ile doğru — ama yapılandırma açıkça yazılır, ve "16 V / düşük akım" hazır ayarı çözünürlük için asla seçilmez.

### Parçayı tanımak

Her iki parça da A0/A1 topraklıyken `0x40`'tan cevap veriyor ve aynı 16 adresli haritayı paylaşıyorlar: **I2C taraması ikisini ayırt edemez.** Ayırmak için `0xFE` işaretçisi okunur — INA226 üretici kimliği `0x5449` döner, INA219'un `0x05` üstünde yazmacı yoktur. Herhangi bir kalibrasyona güvenmeden önce bu yapılır.

## Sonuçlar

- BOM, devre planı, güç planı ve KiCad üreticisindeki `INA226` satırları `INA219` olur; `hk_pins.h`'deki `GPIO11/12` yorumları güncellendi.
- **Alert pini yok.** Alert, Mask/Enable ve Alert Limit yazmaçları INA226'ya özgü. Kesme tabanlı bir aşırı akım koruması bu parçayla kurulamaz; koruma sigortada ve BMS'te kalır.
- `hk_power`'ın arayüzü sensörden bağımsız olduğu için mantık değişmiyor; değişen, `pack_mv`'yi kimin beslediği ve bunun artık açıkça yazılı olması.
- İkinci bir şönt (yük hattı, 0,01 Ω, adres `0x41`) opsiyonel kalır. Varsayılan straplerde iki INA219 çakışır, o yüzden A0 → VS.

## Güvenlik önkoşulu

Yanlış ayarlanmış bir XL4015 çıkışını 20 V'luk girişine doğru itebilir ve o durumda parçanın 26 V payından yalnız ~6 V kalır. ADR-0009'un "önce ayarla, sonra bağla" sırası burada bağlayıcıdır:

> **XL4015'in çıkışı, sensör bağlı değilken, elektronik yükle 16,80 V'ta doğrulanmadan INA219 `TP26`'ya bağlanmaz.**

## Reddedilen seçenekler

| Seçenek | Ret gerekçesi |
|---|---|
| INA226'da ısrar etmek | Parça elde değil, ve INA219 doğru yere konduğunda `G4`'ün istediği ölçümü veriyor. |
| INA219'u yük hattına birincil koymak | `G4`'ün zorunlu ölçümü şarj akımıdır. Yük akımı faydalı ama kapıyı açan o değil. |
| Paket gerilimini INA219'dan almak | Yukarıdaki 2. madde. Yanlışlığı makul göründüğü için en tehlikeli seçenek. |
| Şarj girişi tarafına koymak | 20 V düğümünde 6 V ortak mod payı, korumasız bir buck'ın arkasında. |

## Doğrulama

Hiçbiri yapılmadı; sensör hiçbir devreye bağlanmadı.

- `0xFE` ile parça kimliğinin doğrulanması (INA219 beklenir, cevap gelmemeli).
- XL4015 çıkışının sensörsüz 16,80 V'ta doğrulanması — bağlamadan **önce**.
- 2,00 A CC'de akım okumasının bir DMM ile karşılaştırılması.
- `G4`: CV akımının zamanla düşüşü ve sonlandırma davranışı.
- Şöntün burden düşümünün şarj gerilimine etkisi.
