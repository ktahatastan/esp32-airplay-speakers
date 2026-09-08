#!/usr/bin/env python3
"""Generate the single Harman Kardom documentation schematic sheet.

One speaker, module level. The four speakers repeat the same circuit.

This sheet is the readable overview: functional zones, real symbols, orthogonal
wires, junction dots, net flags for cross-zone nets, and the TP0-TP31 probe
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

W, H = 2720, 3010
REV = "P2"
DATE = "2026-09-08"


def panel_height(lines: int, tail: float = 14) -> float:
    """Box height that actually contains `lines` body lines.

    Two panels were sized by hand and were 50 and 98 units too short, so their
    last lines — the ≤50 mA display budget and the INA219 common-mode limit —
    were rendered outside their own boxes. Nothing complained, because a panel
    is a rectangle and its text is separate. Then the next panel added below
    them painted over that text, because panels have an opaque fill and render
    in append order. Derived from the library's own pitch so it cannot drift.
    """
    return Sheet.PANEL_BODY_TOP + (lines - 1) * Sheet.PANEL_LINE + tail


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
.chg{stroke:#ea580c;stroke-width:2.8}
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
.sym-plate-long{stroke:#0f172a;stroke-width:2.6}
.sym-dot{fill:#ffffff;stroke:#0f172a;stroke-width:1.8}
.sym-ref{font:700 12.5px ui-monospace,Menlo,Consolas,monospace;fill:#b91c1c}
.sym-val{font:11.5px 'Helvetica Neue',Arial,sans-serif;fill:#1e293b}
.sym-pol{font:700 15px 'Helvetica Neue',Arial,sans-serif;fill:#0f172a}
.led-ray{stroke:#0f172a;stroke-width:1.6;fill:none}
.cell-name{font:11.5px 'Helvetica Neue',Arial,sans-serif;fill:#334155}
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


def build() -> Sheet:
    sheet = Sheet(
        W, H,
        "Harman Kardom — tek hoparlör modül seviyesi devre şeması",
        "USB-C PD şarj zinciri, 4S paket ve BMS, anahtarlı güç, 5 V lojik beslemesi, "
        "ESP32-S3 N16R8, PCM5102A I2S DAC, XH-A232 BTL bi-amp, GPIO13/GPIO21 susturma hatları ve "
        "harici pull-down'ları, woofer ve C_SAFE korumalı tweeter, "
        "GC9A01 yuvarlak ekran, INA219 şarj akımı ölçümü, kullanıcı arayüzü ve TP0-TP31 test noktaları.",
    )
    frame(sheet)
    sheet.background.append(
        f'<text class="sheet-title" x="70" y="86">HARMAN KARDOM · TEK HOPARLÖR DEVRE ŞEMASI</text>'
        f'<text class="sheet-sub" x="70" y="108">Modül seviyesi prototip · dört kutuda aynı devre tekrarlanır · '
        f'&#171;ADAY / TBD&#187; değerler ölçümle kilitlenir · üretim PCB şeması değildir</text>')

    # ================================================================ ZONE A
    sheet.zone(70, 130, 1050, 500, "A", "USB-C PD ŞARJ GİRİŞİ  →  16,80 V CC/CV")
    j1 = sheet.block("J1", "USB-C SOKETİ", "harici 65 W PD adaptör", 100, 200, 180,
                     right=["VBUS", "GND"])
    # Pin names are unique per block: the trigger and the converter each carry
    # ground on an input and an output pin, which is one plane inside the module.
    u1 = sheet.block("U1", "PD TETİKLEYİCİ", "sabit 20 V profili", 400, 200, 210,
                     left=["VBUS", "GND IN"], right=["PD20V", "GND OUT"])
    u2 = sheet.block("U2", "XL4015 CC/CV", "16,80 V / 2,00 A'e kalibre", 740, 200, 210,
                     left=["PD20V", "GND IN"], right=["CHG+", "CHG−"])
    sheet.wire([j1.pin("VBUS"), u1.pin("VBUS")], "chg")
    sheet.wire([j1.pin("GND"), u1.pin("GND IN")], "gnd")
    sheet.wire([u1.pin("PD20V"), u2.pin("PD20V")], "chg")
    sheet.wire([u1.pin("GND OUT"), u2.pin("GND IN")], "gnd")
    sheet.netlabel(360, 278, "USB_PD_VBUS")
    sheet.netlabel(700, 278, "PD20V")

    # charge output: fuse, test point, net flags out of the zone
    # The charge output leaves on the pin row itself, so no segment is diagonal,
    # and the return turns at x=1005 which is clear of U2's body (740-950). A
    # channel inside the block would be painted over by the block and read as
    # a disconnected wire.
    chg_plus, chg_minus = u2.pin("CHG+"), u2.pin("CHG−")
    sheet.wire([chg_plus, (1020, chg_plus[1])], "chg")
    sheet.testpoint(1000, chg_plus[1] - 44, 26, anchor=(1000, chg_plus[1]))
    fa, fb = sheet.fuse(1020, chg_plus[1], "F_CHG", "3 A ADAY")
    sheet.wire([fb, (1085, chg_plus[1])], "chg")
    sheet.net_flag(1085, chg_plus[1], "CHG_16V8_RAW", "R")
    sheet.wire([chg_minus, (1005, chg_minus[1]), (1005, 366), (1085, 366)], "gnd")
    sheet.net_flag(1085, 366, "POWER_GND", "R")

    sheet.panel(100, 420, 990, 190, "ŞARJ ZİNCİRİ KURALLARI — ADR-0009")
    sheet.panel_body(100, 420, [
        "USB-C soketi 4S pakete doğrudan bağlanmaz. PD tetikleyici yalnız 20 V profilini anlaşır; şarj profilini XL4015 sağlar.",
        "PD 20 V seçimi ve XL4015 çıkışı (16,80 V / 2,00 A) BATARYA BAĞLI DEĞİLKEN ölçülerek kalibre edilir.",
        "BMS bir şarj cihazı değildir; yalnız koruma ve balans katmanıdır. Aşırı şarj koruması sonlandırma algoritması sayılmaz.",
        "XL4015'te ters polarite koruması yoktur. Kutuplar etiketlenir; ilk enerjilendirme akım sınırlı kaynakla yapılır.",
        ("ŞARJ SONLANDIRMA GARANTİ EDİLMİYOR — G4 ölçümü tamamlanmadan gözetimsiz veya gece boyu şarj yapılmaz.", "panel-warn"),
    ])

    # ================================================================ ZONE B
    sheet.zone(1150, 130, 1500, 470, "B", "4S PAKET, BMS, SİGORTA VE ANA GÜÇ ANAHTARI")
    cell_x = 1240
    cells = []
    for index, top in enumerate((190, 270, 350, 430)):
        cells.append(sheet.cell_v(cell_x, top, f"BT{4 - index}"))
    sheet.netlabel(cell_x + 30, 520, "4 × ASPILSAN INR18650A28 · aynı parti · eşlenmiş · profesyonel punta", "start", 0)
    for index in range(3):
        sheet.wire([cells[index][1], cells[index + 1][0]], "pwr")
    taps = [
        ("B+", cells[0][0][1], 1420, 0),
        ("B3", (cells[0][1][1] + cells[1][0][1]) / 2, 1390, 1),
        ("B2", (cells[1][1][1] + cells[2][0][1]) / 2, 1360, 2),
        ("B1", (cells[2][1][1] + cells[3][0][1]) / 2, 1330, 3),
        ("B−", cells[3][1][1], 1300, 4),
    ]
    u3 = sheet.block("U3", "4S BALANSLI BMS", "common-port referans · balans ve NTC doğrulanacak",
                     1460, 250, 320,
                     left=["B+", "B3", "B2", "B1", "B−"], right=["P+", "P−", "NTC"])
    for tp_number, (name, y, channel, index) in zip((4, 3, 2, 1, 0), taps):
        target = u3.pin(name)
        sheet.wire([(cell_x, y), (channel, y), (channel, target[1]), target], "pwr")
        if y not in (cells[0][0][1],):
            sheet.junction(cell_x, y)
        sheet.testpoint(1180, y, tp_number, anchor=(cell_x, y))

    # protection and hard switch
    pplus = u3.pin("P+")
    sheet.wire([pplus, (1900, pplus[1])], "pwr")
    sheet.testpoint(1845, pplus[1], 5)
    sheet.junction(1900, pplus[1])
    sheet.wire([(1900, pplus[1]), (1900, 200)], "chg")
    sheet.net_flag(1900, 200, "CHG_16V8", "U")
    f1a, f1b = sheet.fuse(1960, pplus[1], "F1", "5 A ADAY")
    sheet.wire([(1900, pplus[1]), f1a], "pwr")
    sheet.wire([f1b, (2076, pplus[1])], "pwr")
    sheet.testpoint(2046, pplus[1], 7)
    s1a, s1b = sheet.switch(2076, pplus[1], "S1", "KM103 / DC-132A")
    sheet.wire([s1b, (2230, pplus[1])], "pwr")
    sheet.testpoint(2186, pplus[1], 8)
    sheet.power_port(2230, pplus[1], "VBAT_SW")
    sheet.junction(2230, pplus[1])
    sheet.netlabel(1900, 404, "S1 kontak değeri belgesiz · 16,8 VDC / 5 A yazılı doğrulanacak, G3'te test edilecek", "start", 0)

    ca_top, ca_bot = sheet.cap_v(2330, pplus[1], "C_A", "1000 µF / 25 V")
    sheet.wire([(2230, pplus[1]), ca_top], "pwr")
    sheet.junction(2330, pplus[1])
    sheet.gnd(ca_bot[0], ca_bot[1])
    d2_a, d2_k = sheet.led_v(2500, pplus[1], "D2", "KM103 12 V", "#fde68a", dnp=True)
    sheet.wire([(2330, pplus[1]), d2_a], "pwr")
    r2_t, r2_b = sheet.resistor_v(2500, d2_k[1], "R2", "R_SW_LED", dnp=True)
    sheet.gnd(r2_b[0], r2_b[1])
    sheet.netlabel(2380, 520, "D2 ve R2 ilk prototipte DNP kalır.", "start", 0)
    sheet.netlabel(2380, 542, "12 V anahtar LED'i 16,8 V hatta", "start", 0)
    sheet.netlabel(2380, 564, "doğrudan bağlanmaz.", "start", 0)

    pminus = u3.pin("P−")
    sheet.wire([pminus, (1860, pminus[1]), (1860, 470)], "gnd")
    sheet.testpoint(1860, 404, 6)
    sheet.junction(1860, 440)
    sheet.wire([(1860, 440), (1960, 440)], "gnd")
    sheet.net_flag(1960, 440, "CHG_NEG", "R")
    sheet.gnd(1860, 470, "POWER_GND")

    ntc = u3.pin("NTC")
    sheet.wire([ntc, (1802, 560), (1960, 560)], "sig")
    sheet.testpoint(1990, 560, 27)
    sheet.net_flag(2005, 560, "NTC", "R")
    sheet.netlabel(1810, 596, "10k NTC · orta hücre grubuna termal temas · yalnız telemetri", "start", 0)

    # ================================================================ ZONE C
    sheet.zone(70, 630, 590, 800, "C", "5 V LOJİK BESLEMESİ")
    u4 = sheet.block("U4", "MP1584 BUCK", "yüksüz 5,10 V'a ayarla", 210, 700, 230,
                     left=["IN+", "IN−"], right=["OUT+", "OUT−"])
    sheet.net_flag(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1], "VBAT_SW", "L")
    sheet.wire([(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1]), u4.pin("IN+")], "pwr")
    sheet.gnd(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1], "POWER_GND")
    sheet.wire([(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1]), u4.pin("IN−")], "gnd")
    out = u4.pin("OUT+")
    jp_a, jp_b = sheet.switch(490, out[1], "", "JP1 · SERVİS AYIRMA")
    sheet.wire([out, jp_a], "v5")
    sheet.testpoint(474, out[1] + 52, 9, anchor=(474, out[1]))
    sheet.wire([jp_b, (600, out[1])], "v5")
    sheet.power_port(600, out[1], "+5V_LOGIC")
    sheet.gnd(u4.pin("OUT−")[0] + 40, u4.pin("OUT−")[1], "STAR_GND")
    sheet.wire([u4.pin("OUT−"), (u4.pin("OUT−")[0] + 40, u4.pin("OUT−")[1])], "gnd")
    sheet.panel(100, 960, 470, 430, "GÜÇ SIRALAMASI")
    sheet.panel_body(100, 960, [
        "1. MP1584 çıkışını ESP32 ve DAC",
        "    BAĞLI DEĞİLKEN 5,10 V'a ayarla.",
        "2. Elektronik yükle droop ve ripple",
        "    ölçümünü tekrarla.",
        "3. USB ile programlarken JP1 açılır;",
        "    USB ve harici 5 V birlikte",
        "    kullanılmaz.",
        "",
        ("TP9 hedefi: normal yükte ≤50 mVpp;", "panel-mono"),
        ("Wi-Fi sıçramasında 5 V hattı 4,75 V", "panel-mono"),
        ("altına düşmemeli.", "panel-mono"),
        "",
        ("TP10 hedefi: brownout/reset üreten", "panel-mono"),
        ("çökme yok. Alt sınır G3'te kilitlenir.", "panel-mono"),
    ])

    # ================================================================ ZONE D
    sheet.zone(680, 630, 510, 800, "D", "ESP32-S3 N16R8 — ADR-0010")
    u5 = sheet.block("U5", "ESP32-S3 DEVKIT", "16 MB flash + 8 MB PSRAM", 830, 700, 330,
                     left=["5V / VBUS", "GND", "3V3", "GPIO7  BUTTON", "GPIO8  LED_R",
                           "GPIO9  LED_G", "GPIO10 LED_B", "GPIO1  BATT_SENSE",
                           "GPIO2  NTC_SENSE", "GPIO47 LCD_SCK", "GPIO41 LCD_MOSI",
                           "GPIO39 LCD_CS", "GPIO40 LCD_DC", "GPIO18 LCD_RST"],
                     # DAC_XSMT sits one row above AMP_MUTE so that it lands on the
                     # same row as U6's XSMT pin: the mute net is then a straight
                     # wire, and the long AMP_MUTE run leaves from the bottom row
                     # where it has a clear corridor under zone E.
                     right=["GPIO4  BCLK", "GPIO5  LRCLK", "GPIO6  DATA",
                            "GPIO11 SDA", "GPIO12 SCL", "GPIO13 DAC_XSMT",
                            "GPIO21 AMP_MUTE"],
                     note="GPIO ataması ADAY · kart şeması ve boot testi olmadan accepted değil")
    v5 = u5.pin("5V / VBUS")
    sheet.net_flag(v5[0] - 30, v5[1], "+5V_LOGIC", "L")
    sheet.wire([(v5[0] - 30, v5[1]), v5], "v5")
    gnd5 = u5.pin("GND")
    sheet.gnd(gnd5[0] - 148, gnd5[1], "STAR_GND")
    sheet.wire([(gnd5[0] - 148, gnd5[1]), gnd5], "gnd")
    v33 = u5.pin("3V3")
    sheet.wire([v33, (v33[0] - 68, v33[1])], "v33")
    sheet.testpoint(v33[0] - 22, v33[1], 10)
    sheet.power_port(v33[0] - 68, v33[1], "+3V3")
    for pin_name, flag in (("GPIO7  BUTTON", "BUTTON_N"), ("GPIO8  LED_R", "LED_R"),
                           ("GPIO9  LED_G", "LED_G"), ("GPIO10 LED_B", "LED_B"),
                           ("GPIO47 LCD_SCK", "LCD_SCK"), ("GPIO41 LCD_MOSI", "LCD_MOSI"),
                           ("GPIO39 LCD_CS", "LCD_CS"), ("GPIO40 LCD_DC", "LCD_DC"),
                           ("GPIO18 LCD_RST", "LCD_RST")):
        point = u5.pin(pin_name)
        sheet.wire([point, (point[0] - 30, point[1])], "dig")
        sheet.net_flag(point[0] - 30, point[1], flag, "L")

    # ================================================================ ZONE E
    sheet.zone(1210, 630, 580, 800, "E", "PCM5102A I²S DAC")
    u6 = sheet.block("U6", "PCM5102A MODÜLÜ", "3-wire I²S · modül köprüleri doğrulanacak",
                     1350, 700, 380,
                     left=["BCK", "LCK / LRCK", "DIN", "SCK", "VIN 5 V", "XSMT", "GND / AGND"],
                     right=["LOUT", "ROUT", "AGND"])
    # I2S: the DAC input pins sit on the same rows as the ESP32 outputs, so each
    # clock is a single straight wire with nothing to cross.
    for source, target, tp, tp_x in (("GPIO4  BCLK", "BCK", 11, 1230),
                                     ("GPIO5  LRCLK", "LCK / LRCK", 12, 1270),
                                     ("GPIO6  DATA", "DIN", 13, 1310)):
        a, b = u5.pin(source), u6.pin(target)
        sheet.wire([a, b], "dig")
        sheet.testpoint(tp_x, a[1], tp)
    sck = u6.pin("SCK")
    sheet.wire([sck, (sck[0] - 44, sck[1])], "gnd")
    sheet.gnd(sck[0] - 44, sck[1])
    vin6 = u6.pin("VIN 5 V")
    sheet.net_flag(vin6[0] - 30, vin6[1], "+5V_LOGIC", "L")
    sheet.wire([(vin6[0] - 30, vin6[1]), vin6], "v5")
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
    # unmeasured driver and whatever the amplifier input happens to be holding.
    xsmt_esp, xsmt_dac = u5.pin("GPIO13 DAC_XSMT"), u6.pin("XSMT")
    sheet.wire([xsmt_esp, xsmt_dac], "dig")
    sheet.testpoint(1300, xsmt_dac[1], 30)
    r6_t, r6_b = sheet.resistor_v(1230, xsmt_dac[1], "R6", "10 kΩ")
    sheet.junction(1230, xsmt_dac[1])
    sheet.gnd(r6_b[0], r6_b[1], "STAR_GND")

    sheet.netlabel(1230, 1200, "FMT=LOW · FLT=LOW · DEMP=LOW · SCK→GND (yalnız 3-wire BCK-PLL modu)", "start", 0)
    sheet.netlabel(1230, 1224, "XSMT modül varsayılanına BIRAKILMAZ: GPIO13 sürer, R6 kapalı tutar.", "start", 0)
    sheet.netlabel(1230, 1248, "LEHİMDEN ÖNCE ÖLÇ: XSMT pad'i ↔ 3V3 direnci. Sert köprü / 0 Ω varsa", "start", 0)
    sheet.netlabel(1230, 1272, "kesilmeden GPIO13 bağlanmaz; pin LOW sürerken 3V3 rayına kısa devredir.", "start", 0)
    sheet.netlabel(1230, 1296, "Modül köprüleri satıcıya göre değişir; pad ismine bakıp lehim yapılmaz.", "start", 0)

    # ================================================================ ZONE F
    sheet.zone(1810, 630, 850, 800, "F", "XH-A232 / TPA3110 BTL Bİ-AMP VE SÜRÜCÜLER")
    # `SD` is a real TPA3110 pin. What is NOT confirmed is whether the XH-A232
    # board brings it out to a pad anyone can solder to, so the pin is labelled
    # ADAY and everything hanging off it is drawn dashed.
    u7 = sheet.block("U7", "XH-A232 / TPA3110", "2 × BTL Class-D · 8–26 V", 1960, 700, 330,
                     left=["L IN", "R IN", "VCC", "GND", "SD  PAD ADAY"],
                     right=["L+", "L−", "R+", "R−"])
    for source, target, tp_dac, tp_amp in (("LOUT", "L IN", 14, 16), ("ROUT", "R IN", 15, 17)):
        a, b = u6.pin(source), u7.pin(target)
        sheet.wire([a, b], "aud")
        sheet.testpoint(1800, a[1], tp_dac)
        sheet.testpoint(1890, a[1], tp_amp)
    vcc7 = u7.pin("VCC")
    sheet.net_flag(vcc7[0] - 30, vcc7[1], "VBAT_SW", "L")
    sheet.wire([(vcc7[0] - 30, vcc7[1]), vcc7], "pwr")
    gnd7 = u7.pin("GND")
    sheet.wire([gnd7, (gnd7[0] - 44, gnd7[1])], "gnd")
    sheet.gnd(gnd7[0] - 44, gnd7[1], "POWER_GND")
    sheet.netlabel(2000, 990, "DAC ↔ amfi kablosu kısa ve ekranlı", "start", 0)
    sheet.netlabel(2000, 1014, "hoparlör çıkış kablosuyla paralel gitmez", "start", 0)

    # Amplifier mute. Dashed for its whole length, R7 included: this branch is a
    # RESERVATION (ADR-0011). It is fitted only if an accessible SD pad is found
    # on the XH-A232, which is still an open decision in the wiring plan §9. The
    # corridor runs under zone E because the ESP is in zone D and the amp in F.
    sd_pin = u7.pin("SD  PAD ADAY")
    mute_esp = u5.pin("GPIO21 AMP_MUTE")
    sheet.wire([mute_esp, (1200, mute_esp[1]), (1200, 1150), (sd_pin[0], 1150), sd_pin], "dig dnp")
    sheet.testpoint(sd_pin[0], 1010, 31)
    r7_t, r7_b = sheet.resistor_v(1880, 1150, "R7", "10 kΩ", dnp=True)
    sheet.junction(1880, 1150)
    sheet.gnd(r7_b[0], r7_b[1], "POWER_GND")
    sheet.netlabel(1830, 1300, "KESİKLİ DAL ADAYDIR: XH-A232'de erişilebilir 'SD' pad'i", "start", 0)
    sheet.netlabel(1830, 1324, "doğrulanmadı (§9 açık karar). Pad yoksa R7 ve bu dal takılmaz;", "start", 0)
    sheet.netlabel(1830, 1348, "firmware kontrollü amfi susturması olmaz, geriye DAC XSMT kalır.", "start", 0)
    sheet.netlabel(1830, 1372, "R7 amfi ucuna monte edilir: kablo koparsa pad LOW kalsın.", "start", 0)

    lp, lm = u7.pin("L+"), u7.pin("L−")
    wof_p, wof_m = sheet.speaker(2450, (lp[1] + lm[1]) / 2, "SPK1", "WOOFER", "Ω TBD · G0 bekliyor")
    sheet.wire([lp, wof_p], "btl")
    sheet.wire([lm, wof_m], "btl")
    sheet.testpoint(2370, lp[1], 18)
    sheet.testpoint(2410, lm[1], 19)

    rp, rm = u7.pin("R+"), u7.pin("R−")
    tweeter_y = 1290
    sheet.wire([rp, (2350, rp[1]), (2350, tweeter_y), (2360, tweeter_y)], "btl")
    csa, csb = sheet.cap_h(2360, tweeter_y, "C_SAFE", "")
    sheet.netlabel(2392, tweeter_y, "kutupsuz film · DEĞER TBD", "middle", -46)
    twe_p, twe_m = sheet.speaker(2450, tweeter_y + 16, "SPK2", "TWEETER", "Ω TBD · G2 bekliyor")
    sheet.wire([csb, twe_p], "btl")
    sheet.wire([rm, (2330, rm[1]), (2330, twe_m[1]), twe_m], "btl")
    sheet.testpoint(2350, 990, 20)
    sheet.testpoint(2330, 1080, 21)

    # ================================================================ ZONE G
    sheet.zone(70, 1470, 830, 380, "G", "KULLANICI ARAYÜZÜ")
    sheet.power_port(160, 1556, "+3V3")
    rpu_t, rpu_b = sheet.resistor_v(160, 1556, "R_PU", "10 kΩ ADAY")
    btn_node = rpu_b
    sheet.wire([btn_node, (160, 1650)], "sig")
    sw_t, sw_b = sheet.pushbutton_v(160, 1650, "SW1", "FONKSİYON / RESET")
    sheet.gnd(sw_b[0], sw_b[1], "STAR_GND")
    sheet.wire([btn_node, (400, btn_node[1])], "dig")
    sheet.junction(160, btn_node[1])
    sheet.testpoint(280, btn_node[1], 22)
    cdb_t, cdb_b = sheet.cap_v(340, btn_node[1], "C_DB", "100 nF OPSİYONEL")
    sheet.junction(340, btn_node[1])
    sheet.gnd(cdb_b[0], cdb_b[1])
    sheet.net_flag(400, btn_node[1], "BUTTON_N", "R")

    led_y_top = 1556
    cathode_y = 0.0
    for offset, (flag, ref, value, colour, tp) in enumerate((
        ("LED_R", "R_R", "680 Ω", "#fecaca", 23),
        ("LED_G", "R_G", "330 Ω", "#bbf7d0", 24),
        ("LED_B", "R_B", "330 Ω", "#bfdbfe", 25),
    )):
        x = 590 + offset * 110
        sheet.net_flag(x, 1512, flag, "D")
        rt, rb = sheet.resistor_v(x, led_y_top, ref, value)
        sheet.wire([(x, 1512), rt], "dig")
        sheet.testpoint(x, 1534, tp)
        at, ak = sheet.led_v(x, rb[1], f"D{offset + 3}", "", colour)
        sheet.wire([rb, at], "sig")
        cathode_y = ak[1]
    sheet.wire([(590, cathode_y), (810, cathode_y)], "sig")
    for offset in range(3):
        sheet.junction(590 + offset * 110, cathode_y)
    sheet.wire([(700, cathode_y), (700, cathode_y + 26)], "gnd")
    sheet.gnd(700, cathode_y + 26, "ORTAK KATOT → STAR_GND")
    sheet.netlabel(100, cathode_y + 92, "Direnç değerleri ADAY: gerçek LED ileri gerilimi ve 2–5 mA hedefine göre hesaplanır.", "start", 0)
    sheet.netlabel(100, cathode_y + 114, "Hazır RGB modülünde seri direnç varsa bu parçalar DNP kalır. LED ve buton kabloları", "start", 0)
    sheet.netlabel(100, cathode_y + 136, "Class-D hoparlör kablolarından ayrı çekilir.", "start", 0)

    # ================================================================ ZONE H
    sheet.zone(70, 1900, 1290, 728, "H", "GC9A01 240×240 YUVARLAK EKRAN — ADR-0017")

    # Direct from the ESP: the bench build has no series resistors. They are not
    # decoration -- 33 R at the driver end slows the edges on flying leads that
    # run beside an analogue audio path, which is a product concern rather than
    # a bench one. Recorded in ADR-0017 so the product wiring puts them back.
    u8 = sheet.block("U8", "GC9A01 MODÜLÜ", "240×240 IPS · 7 pin · SPI3 · 3,3 V lojik · 5 V TOLERANSLI DEĞİL",
                     760, 1962, 300,
                     left=["RST", "CS", "DC", "SDA", "SCL", "GND", "VCC"],
                     note="7 pinli varyant: arka ışık ucu yok, VCC ile birlikte yanar")
    for flag, pin_name in (("LCD_SCK", "SCL"), ("LCD_MOSI", "SDA"),
                           ("LCD_DC", "DC"), ("LCD_CS", "CS")):
        point = u8.pin(pin_name)
        sheet.net_flag(point[0] - 280, point[1], flag, "R")
        sheet.wire([(point[0] - 280, point[1]), point], "dig")

    rst = u8.pin("RST")
    sheet.net_flag(rst[0] - 280, rst[1], "LCD_RST", "R")
    sheet.wire([(rst[0] - 280, rst[1]), rst], "dig")

    # CS must be deasserted through ROM and bootloader. GPIO39 comes up with a
    # weak internal pull-up, and this makes that guaranteed rather than inherited.
    cs_node = u8.pin("CS")
    pu_t, pu_b = sheet.resistor_v(cs_node[0] - 90, cs_node[1] - 150, "R_CS", "10 kΩ", dnp=True)
    sheet.power_port(pu_t[0], pu_t[1], "+3V3")
    sheet.wire([pu_b, (pu_b[0], cs_node[1])], "dig")
    sheet.junction(pu_b[0], cs_node[1])

    vcc8 = u8.pin("VCC")
    sheet.net_flag(vcc8[0] - 280, vcc8[1], "+3V3_LCD", "R")
    sheet.wire([(vcc8[0] - 280, vcc8[1]), vcc8], "v33")
    gnd8 = u8.pin("GND")
    sheet.gnd(gnd8[0] - 280, gnd8[1], "STAR_GND")
    sheet.wire([(gnd8[0] - 280, gnd8[1]), gnd8], "gnd")

    sheet.testpoint(700, u8.pin("SCL")[1] - 44, 28, anchor=(680, u8.pin("SCL")[1]))

    sheet.panel(70, 2340, 1290, panel_height(7), "EKRAN KURALLARI — ADR-0017")
    sheet.panel_body(70, 2340, [
        "Modül 5 V TOLERANSLI DEĞİLDİR. Üstünde LDO olan varyantta bile lojik 3,3 V'tur.",
        "TEZGÂH KABLOLAMASI: sinyaller doğrudan ESP'ye bağlanır. Ürün kablolamasında ESP ucuna 4 × 33 Ω seri direnç girer — uçan kablolarda kenar hızını yavaşlatır ve analog ses yolunun yanından geçerler.",
        "R_CS (10 kΩ) tezgâhta DNP: GPIO39 zayıf dahili pull-up ile açılıyor ve açılış penceresini o kapatıyor. Üründe takılır; doğrulanmamış bir dahili değere güvenmek kalıcı bir çözüm değil.",
        "Panel beslemesi ESP modülünün 3V3'ü değil, MP1584 rayından ayrı bir LDO'dur: panelin anahtarlama yükü BATT_SENSE/NTC_SENSE'in referans aldığı rayı bozmasın (G3'te ölçülür).",
        "Bu varyantta arka ışık ucu YOKTUR: VCC verildiği anda yanar. Firmware çalışmadan önce ekranı karartmanın yolu yok; parlaklık ayarı da yok.",
        "GPIO39-41 (MTCK/MTDO/MTDI) pad-JTAG'i tüketir. Harici prob ile hata ayıklama kalmaz; GPIO19/20'deki USB Serial/JTAG durur. GPIO42 serbest kaldı.",
        ("Panel + arka ışık için bütçe ≤50 mA, bölünmemiş tek yük. Ölçülmedi.", "panel-warn"),
    ])

    # ================================================================ ZONE I
    sheet.zone(1410, 1900, 1240, 728, "I", "INA219 ŞARJ AKIMI ÖLÇÜMÜ — ADR-0018")

    rs1 = sheet.block("RS1", "ŞÖNT", "0,05 Ω · %1 · 1 W · yüksek taraf", 1560, 1962, 250,
                      left=["IN+"], right=["IN−"])
    sheet.net_flag(rs1.pin("IN+")[0] - 150, rs1.pin("IN+")[1], "CHG_16V8_RAW", "R")
    sheet.wire([(rs1.pin("IN+")[0] - 150, rs1.pin("IN+")[1]), rs1.pin("IN+")], "chg")
    sheet.wire([rs1.pin("IN−"), (rs1.pin("IN−")[0] + 150, rs1.pin("IN−")[1])], "chg")
    sheet.net_flag(rs1.pin("IN−")[0] + 150, rs1.pin("IN−")[1], "CHG_16V8", "R")
    sheet.testpoint(rs1.pin("IN−")[0] + 80, rs1.pin("IN−")[1] - 44,
                    29, anchor=(rs1.pin("IN−")[0] + 80, rs1.pin("IN−")[1]))

    u9 = sheet.block("U9", "INA219", "adres 0x40 · A0/A1 → GND", 1620, 2110, 300,
                     left=["VIN+", "VIN−", "VS", "GND"], right=["SDA", "SCL"],
                     note="VBUS PİNİ YOK — paket gerilimini bu parça ÖLÇMEZ")
    sheet.wire([(rs1.pin("IN+")[0] + 40, rs1.pin("IN+")[1]),
                (rs1.pin("IN+")[0] + 40, u9.pin("VIN+")[1]), u9.pin("VIN+")], "sig")
    sheet.junction(rs1.pin("IN+")[0] + 40, rs1.pin("IN+")[1])
    sheet.wire([(rs1.pin("IN−")[0] - 40, rs1.pin("IN−")[1]),
                (rs1.pin("IN−")[0] - 40, u9.pin("VIN−")[1]), u9.pin("VIN−")], "sig")
    sheet.junction(rs1.pin("IN−")[0] - 40, rs1.pin("IN−")[1])
    sheet.power_port(u9.pin("VS")[0] - 70, u9.pin("VS")[1], "+3V3")
    sheet.wire([(u9.pin("VS")[0] - 70, u9.pin("VS")[1]), u9.pin("VS")], "v33")
    sheet.gnd(u9.pin("GND")[0] - 70, u9.pin("GND")[1], "STAR_GND")
    sheet.wire([(u9.pin("GND")[0] - 70, u9.pin("GND")[1]), u9.pin("GND")], "gnd")
    for pin_name, flag in (("SDA", "I2C_SDA"), ("SCL", "I2C_SCL")):
        point = u9.pin(pin_name)
        sheet.wire([point, (point[0] + 60, point[1])], "dig")
        sheet.net_flag(point[0] + 60, point[1], flag, "R")

    sheet.panel(1410, 2340, 1240, panel_height(9), "AKIM SENSÖRÜ KURALLARI — ADR-0018", "danger")
    sheet.panel_body(1410, 2340, [
        ("PAKET GERİLİMİNİN KAYNAĞI BU PARÇA DEĞİLDİR", "panel-warn"),
        ("INA219'un VBUS pini yoktur; gerilimi şöntün YÜK tarafından ölçer, yani yüksek", "panel-text"),
        ("tarafta paket gerilimi eksi kendi burden düşümünü verir. Hata makul kalır, o", "panel-text"),
        ("yüzden hk_power'ın imkânsız-okuma koruması yakalayamaz. pack_mv yalnız", "panel-text"),
        ("GPIO1/BATT_SENSE bölücüsünden gelir.", "panel-text"),
        ("", "panel-text"),
        ("Ortak mod 26 V (INA226'da 40 V): XL4015'in 20 V GİRİŞ tarafına asla konmaz.", "panel-text"),
        ("Bağlamadan önce XL4015 çıkışı, sensör takılı değilken, 16,80 V'ta doğrulanır.", "panel-text"),
        ("INA219 ve INA226 aynı adresten cevap verir; 0xFE okunarak ayırt edilir.", "panel-text"),
    ])

    # ============================================================== PANELS
    sheet.panel(930, 1470, 830, 380, "GÜVENLİK VE ÖLÇÜM KURALLARI", "danger")
    sheet.panel_body(930, 1470, [
        ("BTL ÇIKIŞA ŞASE KLİPSİ TAKMA", "panel-warn"),
        ("L− ve R− hoparlör ekseni değil, Class-D yarım köprü çıkışıdır. TP18–TP21", "panel-text"),
        ("uçlarının hiçbirine osiloskop GND klipsi bağlanmaz. Diferansiyel prob kullan;", "panel-text"),
        ("yoksa iki 10× prob, her iki GND klipsi yalnız TP6'ya, MATH = CH1 − CH2.", "panel-text"),
        ("", "panel-text"),
        ("ENERJİ VERME SIRASI", "panel-warn"),
        ("S1 XH-A232 + dummy-load (akım sınırlı lab kaynağı)  →  S2 ESP32 + buck  →", "panel-mono"),
        ("S3 I²S zinciri + dummy-load  →  S4 woofer düşük seviye  →  S5 tweeter +", "panel-mono"),
        ("C_SAFE çok düşük seviye  →  S6 batarya kabin dışında  →  S7 tam prototip.", "panel-mono"),
        ("", "panel-text"),
        ("C_SAFE tek başına crossover değildir; DSP HPF ve limiter'a karşı son savunmadır.", "panel-text"),
        ("Değeri C = 1 / (2π · R_tweeter · f_safe) ile G2 raporundan gelir.", "panel-text"),
        ("V1'de şarj sırasında amfi kapalıdır (ADR-0004).", "panel-text"),
    ])

    sheet.panel(1790, 1470, 860, 380, "TEST NOKTASI İNDEKSİ — TP0…TP31")
    tp_rows = [
        "TP0  paket B−        TP7  F1 sonrası        TP14 DAC LOUT       TP21 XH R−",
        "TP1  hücre 1 / B1    TP8  VBAT_SW           TP15 DAC ROUT       TP22 buton GPIO7",
        "TP2  hücre 2 / B2    TP9  MP1584 5,10 V     TP16 amfi L IN      TP23 LED_R sürüş",
        "TP3  hücre 3 / B3    TP10 ESP32 3V3         TP17 amfi R IN      TP24 LED_G sürüş",
        "TP4  paket B+        TP11 I²S BCLK          TP18 XH L+          TP25 LED_B sürüş",
        "TP5  BMS P+          TP12 I²S LRCLK         TP19 XH L−          TP26 CHG+ 16,80 V",
        "TP6  BMS P− / GND    TP13 I²S DATA          TP20 XH R+          TP27 NTC uçları",
        "TP28 LCD SCL         TP29 şönt CHG+       TP30 DAC XSMT       TP31 amfi SD ADAY",
    ]
    sheet.panel_body(1790, 1470, [(row, "panel-mono") for row in tp_rows] + [
        "",
        "Güç açma/kapatma kaydı — CH1 TP8 · CH2 TP9 · CH3 TP10 · CH4 TP14/TP15.",
        "Ripple ölçümünde 10× prob, ground-spring ve 20 MHz bant sınırı kullanılır.",
        "Beklenen değer ve geçiş şartları: docs/02-hardware/circuit-and-wiring-plan.md §7",
    ])

    # The mute chain gets its own full-width strip rather than a line inside one
    # of the zones. It is the only safety layer that exists before firmware runs,
    # and the sheet was contradicting itself about it until 2026-09-08.
    sheet.panel(70, 2650, 2580, panel_height(4), "SUSTURMA HATLARI — ADR-0011", "danger")
    sheet.panel_body(70, 2650, [
        ("SUSTURMAYI TUTAN ŞEY GPIO DEĞİL, DİRENÇTİR", "panel-warn"),
        ("R6 ve R7 (10 kΩ pull-down) opsiyonel değildir. Bu parçadaki her aday GPIO reset'ten yüksek empedanslı "
         "çıkar ve ROM, bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce ms. Firmware'in işi "
         "susturmayı BIRAKMAKTIR; firmware hiç çalışmazsa hoparlörler sessiz kalır.", "panel-text"),
        ("OPERATÖR, LEHİMDEN ÖNCE: (1) PCM5102A XSMT pad'i ↔ 3V3 direncini ölç, sert köprü varsa kes. "
         "(2) XH-A232'de erişilebilir SD pad'i var mı, süreklilikle ara. (3) Açılışta TP30 ve TP31 LOW mu, "
         "osiloskopla kaydet.", "panel-text"),
        ("Bu üç ölçüm kaydedilmeden susturma katmanı DOĞRULANMAMIŞTIR; empedansı ölçülmemiş sürücülere "
         "sinyal verilmez.", "panel-warn"),
    ])

    # gates + legend + title block
    #
    # Anchored to the bottom of the sheet rather than to fixed numbers. The
    # first time the sheet grew, these stayed where they were and two new zones
    # landed on top of the title block.
    strip = H - 190
    sheet.panel(70, strip, 1690, 92, "ENERJİ VERME ÖNCESİ ZORUNLU KAPILAR", "gate")
    sheet.panel_line(90, strip + 46, "G0 sürücü DC direnci / empedans / polarite   ·   G1 amfi + 8 Ω ≥50 W non-inductive dummy-load   ·   "
                               "G2 tweeter HPF + limiter + C_SAFE   ·   G3 güç, ripple, brownout, EMI, pop   ·   G4 batarya, BMS, şarj, sonlandırma, NTC",
                     "panel-mono")
    sheet.panel_line(90, strip + 72, "Fiziksel ölçüm kaydı olmadan hiçbir kapı PASS yapılamaz. G0–G5 geçmeden dört üniteye çoğaltma yoktur.", "panel-warn")

    sheet.panel(1790, strip, 860, 92, "", "title")

    # The legend sits on its own strip below both panels. Sharing the title
    # block's rectangle would have hidden it: panels paint an opaque fill and
    # render in append order.
    legend = [("pwr", "batarya / VBAT"), ("chg", "şarj"), ("v5", "5 V"), ("v33", "3V3"),
              ("dig", "dijital / I²S"), ("aud", "analog ses"), ("btl", "BTL çıkış"), ("gnd", "toprak")]
    sheet.overlay.append(f'<text class="panel-title" x="70" y="{strip + 130}">GÖSTERİM</text>')
    for index, (kind, name) in enumerate(legend):
        x = 210 + index * 305
        sheet.overlay.append(f'<line class="w {kind}" x1="{x}" y1="{strip + 125}" x2="{x + 46}" y2="{strip + 125}"/>'
                             f'<text class="legend" x="{x + 56}" y="{strip + 130}">{escape(name)}</text>')
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
                        default=Path(__file__).resolve().parents[2] / "docs/02-hardware/assets/harman-kardom-schematic.svg")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(build().render(CSS), encoding="utf-8")
    print(f"Generated: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
