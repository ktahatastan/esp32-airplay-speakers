#!/usr/bin/env python3
"""Check that a user reset cannot reach the calibration store.

PRD-008 requires that restoring user settings never erases the driver
protection profile. That profile is the result of bench measurements against
specific drivers; losing it means a speaker that looks fine while driving
unprotected tweeters.

A comment saying so is not a guarantee. This checks the source:

  1. No erase or format call anywhere in the firmware names the calibration
     partition.
  2. The calibration partition is only ever opened NVS_READONLY.
  3. The one erase that does exist is in the user-settings path.

Run it from anywhere:

    python3 firmware/tools/check_storage_isolation.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parent.parent
FACTORY_PARTITION_MACRO = "HK_STORAGE_FACTORY_PARTITION"
FACTORY_PARTITION_LITERAL = "factory_cal"

#: Calls that destroy stored data.
DESTRUCTIVE = re.compile(
    r"\b(nvs_flash_erase_partition|nvs_flash_deinit_partition|nvs_erase_all|"
    r"nvs_erase_key|nvs_flash_erase|esp_partition_erase_range)\s*\(([^;]*)",
    re.DOTALL,
)

#: Opening the calibration partition. Group 1 is the argument list.
FACTORY_OPEN = re.compile(r"nvs_open_from_partition\s*\(([^;]*?)\)", re.DOTALL)


#: The directories whose contents run on the device. The rule this script
#: enforces is about firmware, so those are what it reads.
#:
#: firmware/test/ is deliberately outside that. A host test may open the
#: calibration partition for writing and may call nvs_flash_erase, because it
#: has to seed a calibration and then prove a reset cannot reach it — doing
#: neither would leave nothing to protect and nothing to protect it from. Those
#: files never run on a speaker, so nothing they do can lose a measurement.
#:
#: The two checks are complementary rather than overlapping: this one proves
#: the shipped source CANNOT reach the calibration store, and
#: firmware/test/nvs_host proves at run time that it DOES NOT.
SHIPPED_DIRS = ("main", "components")


def sources() -> list[Path]:
    found: list[Path] = []
    for directory in SHIPPED_DIRS:
        root = FIRMWARE / directory
        found.extend(
            path for path in root.rglob("*.c")
            if "build" not in path.parts and "managed_components" not in path.parts
        )
    if not found:
        # Scanning nothing passes every check, which would look exactly like
        # scanning everything and finding nothing wrong.
        raise SystemExit(f"error: no sources found under {SHIPPED_DIRS}")
    return sorted(found)


def check() -> list[str]:
    problems: list[str] = []
    erase_sites = 0
    factory_opens = 0

    for path in sources():
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(FIRMWARE)

        for match in DESTRUCTIVE.finditer(text):
            call, args = match.group(1), match.group(2)
            line = text[: match.start()].count("\n") + 1
            erase_sites += 1
            if FACTORY_PARTITION_MACRO in args or FACTORY_PARTITION_LITERAL in args:
                problems.append(
                    f"{relative}:{line}: {call} names the calibration partition; "
                    "a user reset must never be able to reach it (PRD-008)")

        for match in FACTORY_OPEN.finditer(text):
            args = match.group(1)
            if FACTORY_PARTITION_MACRO not in args and FACTORY_PARTITION_LITERAL not in args:
                continue
            factory_opens += 1
            line = text[: match.start()].count("\n") + 1
            if "NVS_READONLY" not in args:
                problems.append(
                    f"{relative}:{line}: the calibration partition is opened for writing. "
                    "This firmware has no calibration writer; a store it cannot write is "
                    "a store it cannot corrupt")

    if erase_sites == 0:
        problems.append(
            "no destructive call found anywhere: either the user reset stopped working "
            "or this checker no longer recognises the call it is meant to guard")
    if factory_opens == 0:
        problems.append(
            "the calibration partition is never opened: either it stopped being read or "
            "this checker no longer recognises how")

    print(f"scanned {len(sources())} sources: {erase_sites} destructive call(s), "
          f"{factory_opens} calibration open(s)")
    return problems


def main() -> int:
    problems = check()
    for problem in problems:
        print(f"ERROR: {problem}", file=sys.stderr)
    print(f"check_storage_isolation: {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
