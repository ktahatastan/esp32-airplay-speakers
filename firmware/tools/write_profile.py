#!/usr/bin/env python3
"""Pack a calibration profile for `factory_cal` from a values file, and read one back.

The product plays nothing until a valid profile blob sits in `factory_cal`
(namespace `cal`, key `profile`; hk_storage.h), judged at boot by
hk_profile_load(). Until now the only way to get numbers into that blob was to
recompile the firmware. This tool takes the numbers from a values file, packs
them into the 116-byte schema-2 wire format of `hk_profile_t` (hk_profile.h),
and merges the blob into a device's existing `factory_cal` image.

    python3 firmware/tools/write_profile.py VALUES.json --out build/profile
    python3 firmware/tools/write_profile.py VALUES.json --device-dir ~/hk-credentials/A1B2
    python3 firmware/tools/write_profile.py --dump build/profile/profile.bin
    python3 firmware/tools/write_profile.py --dump readback.bin      # a whole partition

What the values file is
-----------------------
A JSON object with two members. `profile` carries every field of
`hk_profile_t` by name (`schema`, `measured_yyyymmdd`, `source`, and the
nineteen numbers); `reserved` is the wire format's own padding and is always
written as zero. `provenance` carries, for each of the nineteen numbers, where it
came from:

    "crossover_hz": {"status": "derived", "source": "<record path or section>"}

`status` is one of `measured`, `derived` or `placeholder`, and `source` names
the record that holds the number. Provenance is documentation: the tool
requires it to be complete, prints it, and never packs it -- the device carries
only the profile's own `source` string and date. What the provenance buys is
that a values file cannot silently promote a placeholder to a measurement: the
status has to be written next to the number, in a file the record keeps.

What is refused
---------------
The same profiles hk_profile_valid() refuses, for the same reasons, under the
same one-word names hk_profile_verdict_name() prints (`schema`, `source`,
`measurement`, `frequency`, `gain`, `ceiling`, `reference`, `timing`, `delay`,
`polarity`, `budget`, `amp-gain`). Every float is judged after rounding to
IEEE single precision, because that is the number the device will see. Only
the structural checks are mirrored here: a corner the filter design cannot
build at the device's sample rate (`unbuildable`) is judged on the device, and
this tool cannot know that a crossover is right -- only a measurement can.

What this tool never does
-------------------------
It never flashes. It prints the `esptool` command and stops, because writing
this partition is what makes the product build play, and that is the owner's
act. And it never generates credentials: `--device-dir` takes the directory
`provision_credentials.py` produced, appends one row to that directory's own
`factory_cal.csv`, and rebuilds the image through
`provision_credentials.build_image()` so the salt, verifier and setup password
that were there are still there. A directory without them is refused. A bare
`profile.bin` is 116 bytes and is NOT an image: flashed at the partition offset
it would erase the credentials and replace nothing.
"""

from __future__ import annotations

import argparse
import importlib.util
import csv
import json
import math
import os
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import provision_credentials  # noqa: E402

FIRMWARE = Path(__file__).resolve().parents[1]
PARTITIONS = FIRMWARE / "partitions.csv"

# ---- The wire format, from hk_profile.h. Append at the end and bump the schema;
# ---- never reorder. test_write_profile.py checks the offsets by hand.

SCHEMA = 2
SOURCE_MAX = 32              # HK_PROFILE_SOURCE_MAX, terminator included
SUPPLY_MV_MIN = 8000.0       # HK_PROFILE_SUPPLY_MV_MIN
SUPPLY_MV_MAX = 26000.0      # HK_PROFILE_SUPPLY_MV_MAX
DELAY_MAX_SAMPLES = 64       # HK_PROFILE_DELAY_MAX_SAMPLES
SUPPLY_BUDGET_MAX = 2.0      # HK_SUPPLY_LIMITER_BUDGET_MAX
AMP_GAINS_DB = (0, 20, 26, 32, 36)  # 0 = not read (bench item C3); else SLOS528F Table 2

#: (field, struct code) in wire order. "H" is uint16, "I" uint32, "f" float32.
FIELDS: tuple[tuple[str, str], ...] = (
    ("schema", "H"),
    ("reserved", "H"),
    ("measured_yyyymmdd", "I"),
    ("source", f"{SOURCE_MAX}s"),
    ("woofer_dcr_ohm", "f"),
    ("tweeter_dcr_ohm", "f"),
    ("woofer_hpf_hz", "f"),
    ("crossover_hz", "f"),
    ("woofer_gain", "f"),
    ("tweeter_gain", "f"),
    ("reference_supply_mv", "f"),
    ("woofer_ceiling", "f"),
    ("tweeter_ceiling", "f"),
    ("woofer_release_ms", "I"),
    ("woofer_hold_ms", "I"),
    ("tweeter_release_ms", "I"),
    ("tweeter_hold_ms", "I"),
    ("woofer_delay_samples", "I"),
    ("tweeter_delay_samples", "I"),
    ("tweeter_polarity", "I"),
    ("supply_budget_sq", "f"),
    ("supply_window_ms", "I"),
    ("amp_gain_db", "I"),
)
STRUCT_FORMAT = "<" + "".join(code for _, code in FIELDS)
BLOB_SIZE = struct.calcsize(STRUCT_FORMAT)
assert BLOB_SIZE == 116, BLOB_SIZE  # sizeof(hk_profile_t); test_profile.c asserts the same literal

#: The header of the record: named in the values file, but not a measurement.
HEADER_FIELDS = ("schema", "measured_yyyymmdd", "source")
#: Every number the device carries. Each needs a provenance entry.
VALUE_FIELDS = tuple(name for name, _ in FIELDS
                     if name not in HEADER_FIELDS and name != "reserved")
PROFILE_FIELDS = HEADER_FIELDS + VALUE_FIELDS
FLOAT_FIELDS = frozenset(name for name, code in FIELDS if code == "f")
STATUSES = ("measured", "derived", "placeholder")

#: Where the device looks (hk_storage.h): the namespace and the key inside `factory_cal`.
FACTORY_NAMESPACE = "cal"    # HK_STORAGE_FACTORY_NAMESPACE
PROFILE_KEY = "profile"      # HK_STORAGE_PROFILE_KEY

#: NVS as `nvs_partition_gen.py` writes it (and the device reads it).
NVS_PAGE = 4096
NVS_ENTRY = 32
NVS_FIRST_ENTRY = 64          # 32-byte page header, then a 32-byte entry-state bitmap
NVS_ENTRIES_PER_PAGE = 126
NVS_TYPE_U8 = 0x01            # a namespace entry: key is the name, data[0] the index
NVS_TYPE_BLOB_DATA = 0x42
NVS_TYPE_BLOB_IDX = 0x48
NVS_STATE_WRITTEN = 0b10


class ValuesError(Exception):
    """The values file is not the shape this tool reads."""


class ProfileRefused(Exception):
    """The profile would be refused by hk_profile_valid(); `verdict` names why."""

    def __init__(self, verdict: str, detail: str):
        super().__init__(f"{verdict}: {detail}")
        self.verdict = verdict
        self.detail = detail


class DeviceDirError(Exception):
    """The device directory is not one provision_credentials.py produced, or the merge cannot be done safely."""


# ---------------------------------------------------------------- numbers

def float32(value: float) -> float:
    """The value the device will see: rounded to IEEE single precision.

    A magnitude a float32 cannot hold becomes the infinity a C conversion
    would produce, so the refusal lands on the field's own verdict rather than
    on a packing error.
    """
    try:
        return struct.unpack("<f", struct.pack("<f", value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)


def _positive(value: float) -> bool:
    """hk_profile.c positive(): finite and strictly above zero."""
    return math.isfinite(value) and value > 0.0


def _unit_range(value: float) -> bool:
    """hk_profile.c unit_range(): a linear gain or ceiling, (0, 1]."""
    return _positive(value) and value <= 1.0


def refusal(profile: dict) -> str | None:
    """hk_profile_valid(), field for field and in the same order.

    Returns None for a profile the device accepts, else the one-word verdict
    hk_profile_verdict_name() would print. Floats are judged as float32.
    """
    p = {name: (float32(profile[name]) if name in FLOAT_FIELDS else profile[name])
         for name in PROFILE_FIELDS}

    if p["schema"] != SCHEMA:
        return "schema"

    # hk_profile_valid(): source[0] != 0, a terminator within the 32 bytes, a
    # date. An embedded NUL is not a device refusal (the C simply truncates
    # there); load_values() rejects it earlier as a values-file mistake.
    source = p["source"].encode("utf-8", "surrogateescape")
    if not source or len(source) >= SOURCE_MAX or p["measured_yyyymmdd"] == 0:
        return "source"

    if not _positive(p["woofer_dcr_ohm"]) or not _positive(p["tweeter_dcr_ohm"]):
        return "measurement"

    if not _positive(p["woofer_hpf_hz"]) or not _positive(p["crossover_hz"]):
        return "frequency"
    if p["woofer_hpf_hz"] >= p["crossover_hz"]:
        return "frequency"

    if not _unit_range(p["woofer_gain"]) or not _unit_range(p["tweeter_gain"]):
        return "gain"
    if not _unit_range(p["woofer_ceiling"]) or not _unit_range(p["tweeter_ceiling"]):
        return "ceiling"

    reference = p["reference_supply_mv"]
    if not math.isfinite(reference) or reference < SUPPLY_MV_MIN or reference > SUPPLY_MV_MAX:
        return "reference"

    if p["woofer_release_ms"] == 0 or p["tweeter_release_ms"] == 0:
        return "timing"

    if (p["woofer_delay_samples"] > DELAY_MAX_SAMPLES
            or p["tweeter_delay_samples"] > DELAY_MAX_SAMPLES
            or (p["woofer_delay_samples"] != 0 and p["tweeter_delay_samples"] != 0)):
        return "delay"

    if p["tweeter_polarity"] > 1:
        return "polarity"

    budget = p["supply_budget_sq"]
    if (not math.isfinite(budget) or not budget > 0.0 or budget > SUPPLY_BUDGET_MAX
            or p["supply_window_ms"] == 0):
        return "budget"

    if p["amp_gain_db"] not in AMP_GAINS_DB:
        return "amp-gain"

    return None


REFUSAL_DETAIL = {
    "schema": f"the schema must be {SCHEMA}; this firmware understands no other",
    "source": f"the source must name a record (1-{SOURCE_MAX - 1} bytes) and the date must be non-zero",
    "measurement": "both DC resistances must be finite and above zero",
    "frequency": "both corners must be above zero, and the subsonic corner below the crossover",
    "gain": "each branch gain must be in (0, 1]",
    "ceiling": "each limiter ceiling must be in (0, 1]",
    "reference": f"the reference supply must be {SUPPLY_MV_MIN:.0f}-{SUPPLY_MV_MAX:.0f} mV (XH-A232: 8-26 V)",
    "timing": "a release time of zero is a switch, not a release",
    "delay": f"at most one branch is delayed, by at most {DELAY_MAX_SAMPLES} samples",
    "polarity": "tweeter polarity is 0 (in phase) or 1 (inverted)",
    "budget": f"the supply budget must be in (0, {SUPPLY_BUDGET_MAX}] and its window above zero",
    "amp-gain": f"amplifier gain is 0 (not read) or one of {AMP_GAINS_DB[1:]} dB",
}


def check(profile: dict) -> None:
    """Raise ProfileRefused with the device's verdict, or return."""
    verdict = refusal(profile)
    if verdict is not None:
        raise ProfileRefused(verdict, REFUSAL_DETAIL[verdict])


# ---------------------------------------------------------------- pack / unpack

def pack(profile: dict) -> bytes:
    """The 116-byte little-endian blob hk_profile_from_blob() reads. Validates first."""
    check(profile)
    values = []
    for name, code in FIELDS:
        if name == "reserved":
            values.append(0)
        elif code.endswith("s"):
            values.append(profile[name].encode("utf-8", "surrogateescape"))  # struct NUL-pads to 32
        else:
            values.append(profile[name])
    blob = struct.pack(STRUCT_FORMAT, *values)
    assert len(blob) == BLOB_SIZE
    return blob


def unpack(blob: bytes) -> dict:
    """The blob back as a profile dict, `reserved` included. Does not validate."""
    if len(blob) != BLOB_SIZE:
        raise ValuesError(f"a profile blob is {BLOB_SIZE} bytes; this is {len(blob)}")
    raw = struct.unpack(STRUCT_FORMAT, blob)
    profile = {}
    for (name, code), value in zip(FIELDS, raw):
        if code.endswith("s"):
            terminator = value.find(b"\0")
            # An unterminated source is what hk_profile_valid() refuses by
            # memchr; keep all 32 bytes so refusal() sees the same thing. The
            # bytes are kept lossless (surrogateescape) so a stored byte that is
            # not UTF-8 neither grows on re-encoding nor changes the verdict.
            value = (value if terminator < 0 else value[:terminator]).decode("utf-8", "surrogateescape")
        profile[name] = value
    return profile


def offsets() -> dict[str, int]:
    """Byte offset of every field, for the dump table and the layout test."""
    table = {}
    position = 0
    for name, code in FIELDS:
        table[name] = position
        position += struct.calcsize("<" + code)
    return table


def format_table(profile: dict) -> str:
    """One line per field: offset, name, value."""
    at = offsets()
    lines = []
    for name, code in FIELDS:
        value = profile[name]
        if code == "f":
            shown = f"{value:g}"
        elif code.endswith("s"):
            shown = repr(value)
        else:
            shown = str(value)
        lines.append(f"  {at[name]:>3}  {name:<22} {shown}")
    return "\n".join(lines)


# ---------------------------------------------------------------- the values file

def _want(mapping: dict, member: str, name: str) -> object:
    if name not in mapping:
        raise ValuesError(f"{member}.{name} is missing")
    return mapping[name]


def load_values(path: Path) -> tuple[dict, dict]:
    """Read and shape-check a values file; returns (profile, provenance).

    Shape only: the numbers are judged by check(). A shape error names the
    member, so a typo in a field name is a refusal here rather than a default
    on the device.
    """
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ValuesError(f"{path}: {error}") from error
    if not isinstance(document, dict):
        raise ValuesError("the values file must be a JSON object")
    # Members beginning with "_" are commentary for the reader; anything else
    # that is not one of the two the tool reads is probably a misplaced field.
    stray = sorted(key for key in document if key not in ("profile", "provenance") and not key.startswith("_"))
    if stray:
        raise ValuesError(f"unexpected top-level members: {', '.join(stray)}")
    for member in ("profile", "provenance"):
        if not isinstance(document.get(member), dict):
            raise ValuesError(f"'{member}' must be a JSON object")

    raw = document["profile"]
    unknown = sorted(set(raw) - set(PROFILE_FIELDS))
    if unknown:
        raise ValuesError(f"profile has fields hk_profile_t does not: {', '.join(unknown)}")
    profile: dict = {}
    for name in PROFILE_FIELDS:
        value = _want(raw, "profile", name)
        if name == "source":
            if not isinstance(value, str):
                raise ValuesError("profile.source must be a string")
            if "\0" in value:
                raise ValuesError("profile.source must not contain a NUL; the device would "
                                  "silently truncate the name there")
        elif name in FLOAT_FIELDS:
            # bool is an int in Python; a `true` where a number belongs is a mistake.
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise ValuesError(f"profile.{name} must be a number")
            value = float(value)
        else:
            if isinstance(value, bool) or not isinstance(value, int):
                raise ValuesError(f"profile.{name} must be an integer")
            limit = 0xFFFF if name == "schema" else 0xFFFFFFFF
            if not 0 <= value <= limit:
                raise ValuesError(f"profile.{name} must be 0..{limit}")
        profile[name] = value

    raw = document["provenance"]
    unknown = sorted(set(raw) - set(VALUE_FIELDS))
    if unknown:
        raise ValuesError(f"provenance names fields hk_profile_t does not carry: {', '.join(unknown)}")
    provenance: dict = {}
    for name in VALUE_FIELDS:
        entry = raw.get(name)
        if not isinstance(entry, dict):
            raise ValuesError(f"provenance.{name} is missing; every number needs a status and a source")
        extra = sorted(set(entry) - {"status", "source", "note"})
        if extra:
            raise ValuesError(f"provenance.{name} has unknown keys: {', '.join(extra)}")
        status = entry.get("status")
        if status not in STATUSES:
            raise ValuesError(f"provenance.{name}.status must be one of {STATUSES}")
        source = entry.get("source")
        if not isinstance(source, str) or not source.strip():
            raise ValuesError(f"provenance.{name}.source must name the record that holds the number")
        provenance[name] = {"status": status, "source": source, **({"note": entry["note"]} if "note" in entry else {})}

    return profile, provenance


def format_provenance(provenance: dict) -> str:
    lines = []
    for name in VALUE_FIELDS:
        entry = provenance[name]
        lines.append(f"  {entry['status']:<11} {name:<22} {entry['source']}")
    return "\n".join(lines)


# ---------------------------------------------------------------- an NVS image

def _entries(image: bytes):
    """Every written entry HEADER of every page: (page, index, entry bytes, following data bytes).

    An entry's span byte (offset 2) says how many 32-byte rows it occupies; a
    blob's payload rows follow its header and are skipped here, so a payload
    row whose first byte happens to equal a namespace index is never mistaken
    for a key of that namespace.
    """
    for page_start in range(0, len(image) - NVS_PAGE + 1, NVS_PAGE):
        page = image[page_start:page_start + NVS_PAGE]
        bitmap = page[32:64]
        index = 0
        while index < NVS_ENTRIES_PER_PAGE:
            state = (bitmap[index // 4] >> ((index % 4) * 2)) & 0b11
            if state != NVS_STATE_WRITTEN:
                index += 1
                continue
            start = NVS_FIRST_ENTRY + index * NVS_ENTRY
            entry = page[start:start + NVS_ENTRY]
            yield page_start // NVS_PAGE, index, entry, page[start + NVS_ENTRY:]
            index += max(1, entry[2])


def _key(entry: bytes) -> str:
    raw = entry[8:24]
    end = raw.find(b"\0")
    return (raw if end < 0 else raw[:end]).decode("ascii", "replace")


def namespace_keys(image: bytes, namespace: str) -> dict[str, int]:
    """Key -> entry type for every written entry in `namespace`. Names only, never values."""
    ns_index = None
    for _, _, entry, _ in _entries(image):
        if entry[0] == 0 and entry[1] == NVS_TYPE_U8 and _key(entry) == namespace:
            ns_index = entry[24]
            break
    if ns_index is None:
        return {}
    found: dict[str, int] = {}
    for _, _, entry, _ in _entries(image):
        if entry[0] == ns_index:
            found.setdefault(_key(entry), entry[1])
    return found


def extract_blob(image: bytes, namespace: str, key: str) -> bytes | None:
    """The blob stored under `namespace`/`key` in an NVS image, or None.

    Chunks (BLOB_DATA) are gathered by chunk index and each is checked against
    its own CRC, so a partly erased or mis-flashed partition is reported as
    "no profile" rather than as a profile with wrong numbers in it. The
    BLOB_IDX entry's total length is checked too when it is present.
    """
    ns_index = None
    for _, _, entry, _ in _entries(image):
        if entry[0] == 0 and entry[1] == NVS_TYPE_U8 and _key(entry) == namespace:
            ns_index = entry[24]
            break
    if ns_index is None:
        return None

    chunks: dict[int, bytes] = {}
    total = None
    for _, _, entry, tail in _entries(image):
        if entry[0] != ns_index or _key(entry) != key:
            continue
        if entry[1] == NVS_TYPE_BLOB_DATA:
            size = struct.unpack_from("<H", entry, 24)[0]
            crc = struct.unpack_from("<I", entry, 28)[0]
            data = tail[:size]
            if len(data) != size or (zlib.crc32(data, 0xFFFFFFFF) & 0xFFFFFFFF) != crc:
                raise ValuesError(f"'{key}' chunk {entry[3]} fails its CRC; the partition is not intact")
            chunks[entry[3]] = data
        elif entry[1] == NVS_TYPE_BLOB_IDX:
            total = struct.unpack_from("<I", entry, 24)[0]
    if not chunks:
        return None
    blob = b"".join(chunks[index] for index in sorted(chunks))
    if total is not None and total != len(blob):
        raise ValuesError(f"'{key}' index says {total} bytes, chunks carry {len(blob)}")
    return blob


# ---------------------------------------------------------------- the partition

def read_partition(name: str, partitions: Path = PARTITIONS) -> tuple[int, int]:
    """(offset, size) of a partition, from partitions.csv, so the printed command cannot drift."""
    with open(partitions, newline="", encoding="utf-8") as handle:
        for row in csv.reader(handle):
            if len(row) < 5 or row[0].strip().startswith("#"):
                continue
            if row[0].strip() == name:
                return int(row[3].strip(), 0), int(row[4].strip(), 0)
    raise ValuesError(f"{partitions.name} has no '{name}' partition")


# ---------------------------------------------------------------- the device directory

CREDENTIAL_FILES = ("prov_salt.bin", "prov_verif.bin", "ap_pass.bin")
CSV_NAME = "factory_cal.csv"
BLOB_NAME = "profile.bin"
PROFILE_ROW = f"{PROFILE_KEY},file,binary,{BLOB_NAME}"
NAMESPACE_ROW = f"{FACTORY_NAMESPACE},namespace,,"


def merge_into_device_dir(device_dir: Path, blob: bytes) -> bool:
    """Put the blob and its CSV row into a provisioned device directory.

    Returns True when the row was appended, False when it was already there.
    Idempotent: a second run with the same blob changes nothing. The CSV is
    only ever appended to, so the credential rows stay exactly as
    provision_credentials.py wrote them.
    """
    if not device_dir.is_dir():
        raise DeviceDirError(f"{device_dir} is not a directory")
    missing = [name for name in (CSV_NAME, *CREDENTIAL_FILES) if not (device_dir / name).is_file()]
    if missing:
        raise DeviceDirError(
            f"{device_dir} lacks {', '.join(missing)}: not a directory provision_credentials.py "
            f"produced. This tool never generates credentials; run provision_credentials.py "
            f"first, then merge the profile into its output.")

    csv_path = device_dir / CSV_NAME
    text = csv_path.read_text(encoding="utf-8")
    rows = [line.strip() for line in text.splitlines() if line.strip()]
    if not rows or rows[0] != "key,type,encoding,value":
        raise DeviceDirError(f"{csv_path} does not start with the nvs_partition_gen header")
    namespaces = [row for row in rows if row.split(",")[1:2] == ["namespace"]]
    if not namespaces or namespaces[-1] != NAMESPACE_ROW:
        # An appended row lands in the LAST namespace declared; if that is not
        # `cal`, the firmware would look for the profile in the wrong place.
        raise DeviceDirError(f"{csv_path}: the last namespace must be 'cal' for an appended row to land in it")

    existing = [row for row in rows if row.split(",")[0] == PROFILE_KEY]
    if existing and existing != [PROFILE_ROW]:
        raise DeviceDirError(
            f"{csv_path} already carries a '{PROFILE_KEY}' row that is not '{PROFILE_ROW}': "
            f"{existing[0]!r}. Refusing to guess which blob is meant.")

    blob_path = device_dir / BLOB_NAME
    blob_path.write_bytes(blob)

    if existing:
        return False
    if not text.endswith("\n"):
        text += "\n"
    csv_path.write_text(text + PROFILE_ROW + "\n", encoding="utf-8")
    return True


CREDENTIAL_KEYS = ("prov_salt", "prov_verif", "ap_pass")


def build_device_image(device_dir: Path, blob: bytes) -> Path:
    """Rebuild factory_cal.bin with provision_credentials' own builder and checks.

    Then read the image back the way --dump does: the credential keys must
    still be in it and the profile stored must be the blob that was merged.
    "Credentials kept" is checked against the image, not inferred from the CSV.
    """
    try:
        image = provision_credentials.build_image(device_dir)
    except provision_credentials.ImageError as error:
        raise DeviceDirError(f"image not built: {error}") from error

    raw = image.read_bytes()
    keys = namespace_keys(raw, FACTORY_NAMESPACE)
    lost = [key for key in CREDENTIAL_KEYS if key not in keys]
    if lost:
        image.unlink(missing_ok=True)
        raise DeviceDirError(f"the built image lost {', '.join(lost)}; removed it rather than "
                             f"leave an image that would strand the device")
    stored = extract_blob(raw, FACTORY_NAMESPACE, PROFILE_KEY)
    if stored != blob:
        image.unlink(missing_ok=True)
        raise DeviceDirError("the built image does not carry the profile that was merged; removed it")
    return image


def flash_instructions(image: Path, offset: int, size: int) -> str:
    return (
        f"\n"
        f"WRITING THIS PARTITION MAKES THE PRODUCT BUILD PLAY. Until now it refused audio\n"
        f"for want of a profile; after this write it will drive whatever is on the\n"
        f"amplifier outputs with these numbers. Before the write (AGENTS.md, hardware\n"
        f"safety): read the amplifier gain strapping (bench item C3) -- no listening on\n"
        f"drivers at 24 V before that; the first energised path is ONE amplifier, ONE\n"
        f"woofer and ONE tweeter, dummy load first, then real drivers at LOW level; the\n"
        f"other three amplifiers get drivers only after that pair passes.\n"
        f"\n"
        f"This tool does not flash. When you have checked what is wired, the owner runs:\n"
        f"\n"
        f"  python3 -m esptool --chip esp32s3 --port <PORT> write_flash {offset:#x} {image}\n"
        f"\n"
        f"Then read it back and check what the device will judge:\n"
        f"\n"
        f"  python3 -m esptool --chip esp32s3 --port <PORT> read_flash {offset:#x} {size:#x} readback.bin\n"
        f"  python3 {Path(__file__).resolve().relative_to(FIRMWARE.parent)} --dump readback.bin\n"
    )


# ---------------------------------------------------------------- commands

def dump(path: Path) -> int:
    raw = path.read_bytes()
    if len(raw) == BLOB_SIZE:
        blob = raw
        print(f"{path}: a bare profile blob ({BLOB_SIZE} bytes)")
    else:
        if raw[:4] == b"\xff\xff\xff\xff":
            print(f"ERROR: {path} begins with erased flash: no NVS page, no profile, "
                  f"and no credentials either", file=sys.stderr)
            return 1
        keys = namespace_keys(raw, FACTORY_NAMESPACE)
        print(f"{path}: an NVS image ({len(raw)} bytes); keys in namespace '{FACTORY_NAMESPACE}': "
              f"{', '.join(sorted(keys)) or 'none'}")
        absent = [name for name in ("prov_salt", "prov_verif", "ap_pass") if name not in keys]
        if absent:
            print(f"WARNING: credentials missing from this image: {', '.join(absent)}. A device "
                  f"flashed with it cannot be provisioned.", file=sys.stderr)
        try:
            blob = extract_blob(raw, FACTORY_NAMESPACE, PROFILE_KEY)
        except ValuesError as error:
            print(f"ERROR: {error}", file=sys.stderr)
            return 1
        if blob is None:
            print(f"ERROR: no '{PROFILE_KEY}' blob in this image; the device will refuse audio (ABSENT)",
                  file=sys.stderr)
            return 1
        if len(blob) != BLOB_SIZE:
            print(f"ERROR: the stored profile is {len(blob)} bytes, not {BLOB_SIZE}; "
                  f"hk_profile_from_blob() refuses it by length (schema)", file=sys.stderr)
            return 1

    profile = unpack(blob)
    print("  off  field                  value")
    print(format_table(profile))
    verdict = refusal(profile)
    if verdict is None:
        print("verdict: ok (hk_profile_valid); buildability at the sample rate is judged on the device")
        return 0
    print(f"verdict: {verdict} -- {REFUSAL_DETAIL[verdict]}", file=sys.stderr)
    return 1


def write(values: Path, out: Path | None, device_dir: Path | None) -> int:
    profile, provenance = load_values(values)
    blob = pack(profile)  # refuses before anything is written

    counts = {status: sum(1 for entry in provenance.values() if entry["status"] == status)
              for status in STATUSES}
    print(f"{values}: '{profile['source']}' dated {profile['measured_yyyymmdd']}, "
          f"schema {profile['schema']}, {len(blob)} bytes")
    print(format_provenance(provenance))
    print(f"  {counts['measured']} measured, {counts['derived']} derived, "
          f"{counts['placeholder']} placeholder of {len(VALUE_FIELDS)}")
    if counts["placeholder"] or counts["derived"]:
        print("  This profile protects the drivers by argument, not by evidence: it is not a "
              "G0/G2 calibration and must never be mistaken for one.")

    if device_dir is None:
        out = out or Path("build/profile")
        out.mkdir(parents=True, exist_ok=True)
        blob_path = out / BLOB_NAME
        blob_path.write_bytes(blob)
        offset, _ = read_partition("factory_cal")
        print(f"\nwrote {blob_path} ({len(blob)} bytes)")
        print(f"This is the blob, NOT a partition image: never flash it at {offset:#x}, it would "
              f"erase the credentials there and replace nothing. Merge it into a provisioned "
              f"device directory with --device-dir to get a factory_cal.bin.")
        return 0

    # Everything that can refuse is asked before the directory is touched, so a
    # run either merges and builds or leaves the credentials directory as it was.
    offset, size = read_partition("factory_cal")
    if size != provision_credentials.IMAGE_SIZE:
        raise DeviceDirError(
            f"partitions.csv says factory_cal is {size:#x} bytes but provision_credentials.py "
            f"builds {provision_credentials.IMAGE_SIZE:#x}; fix that before building an image")
    if not os.environ.get("IDF_PATH"):
        raise DeviceDirError("IDF_PATH is not set; source $IDF_PATH/export.sh first. The image is "
                             "built by ESP-IDF's nvs_partition_gen.py, and nothing was written.")
    # In ESP-IDF v5.5 nvs_partition_gen.py is a wrapper around a pip package that
    # lives only in the IDF python env; IDF_PATH alone does not prove this
    # interpreter can run it, and a failed build would delete the image.
    if importlib.util.find_spec("esp_idf_nvs_partition_gen") is None:
        raise DeviceDirError("this python cannot import esp_idf_nvs_partition_gen; run under the "
                             "IDF python env (. $IDF_PATH/export.sh, then `python`, not a bare "
                             "python3). Nothing was written.")
    appended = merge_into_device_dir(device_dir, blob)
    print(f"\n{device_dir / CSV_NAME}: profile row {'appended' if appended else 'already present'}; "
          f"{device_dir / BLOB_NAME} written")
    image = build_device_image(device_dir, blob)
    print(f"{image}: {image.stat().st_size} bytes; read back: credentials kept, profile merged")
    print(flash_instructions(image, offset, size))
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("values", nargs="?", type=Path,
                        help="the values JSON (profile + provenance)")
    parser.add_argument("--out", type=Path,
                        help="where to write profile.bin without a device directory "
                             "(default build/profile; keep it out of Git)")
    parser.add_argument("--device-dir", type=Path,
                        help="a directory provision_credentials.py produced: merge the profile "
                             "into its factory_cal.csv and rebuild factory_cal.bin (needs IDF_PATH)")
    parser.add_argument("--dump", type=Path, metavar="FILE",
                        help="print a profile back as a table: a bare profile.bin, or a whole "
                             "factory_cal partition read with esptool read_flash")
    args = parser.parse_args(argv)

    if args.dump is not None:
        if args.values is not None or args.out is not None or args.device_dir is not None:
            parser.error("--dump takes no other arguments")
        return dump(args.dump)
    if args.values is None:
        parser.error("a values file is required (or --dump FILE)")
    if args.out is not None and args.device_dir is not None:
        parser.error("--out and --device-dir are alternatives: the blob goes into the device directory")

    try:
        return write(args.values, args.out, args.device_dir)
    except ProfileRefused as error:
        print(f"REFUSED ({error.verdict}): {error.detail}. The device would refuse this blob the "
              f"same way, so nothing was written.", file=sys.stderr)
        return 1
    except (ValuesError, DeviceDirError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
