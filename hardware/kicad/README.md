# Harman Kardom KiCad şeması

Bu klasör tek hoparlörlük modül-temelli prototip şemasını yeniden üretilebilir biçimde tutar. Dört hoparlörde aynı şema tekrarlanır.

## Kurulum ve üretim

KiCad 10 ve Python 3.11+ kurulu ortamda:

```powershell
python -m pip install -r hardware/kicad/requirements.txt
python hardware/kicad/generate_harman_kardom.py
```

Script `kicad-sch-api` ile doğrudan yerel `.kicad_sch` biçimini üretir. Üretilen ve Git'e alınan kaynaklar:

- `generated/harman-kardom.kicad_pro`
- `generated/harman-kardom.kicad_sch`

## Doğrulama

ERC raporu ve geçici PDF önizlemesi de üretmek için:

```powershell
python hardware/kicad/generate_harman_kardom.py --validate
```

`kicad-cli` PATH üzerinde değilse script yaygın Windows KiCad dizinlerini arar. Özel konum `--kicad-cli 'D:\KiCad\bin\kicad-cli.exe'` ile verilebilir. PDF, ERC raporu, KiCad kullanıcı durumu ve yedekleri `.gitignore` kapsamındadır.

Modül konektörleri mantıksal pin sırasını gösterir. Fiziksel modül pin sırası değildir; üretim PCB'sine geçmeden gerçek kartların baskı yazıları, veri sayfaları ve süreklilik ölçümleriyle özel sembollere dönüştürülmelidir. Beklenen ERC sonucu sıfır hata ve yalnız bilerek açık bırakılmış `TBD/NC` pin uyarılarıdır.

## Güvenlik durumu

Şema `candidate` seviyesindedir. Nova sürücü empedansları, `C_SAFE`, BMS balans parametreleri, KM103 / DC-132A güç anahtarının 16,8 V DC kesme kapasitesi, 12 V dahili LED pin dizilimi/akımı ve XH-A232 `SD/MUTE` erişimi ölçülmeden üretim tasarımı kabul edilmez. `R_SW_LED` bu ölçüme kadar `DNP` kalır. BTL çıkışlarının hiçbir eksi ucu GND değildir. V1'de şarj sırasında çalma yasaktır.
