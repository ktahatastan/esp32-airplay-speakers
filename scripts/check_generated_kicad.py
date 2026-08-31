#!/usr/bin/env python3
"""Detect a KiCad sheet that no longer matches the generator that produced it.

hardware/kicad/generated/harman-kardom.kicad_sch is generated output, not a
source file. Regenerating it needs KiCad's symbol libraries installed, which is
not true of every machine that can edit the generator — so the generator can be
changed, committed and pushed while the sheet silently keeps describing the
previous design.

That is worse than having no sheet at all. A schematic nobody can tell is stale
is a schematic someone will solder from.

So the generator's hash is recorded beside its output. If they disagree, the
sheet is stale and this says so, with the command that fixes it.

Usage:
    check_generated_kicad.py            # report, non-zero if stale
    check_generated_kicad.py --record   # after a successful regeneration
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "hardware/kicad/generate_harman_kardom.py"
SHEET = ROOT / "hardware/kicad/generated/harman-kardom.kicad_sch"
STAMP = ROOT / "hardware/kicad/generated/.generator-sha256"


def generator_hash() -> str:
    return hashlib.sha256(GENERATOR.read_bytes()).hexdigest()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", action="store_true",
                        help="record the current generator hash as matching the sheet")
    args = parser.parse_args(argv)

    if not GENERATOR.exists():
        print(f"error: {GENERATOR.relative_to(ROOT)} is missing")
        return 1

    current = generator_hash()

    if args.record:
        if not SHEET.exists():
            print("error: refusing to record a hash with no generated sheet present")
            return 1
        STAMP.write_text(current + "\n", encoding="utf-8")
        print(f"recorded: the sheet matches generator {current[:12]}")
        return 0

    if not SHEET.exists():
        print("check_generated_kicad: no generated sheet, nothing to be stale")
        return 0

    if not STAMP.exists():
        print("check_generated_kicad: the sheet has no recorded generator hash,")
        print("so whether it is current cannot be established.")
        print("Regenerate it and run this with --record.")
        return 1

    recorded = STAMP.read_text(encoding="utf-8").strip()
    if recorded != current:
        print("The committed KiCad sheet is STALE: it was generated from a")
        print("different version of the generator.\n")
        print(f"  generator now : {current[:12]}")
        print(f"  sheet built by: {recorded[:12]}\n")
        print("Regenerate it on a machine with KiCad's symbol libraries:")
        print("  hardware/kicad/.venv/bin/python hardware/kicad/generate_harman_kardom.py")
        print("  python3 scripts/check_generated_kicad.py --record")
        print("\ncheck_generated_kicad: 1 problem")
        return 1

    print("check_generated_kicad: the sheet matches its generator")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
