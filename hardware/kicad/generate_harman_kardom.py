#!/usr/bin/env python3
"""Generate the Harman Kardom module-level KiCad 10 schematic.

Purchased modules are modeled as connectors: pin order is a logical design
contract, not a claim about a seller board's physical header order. Verify
every module silkscreen and datasheet before wiring. This is a prototype
interconnect drawing, not a production PCB design.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import kicad_sch_api as ksa

ROOT = Path(__file__).resolve().parent
DEFAULT_OUTPUT = ROOT / "generated"


@dataclass(frozen=True)
class Module:
    reference: str
    value: str
    position: tuple[float, float]
    pins: tuple[str, ...]


MODULES = (
    Module("J1", "USB-C PD INPUT", (35, 48), ("USB_PD_VBUS", "POWER_GND")),
    Module("U1", "USB-C PD TRIGGER / 20 V", (82, 48), ("USB_PD_VBUS", "POWER_GND", "PD20V", "POWER_GND")),
    Module("U2", "XL4015 CC/CV / 16.80 V 2.00 A", (137, 48), ("PD20V", "POWER_GND", "CHG_16V8", "CHG_NEG")),
    Module("U3", "4S BALANCED BMS / COMMON PORT CANDIDATE", (244, 48), ("BMINUS", "B1", "B2", "B3", "BPLUS", "PACK_POS", "PACK_NEG", "NTC_BMS")),
    Module("U4", "MP1584 / ADJUST TO 5.10 V", (72, 105), ("VBAT_SW", "POWER_GND", "V5_SYS", "STAR_GND")),
    Module("JP1", "USB-SYSTEM 5 V ISOLATION", (125, 105), ("V5_SYS", "V5_LOGIC")),
    Module("U5", "ESP32-S3 N16R8 DEV BOARD", (237, 109), (
        "V5_LOGIC", "STAR_GND", "ESP_3V3", "I2S_BCLK", "I2S_LRCLK", "I2S_DATA",
        "BUTTON_N", "LED_R", "LED_G", "LED_B", "I2C_SDA", "I2C_SCL",
        "AMP_MUTE_TBD", "USB_DN_TBD", "USB_DP_TBD", "NC_RESERVED",
    )),
    Module("D1", "COMMON-CATHODE RGB LED", (327, 107), ("LED_R", "LED_G", "LED_B", "STAR_GND")),
    Module("U6", "PCM5102A I2S DAC MODULE", (90, 179), (
        "V5_LOGIC", "STAR_GND", "I2S_BCLK", "I2S_LRCLK", "I2S_DATA", "SCK_GND",
        "FMT_LOW", "XSMT_HIGH", "DAC_LOUT", "DAC_AGND", "DAC_ROUT", "NC_DAC",
    )),
    Module("U7", "XH-A232 / TPA3110 BTL AMP", (203, 179), (
        "VBAT_SW", "POWER_GND", "DAC_LOUT", "DAC_AGND", "DAC_ROUT",
        "AMP_L_PLUS", "AMP_L_MINUS", "AMP_R_PLUS", "AMP_R_MINUS", "AMP_SD_TBD",
    )),
    Module("J2", "WOOFER / IMPEDANCE TBD", (279, 166), ("AMP_L_PLUS", "AMP_L_MINUS")),
    Module("J3", "TWEETER / IMPEDANCE TBD", (344, 184), ("TWEETER_POS", "AMP_R_MINUS")),
    Module("JTP1", "TP0-TP9 POWER", (88, 246), ("BMINUS", "B1", "B2", "B3", "BPLUS", "PACK_POS", "PACK_NEG", "FUSED_POS", "VBAT_SW", "V5_SYS")),
    Module("JTP2", "TP10-TP17 SIGNAL", (195, 246), ("ESP_3V3", "I2S_BCLK", "I2S_LRCLK", "I2S_DATA", "DAC_LOUT", "DAC_ROUT", "BUTTON_N", "LED_R")),
    Module("JTP3", "TP18-TP25 BTL/CHARGE", (297, 246), ("AMP_L_PLUS", "AMP_L_MINUS", "AMP_R_PLUS", "AMP_R_MINUS", "CHG_16V8", "CHG_NEG", "NTC_BMS", "STAR_GND")),
)


def add_module(schematic, module: Module) -> None:
    schematic.components.add(
        lib_id=f"Connector_Generic:Conn_01x{len(module.pins):02d}",
        reference=module.reference,
        value=module.value,
        position=module.position,
    )
    for number, net in enumerate(module.pins, start=1):
        schematic.add_label(net, pin=(module.reference, str(number)), size=1.0)


def add_section(schematic, title: str, start: tuple[float, float], end: tuple[float, float]) -> None:
    schematic.add_rectangle(start, end, stroke_width=0.35)
    schematic.add_text(title, ((start[0] + end[0]) / 2, start[1] + 3), size=2.2, bold=True)


def add_note(schematic, text: str, position: tuple[float, float], *, size: float = 1.27, bold: bool = False) -> None:
    schematic.add_text(text, position, size=size, bold=bold)


def add_series_battery(schematic) -> None:
    for index, y in enumerate((35, 45, 55, 65), start=1):
        schematic.components.add("Device:Battery_Cell", reference=f"BT{index}", value=f"ASPILSAN A28 18650 / CELL {index}", position=(183, y))
    for reference, pin, net in (
        ("BT1", "1", "BPLUS"), ("BT1", "2", "B3"),
        ("BT2", "1", "B3"), ("BT2", "2", "B2"),
        ("BT3", "1", "B2"), ("BT3", "2", "B1"),
        ("BT4", "1", "B1"), ("BT4", "2", "BMINUS"),
    ):
        schematic.add_label(net, pin=(reference, pin), size=1.0)


def add_power_path(schematic) -> None:
    schematic.components.add("Device:Fuse", reference="F1", value="5 A STARTING CANDIDATE", position=(292, 47), rotation=90)
    schematic.components.add("Switch:SW_SPST", reference="S1", value="KM103 / DC-132A CONTACTS / VERIFY 16.8 VDC 5 A", position=(333, 47), rotation=90)
    for reference, pin, net in (
        ("F1", "1", "PACK_POS"), ("F1", "2", "FUSED_POS"),
        ("S1", "1", "FUSED_POS"), ("S1", "2", "VBAT_SW"),
    ):
        schematic.add_label(net, pin=(reference, pin), size=1.0)
    schematic.components.add("Device:LED", reference="D2", value="KM103 INTERNAL 12 V LED", position=(333, 59), rotation=90)
    schematic.add_label("VBAT_SW", pin=("D2", "2"), size=1.0)
    schematic.add_label("SW_LED_RETURN_TBD", pin=("D2", "1"), size=1.0)
    schematic.components.add("Device:R", reference="R2", value="R_SW_LED TBD / DNP", position=(350, 59), rotation=90)
    schematic.add_label("SW_LED_RETURN_TBD", pin=("R2", "1"), size=1.0)
    schematic.add_label("POWER_GND", pin=("R2", "2"), size=1.0)
    schematic.components.add("Device:C_Polarized", reference="C1", value="1000 uF / 25 V LOW-ESR", position=(34, 106))
    schematic.add_label("VBAT_SW", pin=("C1", "1"), size=1.0)
    schematic.add_label("POWER_GND", pin=("C1", "2"), size=1.0)


def add_ui_and_tweeter_safety(schematic) -> None:
    schematic.components.add("Switch:SW_Push", reference="SW1", value="FUNCTION / RESET / PROVISION", position=(286, 105), rotation=90)
    schematic.add_label("BUTTON_N", pin=("SW1", "1"), size=1.0)
    schematic.add_label("STAR_GND", pin=("SW1", "2"), size=1.0)
    schematic.components.add("Device:R", reference="R1", value="10 k PULL-UP", position=(286, 124))
    schematic.add_label("ESP_3V3", pin=("R1", "1"), size=1.0)
    schematic.add_label("BUTTON_N", pin=("R1", "2"), size=1.0)
    schematic.components.add("Device:C", reference="C2", value="C_SAFE / TBD FILM HPF", position=(311, 184), rotation=90)
    schematic.add_label("AMP_R_PLUS", pin=("C2", "1"), size=1.0)
    schematic.add_label("TWEETER_POS", pin=("C2", "2"), size=1.0)


def build_schematic():
    schematic = ksa.create_schematic("harman-kardom")
    schematic.set_paper_size("A3")
    schematic.set_title_block(
        title="Harman Kardom - Module-Level Prototype", date="2026-08-30",
        rev="0.5-candidate", company="Harman Kardom",
        comments={1: "4S Li-ion / USB-C PD / ESP32-S3 / PCM5102A / XH-A232",
                  2: "DO NOT ENERGIZE DRIVERS BEFORE G0-G2 MEASUREMENTS",
                  3: "BTL OUTPUT NEGATIVES ARE NOT GND",
                  4: "Generated by hardware/kicad/generate_harman_kardom.py"},
    )
    add_section(schematic, "A. USB-C PD + 4S CC/CV CHARGE", (25, 15), (155, 78))
    add_section(schematic, "B. 4S PACK, BMS, FUSE, HARD POWER", (162, 15), (375, 78))
    add_section(schematic, "C. SWITCHED POWER", (25, 86), (150, 139))
    add_section(schematic, "D. ESP32-S3 + USER INTERFACE", (157, 86), (375, 139))
    add_section(schematic, "E. I2S DAC + BTL AMP + DRIVERS", (25, 148), (375, 217))
    add_section(schematic, "F. TEST POINTS / OSCILLOSCOPE ACCESS", (25, 224), (375, 274))
    for module in MODULES:
        add_module(schematic, module)
    add_series_battery(schematic)
    add_power_path(schematic)
    add_ui_and_tweeter_safety(schematic)
    add_note(schematic, "Set PD trigger to 20 V without the battery connected.", (90, 66))
    add_note(schematic, "Calibrate XL4015 to 16.80 V / 2.00 A on a dummy load. V1: CHARGE WHILE PLAYING DISABLED.", (90, 70), bold=True)
    add_note(schematic, "BMS pin order is logical only. Verify actual silkscreen, balance threshold/current and NTC behavior.", (268, 67))
    add_note(schematic, "S1 contact rating is undocumented: verify 16.8 VDC / 5 A. D2 is 12 V only; keep R2 DNP until LED current is measured.", (318, 72), size=1.05, bold=True)
    add_note(schematic, "Adjust U4 to 5.10 V before connecting ESP32/DAC. Keep JP1 open until USB backfeed safety is proven.", (88, 130))
    add_note(schematic, "GPIO candidates: BCLK=4, LRCLK=5, DATA=6, BTN=7, RGB=8/9/10, I2C=11/12.", (266, 129))
    add_note(schematic, "Avoid strapping GPIO0/3/45/46 and native USB GPIO19/20.", (266, 133))
    add_note(schematic, "PCM5102A: SCK->GND for 3-wire PLL; verify module solder bridges.", (92, 205))
    add_note(schematic, "DANGER: L-/R- are BTL switching outputs. NEVER connect them to GND.", (274, 205), size=1.55, bold=True)
    add_note(schematic, "C_SAFE remains TBD until tweeter DCR/impedance and safe HPF are measured.", (274, 210), bold=True)
    add_note(schematic, "Scope ground: POWER_GND/STAR_GND only. For BTL use a differential probe or CH1-CH2; both ground clips at POWER_GND.", (200, 263), bold=True)
    add_note(schematic, "G0 driver impedance BLOCKED | G1 dummy-load amp BLOCKED | G2 C_SAFE BLOCKED | G3 power/thermal BLOCKED | G4 battery/BMS BLOCKED", (200, 268), size=1.35, bold=True)
    return schematic


def locate_kicad_cli(explicit: str | None) -> str:
    if explicit:
        candidate = Path(explicit)
        if candidate.is_file():
            return str(candidate)
        raise FileNotFoundError(f"kicad-cli not found at {candidate}")
    found = shutil.which("kicad-cli")
    if found:
        return found
    if sys.platform.startswith("win"):
        for version in ("10.0", "9.0", "8.0"):
            candidate = Path(r"C:\Program Files\KiCad") / version / "bin" / "kicad-cli.exe"
            if candidate.is_file():
                return str(candidate)
    raise FileNotFoundError("kicad-cli not found; install KiCad 8 or newer")


def write_project(output_dir: Path) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    schematic_path = output_dir / "harman-kardom.kicad_sch"
    project_path = output_dir / "harman-kardom.kicad_pro"
    build_schematic().save(schematic_path)
    project = {"board": {}, "boards": [], "cvpcb": {}, "erc": {}, "libraries": {},
               "meta": {"filename": project_path.name, "version": 1}, "net_settings": {},
               "pcbnew": {}, "schematic": {}, "text_variables": {}}
    project_path.write_text(json.dumps(project, indent=2) + "\n", encoding="utf-8")
    return schematic_path


def run_checks(cli: str, schematic_path: Path) -> None:
    report = schematic_path.with_suffix(".erc.rpt")
    pdf = schematic_path.with_suffix(".pdf")
    erc = subprocess.run([cli, "sch", "erc", "--output", str(report), str(schematic_path)], check=False)
    report_text = report.read_text(encoding="utf-8")
    if "Errors 0" not in report_text:
        raise RuntimeError(f"KiCad ERC found errors; inspect {report}")
    subprocess.run([cli, "sch", "export", "pdf", "--output", str(pdf), str(schematic_path)], check=True)
    print(f"ERC report: {report} (exit {erc.returncode}; module-level warnings are expected)")
    print(f"Preview PDF: {pdf}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--validate", action="store_true", help="Run KiCad ERC and export a PDF preview")
    parser.add_argument("--kicad-cli", help="Explicit kicad-cli executable path")
    args = parser.parse_args()
    try:
        schematic_path = write_project(args.output_dir.resolve())
        print(f"Generated: {schematic_path}")
        if args.validate:
            run_checks(locate_kicad_cli(args.kicad_cli), schematic_path)
    except (FileNotFoundError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
