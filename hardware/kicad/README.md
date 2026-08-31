# Harman Kardom KiCad şeması

Bu klasör tek hoparlörün modül seviyesi **elektriksel kaynağını** tutar. Dört hoparlörde aynı şema tekrarlanır. Belgelerde kullanılan okunabilir tek sayfalık pafta ayrı bir çıktıdır: `hardware/diagrams/`.

## Üretme

KiCad 8+ ve Python 3.11+ kurulu bir ortamda:

```bash
python3 -m pip install -r hardware/kicad/requirements.txt
python3 hardware/kicad/generate_harman_kardom.py
```

Script her çalıştırmada bir **yapısal self-check** uygular ve sorun bulursa dosya yazmadan `1` ile çıkar:

- Her tel ucu gerçekten bir pinin veya bir köşe noktasının üstünde mi? (Pinin *yanında* duran bir tel bağlıymış gibi görünür ama değildir; bu betiğin en çok maruz kaldığı hata budur.)
- `TP0…TP27` boşluksuz ve tekrarsız mı?
- Referans tekrarı var mı?

Git'e alınan çıktılar:

- `generated/harman-kardom.kicad_pro`
- `generated/harman-kardom.kicad_sch`

Çıktı **deterministiktir**: değişmemiş bir scripti tekrar çalıştırmak birebir aynı dosyayı üretir. Kütüphane her çalıştırmada rastgele UUID ürettiği için üretim sonrası tüm UUID'ler tek bir eşleme tablosundan geçirilir; böylece hem dosya boşuna değişmez hem de sembol örneklerinin `(path "/<sayfa-uuid>")` referansları bozulmaz. Araya bir eleman eklemek sonraki UUID'leri kaydırır; bu durumda diff'i satır satır incelemek yerine "yeniden üretilmiş çıktı" olarak değerlendirin.

## Doğrulama

```bash
python3 hardware/kicad/generate_harman_kardom.py --validate
```

`--validate`, KiCad CLI ile ERC raporu ve PDF önizlemesi üretir; bunlar geçici çıktıdır ve `.gitignore` kapsamındadır. `kicad-cli` PATH üzerinde değilse script yaygın Windows ve macOS kurulum dizinlerini arar; özel konum `--kicad-cli <yol>` ile verilir.

Beklenen ERC sonucu: sıfır hata ve yalnız bilerek tek bağlantılı bırakılmış dört net için uyarı — `AMP_SD_TBD`, `AMP_MUTE_TBD`, `I2C_SDA` ve `I2C_SCL`. Bu liste scriptteki `EXPECTED_OPEN_NETS` kümesinden gelir; başka bir netin tek bağlantılı kalması self-check tarafından hata sayılır.

> Bu satır ERC çalıştırılarak doğrulanmadı; KiCad henüz kurulu değil. Tek bağlantılı net listesi üretilen `.kicad_sch` dosyasından sayılarak elde edildi.

## Çizim kuralları

- Her yerleşim `2.54 mm` ızgaradadır. `kicad-sch-api` konumları `1.27 mm` ızgaraya snap ettiği için ızgara dışı bir yerleşim pini sessizce kaydırır.
- Tel uçları elle yazılmaz; `get_component_pin_position()` ile çözülür. Stok sembolün iç geometrisi varsayımdan farklı olsa bile tel pinin üstüne oturur.
- Bitişik parçalar arası bağlantı **gerçek telle** çizilir: hücre serisi, `F1 → S1`, buton düğümü, `C_SAFE → tweeter`, şarj zinciri.
- Sayfayı boydan boya geçmesi gereken raylar için net etiketi ve power sembolü kullanılır. Bu KiCad'in olağan pratiğidir; her ray için sayfa boyu tel çekmek okunabilirliği düşürür.
- Satın alınan kartlar konektör olarak çizilir. Pin **sırası mantıksal tasarım sözleşmesidir**, satıcı kartının fiziksel header sırası değildir.

## Güvenlik durumu

Şema `candidate` seviyesindedir. Nova sürücü empedansları, `C_SAFE`, BMS balans parametreleri, KM103 / DC-132A anahtarının 16,8 V DC kesme kapasitesi, 12 V dahili LED akımı ve XH-A232 `SD/MUTE` erişimi ölçülmeden üretim tasarımı kabul edilmez. `D2` ve `R2` bu ölçüme kadar `DNP` kalır.

`AMP_L_MINUS` ve `AMP_R_MINUS` BTL anahtarlama çıkışıdır, GND değildir. V1'de şarj sırasında çalma yasaktır (ADR-0004). XL4015 şarj sonlandırması kanıtlanmamıştır (ADR-0009).
