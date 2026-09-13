#!/usr/bin/env python3
"""Write a device's setup directory: a bare factory_cal.csv, the QR payloads and a label.

The name is historical. Until ADR-0023 this tool generated a per-device
provisioning password, computed the salt and verifier the device held for it,
and wrote a label that was the only copy of that password. Since ADR-0023 the
speaker is set up without a PIN: protocomm Security 1 with no proof of
possession on both BLE and SoftAP, and an open setup network. There is no
per-device secret any more, and the firmware reads nothing from `factory_cal`
to open provisioning. The file keeps its name because the record, the bench
procedures and the CI recipes refer to it by that name.

What it produces, per device:

  <out>/<id>/factory_cal.csv   NVS CSV: the `cal` namespace and its schema row, nothing else
  <out>/<id>/qr.txt            the two QR payloads (BLE, SoftAP), one per line
  <out>/<id>/label.txt         the device name and the same two payloads, for a printed label

None of it is secret. The files get default permissions, and the tool never
asks for, generates or prints a password. The QR is optional: the stock
Espressif apps list a device whose advertised name starts with `PROV_` without
any QR, and the QR only spares the user reading the name off the list.

The CSV is what gives a board a `factory_cal` image to flash: hk_storage
expects the schema row, and `write_profile.py` merges the calibration profile
into this same directory. `--image` builds and checks that image, and is the
only step that needs ESP-IDF (`IDF_PATH`); a directory and a label need
nothing.

    python3 firmware/tools/provision_credentials.py --device A1B2 --out ~/hk-credentials
    python3 firmware/tools/provision_credentials.py --count 4
    python3 firmware/tools/provision_credentials.py --device A1B2 --image --out ~/hk-credentials
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

#: The one setup name (hk_identity.h, ADR-0023): the BLE advertisement and the
#: setup network's SSID are the same string. The `PROV_` prefix is the one the
#: stock Espressif provisioning apps filter their device lists by, so the
#: speaker is listed without a QR and without changing a setting in the app.
#: The AirPlay and mDNS names are not this and do not carry the prefix.
SETUP_NAME = "PROV_Merzarkabul-{device_id}"

#: The security level the device advertises in `proto-ver`: hk_network starts
#: the manager with WIFI_PROV_SECURITY_1 and no proof of possession, so the
#: session key comes from the key exchange alone (ADR-0023).
#:
#: It is written into the QR on purpose. ESP-IDF's own payload for a device
#: without a proof of possession is just {ver, name, transport}, but both Espressif
#: provisioning libraries take an absent `security` to mean 2 (read from their
#: sources; the store builds are unverified). In the session the device's own
#: `proto-ver` wins, so a wrong assumption would probably be corrected -- but a
#: QR that says what the device does is the honest form, and it protects a
#: client that trusts the QR first.
SECURITY = 1

TRANSPORTS = ("ble", "softap")

#: The schema version hk_storage expects in `factory_cal` (HK_SCHEMA_FACTORY_VERSION).
FACTORY_SCHEMA = 1

#: The whole CSV. The namespace and key match hk_storage.h; the profile row is
#: appended later by write_profile.py, into this same namespace.
FACTORY_CAL_CSV = (
    "key,type,encoding,value\n"
    "cal,namespace,,\n"
    f"schema,data,u32,{FACTORY_SCHEMA}\n"
)


def setup_name(device_id: str) -> str:
    return SETUP_NAME.format(device_id=device_id)


def qr_payload(device_id: str, transport: str) -> str:
    """The payload the Espressif provisioning apps read from a QR.

    Field names follow ESP-IDF's wifi_prov_print_qr(); there is no `pop`, no
    `username` and no `password`, because the device asks for none of them.
    """
    if transport not in TRANSPORTS:
        raise ValueError(f"transport must be one of {TRANSPORTS}, not {transport!r}")
    return json.dumps({
        "ver": "v1",
        "name": setup_name(device_id),
        "transport": transport,
        "security": SECURITY,
    }, separators=(",", ":"))


def label_text(device_id: str) -> str:
    return (
        f"Merzarkabul Airplay Speakers {device_id}\n"
        f"\n"
        f"setup name : {setup_name(device_id)}  (BLE, and the open setup network's SSID)\n"
        f"\n"
        f"QR (BLE):\n{qr_payload(device_id, 'ble')}\n"
        f"\n"
        f"QR (SoftAP):\n{qr_payload(device_id, 'softap')}\n"
    )


#: factory_cal size, from firmware/partitions.csv. Both boards use the same one.
IMAGE_SIZE = 0xD000
PARTITION_HINT = "firmware/partitions.csv"


class ImageError(Exception):
    """The partition image was not produced, or was produced wrong."""


def build_image(device_dir: Path) -> Path:
    """Generate the factory_cal image for one device, and prove it is usable.

    Run from inside the device directory on purpose. A CSV row of type `file`
    (the profile row write_profile.py appends, or the legacy rows on a board
    provisioned before ADR-0023) names its input without a path, and
    nvs_partition_gen.py resolves that against the working directory rather
    than against the CSV -- so invoking it from the repository root silently
    fails to find the inputs. That is not a harmless mistake: the tool still
    writes an output file, and a short file flashed at the factory_cal offset
    erases the schema and the profile that were there and puts nothing in
    their place, leaving a product that refuses audio for want of a profile.

    So the image is checked before anyone can flash it: exact size, and an NVS
    page header where an erased region would read 0xFF.
    """
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        raise ImageError("IDF_PATH is not set; source $IDF_PATH/export.sh first")
    generator = (Path(idf_path) / "components" / "nvs_flash"
                 / "nvs_partition_generator" / "nvs_partition_gen.py")
    if not generator.is_file():
        raise ImageError(f"{generator} does not exist")

    image = device_dir / "factory_cal.bin"

    def reject(reason: str) -> ImageError:
        """Remove the image before complaining about it.

        A failed run still leaves a partial file behind, and a partial file at
        this path is the whole danger: it looks like the thing to flash. Nothing
        that is not known-good survives this function.
        """
        image.unlink(missing_ok=True)
        return ImageError(reason)

    result = subprocess.run(
        [sys.executable, str(generator), "generate",
         "factory_cal.csv", "factory_cal.bin", hex(IMAGE_SIZE)],
        cwd=device_dir, capture_output=True, text=True)
    if result.returncode != 0:
        detail = (result.stderr.strip().splitlines() or ["no output"])[-1]
        raise reject(f"nvs_partition_gen.py failed: {detail}")
    if not image.is_file():
        raise reject("nvs_partition_gen.py reported success but wrote no image")

    size = image.stat().st_size
    if size != IMAGE_SIZE:
        raise reject(f"image is {size} bytes, not {IMAGE_SIZE}; a short image flashed "
                     f"at that offset erases the schema and the profile and replaces nothing")
    if image.read_bytes()[:4] == b"\xff\xff\xff\xff":
        raise reject("image begins with erased flash, so it carries no NVS page: an "
                     "input named in factory_cal.csv was not found")
    return image


def write_device(out_dir: Path, device_id: str) -> Path:
    """Write the three files for one device and return its directory.

    Nothing here is secret, so no file is restricted: a file with owner-only
    permissions teaches the reader that it holds something, and these hold a
    name.
    """
    device_dir = out_dir / device_id
    device_dir.mkdir(parents=True, exist_ok=True)

    # A directory that already holds a factory_cal.csv keeps it: a merged
    # profile row (write_profile.py) or the legacy rows of a board set up
    # before ADR-0023 must not be reset to the bare two rows under a
    # --image that would then quietly flash a schema-only partition. Only the
    # name files are rewritten.
    csv_path = device_dir / "factory_cal.csv"
    if csv_path.exists() and csv_path.read_text(encoding="utf-8") != FACTORY_CAL_CSV:
        print(f"{device_id}: {csv_path} already exists with its own rows; left untouched")
    else:
        csv_path.write_text(FACTORY_CAL_CSV, encoding="utf-8")
    (device_dir / "qr.txt").write_text(
        qr_payload(device_id, "ble") + "\n" + qr_payload(device_id, "softap") + "\n",
        encoding="utf-8")
    (device_dir / "label.txt").write_text(label_text(device_id), encoding="utf-8")

    print(f"{device_id}: {setup_name(device_id)} -> {device_dir}")
    return device_dir


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path,
                        default=Path("build/provisioning"),
                        help="output directory (nothing in it is secret; keep it out of "
                             "Git all the same, it is per-board output, not source)")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--device", action="append",
                       help="device id, the XXXX suffix of the setup name; repeatable")
    parser.add_argument("--image", action="store_true",
                        help="also build and verify each factory_cal.bin "
                             "(the one step that needs IDF_PATH)")
    group.add_argument("--count", type=int,
                       help="generate this many devices with placeholder ids")
    args = parser.parse_args()

    device_ids = args.device or [f"DEV{index + 1}" for index in range(args.count)]
    for device_id in device_ids:
        write_device(args.out, device_id)

    print(f"\n{len(device_ids)} device(s) written under {args.out}")
    print("No PIN: the label carries the setup name and the QR text, and nothing to keep secret.")

    if args.image:
        failures = 0
        for device_id in device_ids:
            try:
                image = build_image(args.out / device_id)
            except ImageError as error:
                print(f"ERROR: {device_id}: {error}", file=sys.stderr)
                failures += 1
                continue
            print(f"  {device_id}: {image} ({image.stat().st_size} bytes)")
        if failures:
            return 1
        print(f"\nFlash each at the factory_cal offset in {PARTITION_HINT}.")
        return 0

    print("\nBuild a partition image with --image, or by hand, per device:")
    print("  cd <out>/<id> && python3 \\")
    print("    $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py \\")
    print(f"    generate factory_cal.csv factory_cal.bin 0x{IMAGE_SIZE:x}")
    print("The `cd` is not optional once the CSV names an input file (the profile row")
    print("write_profile.py appends): nvs_partition_gen.py resolves those against the")
    print("working directory, not against the CSV. Run it from anywhere else and the")
    print("inputs are not found -- and a truncated image flashed at that offset erases")
    print("the schema and the profile that were there. --image does the cd for you and")
    print("checks the result, which is why it exists.")
    print(f"Check the result is exactly {IMAGE_SIZE} bytes before flashing it.")
    print(f"Then flash it at the factory_cal offset from {PARTITION_HINT}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
