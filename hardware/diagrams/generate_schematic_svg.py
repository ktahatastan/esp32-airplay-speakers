#!/usr/bin/env python3
"""Generate the single Merzarkabul Airplay Speakers documentation schematic sheet.

One speaker, module level. The four speakers repeat the same circuit.

This sheet is the readable overview: functional zones, real symbols, orthogonal
wires, junction dots, net flags for cross-zone nets, and the TP0-TP21 probe
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

W, H = 2720, 2290
REV = "P3"
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


def build() -> Sheet:
    sheet = Sheet(
        W, H,
        "Merzarkabul Airplay Speakers — tek hoparlör modül seviyesi devre şeması",
        "19 V DC adaptör girişi, ters polarite adayı ve bulk kondansatör, 5 V lojik beslemesi, "
        "ESP32-S3 N16R8, PCM5102A I2S DAC, XH-A232 BTL bi-amp, GPIO13/GPIO21 susturma hatları ve "
        "harici pull-down'ları, woofer ve C_SAFE korumalı tweeter, "
        "kullanıcı arayüzü ve TP0-TP21 test noktaları.",
    )
    frame(sheet)
    sheet.background.append(
        f'<text class="sheet-title" x="70" y="86">MERZARKABUL · TEK HOPARLÖR DEVRE ŞEMASI</text>'
        f'<text class="sheet-sub" x="70" y="108">Modül seviyesi prototip · dört kutuda aynı devre tekrarlanır · '
        f'&#171;ADAY / TBD&#187; değerler ölçümle kilitlenir · üretim PCB şeması değildir</text>')

    # ================================================================ ZONE A
    sheet.zone(70, 130, 2580, 400, "A", "19 V DC GİRİŞ, TERS POLARİTE ADAYI VE BULK KONDANSATÖR — ADR-0020")
    j1 = sheet.block("J1", "DC GİRİŞ JAKI", "5,5 × 2,1 mm · merkez pozitif · 19 V adaptör", 100, 200, 300,
                     right=["+19 V", "GND"])
    jack_plus, jack_gnd = j1.pin("+19 V"), j1.pin("GND")
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
    ca_top, ca_bot = sheet.cap_v(800, jack_plus[1], "C_A", "1000 µF / 25 V", polarized=True)
    sheet.junction(800, jack_plus[1])
    sheet.gnd(ca_bot[0], ca_bot[1])
    # Ground leaves the jack on its own row and drops to the POWER_GND symbol
    # left of D2, so nothing crosses the +19 V run.
    sheet.wire([jack_gnd, (460, jack_gnd[1]), (460, 380)], "gnd")
    sheet.testpoint(441, jack_gnd[1], 2)
    sheet.gnd(460, 380, "POWER_GND")
    sheet.netlabel(100, 470, "D2 takılmazsa yerine 0 Ω köprü gelir; DC_IN ile VIN o zaman tek nettir.", "start", 0)
    sheet.netlabel(100, 494, "V1'de güç anahtarı yok: cihaz adaptör çekilerek kapanır, boşta bekleme firmware'in idle standby'ıdır.", "start", 0)

    sheet.panel(1000, 180, 1620, panel_height(7), "GÜÇ GİRİŞİ KURALLARI — ADR-0020")
    sheet.panel_body(1000, 180, [
        ("JAK POLARİTESİ İLK ENERJİLENDİRMEDEN ÖNCE ÖLÇÜ ALETİYLE DOĞRULANIR: merkez pozitif. Etiket okumak ölçüm değildir (G1 satırı).", "panel-warn"),
        "19 V iki giriş penceresinin de içindedir: TPA3110 8–26 V, MP1584 4,5–28 V. VIN amfiyi doğrudan besler; arada regülatör yoktur.",
        "İlk enerjilendirme adaptörle değil, akım sınırlı laboratuvar kaynağıyla yapılır. Adaptörün akım sınıfı G1'de ölçülen tepe akımdan gelir; varsayılmaz.",
        "D2 adaydır: Schottky, ideal-diyot modülü ya da 0 Ω köprü. Kararı G1'de TP0−TP1 düşümü ve ısı verir.",
        "Adaptör çıkışı koruma toprağına bağlı olabilir. Adaptörle osiloskop bağlamadan önce PE/izolasyon ilişkisi ölçülür; belirsizse scope bağlanmaz.",
        "Kapanış adaptör çekilerek olur; kapanış pop kaydı da öyle alınır. VIN çökerken amfi 8 V altında kendi kilidiyle susar, buck 4,5 V girişe kadar 5 V verir.",
        ("TPA3110D2'nin 19 V'ta 4 Ω sınıfı sürücülere vereceği güç veri sayfasından değil G1 dummy-load ölçümünden yazılır.", "panel-warn"),
    ])

    # ================================================================ ZONE B
    sheet.zone(70, 630, 590, 800, "B", "5 V LOJİK BESLEMESİ")
    u4 = sheet.block("U4", "MP1584 BUCK", "yüksüz 5,10 V'a ayarla", 210, 700, 230,
                     left=["IN+", "IN−"], right=["OUT+", "OUT−"])
    sheet.net_flag(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1], "VIN", "L")
    sheet.wire([(u4.pin("IN+")[0] - 30, u4.pin("IN+")[1]), u4.pin("IN+")], "pwr")
    sheet.gnd(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1], "POWER_GND")
    sheet.wire([(u4.pin("IN−")[0] - 40, u4.pin("IN−")[1]), u4.pin("IN−")], "gnd")
    out = u4.pin("OUT+")
    jp_a, jp_b = sheet.switch(490, out[1], "", "JP1 · SERVİS AYIRMA")
    sheet.wire([out, jp_a], "v5")
    sheet.testpoint(474, out[1] + 52, 3, anchor=(474, out[1]))
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
        ("TP3 hedefi: normal yükte ≤50 mVpp;", "panel-mono"),
        ("Wi-Fi sıçramasında 5 V hattı 4,75 V", "panel-mono"),
        ("altına düşmemeli.", "panel-mono"),
        "",
        ("TP4 hedefi: brownout/reset üreten", "panel-mono"),
        ("çökme yok. Alt sınır G1'de kilitlenir.", "panel-mono"),
    ])

    # ================================================================ ZONE C
    sheet.zone(680, 630, 510, 800, "C", "ESP32-S3 N16R8 — ADR-0010")
    u5 = sheet.block("U5", "ESP32-S3 DEVKIT", "16 MB flash + 8 MB PSRAM", 830, 700, 330,
                     left=["5V / VBUS", "GND", "3V3", "GPIO7  BUTTON", "GPIO8  LED_R",
                           "GPIO9  LED_G", "GPIO10 LED_B"],
                     # DAC_XSMT sits one row above AMP_MUTE so that it lands on the
                     # same row as U6's XSMT pin: the mute net is then a straight
                     # wire, and the long AMP_MUTE run leaves from the bottom row
                     # where it has a clear corridor under zone D.
                     right=["GPIO4  BCLK", "GPIO5  LRCLK", "GPIO6  DATA",
                            "GPIO13 DAC_XSMT", "GPIO21 AMP_MUTE"],
                     note="GPIO ataması ADAY · kart şeması ve boot testi olmadan accepted değil")
    v5 = u5.pin("5V / VBUS")
    sheet.net_flag(v5[0] - 30, v5[1], "+5V_LOGIC", "L")
    sheet.wire([(v5[0] - 30, v5[1]), v5], "v5")
    gnd5 = u5.pin("GND")
    sheet.gnd(gnd5[0] - 148, gnd5[1], "STAR_GND")
    sheet.wire([(gnd5[0] - 148, gnd5[1]), gnd5], "gnd")
    v33 = u5.pin("3V3")
    sheet.wire([v33, (v33[0] - 68, v33[1])], "v33")
    sheet.testpoint(v33[0] - 22, v33[1], 4)
    sheet.power_port(v33[0] - 68, v33[1], "+3V3")
    for pin_name, flag in (("GPIO7  BUTTON", "BUTTON_N"), ("GPIO8  LED_R", "LED_R"),
                           ("GPIO9  LED_G", "LED_G"), ("GPIO10 LED_B", "LED_B")):
        point = u5.pin(pin_name)
        sheet.wire([point, (point[0] - 30, point[1])], "dig")
        sheet.net_flag(point[0] - 30, point[1], flag, "L")

    # ================================================================ ZONE D
    sheet.zone(1210, 630, 580, 800, "D", "PCM5102A I²S DAC")
    # XSMT is the fourth row so that it shares a row with U5's GPIO13 pin: the
    # DAC mute is then one straight wire, and the AMP_MUTE corridor leaving from
    # the row below it never has to cross it. The block starts at x=1400 and is
    # 330 wide so that its ground symbols clear R6, which hangs from the XSMT
    # wire at x=1234.
    u6 = sheet.block("U6", "PCM5102A MODÜLÜ", "3-wire I²S · modül köprüleri doğrulanacak",
                     1400, 700, 330,
                     left=["BCK", "LCK / LRCK", "DIN", "XSMT", "SCK", "VIN 5 V", "GND / AGND"],
                     right=["LOUT", "ROUT", "AGND"])
    # I2S: the DAC input pins sit on the same rows as the ESP32 outputs, so each
    # clock is a single straight wire with nothing to cross.
    for source, target, tp, tp_x in (("GPIO4  BCLK", "BCK", 5, 1230),
                                     ("GPIO5  LRCLK", "LCK / LRCK", 6, 1270),
                                     ("GPIO6  DATA", "DIN", 7, 1310)):
        a, b = u5.pin(source), u6.pin(target)
        sheet.wire([a, b], "dig")
        sheet.testpoint(tp_x, a[1], tp)
    # Short stub: R6's value text ends just left of this ground symbol.
    sck = u6.pin("SCK")
    sheet.wire([sck, (sck[0] - 26, sck[1])], "gnd")
    sheet.gnd(sck[0] - 26, sck[1])
    # Routed down to a rail symbol under the block: a flag on this row would
    # sit between R6's ground and the SCK ground with no room to spare.
    vin6 = u6.pin("VIN 5 V")
    sheet.wire([vin6, (1300, vin6[1]), (1300, 1040)], "v5")
    sheet.power_port(1300, 1040, "+5V_LOGIC", "down")
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
    sheet.testpoint(1300, xsmt_dac[1], 20)
    r6_t, r6_b = sheet.resistor_v(1234, xsmt_dac[1], "R6", "10 kΩ")
    sheet.junction(1234, xsmt_dac[1])
    sheet.gnd(r6_b[0], r6_b[1], "STAR_GND")

    sheet.netlabel(1230, 1200, "FMT=LOW · FLT=LOW · DEMP=LOW · SCK→GND (yalnız 3-wire BCK-PLL modu)", "start", 0)
    sheet.netlabel(1230, 1224, "XSMT modül varsayılanına BIRAKILMAZ: GPIO13 sürer, R6 kapalı tutar.", "start", 0)
    sheet.netlabel(1230, 1248, "LEHİMDEN ÖNCE ÖLÇ: XSMT pad'i ↔ 3V3 direnci. Sert köprü / 0 Ω varsa", "start", 0)
    sheet.netlabel(1230, 1272, "kesilmeden GPIO13 bağlanmaz; pin LOW sürerken 3V3 rayına kısa devredir.", "start", 0)
    sheet.netlabel(1230, 1296, "Modül köprüleri satıcıya göre değişir; pad ismine bakıp lehim yapılmaz.", "start", 0)

    # ================================================================ ZONE E
    sheet.zone(1810, 630, 850, 800, "E", "XH-A232 / TPA3110 BTL Bİ-AMP VE SÜRÜCÜLER")
    # `SD` is a real TPA3110 pin. What is NOT confirmed is whether the XH-A232
    # board brings it out to a pad anyone can solder to, so the pin is labelled
    # ADAY and everything hanging off it is drawn dashed.
    u7 = sheet.block("U7", "XH-A232 / TPA3110", "2 × BTL Class-D · 8–26 V", 1960, 700, 330,
                     left=["L IN", "R IN", "VCC", "GND", "SD  PAD ADAY"],
                     right=["L+", "L−", "R+", "R−"])
    for source, target, tp_dac, tp_amp in (("LOUT", "L IN", 8, 10), ("ROUT", "R IN", 9, 11)):
        a, b = u6.pin(source), u7.pin(target)
        sheet.wire([a, b], "aud")
        sheet.testpoint(1800, a[1], tp_dac)
        sheet.testpoint(1890, a[1], tp_amp)
    vcc7 = u7.pin("VCC")
    sheet.net_flag(vcc7[0] - 30, vcc7[1], "VIN", "L")
    sheet.wire([(vcc7[0] - 30, vcc7[1]), vcc7], "pwr")
    gnd7 = u7.pin("GND")
    sheet.wire([gnd7, (gnd7[0] - 44, gnd7[1])], "gnd")
    sheet.gnd(gnd7[0] - 44, gnd7[1], "POWER_GND")
    sheet.netlabel(2000, 990, "DAC ↔ amfi kablosu kısa ve ekranlı", "start", 0)
    sheet.netlabel(2000, 1014, "hoparlör çıkış kablosuyla paralel gitmez", "start", 0)

    # Amplifier mute. Dashed for its whole length, R7 included: this branch is a
    # RESERVATION (ADR-0011). It is fitted only if an accessible SD pad is found
    # on the XH-A232, which is still an open decision in the wiring plan §9. The
    # corridor runs under zone D because the ESP is in zone C and the amp in E.
    sd_pin = u7.pin("SD  PAD ADAY")
    mute_esp = u5.pin("GPIO21 AMP_MUTE")
    sheet.wire([mute_esp, (1196, mute_esp[1]), (1196, 1150), (sd_pin[0], 1150), sd_pin], "dig dnp")
    sheet.testpoint(sd_pin[0], 1010, 21)
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
    sheet.testpoint(2370, lp[1], 12)
    sheet.testpoint(2410, lm[1], 13)

    rp, rm = u7.pin("R+"), u7.pin("R−")
    tweeter_y = 1290
    sheet.wire([rp, (2350, rp[1]), (2350, tweeter_y), (2360, tweeter_y)], "btl")
    csa, csb = sheet.cap_h(2360, tweeter_y, "C_SAFE", "")
    sheet.netlabel(2392, tweeter_y, "kutupsuz film · DEĞER TBD", "middle", -46)
    twe_p, twe_m = sheet.speaker(2450, tweeter_y + 16, "SPK2", "TWEETER", "Ω TBD · G2 bekliyor")
    sheet.wire([csb, twe_p], "btl")
    sheet.wire([rm, (2330, rm[1]), (2330, twe_m[1]), twe_m], "btl")
    sheet.testpoint(2350, 990, 14)
    sheet.testpoint(2330, 1080, 15)

    # ================================================================ ZONE F
    sheet.zone(70, 1470, 830, 380, "F", "KULLANICI ARAYÜZÜ")
    sheet.power_port(160, 1556, "+3V3")
    rpu_t, rpu_b = sheet.resistor_v(160, 1556, "R_PU", "10 kΩ ADAY")
    btn_node = rpu_b
    sheet.wire([btn_node, (160, 1650)], "sig")
    sw_t, sw_b = sheet.pushbutton_v(160, 1650, "SW1", "FONKSİYON / RESET")
    sheet.gnd(sw_b[0], sw_b[1], "STAR_GND")
    sheet.wire([btn_node, (400, btn_node[1])], "dig")
    sheet.junction(160, btn_node[1])
    sheet.testpoint(280, btn_node[1], 16)
    cdb_t, cdb_b = sheet.cap_v(340, btn_node[1], "C_DB", "100 nF OPSİYONEL")
    sheet.junction(340, btn_node[1])
    sheet.gnd(cdb_b[0], cdb_b[1])
    sheet.net_flag(400, btn_node[1], "BUTTON_N", "R")

    led_y_top = 1556
    cathode_y = 0.0
    for offset, (flag, ref, value, colour, tp) in enumerate((
        ("LED_R", "R_R", "680 Ω", "#fecaca", 17),
        ("LED_G", "R_G", "330 Ω", "#bbf7d0", 18),
        ("LED_B", "R_B", "330 Ω", "#bfdbfe", 19),
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

    # ============================================================== PANELS
    sheet.panel(930, 1470, 830, 380, "GÜVENLİK VE ÖLÇÜM KURALLARI", "danger")
    sheet.panel_body(930, 1470, [
        ("BTL ÇIKIŞA ŞASE KLİPSİ TAKMA", "panel-warn"),
        ("L− ve R− hoparlör ekseni değil, Class-D yarım köprü çıkışıdır. TP12–TP15", "panel-text"),
        ("uçlarının hiçbirine osiloskop GND klipsi bağlanmaz. Diferansiyel prob kullan;", "panel-text"),
        ("yoksa iki 10× prob, her iki GND klipsi yalnız TP2'ye, MATH = CH1 − CH2.", "panel-text"),
        ("", "panel-text"),
        ("ENERJİ VERME SIRASI", "panel-warn"),
        ("S1 XH-A232 + dummy-load (akım sınırlı lab kaynağı)  →  S2 ESP32 + buck  →", "panel-mono"),
        ("S3 I²S zinciri + dummy-load  →  S4 woofer düşük seviye  →  S5 tweeter +", "panel-mono"),
        ("C_SAFE çok düşük seviye  →  S6 tam prototip, 19 V adaptörle  →  S7 dört tekrar.", "panel-mono"),
        ("", "panel-text"),
        ("C_SAFE tek başına crossover değildir; DSP HPF ve limiter'a karşı son savunmadır.", "panel-text"),
        ("Değeri C = 1 / (2π · R_tweeter · f_safe) ile G2 raporundan gelir.", "panel-text"),
        ("Jak polaritesi ölçülmeden ve D2 kararı verilmeden adaptör takılmaz (ADR-0020).", "panel-text"),
    ])

    sheet.panel(1790, 1470, 860, 380, "TEST NOKTASI İNDEKSİ — TP0…TP21")
    # One label per probe point, in TP order, laid out column-major with each
    # column as its own text element. Padding with spaces does not work: SVG
    # text collapses runs of whitespace, so a padded table never lines up.
    tp_labels = [
        "jak + / DC_IN", "VIN (D2 sonrası)", "jak − / GND", "MP1584 5,10 V", "ESP32 3V3",
        "I²S BCLK", "I²S LRCLK", "I²S DATA", "DAC LOUT", "DAC ROUT", "amfi L IN", "amfi R IN",
        "XH L+", "XH L−", "XH R+", "XH R−", "buton GPIO7", "LED_R sürüş", "LED_G sürüş",
        "LED_B sürüş", "DAC XSMT", "amfi SD ADAY",
    ]
    per_column, column_pitch = 6, 205
    baseline = 1470 + Sheet.PANEL_BODY_TOP
    for row in range(per_column):
        for column, number in enumerate(range(row, len(tp_labels), per_column)):
            sheet.panel_line(1790 + 20 + column * column_pitch, baseline,
                             f"TP{number} {tp_labels[number]}", "panel-mono")
        baseline += Sheet.PANEL_LINE
    for line in ("",
                 "Güç açma/kapatma kaydı — CH1 TP1 · CH2 TP3 · CH3 TP4 · CH4 TP8/TP9.",
                 "Ripple ölçümünde 10× prob, ground-spring ve 20 MHz bant sınırı kullanılır.",
                 "Beklenen değer ve geçiş şartları: docs/02-hardware/circuit-and-wiring-plan.md §7"):
        if line:
            sheet.panel_line(1790 + 20, baseline, line)
        baseline += Sheet.PANEL_LINE

    # The mute chain gets its own full-width strip rather than a line inside one
    # of the zones. It is the only safety layer that exists before firmware runs,
    # and the sheet was contradicting itself about it until 2026-09-08.
    sheet.panel(70, 1900, 2580, panel_height(4), "SUSTURMA HATLARI — ADR-0011", "danger")
    sheet.panel_body(70, 1900, [
        ("SUSTURMAYI TUTAN ŞEY GPIO DEĞİL, DİRENÇTİR", "panel-warn"),
        ("R6 ve R7 (10 kΩ pull-down) opsiyonel değildir. Bu parçadaki her aday GPIO reset'ten yüksek empedanslı "
         "çıkar ve ROM, bootloader ve uygulama başlangıcı boyunca öyle kalır — yüzlerce ms. Firmware'in işi "
         "susturmayı BIRAKMAKTIR; firmware hiç çalışmazsa hoparlörler sessiz kalır.", "panel-text"),
        ("OPERATÖR, LEHİMDEN ÖNCE: (1) PCM5102A XSMT pad'i ↔ 3V3 direncini ölç, sert köprü varsa kes. "
         "(2) XH-A232'de erişilebilir SD pad'i var mı, süreklilikle ara. (3) Açılışta TP20 ve TP21 LOW mu, "
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
    sheet.panel_line(90, strip + 46, "G0 sürücü DC direnci / empedans / polarite   ·   G1 amfi + 8 Ω ≥50 W non-inductive dummy-load, jak polaritesi, "
                               "besleme çöküşü, brownout, pop   ·   G2 tweeter HPF + limiter + C_SAFE",
                     "panel-mono")
    sheet.panel_line(90, strip + 72, "Fiziksel ölçüm kaydı olmadan hiçbir kapı PASS yapılamaz. G0–G2 geçmeden dört üniteye çoğaltma yoktur.", "panel-warn")

    sheet.panel(1790, strip, 860, 92, "", "title")

    # The legend sits on its own strip below both panels. Sharing the title
    # block's rectangle would have hidden it: panels paint an opaque fill and
    # render in append order.
    legend = [("pwr", "19 V DC / VIN"), ("v5", "5 V"), ("v33", "3V3"),
              ("dig", "dijital / I²S"), ("aud", "analog ses"), ("btl", "BTL çıkış"), ("gnd", "toprak")]
    sheet.overlay.append(f'<text class="panel-title" x="70" y="{strip + 130}">GÖSTERİM</text>')
    for index, (kind, name) in enumerate(legend):
        x = 210 + index * 340
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
                        default=Path(__file__).resolve().parents[2] / "docs/02-hardware/assets/merzarkabul-schematic.svg")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(build().render(CSS), encoding="utf-8")
    print(f"Generated: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
