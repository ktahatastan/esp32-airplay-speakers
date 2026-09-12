#!/usr/bin/env python3
"""Generate the single Merzarkabul Airplay Speakers documentation schematic sheet.

One cabinet, module level: one ESP32-S3, one PCM5102A, four identical XH-A232
amplifiers, eight drivers, one programme.

This sheet is the readable overview: functional zones, real symbols, orthogonal
wires, junction dots, net flags for cross-zone nets, and the TP0-TP34 probe
index used during bring-up. The electrical source of truth for netlist and ERC
is the KiCad project under hardware/kicad/.

    python3 hardware/diagrams/generate_schematic_svg.py

Every coordinate below is derived from the block and symbol geometry in
schematic_lib, never eyeballed, so the drawing cannot silently grow the
overlapping labels that a hand-edited SVG accumulates.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from xml.sax.saxutils import escape

from schematic_lib import Sheet

W, H = 2720, 2860
REV = "P4"
DATE = "2026-09-12"


def panel_height(lines: int, tail: float = 14) -> float:
    """Box height that actually contains `lines` body lines.

    Panels sized by hand have ended up too short before, with their last lines
    rendered outside their own boxes. Nothing complained, because a panel is a
    rectangle and its text is separate; the next panel added below then painted
    over that text, because panels have an opaque fill and render in append
    order. Derived from the library's own pitch so it cannot drift.
    """
    return Sheet.PANEL_BODY_TOP + (lines - 1) * Sheet.PANEL_LINE + tail


HOP = 9


def vertical_with_hops(sheet: Sheet, x: float, y0: float, y1: float,
                       crossings: list[float], kind: str) -> None:
    """Vertical wire from y0 down to y1 that hops over the horizontals in `crossings`.

    A tap from the far bus has to pass the near bus to reach its pin. Drawing
    that as a plain crossing would leave the reader to guess whether the two
    wires touch, so the vertical is broken at each crossing and bridged with a
    half circle. Junction dots mean connection; hops mean none.
    """
    y = y0
    for yc in sorted(crossings):
        if not (y0 < yc < y1):
            continue
        sheet.wire([(x, y), (x, yc - HOP)], kind)
        sheet.wires.append(f'<path class="w {kind}" d="M {x},{yc - HOP} A {HOP} {HOP} 0 0 1 {x},{yc + HOP}"/>')
        y = yc + HOP
    sheet.wire([(x, y), (x, y1)], kind)


CSS = """
.page{fill:#ffffff}
.frame{fill:none;stroke:#0f172a;stroke-width:2}
.frame-inner{fill:none;stroke:#0f172a;stroke-width:1}
.ruler{font:600 13px 'Helvetica Neue',Arial,sans-serif;fill:#64748b;text-anchor:middle}
.sheet-title{font:700 34px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.sheet-sub{font:15px 'Helvetica Neue',Arial,sans-serif;fill:#475569}
.zone{fill:#fcfdff;stroke:#cbd5e1;stroke-width:1.4;stroke-dasharray:9 6}
.zone-tab{fill:#e2e8f0;stroke:none}
.zone-letter{font:700 15px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.zone-title{font:700 14px 'Helvetica Neue',Arial,sans-serif;fill:#334155}
.blk{fill:#ffffff;stroke:#0f172a;stroke-width:2}
.blk-div{stroke:#94a3b8;stroke-width:1}
.blk-ref{font:700 14px ui-monospace,Menlo,Consolas,monospace;fill:#b91c1c}
.blk-title{font:700 17px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a;text-anchor:middle}
.blk-sub{font:12px 'Helvetica Neue',Arial,sans-serif;fill:#64748b;text-anchor:middle}
.blk-note{font:11.5px 'Helvetica Neue',Arial,sans-serif;fill:#64748b;text-anchor:middle}
.pin{stroke:#0f172a;stroke-width:1.6}
.pin-name{font:12px ui-monospace,Menlo,Consolas,monospace;fill:#0f172a}
.w{fill:none;stroke-linecap:round;stroke-linejoin:round;stroke-width:2.2}
.sig{stroke:#0f172a}
.pwr{stroke:#b91c1c;stroke-width:3}
.v5{stroke:#a16207;stroke-width:2.8}
.v33{stroke:#ca8a04;stroke-width:2.4}
.dig{stroke:#1d4ed8;stroke-width:2.2}
.aud{stroke:#047857;stroke-width:2.6}
.btl{stroke:#7e22ce;stroke-width:3}
.gnd{stroke:#334155;stroke-width:2.4}
.rail{stroke:#b91c1c;stroke-width:2.4}
.rail-bar{stroke:#b91c1c;stroke-width:3}
.rail-name{font:700 12px ui-monospace,Menlo,Consolas,monospace;fill:#b91c1c}
.gnd-bar{stroke:#334155;stroke-width:2.4}
.gnd-name{font:11px ui-monospace,Menlo,Consolas,monospace;fill:#334155}
.junction{fill:#0f172a}
.net{font:11.5px ui-monospace,Menlo,Consolas,monospace;fill:#1e293b}
.sym{fill:#ffffff;stroke:#0f172a;stroke-width:2}
.dnp{stroke-dasharray:6 4;opacity:.55}
.sym-line{stroke:#0f172a;stroke-width:2;fill:none}
.sym-plate{stroke:#0f172a;stroke-width:2.6}
.sym-dot{fill:#ffffff;stroke:#0f172a;stroke-width:1.8}
.sym-ref{font:700 12.5px ui-monospace,Menlo,Consolas,monospace;fill:#b91c1c}
.sym-val{font:11.5px 'Helvetica Neue',Arial,sans-serif;fill:#1e293b}
.sym-pol{font:700 15px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.led-ray{stroke:#0f172a;stroke-width:1.6;fill:none}
.drv-ref{font:700 13px ui-monospace,Menlo,Consolas,monospace;fill:#b91c1c}
.drv-name{font:700 15px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.drv-detail{font:11.5px 'Helvetica Neue',Arial,sans-serif;fill:#b45309}
.flag{fill:#eef2ff;stroke:#3730a3;stroke-width:1.6}
.flag-name{font:700 11.5px ui-monospace,Menlo,Consolas,monospace;fill:#3730a3}
.tp{fill:#fff7ed;stroke:#c2410c;stroke-width:2}
.tp-text{font:700 12px 'Helvetica Neue',Arial,sans-serif;fill:#9a3412;text-anchor:middle}
.tp-leader{stroke:#c2410c;stroke-width:1.3;stroke-dasharray:4 3}
.panel{fill:#f8fafc;stroke:#94a3b8;stroke-width:1.4}
.panel.danger{fill:#fef2f2;stroke:#dc2626}
.panel.gate{fill:#fffbeb;stroke:#d97706}
.panel.title{fill:#ffffff;stroke:#0f172a;stroke-width:2}
.panel-title{font:700 15px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.danger-t{fill:#b91c1c}
.gate-t{fill:#92400e}
.panel-text{font:12.5px 'Helvetica Neue',Arial,sans-serif;fill:#1e293b}
.panel-mono{font:12px ui-monospace,Menlo,Consolas,monospace;fill:#1e293b}
.panel-warn{font:700 12.5px 'Helvetica Neue',Arial,sans-serif;fill:#b91c1c}
.tb-key{font:10.5px 'Helvetica Neue',Arial,sans-serif;fill:#64748b;letter-spacing:.06em}
.tb-val{font:700 14px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.tb-line{stroke:#cbd5e1;stroke-width:1}
.legend{font:12px 'Helvetica Neue',Arial,sans-serif;fill:#334155}
"""


def frame(sheet: Sheet) -> None:
    sheet.background.append(f'<rect class="frame" x="16" y="16" width="{W - 32}" height="{H - 32}"/>')
    sheet.background.append(f'<rect class="frame-inner" x="42" y="42" width="{W - 84}" height="{H - 84}"/>')
    columns = 8
    step = (W - 84) / columns
    for index in range(columns):
        cx = 42 + step * (index + 0.5)
        sheet.background.append(f'<text class="ruler" x="{cx:.0f}" y="34">{index + 1}</text>')
        sheet.background.append(f'<text class="ruler" x="{cx:.0f}" y="{H - 22}">{index + 1}</text>')
        if index:
            gx = 42 + step * index
            sheet.background.append(f'<line class="tb-line" x1="{gx:.0f}" y1="16" x2="{gx:.0f}" y2="42"/>')
            sheet.background.append(f'<line class="tb-line" x1="{gx:.0f}" y1="{H - 42}" x2="{gx:.0f}" y2="{H - 16}"/>')
    rows = "ABCD"
    rstep = (H - 84) / len(rows)
    for index, letter in enumerate(rows):
        cy = 42 + rstep * (index + 0.5)
        sheet.background.append(f'<text class="ruler" x="29" y="{cy + 5:.0f}">{letter}</text>')
        sheet.background.append(f'<text class="ruler" x="{W - 29}" y="{cy + 5:.0f}">{letter}</text>')
        if index:
            gy = 42 + rstep * index
            sheet.background.append(f'<line class="tb-line" x1="16" y1="{gy:.0f}" x2="42" y2="{gy:.0f}"/>')
            sheet.background.append(f'<line class="tb-line" x1="{W - 42}" y1="{gy:.0f}" x2="{W - 16}" y2="{gy:.0f}"/>')


#: Probe index TP0-TP34. One label per probe point, in TP order. This is the
#: single numbering shared with docs/02-hardware/circuit-and-wiring-plan.md §7.1
#: and hardware/kicad/generate_merzarkabul.py TEST_POINTS.
TP_LABELS = [
    "jak + / DC_IN", "VIN (D2 sonrası)", "jak − / POWER_GND", "buck A 5,10 V (ESP)", "buck B 5,10 V (DAC)",
    "ESP32 3V3", "I²S BCLK", "I²S LRCLK", "I²S DATA", "DAC LOUT", "DAC ROUT",
    "amfi L IN bus", "amfi R IN bus",
    "amfi 1 L+", "amfi 1 L−", "amfi 1 R+", "amfi 1 R−",
    "amfi 2 L+", "amfi 2 L−", "amfi 2 R+", "amfi 2 R−",
    "amfi 3 L+", "amfi 3 L−", "amfi 3 R+", "amfi 3 R−",
    "amfi 4 L+", "amfi 4 L−", "amfi 4 R+", "amfi 4 R−",
    "buton GPIO7", "LED_R sürüş", "LED_G sürüş", "LED_B sürüş", "DAC XSMT", "amfi SD bus ADAY",
]


def build() -> Sheet:
    sheet = Sheet(
        W, H,
        "Merzarkabul Airplay Speakers — tek kabin modül seviyesi devre şeması",
        "24 V / 2,9 A DC adaptör girişi, ters polarite adayı ve bulk kondansatör, iki 5 V buck "
        "(A: ESP32-S3, B: PCM5102A), ESP32-S3 N16R8, PCM5102A I2S DAC, LOUT/ROUT'un dört XH-A232 "
        "girişine dağıtımı, GPIO13/GPIO21 susturma hatları ve harici pull-down'ları, dört woofer ve "
        "dört C_SAFE korumalı tweeter, kullanıcı arayüzü ve TP0-TP34 test noktaları.",
    )
    frame(sheet)
    sheet.background.append(
        f'<text class="sheet-title" x="70" y="86">MERZARKABUL · TEK KABİN DEVRE ŞEMASI</text>'
        f'<text class="sheet-sub" x="70" y="108">Modül seviyesi prototip · bir ESP32-S3, bir PCM5102A, dört XH-A232, '
        f'sekiz sürücü, tek program · &#171;ADAY / TBD&#187; değerler ölçümle kilitlenir · üretim PCB şeması değildir</text>')

    # ================================================================ ZONE A
    sheet.zone(70, 130, 2580, 400, "A", "24 V DC GİRİŞ, TERS POLARİTE ADAYI VE BULK KONDANSATÖR — ADR-0020")
    j1 = sheet.block("J1", "DC GİRİŞ JAKI", "5,5 × 2,1 mm · merkez pozitif · 24 V / 2,9 A adaptör", 100, 200, 300,
                     right=["+24 V", "GND"])
    jack_plus, jack_gnd = j1.pin("+24 V"), j1.pin("GND")
    # TP0 is the raw jack pin, TP1 the rail behind D2: their difference is the
    # diode's forward drop under load, which is the G1 number that decides
    # whether D2 stays or becomes a 0 R link. Both probe points sit on the wire.
    sheet.wire([jack_plus, (540, jack_plus[1])], "pwr")
    sheet.testpoint(470, jack_plus[1], 0)
    # D2 starts far enough right that its value text clears the ground drop
    # and TP2 on the row below.
    d2_a, d2_k = sheet.diode_h(540, jack_plus[1], "D2", "Schottky / ideal-diyot ADAY")
    sheet.wire([d2_k, (800, jack_plus[1])], "pwr")
    sheet.testpoint(660, jack_plus[1], 1)
    sheet.power_port(720, jack_plus[1], "VIN")
    sheet.junction(720, jack_plus[1])
    ca_top, ca_bot = sheet.cap_v(800, jack_plus[1], "C_A", "1000 µF / 35 V", polarized=True)
    sheet.junction(800, jack_plus[1])
    sheet.gnd(ca_bot[0], ca_bot[1])
    # Ground leaves the jack on its own row and drops to the POWER_GND symbol
    # left of D2, so nothing crosses the +24 V run.
    sheet.wire([jack_gnd, (460, jack_gnd[1]), (460, 380)], "gnd")
    sheet.testpoint(441, jack_gnd[1], 2)
    sheet.gnd(460, 380, "POWER_GND")
    sheet.netlabel(100, 470, "D2 takılmazsa yerine 0 Ω köprü gelir; DC_IN ile VIN o zaman tek nettir. C_A tektir ve jak girişindedir.", "start", 0)
    sheet.netlabel(100, 494, "V1'de güç anahtarı yok: cihaz adaptör çekilerek kapanır, boşta bekleme firmware'in idle standby'ıdır.", "start", 0)

    sheet.panel(1000, 180, 1620, panel_height(9), "GÜÇ GİRİŞİ KURALLARI — ADR-0020")
    sheet.panel_body(1000, 180, [
        ("ADAPTÖRÜN YÜKSÜZ ÇIKIŞI BAĞLANMADAN ÖNCE DMM İLE ÖLÇÜLÜR: < 25,5 V değilse bağlanmaz. 24 V, TPA3110'un 26 V tavanının 2 V altındadır (G1 satırı).", "panel-warn"),
        ("JAK POLARİTESİ İLK ENERJİLENDİRMEDEN ÖNCE ÖLÇÜ ALETİYLE DOĞRULANIR: merkez pozitif. Etiket okumak ölçüm değildir (G1 satırı).", "panel-warn"),
        "24 V iki giriş penceresinin de içindedir: TPA3110 8–26 V, MP1584 4,5–28 V. VIN dört amfiyi doğrudan besler; arada regülatör yoktur.",
        "Adaptör 24 V / 2,9 A (≈70 W) ile verilidir; limiter tavanı bu bütçeden türetilir. Sekiz BTL kanal 70 W'ın çok üstünü çekebilir ve çöken VIN ESP32'yi şarkı ortasında sıfırlar.",
        "G1, dört amfi limiter tavanında sürülürken toplam akımı ve VIN çöküşünü ölçer. İlk enerjilendirme adaptörle değil, akım sınırlı laboratuvar kaynağıyla yapılır.",
        "D2 adaydır: Schottky, ideal-diyot modülü ya da 0 Ω köprü. 2,9 A'da bir Schottky ≈1 W veya üstü ısınır; kararı G1'de TP0−TP1 düşümü ve ısı verir.",
        "Adaptör çıkışı koruma toprağına bağlı olabilir. Adaptörle osiloskop bağlamadan önce PE/izolasyon ilişkisi ölçülür; belirsizse scope bağlanmaz.",
        "Kapanış adaptör çekilerek olur; kapanış pop kaydı da öyle alınır. VIN çökerken amfiler 8 V altında kendi kilidiyle susar, buck'lar 4,5 V girişe kadar 5 V verir.",
        ("TPA3110D2'nin 24 V'ta 4 Ω sınıfı sürücülere vereceği güç veri sayfasından değil G1 dummy-load ölçümünden yazılır. Jak kontağının 2,9 A sürekli değeri tedarikçiye sorulur.", "panel-warn"),
    ])

    # ================================================================ ZONE B
    sheet.zone(70, 630, 590, 800, "B", "5 V BESLEMELERİ — İKİ BUCK (ADR-0020)")
    # Two bucks, not one: a shared buck put audible hiss into the DAC on the
    # bench (owner's observation, 2026-09-12, recorded in ADR-0020). Buck A
    # carries the USB backfeed jumper because only the ESP devkit has USB;
    # buck B feeds the DAC and has no jumper.
    u3 = sheet.block("U3", "MP1584 BUCK A", "ESP32-S3 · yüksüz 5,10 V'a ayarla", 210, 700, 230,
                     left=["IN+", "IN−"], right=["OUT+", "OUT−"],
                     note="JP1 servis ayırma: USB ile programlarken açık")
    sheet.net_flag(u3.pin("IN+")[0] - 30, u3.pin("IN+")[1], "VIN", "L")
    sheet.wire([(u3.pin("IN+")[0] - 30, u3.pin("IN+")[1]), u3.pin("IN+")], "pwr")
    sheet.gnd(u3.pin("IN−")[0] - 40, u3.pin("IN−")[1], "POWER_GND")
    sheet.wire([(u3.pin("IN−")[0] - 40, u3.pin("IN−")[1]), u3.pin("IN−")], "gnd")
    out = u3.pin("OUT+")
    jp_a, jp_b = sheet.switch(490, out[1], "", "JP1 · SERVİS AYIRMA")
    sheet.wire([out, jp_a], "v5")
    sheet.testpoint(474, out[1] + 52, 3, anchor=(474, out[1]))
    sheet.wire([jp_b, (600, out[1])], "v5")
    sheet.power_port(600, out[1], "+5V_LOGIC")
    sheet.gnd(u3.pin("OUT−")[0] + 40, u3.pin("OUT−")[1], "STAR_GND")
    sheet.wire([u3.pin("OUT−"), (u3.pin("OUT−")[0] + 40, u3.pin("OUT−")[1])], "gnd")

    u4 = sheet.block("U4", "MP1584 BUCK B", "PCM5102A · yüksüz 5,10 V'a ayarla", 210, 900, 230,
                     left=["IN+", "IN−"], right=["OUT+", "OUT−"],
                     note="JP yok: USB geri beslemesi yalnız ESP geliştirme kartında")
    sheet.net_flag(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1], "VIN", "L")
    sheet.wire([(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1]), u4.pin("IN+")], "pwr")
    sheet.gnd(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1], "POWER_GND")
    sheet.wire([(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1]), u4.pin("IN−")], "gnd")
    out_b = u4.pin("OUT+")
    sheet.wire([out_b, (600, out_b[1])], "v5")
    sheet.testpoint(510, out_b[1], 4)
    sheet.power_port(600, out_b[1], "+5V_DAC")
    sheet.gnd(u4.pin("OUT−")[0] + 40, u4.pin("OUT−")[1], "STAR_GND")
    sheet.wire([u4.pin("OUT−"), (u4.pin("OUT−")[0] + 40, u4.pin("OUT−")[1])], "gnd")

    sheet.panel(100, 1080, 470, panel_height(12), "GÜÇ SIRALAMASI")
    sheet.panel_body(100, 1080, [
        "1. Her iki buck'ın çıkışını yük",
        "    BAĞLI DEĞİLKEN 5,10 V'a ayarla.",
        "2. Elektronik yükle droop ve ripple",
        "    ölçümünü iki buck'ta da tekrarla.",
        "3. USB ile programlarken JP1 açılır;",
        "    USB ve harici 5 V birlikte kullanılmaz.",
        "4. Ayrı buck'lar: paylaşılan tek buck",
        "    DAC'a duyulur hışırtı verdi (ADR-0020).",
        "",
        ("TP3/TP4 hedefi: normal yükte ≤50 mVpp;", "panel-mono"),
        ("Wi-Fi sıçramasında 4,75 V altına inmez.", "panel-mono"),
        ("TP5 hedefi: brownout/reset üreten çökme yok.", "panel-mono"),
    ])

    # ================================================================ ZONE C
    sheet.zone(680, 630, 510, 800, "C", "ESP32-S3 N16R8 — ADR-0010")
    u5 = sheet.block("U5", "ESP32-S3 DEVKIT", "16 MB flash + 8 MB PSRAM", 830, 700, 330,
                     # AMP_MUTE leaves on the left as a net flag: the mute bus
                     # lives under the amplifier bank in zone E, and a flag is
                     # how a net crosses zones on this sheet.
                     left=["5V / VBUS", "GND", "3V3", "GPIO7  BUTTON", "GPIO8  LED_R",
                           "GPIO9  LED_G", "GPIO10 LED_B", "GPIO21 AMP_MUTE"],
                     # DAC_XSMT lands on the same row as U6's XSMT pin, so the
                     # DAC mute is one straight wire.
                     right=["GPIO4  BCLK", "GPIO5  LRCLK", "GPIO6  DATA", "GPIO13 DAC_XSMT"],
                     note="GPIO ataması ADAY · kart şeması ve boot testi olmadan accepted değil")
    v5 = u5.pin("5V / VBUS")
    sheet.net_flag(v5[0] - 30, v5[1], "+5V_LOGIC", "L")
    sheet.wire([(v5[0] - 30, v5[1]), v5], "v5")
    gnd5 = u5.pin("GND")
    sheet.gnd(gnd5[0] - 148, gnd5[1], "STAR_GND")
    sheet.wire([(gnd5[0] - 148, gnd5[1]), gnd5], "gnd")
    v33 = u5.pin("3V3")
    sheet.wire([v33, (v33[0] - 68, v33[1])], "v33")
    sheet.testpoint(v33[0] - 22, v33[1], 5)
    sheet.power_port(v33[0] - 68, v33[1], "+3V3")
    for pin_name, flag in (("GPIO7  BUTTON", "BUTTON_N"), ("GPIO8  LED_R", "LED_R"),
                           ("GPIO9  LED_G", "LED_G"), ("GPIO10 LED_B", "LED_B"),
                           ("GPIO21 AMP_MUTE", "AMP_MUTE")):
        point = u5.pin(pin_name)
        sheet.wire([point, (point[0] - 30, point[1])], "dig")
        sheet.net_flag(point[0] - 30, point[1], flag, "L")

    # ================================================================ ZONE D
    sheet.zone(1210, 630, 620, 800, "D", "PCM5102A I²S DAC")
    # XSMT is the fourth row so that it shares a row with U5's GPIO13 pin. The
    # block starts at x=1400 and is 330 wide so that its ground symbols clear
    # R6, which hangs from the XSMT wire at x=1234.
    u6 = sheet.block("U6", "PCM5102A MODÜLÜ", "3-wire I²S · modül köprüleri doğrulanacak",
                     1400, 700, 330,
                     left=["BCK", "LCK / LRCK", "DIN", "XSMT", "SCK", "VIN 5 V", "GND / AGND"],
                     right=["LOUT", "ROUT", "AGND"])
    # I2S: the DAC input pins sit on the same rows as the ESP32 outputs, so each
    # clock is a single straight wire with nothing to cross.
    for source, target, tp, tp_x in (("GPIO4  BCLK", "BCK", 6, 1230),
                                     ("GPIO5  LRCLK", "LCK / LRCK", 7, 1270),
                                     ("GPIO6  DATA", "DIN", 8, 1310)):
        a, b = u5.pin(source), u6.pin(target)
        sheet.wire([a, b], "dig")
        sheet.testpoint(tp_x, a[1], tp)
    # Short stub: R6's value text ends just left of this ground symbol.
    sck = u6.pin("SCK")
    sheet.wire([sck, (sck[0] - 26, sck[1])], "gnd")
    sheet.gnd(sck[0] - 26, sck[1])
    # Routed down to a rail symbol under the block: a flag on this row would
    # sit between R6's ground and the SCK ground with no room to spare. The DAC
    # has its own buck (B), hence its own rail name.
    vin6 = u6.pin("VIN 5 V")
    sheet.wire([vin6, (1300, vin6[1]), (1300, 1040)], "v5")
    sheet.power_port(1300, 1040, "+5V_DAC", "down")
    # Short stub: the mute net and its pull-down occupy the lane to the left of
    # this ground, so a long one would put the STAR_GND caption on the wire.
    gnd6 = u6.pin("GND / AGND")
    sheet.wire([gnd6, (gnd6[0] - 18, gnd6[1])], "gnd")
    sheet.gnd(gnd6[0] - 18, gnd6[1], "STAR_GND")
    agnd = u6.pin("AGND")
    sheet.wire([agnd, (agnd[0], 990)], "gnd")
    sheet.gnd(agnd[0], 990, "AGND")

    # DAC mute. ADR-0011: the pull-down is the mute, the GPIO only releases it.
    # Drawn as a real component with a designator because a note cannot be
    # ordered, stuffed or checked, and this net is the last thing between an
    # unmeasured driver and whatever the amplifier inputs happen to be holding.
    xsmt_esp, xsmt_dac = u5.pin("GPIO13 DAC_XSMT"), u6.pin("XSMT")
    sheet.wire([xsmt_esp, xsmt_dac], "dig")
    sheet.testpoint(1300, xsmt_dac[1], 33)
    r6_t, r6_b = sheet.resistor_v(1234, xsmt_dac[1], "R6", "10 kΩ")
    sheet.junction(1234, xsmt_dac[1])
    sheet.gnd(r6_b[0], r6_b[1], "STAR_GND")

    sheet.netlabel(1230, 1200, "FMT=LOW · FLT=LOW · DEMP=LOW · SCK→GND (yalnız 3-wire BCK-PLL modu)", "start", 0)
    sheet.netlabel(1230, 1224, "XSMT modül varsayılanına BIRAKILMAZ: GPIO13 sürer, R6 kapalı tutar.", "start", 0)
    sheet.netlabel(1230, 1248, "LEHİMDEN ÖNCE ÖLÇ: XSMT pad'i ↔ 3V3 direnci. Sert köprü / 0 Ω varsa", "start", 0)
    sheet.netlabel(1230, 1272, "kesilmeden GPIO13 bağlanmaz; pin LOW sürerken 3V3 rayına kısa devredir.", "start", 0)
    sheet.netlabel(1230, 1296, "Modül köprüleri satıcıya göre değişir; pad ismine bakıp lehim yapılmaz.", "start", 0)

    # The DAC outputs leave to the right and drop into zone E, where each becomes
    # a bus feeding all four amplifier inputs. The lower pin gets the inner
    # vertical so that neither horizontal run crosses the other's drop; the
    # AGND stub below them ends in its own symbol and never reaches the drops.
    bus_r_y, bus_l_y = 1530, 1560
    lout, rout = u6.pin("LOUT"), u6.pin("ROUT")
    lout_x, rout_x = 1810, 1786
    sheet.wire([lout, (lout_x, lout[1])], "aud")
    vertical_with_hops(sheet, lout_x, lout[1], bus_l_y, [bus_r_y], "aud")
    sheet.testpoint(lout_x, 1000, 9)
    sheet.wire([rout, (rout_x, rout[1]), (rout_x, bus_r_y)], "aud")
    sheet.testpoint(rout_x, 1060, 10)

    # ================================================================ ZONE F
    # User interface, on the right of the DAC: every net here is a flag, so the
    # zone can sit wherever the sheet has room.
    fx, fy = 1850, 630
    sheet.zone(fx, fy, 800, 380, "F", "KULLANICI ARAYÜZÜ")
    rpu_x = fx + 90
    sheet.power_port(rpu_x, fy + 116, "+3V3")
    rpu_t, rpu_b = sheet.resistor_v(rpu_x, fy + 116, "R_PU", "10 kΩ ADAY")
    btn_node = rpu_b
    sheet.wire([btn_node, (rpu_x, fy + 210)], "sig")
    sw_t, sw_b = sheet.pushbutton_v(rpu_x, fy + 210, "SW1", "FONKSİYON / RESET")
    sheet.gnd(sw_b[0], sw_b[1], "STAR_GND")
    sheet.wire([btn_node, (rpu_x + 240, btn_node[1])], "dig")
    sheet.junction(rpu_x, btn_node[1])
    sheet.testpoint(rpu_x + 120, btn_node[1], 29)
    cdb_t, cdb_b = sheet.cap_v(rpu_x + 180, btn_node[1], "C_DB", "100 nF OPSİYONEL")
    sheet.junction(rpu_x + 180, btn_node[1])
    sheet.gnd(cdb_b[0], cdb_b[1])
    sheet.net_flag(rpu_x + 240, btn_node[1], "BUTTON_N", "R")

    led_y_top = fy + 116
    cathode_y = 0.0
    led_x0 = fx + 520
    for offset, (flag, ref, value, colour, tp) in enumerate((
        ("LED_R", "R_R", "680 Ω", "#fecaca", 30),
        ("LED_G", "R_G", "330 Ω", "#bbf7d0", 31),
        ("LED_B", "R_B", "330 Ω", "#bfdbfe", 32),
    )):
        x = led_x0 + offset * 110
        sheet.net_flag(x, fy + 72, flag, "D")
        rt, rb = sheet.resistor_v(x, led_y_top, ref, value)
        sheet.wire([(x, fy + 72), rt], "dig")
        sheet.testpoint(x, fy + 94, tp)
        at, ak = sheet.led_v(x, rb[1], f"D{offset + 3}", "", colour)
        sheet.wire([rb, at], "sig")
        cathode_y = ak[1]
    sheet.wire([(led_x0, cathode_y), (led_x0 + 220, cathode_y)], "sig")
    for offset in range(3):
        sheet.junction(led_x0 + offset * 110, cathode_y)
    sheet.wire([(led_x0 + 110, cathode_y), (led_x0 + 110, cathode_y + 26)], "gnd")
    sheet.gnd(led_x0 + 110, cathode_y + 26, "ORTAK KATOT → STAR_GND")
    sheet.netlabel(fx + 30, cathode_y + 92, "Direnç değerleri ADAY: gerçek LED ileri gerilimi ve 2–5 mA hedefine göre hesaplanır.", "start", 0)
    sheet.netlabel(fx + 30, cathode_y + 114, "Hazır RGB modülünde seri direnç varsa bu parçalar DNP kalır. LED ve buton kabloları", "start", 0)
    sheet.netlabel(fx + 30, cathode_y + 136, "Class-D hoparlör kablolarından ayrı çekilir.", "start", 0)

    sheet.panel(fx, 1050, 800, panel_height(14), "GÜVENLİK VE ÖLÇÜM KURALLARI", "danger")
    sheet.panel_body(fx, 1050, [
        ("BTL ÇIKIŞA ŞASE KLİPSİ TAKMA", "panel-warn"),
        ("L− ve R− hoparlör eksisi değil, Class-D yarım köprü çıkışıdır. TP13–TP28", "panel-text"),
        ("uçlarının hiçbirine osiloskop GND klipsi bağlanmaz. Diferansiyel prob kullan;", "panel-text"),
        ("yoksa iki 10× prob, her iki GND klipsi yalnız TP2'ye, MATH = CH1 − CH2.", "panel-text"),
        ("", "panel-text"),
        ("ENERJİ VERME SIRASI", "panel-warn"),
        ("S1 bir XH-A232 + dummy-load (lab kaynağı 8–24 V) → S2 ESP32 + buck A, DAC + buck B", "panel-mono"),
        ("→ S3 I²S zinciri + tek amfi + dummy-load → S4 bir woofer, düşük seviye", "panel-mono"),
        ("→ S5 bir tweeter + C_SAFE, çok düşük seviye → S6 tek amfi + sürücü çifti, 24 V", "panel-mono"),
        ("adaptör → S7 dört amfi VIN'de: limiter tavanında toplam akım, VIN çöküşü, termal", "panel-mono"),
        ("→ S8 tam kabin soak (G8). Diğer üç amfi sürücülere ancak S6'dan sonra bağlanır.", "panel-mono"),
        ("", "panel-text"),
        ("C_SAFE tek başına crossover değildir; DSP HPF ve limiter'a karşı son savunmadır.", "panel-text"),
        ("Yüksüz çıkış ve jak polaritesi ölçülmeden, D2 kararı verilmeden adaptör takılmaz.", "panel-text"),
    ])

    # ================================================================ ZONE E
    # One DAC, four identical amplifiers. LOUT (woofer band) and ROUT (tweeter
    # band) are two horizontal buses with a junction into every amplifier
    # input; the GPIO21 mute is a third bus under the bank, dashed for its whole
    # length because the SD pad access is still an open decision (wiring plan
    # §9). Every C_SAFE and every pull-down is a countable part with its own
    # designator: a repeat marker cannot be ordered, stuffed or checked.
    ez_top = 1470
    sheet.zone(70, ez_top, 2580, 780, "E",
               "DAC → DÖRT XH-A232 / TPA3110 BTL AMFİ, SUSTURMA BUS'I VE SEKİZ SÜRÜCÜ — ADR-0002, ADR-0011")
    ey = 1620
    mute_y = 2060
    pitch = 630
    groups = [130 + k * pitch for k in range(4)]
    amps = []
    for k, gx in enumerate(groups):
        n = k + 1
        amp = sheet.block(f"U{7 + k}", f"XH-A232 #{n}", "TPA3110D2 · 2 × BTL Class-D · 8–26 V", gx + 110, ey, 250,
                          left=["L IN", "R IN", "SD  PAD ADAY", "VCC", "GND"],
                          right=["L+", "L−", "R+", "R−"])
        amps.append(amp)
        stub = amp.pin("L IN")[0]
        # Input taps. The near bus (LOUT) feeds the top row and the far bus
        # (ROUT) the row below it, each from its own vertical, so the two
        # horizontals never cross; the ROUT vertical hops the LOUT bus.
        l_tap_x, r_tap_x = gx + 70, gx + 46
        l_in, r_in = amp.pin("L IN"), amp.pin("R IN")
        sheet.wire([(l_tap_x, bus_l_y), (l_tap_x, l_in[1]), l_in], "aud")
        vertical_with_hops(sheet, r_tap_x, bus_r_y, r_in[1], [bus_l_y], "aud")
        sheet.wire([(r_tap_x, r_in[1]), r_in], "aud")
        if 0 < k < 3:
            sheet.junction(l_tap_x, bus_l_y)
            sheet.junction(r_tap_x, bus_r_y)
        # Mute: straight down from the SD stub to the dashed bus, one pull-down
        # per amplifier at the amplifier end. Four 10 k in parallel is 2.5 k,
        # about 1.3 mA when GPIO21 drives high: comfortable for the pin.
        sd = amp.pin("SD  PAD ADAY")
        sheet.wire([sd, (sd[0], mute_y)], "dig dnp")
        if k:
            sheet.junction(sd[0], mute_y)
        r_x = gx + 150
        r_t, r_b = sheet.resistor_v(r_x, mute_y, f"R{7 + k}", "10 kΩ", dnp=True)
        if k < 3:
            sheet.junction(r_x, mute_y)
        sheet.gnd(r_b[0], r_b[1], "POWER_GND")
        vcc = amp.pin("VCC")
        sheet.net_flag(vcc[0] - 30, vcc[1], "VIN", "L")
        sheet.wire([(vcc[0] - 30, vcc[1]), vcc], "pwr")
        gnd_pin = amp.pin("GND")
        sheet.wire([gnd_pin, (gnd_pin[0] - 44, gnd_pin[1])], "gnd")
        sheet.gnd(gnd_pin[0] - 44, gnd_pin[1], "POWER_GND")
        # Outputs. Woofer straight off L+/L−; tweeter below it through its own
        # C_SAFE, so that every tweeter's fuse is a part on the sheet.
        lp, lm = amp.pin("L+"), amp.pin("L−")
        wof_p, wof_m = sheet.speaker(gx + 450, (lp[1] + lm[1]) / 2, f"W{n}", f"WOOFER {n}", "4 Ω sınıfı · Fs G0 bekliyor")
        sheet.wire([lp, wof_p], "btl")
        sheet.wire([lm, wof_m], "btl")
        tp_base = 13 + 4 * k
        sheet.testpoint(gx + 410, lp[1], tp_base)
        sheet.testpoint(gx + 430, lm[1], tp_base + 1)
        rp, rm = amp.pin("R+"), amp.pin("R−")
        tweeter_y = ey + 290
        sheet.wire([rp, (gx + 412, rp[1]), (gx + 412, tweeter_y), (gx + 420, tweeter_y)], "btl")
        csa, csb = sheet.cap_h(gx + 420, tweeter_y, f"C_SAFE{n}", "")
        # Anchored at the capacitor's left terminal so that the text starts to
        # the right of the two BTL verticals instead of being centred on them.
        sheet.netlabel(gx + 420, tweeter_y, "film ≥50 V · 10 µF ADAY", "start", -46)
        twe_p, twe_m = sheet.speaker(gx + 510, tweeter_y + 16, f"T{n}", f"TWEETER {n}", "4 Ω sınıfı · G2 bekliyor")
        sheet.wire([csb, twe_p], "btl")
        sheet.wire([rm, (gx + 398, rm[1]), (gx + 398, twe_m[1]), twe_m], "btl")
        sheet.testpoint(gx + 397, rp[1], tp_base + 2)
        sheet.testpoint(gx + 490, twe_m[1], tp_base + 3)

    # The buses themselves, drawn once the tap positions are known. Each starts
    # at the first amplifier's tap and ends at the last one's, so those two are
    # corners rather than junctions.
    sheet.wire([(groups[0] + 70, bus_l_y), (groups[3] + 70, bus_l_y)], "aud")
    sheet.wire([(groups[0] + 46, bus_r_y), (groups[3] + 46, bus_r_y)], "aud")
    sheet.junction(lout_x, bus_l_y)
    sheet.junction(rout_x, bus_r_y)
    sheet.testpoint(1870, bus_l_y, 11)
    sheet.testpoint(1840, bus_r_y, 12)
    sheet.netlabel(groups[0] + 90, bus_r_y - 14, "DAC_ROUT · tweeter bandı · dört R IN paralel", "start", 0)
    sheet.netlabel(groups[0] + 90, bus_l_y + 22, "DAC_LOUT · woofer bandı · dört L IN paralel", "start", 0)
    mute_start = 170
    sheet.wire([(mute_start, mute_y), (groups[3] + 150, mute_y)], "dig dnp")
    sheet.net_flag(mute_start, mute_y, "AMP_MUTE", "L")
    sheet.junction(amps[0].pin("SD  PAD ADAY")[0], mute_y)
    sheet.testpoint(groups[0] + 120, mute_y, 34)

    sheet.netlabel(130, 2200, "Hat seviyesi fan-out: XH-A232 girişi 10 kΩ sınıfı, dördü paralel ≈2,5 kΩ — PCM5102A için rahat bir yük (aritmetik; G1 dört giriş bağlıyken DAC çıkış seviyesini kaydeder). "
                              "DAC ↔ amfi bankı kablosu kısa ve ekranlı; fan-out noktası amfi bankında.", "start", 0)
    sheet.netlabel(130, 2224, "KESİKLİ DAL ADAYDIR: XH-A232'de erişilebilir SD pad'i doğrulanmadı (§9). Dört kart aynı revizyon olmalı; dal ya dördünde ya hiçbirinde takılır. Pad yoksa R7–R10 ve bu dal takılmaz, "
                              "firmware kontrollü amfi susturması olmaz, geriye DAC XSMT kalır. Her R7 kendi amfisinin ucuna monte edilir: kablo koparsa pad LOW kalsın.", "start", 0)

    # ============================================================== PANELS
    panels_y = 2290
    sheet.panel(70, panels_y, 1690, panel_height(12), "TEST NOKTASI İNDEKSİ — TP0…TP34")
    # Column-major, each column its own text element. Padding with spaces does
    # not work: SVG text collapses runs of whitespace, so a padded table never
    # lines up.
    per_column, column_pitch = 7, 330
    baseline = panels_y + Sheet.PANEL_BODY_TOP
    for row in range(per_column):
        for column, number in enumerate(range(row, len(TP_LABELS), per_column)):
            sheet.panel_line(70 + 20 + column * column_pitch, baseline,
                             f"TP{number} {TP_LABELS[number]}", "panel-mono")
        baseline += Sheet.PANEL_LINE
    for line in ("",
                 "Güç açma/kapatma kaydı — CH1 TP1 · CH2 TP3 · CH3 TP5 · CH4 TP9/TP10.   Susturma kaydı — CH1 TP1 · CH2 TP33 · CH3 TP34 · CH4 TP9/TP10.",
                 "Ripple ölçümünde 10× prob, ground-spring ve 20 MHz bant sınırı kullanılır. TP1 çöküşü dört amfi limiter tavanında sürülürken, 2,9 A bütçesine karşı kaydedilir.",
                 "Beklenen değer ve geçiş şartları: docs/02-hardware/circuit-and-wiring-plan.md §7"):
        if line:
            sheet.panel_line(70 + 20, baseline, line)
        baseline += Sheet.PANEL_LINE

    # The mute chain gets its own panel rather than a line inside one of the
    # zones. It is the only safety layer that exists before firmware runs.
    sheet.panel(1790, panels_y, 860, panel_height(12), "SUSTURMA HATLARI — ADR-0011", "danger")
    sheet.panel_body(1790, panels_y, [
        ("SUSTURMAYI TUTAN ŞEY GPIO DEĞİL, DİRENÇTİR", "panel-warn"),
        ("R6 ve dört R7 (R7–R10, 10 kΩ pull-down) opsiyonel değildir. Bu parçadaki her aday", "panel-text"),
        ("GPIO reset'ten yüksek empedanslı çıkar ve ROM, bootloader ve uygulama başlangıcı", "panel-text"),
        ("boyunca öyle kalır — yüzlerce ms. Firmware'in işi susturmayı BIRAKMAKTIR;", "panel-text"),
        ("firmware hiç çalışmazsa sekiz sürücü sessiz kalır.", "panel-text"),
        ("Dört SD pad'i tek GPIO21 hattına paraleldir; her amfide kendi R7'si: dört 10 kΩ", "panel-text"),
        ("paralelde 2,5 kΩ, HIGH'da ≈1,3 mA — GPIO için rahat. Dal ya dördünde ya hiçbirinde.", "panel-text"),
        ("OPERATÖR, LEHİMDEN ÖNCE: (1) PCM5102A XSMT pad'i ↔ 3V3 direncini ölç, sert köprü", "panel-text"),
        ("varsa kes. (2) Dört XH-A232'de de erişilebilir SD pad'i var mı, süreklilikle ara.", "panel-text"),
        ("(3) Açılışta TP33 ve TP34 LOW mu, osiloskopla kaydet.", "panel-text"),
        ("Bu ölçümler kaydedilmeden susturma katmanı DOĞRULANMAMIŞTIR; empedans eğrisi", "panel-warn"),
        ("ölçülmemiş sürücülere sinyal verilmez.", "panel-warn"),
    ])

    # gates + legend + title block
    #
    # Anchored to the bottom of the sheet rather than to fixed numbers. The
    # first time the sheet grew, these stayed where they were and two new zones
    # landed on top of the title block.
    strip = H - 190
    sheet.panel(70, strip, 1690, 92, "ENERJİ VERME ÖNCESİ ZORUNLU KAPILAR", "gate")
    sheet.panel_line(90, strip + 46, "G0 sürücü DC direnci / empedans / polarite   ·   G1 tek amfi + 8 Ω ≥50 W non-inductive dummy-load, adaptör yüksüz < 25,5 V, "
                                     "jak polaritesi, dört amfi tam yükte VIN çöküşü, brownout, pop   ·   G2 tweeter HPF + limiter + C_SAFE",
                     "panel-mono")
    sheet.panel_line(90, strip + 72, "Fiziksel ölçüm kaydı olmadan hiçbir kapı PASS yapılamaz. İlk enerjilenen yol bir amfi, bir woofer, bir tweeter'dır; "
                                     "diğer üç amfi G0–G2 o çift üzerinde geçmeden sürücülere bağlanmaz.", "panel-warn")

    sheet.panel(1790, strip, 860, 92, "", "title")

    # The legend sits on its own strip below both panels. Sharing the title
    # block's rectangle would have hidden it: panels paint an opaque fill and
    # render in append order.
    legend = [("pwr", "24 V DC / VIN"), ("v5", "5 V (A → ESP, B → DAC)"), ("v33", "3V3"),
              ("dig", "dijital / I²S / susturma"), ("aud", "analog ses bus"), ("btl", "BTL çıkış"), ("gnd", "toprak")]
    sheet.overlay.append(f'<text class="panel-title" x="70" y="{strip + 130}">GÖSTERİM</text>')
    for index, (kind, name) in enumerate(legend):
        x = 210 + index * 270
        sheet.overlay.append(f'<line class="w {kind}" x1="{x}" y1="{strip + 125}" x2="{x + 46}" y2="{strip + 125}"/>'
                             f'<text class="legend" x="{x + 56}" y="{strip + 130}">{escape(name)}</text>')
    hop_x = 210 + len(legend) * 270
    sheet.overlay.append(
        f'<line class="w sig" x1="{hop_x}" y1="{strip + 125}" x2="{hop_x + 46}" y2="{strip + 125}"/>'
        f'<path class="w sig" d="M {hop_x + 23},{strip + 125 + 14} A {HOP} {HOP} 0 0 1 {hop_x + 23},{strip + 125 - 4}"/>'
        f'<text class="legend" x="{hop_x + 56}" y="{strip + 130}">{escape("yarım daire: kesişme, bağlantı yok · içi dolu nokta: bağlantı")}</text>')
    fields = [("BELGE", "HK-HW-SCH", 1810), ("REV", REV, 2010), ("DURUM", "CANDIDATE", 2130), ("TARİH", DATE, 2330), ("SAYFA", "1 / 1", 2500)]
    for key, value, x in fields:
        sheet.overlay.append(f'<text class="tb-key" x="{x}" y="{strip + 32}">{escape(key)}</text>'
                             f'<text class="tb-val" x="{x}" y="{strip + 58}">{escape(value)}</text>')
    sheet.overlay.append(f'<text class="tb-key" x="1810" y="{strip + 82}">ÜRETEN  hardware/diagrams/generate_schematic_svg.py  ·  '
                         'ELEKTRİKSEL KAYNAK  hardware/kicad/</text>')
    return sheet


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[2] / "docs/02-hardware/assets/merzarkabul-schematic.svg")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(build().render(CSS), encoding="utf-8")
    print(f"Generated: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
