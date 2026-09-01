#!/usr/bin/env python3
"""Bring a Harman Kardom speaker back over USB without destroying its calibration.

This is the recovery path the OTA plan promises will exist in every release. It
is a script rather than a paragraph because the dangerous mistake here is one
command away from the correct one.

    esptool erase_flash        <- erases factory_cal too

`factory_cal` holds driver protection measurements taken with the drivers, an
amplifier and an afternoon. Nothing regenerates them from software, and a
speaker whose calibration is gone will refuse to play at all — by design, since
inventing a protection profile is what the project forbids. So this writes only
the four regions that make a device boot and leaves every data partition alone:

    0x00000  bootloader
    0x08000  partition table
    0x0f000  ota data          (resets the A/B selector to slot 0)
    0x20000  application       (ota_0)

The offsets are read from partitions.csv rather than written here, so a
repartition cannot leave this script quietly writing into the wrong place.

Usage:
    recover.py --port /dev/cu.usbmodem1101
    recover.py --port ... --build build-release --image harman-kardom-signed.bin
    recover.py --port ... --dry-run
"""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parent.parent
PARTITIONS = FIRMWARE / "partitions.csv"

#: Partitions this script must never write. Everything a user or a bench
#: measurement put on the device lives in one of them.
PRESERVE = ("nvs", "nvs_keys", "factory_cal", "storage", "phy_init")


class RecoveryError(Exception):
    """Something that must stop the flash rather than be worked around."""


def read_offsets() -> dict[str, int]:
    """Offsets from partitions.csv, so this script cannot drift from the table."""
    offsets: dict[str, int] = {}
    with open(PARTITIONS, newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if not row or row[0].strip().startswith("#"):
                continue
            if len(row) < 4:
                continue
            name = row[0].strip()
            try:
                offsets[name] = int(row[3].strip(), 0)
            except ValueError:
                continue
    for required in ("otadata", "ota_0", *PRESERVE):
        if required not in offsets:
            raise RecoveryError(f"{PARTITIONS.name} has no '{required}' partition")
    return offsets


def plan(build: Path, image: str, offsets: dict[str, int]) -> list[tuple[int, Path]]:
    """The four regions to write, in ascending offset order."""
    items = [
        (0x0, build / "bootloader" / "bootloader.bin"),
        (0x8000, build / "partition_table" / "partition-table.bin"),
        (offsets["otadata"], build / "ota_data_initial.bin"),
        (offsets["ota_0"], build / image),
    ]
    missing = [str(path) for _, path in items if not path.is_file()]
    if missing:
        raise RecoveryError(
            "these build outputs are missing:\n  " + "\n  ".join(missing) +
            "\n\nBuild first:  idf.py -C firmware build")
    return items


def check_no_overlap(items, offsets: dict[str, int]) -> None:
    """Refuse to write anything that would land on a preserved partition.

    The offsets come from the same file the device was partitioned with, so
    this should never fire. It exists because the consequence of it firing and
    nobody noticing is a calibration nobody can get back.
    """
    guarded = sorted((offsets[name], name) for name in PRESERVE)
    for start, path in items:
        end = start + path.stat().st_size
        for offset, name in guarded:
            if start < offset and end > offset:
                raise RecoveryError(
                    f"{path.name} at 0x{start:x} is {end - start} bytes and would "
                    f"overwrite '{name}' at 0x{offset:x}. Refusing.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", help="serial port; esptool autodetects if omitted")
    parser.add_argument("--build", default="build",
                        help="build directory under firmware/ (default: build)")
    parser.add_argument("--image", default="harman-kardom.bin",
                        help="application image within the build directory")
    parser.add_argument("--baud", default="460800")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the command without running it")
    args = parser.parse_args(argv)

    try:
        offsets = read_offsets()
        build = FIRMWARE / args.build
        items = plan(build, args.image, offsets)
        check_no_overlap(items, offsets)
    except RecoveryError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    command = [sys.executable, "-m", "esptool", "--chip", "esp32s3",
               "-b", args.baud, "--before", "default_reset", "--after", "hard_reset"]
    if args.port:
        command += ["--port", args.port]
    command += ["write_flash", "--flash_mode", "dio",
                "--flash_size", "16MB", "--flash_freq", "80m"]
    for offset, path in items:
        command += [f"0x{offset:x}", str(path)]

    print("Writing only the boot regions. These are left untouched:")
    for name in PRESERVE:
        print(f"  0x{offsets[name]:06x}  {name}")
    print()
    print(" ".join(command))

    if args.dry_run:
        return 0

    print()
    return subprocess.run(command).returncode


if __name__ == "__main__":
    raise SystemExit(main())
