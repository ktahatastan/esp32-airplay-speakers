> **Üretilmiş şema depoda yok — bilerek.**
>
> `generated/merzarkabul.kicad_sch` ve `generated/merzarkabul.kicad_pro` üreteç çıktısıdır ve bu makinede üretilemez: KiCad sembol kütüphaneleri kurulu olmadığı için `kicad-sch-api` sembol yerleştirmeyi reddediyor. Üreteç kaynaktır, `.kicad_sch` türevdir.
>
> Bayat bir şemayı depoda tutmak, hiç tutmamaktan kötüdür: bayat olduğu anlaşılamayan bir çizimden birisi lehim yapar.
>
> KiCad kurulu bir makinede üretmek iki komut:
>
> ```bash
> hardware/kicad/.venv/bin/python hardware/kicad/generate_merzarkabul.py --validate
> python3 scripts/check_generated_kicad.py --record
> ```
>
> `scripts/check_generated_kicad.py` pafta yokken bunu bir notla geçer; pafta eklendikten sonra şemanın üretecine uyup uymadığını CI'da denetler ve üreteç değişip çıktı değişmezse durur.

# Merzarkabul KiCad şeması

Bu klasör tek kabinin (bir ESP32-S3, bir PCM5102A, iki buck, dört XH-A232, sekiz sürücü) modül seviyesi **elektriksel kaynağını** tutar. Belgelerde kullanılan okunabilir tek sayfalık pafta ayrı bir çıktıdır: `hardware/diagrams/`.

## Üretme

KiCad 8+ ve Python 3.11+ kurulu bir ortamda:

```bash
python3 -m pip install -r hardware/kicad/requirements.txt
python3 hardware/kicad/generate_merzarkabul.py
```

Script her çalıştırmada bir **yapısal self-check** uygular ve sorun bulursa dosya yazmadan `1` ile çıkar:

- Her tel ucu gerçekten bir pinin veya bir köşe noktasının üstünde mi? (Pinin *yanında* duran bir tel bağlıymış gibi görünür ama değildir; bu betiğin en çok maruz kaldığı hata budur.)
- `TP0…TP33` boşluksuz ve tekrarsız mı?
- Tek bağlantılı net var mı? `EXPECTED_OPEN_NETS` boştur; tek pinli her net ya kablolanmamıştır ya da yazım hatasıdır.
- Referans tekrarı var mı?

Üretilen çıktılar:

- `generated/merzarkabul.kicad_pro`
- `generated/merzarkabul.kicad_sch`

Çıktı **deterministiktir**: değişmemiş bir scripti tekrar çalıştırmak birebir aynı dosyayı üretir. Kütüphane her çalıştırmada rastgele UUID ürettiği için üretim sonrası tüm UUID'ler tek bir eşleme tablosundan geçirilir; böylece hem dosya boşuna değişmez hem de sembol örneklerinin `(path "/<sayfa-uuid>")` referansları bozulmaz. Araya bir eleman eklemek sonraki UUID'leri kaydırır; bu durumda diff'i satır satır incelemek yerine "yeniden üretilmiş çıktı" olarak değerlendirin.

## Doğrulama

```bash
python3 hardware/kicad/generate_merzarkabul.py --validate
```

`--validate`, KiCad CLI ile ERC raporu ve PDF önizlemesi üretir; bunlar geçici çıktıdır ve `.gitignore` kapsamındadır. `kicad-cli` PATH üzerinde değilse script yaygın Windows ve macOS kurulum dizinlerini arar; özel konum `--kicad-cli <yol>` ile verilir.

Beklenen ERC sonucu: sıfır hata ve tek bağlantılı net yok. `EXPECTED_OPEN_NETS` boştur; bir netin tek bağlantılı kalması self-check tarafından hata sayılır.

> Bu satır ERC çalıştırılarak doğrulanmadı; KiCad bu makinede kurulu değil.

## Çizim kuralları

- Her yerleşim `2.54 mm` ızgaradadır. `kicad-sch-api` konumları `1.27 mm` ızgaraya snap ettiği için ızgara dışı bir yerleşim pini sessizce kaydırır.
- Tel uçları elle yazılmaz; `get_component_pin_position()` ile çözülür. Stok sembolün iç geometrisi varsayımdan farklı olsa bile tel pinin üstüne oturur.
- Bitişik parçalar arası bağlantı **gerçek telle** çizilir: `J1 → D2 → C_A`, buton düğümü, dört `C_SAFE → tweeter`. Dört amfi ayrı konnektördür (`U7`-`U10`); `DAC_LOUT` ve `DAC_ROUT` dağıtımı net etiketiyle bağlanır. Amfi kartında susturma girişi yoktur; sayfadaki tek susturma `DAC_XSMT` ve `R6`'dır.
- Sayfayı boydan boya geçmesi gereken raylar için net etiketi ve power sembolü kullanılır. Bu KiCad'in olağan pratiğidir; her ray için sayfa boyu tel çekmek okunabilirliği düşürür.
- Satın alınan kartlar konektör olarak çizilir. Pin **sırası mantıksal tasarım sözleşmesidir**, satıcı kartının fiziksel header sırası değildir.

## Güvenlik durumu

Şema `candidate` seviyesindedir. Nova sürücü empedansları, `C_SAFE`, jak polaritesi, `D2` ters polarite adayının düşümü ölçülmeden üretim tasarımı kabul edilmez.

`AMPn_L_MINUS` ve `AMPn_R_MINUS` (dört amfi) BTL anahtarlama çıkışıdır, GND değildir. Sürücülere G0-G2 geçilmeden enerji verilmez; ilk enerjilenen yol bir amfi, bir woofer ve bir tweeter'dır.
