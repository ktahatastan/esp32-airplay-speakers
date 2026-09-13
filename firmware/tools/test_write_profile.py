#!/usr/bin/env python3
"""Tests for write_profile.py.

The tool exists so that a profile reaches `factory_cal` without a recompile and
without losing the rows that share the partition -- the schema row, and on a
board provisioned before ADR-0023 the legacy provisioning rows -- and so that
a blob the device would refuse is refused here first, by the same name. So the
cases are: the wire format is the one hk_profile.h declares (offsets computed
by hand, not by the tool); every refusal hk_profile_valid() makes is made here
under the same word; the merge into a device directory adds one row, once,
accepts exactly what provision_credentials.py writes, and keeps a legacy
directory's rows and files untouched.

The image build needs ESP-IDF's nvs_partition_gen.py, so those cases run only
under the IDF python env (IDF_PATH set and its NVS generator importable) and
are skipped with a message otherwise; the NVS reader that --dump uses is
tested without it, on a page assembled here by hand.
"""
import contextlib
import importlib.util
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import provision_credentials  # noqa: E402
import write_profile  # noqa: E402

TOOL = Path(__file__).resolve().parent / "write_profile.py"
REPO = Path(__file__).resolve().parents[2]
TONIGHT = REPO / "docs/assets/measurements/drivers/profile-2026-09-12-provisional.json"


def sample() -> dict:
    """NOT DRIVER VALUES. Invented for the arithmetic, like test_profile.c's sample()."""
    return {
        "schema": 2,
        "measured_yyyymmdd": 20260908,
        "source": "synthetic-not-a-measurement",
        "woofer_dcr_ohm": 6.0,
        "tweeter_dcr_ohm": 5.0,
        "woofer_hpf_hz": 45.0,
        "crossover_hz": 2200.0,
        "woofer_gain": 0.9,
        "tweeter_gain": 0.7,
        "reference_supply_mv": 24000.0,
        "woofer_ceiling": 0.8,
        "tweeter_ceiling": 0.5,
        "woofer_release_ms": 120,
        "woofer_hold_ms": 30,
        "tweeter_release_ms": 80,
        "tweeter_hold_ms": 15,
        "woofer_delay_samples": 0,
        "tweeter_delay_samples": 0,
        "tweeter_polarity": 0,
        "supply_budget_sq": 0.3,
        "supply_window_ms": 50,
        "amp_gain_db": 0,
    }


def provenance_for(profile: dict, status="placeholder") -> dict:
    return {name: {"status": status, "source": "synthetic"} for name in write_profile.VALUE_FIELDS}


def values_document(profile=None, provenance=None) -> dict:
    profile = profile or sample()
    return {"profile": profile, "provenance": provenance or provenance_for(profile)}


class TestWireFormat(unittest.TestCase):
    def test_the_blob_is_116_bytes(self):
        """sizeof(hk_profile_t); test_profile.c asserts the same literal on the C side."""
        self.assertEqual(struct.calcsize(write_profile.STRUCT_FORMAT), 116)
        self.assertEqual(write_profile.BLOB_SIZE, 116)
        self.assertEqual(len(write_profile.pack(sample())), 116)

    def test_golden_offsets_computed_by_hand(self):
        """From hk_profile.h, counted by hand and not taken from the tool: 2+2+4
        header, 32 source, nine floats, seven uint32, one float, two uint32."""
        at = write_profile.offsets()
        self.assertEqual(at["schema"], 0)
        self.assertEqual(at["reserved"], 2)
        self.assertEqual(at["measured_yyyymmdd"], 4)
        self.assertEqual(at["source"], 8)
        self.assertEqual(at["woofer_dcr_ohm"], 40)
        self.assertEqual(at["crossover_hz"], 40 + 3 * 4)
        self.assertEqual(at["tweeter_ceiling"], 40 + 8 * 4)
        self.assertEqual(at["woofer_release_ms"], 76)
        self.assertEqual(at["tweeter_polarity"], 76 + 6 * 4)
        self.assertEqual(at["supply_budget_sq"], 104)
        self.assertEqual(at["supply_window_ms"], 108)
        self.assertEqual(at["amp_gain_db"], 112)

    def test_golden_bytes(self):
        p = sample()
        p["crossover_hz"] = 2200.0
        p["tweeter_polarity"] = 1
        p["amp_gain_db"] = 26
        blob = write_profile.pack(p)
        self.assertEqual(blob[0:2], b"\x02\x00")            # schema 2, little-endian
        self.assertEqual(blob[2:4], b"\x00\x00")            # reserved, always zero
        self.assertEqual(blob[4:8], struct.pack("<I", 20260908))
        self.assertEqual(blob[8:40], b"synthetic-not-a-measurement".ljust(32, b"\0"))
        self.assertEqual(blob[52:56], struct.pack("<f", 2200.0))
        self.assertEqual(blob[100:104], struct.pack("<I", 1))
        self.assertEqual(blob[112:116], struct.pack("<I", 26))

    def test_pack_dump_round_trip(self):
        p = sample()
        back = write_profile.unpack(write_profile.pack(p))
        self.assertEqual(back.pop("reserved"), 0)
        for name, value in p.items():
            if name in write_profile.FLOAT_FIELDS:
                self.assertEqual(back[name], write_profile.float32(value), name)
            else:
                self.assertEqual(back[name], value, name)

    def test_a_blob_of_another_length_is_not_a_profile(self):
        with self.assertRaises(write_profile.ValuesError):
            write_profile.unpack(b"\0" * 84)   # a schema-1 blob


class TestRefusals(unittest.TestCase):
    """Each case is one hk_profile_valid() refusal, under the word the device prints."""

    def refuse(self, verdict, **changes):
        p = sample()
        p.update(changes)
        self.assertEqual(write_profile.refusal(p), verdict, changes)
        with self.assertRaises(write_profile.ProfileRefused) as caught:
            write_profile.pack(p)
        self.assertEqual(caught.exception.verdict, verdict)

    def test_the_sample_is_accepted(self):
        self.assertIsNone(write_profile.refusal(sample()))

    def test_schema(self):
        self.refuse("schema", schema=3)
        self.refuse("schema", schema=1)

    def test_source(self):
        self.refuse("source", source="")
        self.refuse("source", source="x" * 32)      # no room for the terminator
        self.refuse("source", measured_yyyymmdd=0)
        self.assertIsNone(write_profile.refusal({**sample(), "source": "x" * 31}))
        # The device truncates at an embedded NUL rather than refusing; the
        # tool mirrors that verdict, and load_values() is where a NUL in the
        # values file is caught as a mistake.
        self.assertIsNone(write_profile.refusal({**sample(), "source": "abc\0def"}))
        # A stored byte that is not UTF-8 must not grow on the way back through
        # dump -> refusal: 30 bytes with one bad byte are still 30 bytes.
        blob = bytearray(write_profile.pack({**sample(), "source": "x" * 30}))
        blob[8 + 5] = 0xFF
        self.assertIsNone(write_profile.refusal(write_profile.unpack(bytes(blob))))

    def test_measurement(self):
        self.refuse("measurement", woofer_dcr_ohm=0.0)
        self.refuse("measurement", tweeter_dcr_ohm=-1.0)
        self.refuse("measurement", woofer_dcr_ohm=float("nan"))

    def test_frequency(self):
        self.refuse("frequency", woofer_hpf_hz=3000.0)         # above the crossover
        self.refuse("frequency", woofer_hpf_hz=2200.0)         # equal to it
        self.refuse("frequency", crossover_hz=0.0)
        self.refuse("frequency", crossover_hz=1e40)            # inf after float32

    def test_gain(self):
        self.refuse("gain", woofer_gain=1.5)
        self.refuse("gain", tweeter_gain=0.0)
        self.assertIsNone(write_profile.refusal({**sample(), "woofer_gain": 1.0}))

    def test_ceiling(self):
        self.refuse("ceiling", woofer_ceiling=1.01)
        self.refuse("ceiling", tweeter_ceiling=0.0)

    def test_reference(self):
        self.refuse("reference", reference_supply_mv=7999.0)
        self.refuse("reference", reference_supply_mv=26001.0)
        self.refuse("reference", reference_supply_mv=0.0)

    def test_timing(self):
        self.refuse("timing", woofer_release_ms=0)
        self.refuse("timing", tweeter_release_ms=0)
        self.assertIsNone(write_profile.refusal({**sample(), "woofer_hold_ms": 0}))

    def test_delay(self):
        self.refuse("delay", woofer_delay_samples=3, tweeter_delay_samples=3)
        self.refuse("delay", woofer_delay_samples=65)
        self.refuse("delay", tweeter_delay_samples=65)
        self.assertIsNone(write_profile.refusal({**sample(), "tweeter_delay_samples": 64}))

    def test_polarity(self):
        self.refuse("polarity", tweeter_polarity=2)

    def test_budget(self):
        self.refuse("budget", supply_budget_sq=0.0)
        self.refuse("budget", supply_budget_sq=3.0)
        self.refuse("budget", supply_window_ms=0)
        self.assertIsNone(write_profile.refusal({**sample(), "supply_budget_sq": 2.0}))

    def test_amp_gain(self):
        self.refuse("amp-gain", amp_gain_db=30)
        for known in (0, 20, 26, 32, 36):
            self.assertIsNone(write_profile.refusal({**sample(), "amp_gain_db": known}))

    def test_floats_are_judged_as_float32(self):
        """1 + 2^-30 is above 1.0 as a double and exactly 1.0 as a float32; the
        device sees the float32, so the tool must accept it as a gain."""
        self.assertIsNone(write_profile.refusal({**sample(), "woofer_gain": 1.0 + 2.0 ** -30}))


class TestValuesFile(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def _write(self, document) -> Path:
        path = Path(self.tmp.name) / "values.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def test_a_complete_file_loads(self):
        profile, provenance = write_profile.load_values(self._write(values_document()))
        self.assertEqual(profile, {**sample(), "reference_supply_mv": 24000.0})
        self.assertEqual(set(provenance), set(write_profile.VALUE_FIELDS))

    def test_a_missing_profile_field_is_named(self):
        p = sample()
        del p["crossover_hz"]
        with self.assertRaises(write_profile.ValuesError) as caught:
            write_profile.load_values(self._write(values_document(p, provenance_for(sample()))))
        self.assertIn("crossover_hz", str(caught.exception))

    def test_a_field_hk_profile_t_does_not_have_is_refused(self):
        """A typo would otherwise be silently ignored next to a missing field."""
        p = sample()
        p["crosover_hz"] = 2200.0
        with self.assertRaises(write_profile.ValuesError) as caught:
            write_profile.load_values(self._write(values_document(p, provenance_for(sample()))))
        self.assertIn("crosover_hz", str(caught.exception))

    def test_a_missing_provenance_entry_is_refused(self):
        provenance = provenance_for(sample())
        del provenance["tweeter_gain"]
        with self.assertRaises(write_profile.ValuesError) as caught:
            write_profile.load_values(self._write(values_document(sample(), provenance)))
        self.assertIn("tweeter_gain", str(caught.exception))

    def test_a_provenance_status_outside_the_three_is_refused(self):
        provenance = provenance_for(sample())
        provenance["crossover_hz"]["status"] = "verified"
        with self.assertRaises(write_profile.ValuesError):
            write_profile.load_values(self._write(values_document(sample(), provenance)))

    def test_a_bool_where_a_number_belongs_is_refused(self):
        p = sample()
        p["tweeter_polarity"] = True
        with self.assertRaises(write_profile.ValuesError):
            write_profile.load_values(self._write(values_document(p, provenance_for(sample()))))

    def test_tonights_values_file_is_honest(self):
        """The record's own file: it must pack, and it may call only the two DC
        resistances measured -- everything else in it is a choice or a placeholder."""
        profile, provenance = write_profile.load_values(TONIGHT)
        self.assertIsNone(write_profile.refusal(profile))
        self.assertEqual(profile["source"], "provisional-2026-09-12")
        self.assertEqual(profile["measured_yyyymmdd"], 20260912)
        measured = sorted(name for name, entry in provenance.items() if entry["status"] == "measured")
        self.assertEqual(measured, ["tweeter_dcr_ohm", "woofer_dcr_ohm"])
        # 3500 Hz is the owner's choice inside a measured range, not a number a
        # measurement produced: the record calls it a placeholder and so must the file.
        self.assertEqual(provenance["crossover_hz"]["status"], "placeholder")
        self.assertEqual(sorted(name for name, entry in provenance.items() if entry["status"] == "derived"), [])
        self.assertEqual(profile["amp_gain_db"], 0, "C3 has not been read")


class TestPartitionTable(unittest.TestCase):
    def test_factory_cal_offset_and_size_come_from_the_table(self):
        offset, size = write_profile.read_partition("factory_cal")
        self.assertEqual(offset, 0x13000)
        self.assertEqual(size, 0xD000)
        self.assertEqual(size, write_profile.provision_credentials.IMAGE_SIZE)


LEGACY_FILES = ("prov_salt.bin", "prov_verif.bin", "ap_pass.bin")


def fake_device_dir(root: Path) -> Path:
    """The shape provision_credentials.write_device() leaves since ADR-0023:
    a CSV with the namespace and the schema row, and nothing the CSV names."""
    device = root / "FAKE"
    device.mkdir()
    (device / "factory_cal.csv").write_text(
        "key,type,encoding,value\n"
        "cal,namespace,,\n"
        "schema,data,u32,1\n",
        encoding="utf-8")
    return device


def legacy_device_dir(root: Path) -> Path:
    """The shape a board provisioned before ADR-0023 left behind (the product
    board and the devkit both have one): the three rows of the retired
    Security 2 design and the files they name, with fake bytes."""
    device = root / "LEGACY"
    device.mkdir()
    (device / "prov_salt.bin").write_bytes(b"s" * 16)
    (device / "prov_verif.bin").write_bytes(b"v" * 384)
    (device / "ap_pass.bin").write_bytes(b"NOTAPASSWORD")
    (device / "factory_cal.csv").write_text(
        "key,type,encoding,value\n"
        "cal,namespace,,\n"
        "schema,data,u32,1\n"
        "prov_salt,file,binary,prov_salt.bin\n"
        "prov_verif,file,binary,prov_verif.bin\n"
        "ap_pass,file,binary,ap_pass.bin\n",
        encoding="utf-8")
    return device


class TestDeviceDirMerge(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.device = fake_device_dir(self.root)
        self.blob = write_profile.pack(sample())

    def _rows(self, device=None):
        return ((device or self.device) / "factory_cal.csv").read_text(encoding="utf-8").splitlines()

    def test_the_row_is_appended_once(self):
        before = self._rows()

        self.assertTrue(write_profile.merge_into_device_dir(self.device, self.blob))
        self.assertEqual(self._rows(), before + ["profile,file,binary,profile.bin"])
        self.assertEqual((self.device / "profile.bin").read_bytes(), self.blob)

        # Idempotent: the second run changes nothing.
        self.assertFalse(write_profile.merge_into_device_dir(self.device, self.blob))
        self.assertEqual(self._rows(), before + ["profile,file,binary,profile.bin"])

        # And nothing but the blob was added to the directory.
        self.assertEqual(sorted(path.name for path in self.device.iterdir()),
                         ["factory_cal.csv", "profile.bin"])

    def test_what_provision_credentials_writes_is_what_this_tool_accepts(self):
        """The contract between the two tools, checked on the real output rather
        than on a fixture: a directory written by provision_credentials.py today
        merges without any file the fixture might have invented."""
        with contextlib.redirect_stdout(None):
            device = provision_credentials.write_device(self.root, "A1B2")
        self.assertTrue(write_profile.merge_into_device_dir(device, self.blob))
        self.assertEqual(self._rows(device)[-1], "profile,file,binary,profile.bin")
        self.assertEqual(sorted(path.name for path in device.iterdir()),
                         ["factory_cal.csv", "label.txt", "profile.bin", "qr.txt"])

    def test_a_legacy_directory_merges_and_keeps_its_rows_and_files(self):
        """The product board (932C) and the devkit (06C4) were provisioned before
        ADR-0023; their directories carry the three rows and files, and a merge
        must leave them exactly as they were. The firmware no longer reads
        them, but the merge is not the place to decide that."""
        device = legacy_device_dir(self.root)
        before = self._rows(device)
        legacy = {name: (device / name).read_bytes() for name in LEGACY_FILES}

        self.assertTrue(write_profile.merge_into_device_dir(device, self.blob))
        self.assertEqual(self._rows(device), before + ["profile,file,binary,profile.bin"])
        self.assertFalse(write_profile.merge_into_device_dir(device, self.blob))
        self.assertEqual(self._rows(device), before + ["profile,file,binary,profile.bin"])

        for name, content in legacy.items():
            self.assertEqual((device / name).read_bytes(), content, name)

    def test_a_directory_without_a_csv_is_refused(self):
        empty = self.root / "EMPTY"
        empty.mkdir()
        with self.assertRaises(write_profile.DeviceDirError) as caught:
            write_profile.merge_into_device_dir(empty, self.blob)
        self.assertIn("factory_cal.csv", str(caught.exception))
        self.assertIn("provision_credentials.py", str(caught.exception))
        self.assertEqual(list(empty.iterdir()), [])

    def test_a_csv_without_a_schema_row_is_refused(self):
        """Without the schema row hk_storage refuses the whole store, profile
        included; an image built from such a CSV would look complete."""
        (self.device / "factory_cal.csv").write_text(
            "key,type,encoding,value\ncal,namespace,,\n", encoding="utf-8")
        with self.assertRaises(write_profile.DeviceDirError) as caught:
            write_profile.merge_into_device_dir(self.device, self.blob)
        self.assertIn("schema", str(caught.exception))
        self.assertFalse((self.device / "profile.bin").exists())

    def test_a_legacy_csv_whose_input_file_is_missing_is_refused(self):
        """A row that names a file the directory lacks would fail inside
        nvs_partition_gen.py, and that failure used to surface as an image with
        nothing in it. It is refused here, by name, before anything is written."""
        device = legacy_device_dir(self.root)
        (device / "prov_verif.bin").unlink()
        with self.assertRaises(write_profile.DeviceDirError) as caught:
            write_profile.merge_into_device_dir(device, self.blob)
        self.assertIn("prov_verif.bin", str(caught.exception))
        self.assertFalse((device / "profile.bin").exists())
        self.assertNotIn("profile,file,binary,profile.bin", self._rows(device))

    def test_a_different_profile_row_is_refused_rather_than_guessed(self):
        csv_path = self.device / "factory_cal.csv"
        csv_path.write_text(csv_path.read_text() + "profile,file,binary,other.bin\n")
        with self.assertRaises(write_profile.DeviceDirError):
            write_profile.merge_into_device_dir(self.device, self.blob)

    def test_a_csv_whose_last_namespace_is_not_cal_is_refused(self):
        """An appended row lands in the last namespace declared, which would be
        a profile the firmware never finds."""
        csv_path = self.device / "factory_cal.csv"
        csv_path.write_text(csv_path.read_text() + "other,namespace,,\nx,data,u32,1\n")
        with self.assertRaises(write_profile.DeviceDirError) as caught:
            write_profile.merge_into_device_dir(self.device, self.blob)
        self.assertIn("cal", str(caught.exception))

    def test_namespace_rows_files_each_row_under_the_namespace_declared_before_it(self):
        text = ("key,type,encoding,value\n"
                "cal,namespace,,\n"
                "schema,data,u32,1\n"
                "other,namespace,,\n"
                "x,data,u32,1\n")
        self.assertEqual([row[0] for row in write_profile.namespace_rows(text, "cal")], ["schema"])
        self.assertEqual([row[0] for row in write_profile.namespace_rows(text, "other")], ["x"])
        self.assertEqual(write_profile.namespace_rows(text, "none"), [])


# ---------------------------------------------------------------- an NVS page by hand

def nvs_entry(ns: int, kind: int, span: int, chunk: int, key: str, data: bytes) -> bytes:
    """One 32-byte entry as nvs_partition_gen.py lays it out: header CRC over
    bytes 0-3 and 8-31, key NUL-padded to 16, eight bytes of data."""
    entry = bytearray(32)
    entry[0], entry[1], entry[2], entry[3] = ns, kind, span, chunk
    entry[8:24] = key.encode("ascii").ljust(16, b"\0")
    entry[24:32] = data
    crc = zlib.crc32(bytes(entry[0:4]) + bytes(entry[8:32]), 0xFFFFFFFF) & 0xFFFFFFFF
    struct.pack_into("<I", entry, 4, crc)
    return bytes(entry)


def nvs_page_with(entries: list[bytes]) -> bytes:
    """A 4096-byte page holding `entries` (each 32 bytes, data rows included),
    every one marked written in the state bitmap."""
    page = bytearray(b"\xff" * 4096)
    struct.pack_into("<I", page, 0, 0xFFFFFFFE)   # ACTIVE
    bitmap = bytearray(b"\xff" * 32)
    for index, entry in enumerate(entries):
        page[64 + index * 32:64 + (index + 1) * 32] = entry
        bitmap[index // 4] &= ~(1 << ((index % 4) * 2)) & 0xFF
    page[32:64] = bitmap
    return bytes(page)


def nvs_image_with_profile(blob: bytes, with_legacy_rows=False, corrupt=False) -> bytes:
    """A page with the `cal` namespace, its schema row (a u32, as
    nvs_partition_gen.py stores one), the profile, and -- for a board
    provisioned before ADR-0023 -- the three legacy blobs."""
    ns = 1
    rows = [nvs_entry(0, 0x01, 1, 0xFF, "cal", bytes([ns]) + b"\xff" * 7),
            nvs_entry(ns, 0x04, 1, 0xFF, "schema", struct.pack("<I", 1) + b"\xff" * 4)]
    if with_legacy_rows:
        for key in ("prov_salt", "prov_verif", "ap_pass"):
            payload = b"x" * 16
            rows.append(nvs_entry(ns, 0x42, 2, 0, key,
                                  struct.pack("<HHI", 16, 0xFFFF, zlib.crc32(payload, 0xFFFFFFFF) & 0xFFFFFFFF)))
            rows.append(payload.ljust(32, b"\xff"))
            rows.append(nvs_entry(ns, 0x48, 1, 0xFF, key, struct.pack("<IBBH", 16, 1, 0, 0xFFFF)))
    crc = zlib.crc32(blob, 0xFFFFFFFF) & 0xFFFFFFFF
    if corrupt:
        crc ^= 1
    rows.append(nvs_entry(ns, 0x42, 1 + (len(blob) + 31) // 32, 0, "profile",
                          struct.pack("<HHI", len(blob), 0xFFFF, crc)))
    padded = blob.ljust((len(blob) + 31) // 32 * 32, b"\xff")
    rows.extend(padded[i:i + 32] for i in range(0, len(padded), 32))
    rows.append(nvs_entry(ns, 0x48, 1, 0xFF, "profile", struct.pack("<IBBH", len(blob), 1, 0, 0xFFFF)))
    return nvs_page_with(rows) + b"\xff" * 4096 * 2


class TestNvsReader(unittest.TestCase):
    def test_the_profile_is_found_in_a_hand_built_image(self):
        blob = write_profile.pack(sample())
        image = nvs_image_with_profile(blob)
        self.assertEqual(write_profile.extract_blob(image, "cal", "profile"), blob)
        self.assertEqual(sorted(write_profile.namespace_keys(image, "cal")), ["profile", "schema"])

    def test_a_legacy_image_lists_its_rows_too(self):
        """What the product board's readback shows today: the reader names the
        legacy keys like any other, and --dump says what they are."""
        blob = write_profile.pack(sample())
        image = nvs_image_with_profile(blob, with_legacy_rows=True)
        self.assertEqual(sorted(write_profile.namespace_keys(image, "cal")),
                         ["ap_pass", "profile", "prov_salt", "prov_verif", "schema"])
        self.assertEqual(write_profile.extract_blob(image, "cal", "profile"), blob)

    def test_payload_rows_are_not_mistaken_for_keys(self):
        """A blob's data rows follow its header; a data row whose first byte
        happens to equal the namespace index must not be listed as a key."""
        source = "x" * 24 + "\x01" + "y" * 6         # byte 24 of the source is 0x01, ns index 1
        blob = write_profile.pack({**sample(), "source": source})
        image = nvs_image_with_profile(blob, with_legacy_rows=True)
        self.assertEqual(sorted(write_profile.namespace_keys(image, "cal")),
                         ["ap_pass", "profile", "prov_salt", "prov_verif", "schema"])
        self.assertEqual(write_profile.extract_blob(image, "cal", "profile"), blob)

    def test_no_profile_is_none_not_garbage(self):
        image = nvs_image_with_profile(write_profile.pack(sample()))
        self.assertIsNone(write_profile.extract_blob(image, "cal", "nothing"))
        self.assertIsNone(write_profile.extract_blob(image, "other", "profile"))

    def test_a_chunk_that_fails_its_crc_is_an_error_not_a_profile(self):
        image = nvs_image_with_profile(write_profile.pack(sample()), corrupt=True)
        with self.assertRaises(write_profile.ValuesError):
            write_profile.extract_blob(image, "cal", "profile")


class TestCommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def _run(self, *args):
        return subprocess.run([sys.executable, str(TOOL), *args], capture_output=True, text=True)

    def _values(self, document) -> Path:
        path = self.root / "values.json"
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def test_writes_the_blob_and_says_it_is_not_an_image(self):
        out = self.root / "out"
        result = self._run(str(self._values(values_document())), "--out", str(out))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((out / "profile.bin").read_bytes(), write_profile.pack(sample()))
        self.assertIn("NOT a partition image", result.stdout)
        self.assertIn("0x13000", result.stdout)
        self.assertIn("placeholder", result.stdout)

    def test_a_refused_profile_writes_nothing_and_names_the_verdict(self):
        p = sample()
        p["woofer_hpf_hz"] = 5000.0
        out = self.root / "out"
        result = self._run(str(self._values(values_document(p, provenance_for(sample())))),
                           "--out", str(out))
        self.assertEqual(result.returncode, 1)
        self.assertIn("REFUSED (frequency)", result.stderr)
        self.assertFalse((out / "profile.bin").exists())

    def test_dump_prints_every_field_and_the_verdict(self):
        blob_path = self.root / "profile.bin"
        blob_path.write_bytes(write_profile.pack(sample()))
        result = self._run("--dump", str(blob_path))
        self.assertEqual(result.returncode, 0, result.stderr)
        for name, _ in write_profile.FIELDS:
            self.assertIn(name, result.stdout)
        self.assertIn("verdict: ok", result.stdout)

    def test_dump_of_a_refused_blob_exits_nonzero_with_the_word(self):
        p = sample()
        p["tweeter_polarity"] = 2
        blob_path = self.root / "profile.bin"
        # Packed by hand: pack() would refuse it, and the point is a stored blob.
        blob_path.write_bytes(struct.pack(
            write_profile.STRUCT_FORMAT,
            *[0 if name == "reserved" else (p[name].encode() if name == "source" else p[name])
              for name, _ in write_profile.FIELDS]))
        result = self._run("--dump", str(blob_path))
        self.assertEqual(result.returncode, 1)
        self.assertIn("verdict: polarity", result.stderr)

    def test_dump_of_an_erased_partition_is_named(self):
        erased = self.root / "readback.bin"
        erased.write_bytes(b"\xff" * 0xD000)
        result = self._run("--dump", str(erased))
        self.assertEqual(result.returncode, 1)
        self.assertIn("erased", result.stderr)

    def test_dump_of_a_legacy_image_names_the_rows_as_dead_data(self):
        """A readback from the product board lists the three legacy keys; the
        dump must say they are the retired design's rows, not warn about them
        and not treat their absence elsewhere as a fault."""
        image = self.root / "readback.bin"
        image.write_bytes(nvs_image_with_profile(write_profile.pack(sample()), with_legacy_rows=True))
        result = self._run("--dump", str(image))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("legacy rows", result.stdout)
        self.assertIn("ADR-0023", result.stdout)
        self.assertIn("verdict: ok", result.stdout)
        self.assertEqual(result.stderr, "")

    def test_dump_of_a_fresh_image_warns_about_nothing(self):
        image = self.root / "readback.bin"
        image.write_bytes(nvs_image_with_profile(write_profile.pack(sample())))
        result = self._run("--dump", str(image))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("profile, schema", result.stdout)
        self.assertNotIn("legacy", result.stdout)
        self.assertEqual(result.stderr, "")

    def test_device_dir_without_idf_refuses_before_touching_the_directory(self):
        device = fake_device_dir(self.root)
        before = (device / "factory_cal.csv").read_text()
        env = {key: value for key, value in os.environ.items() if key != "IDF_PATH"}
        result = subprocess.run(
            [sys.executable, str(TOOL), str(self._values(values_document())), "--device-dir", str(device)],
            capture_output=True, text=True, env=env)
        self.assertEqual(result.returncode, 1)
        self.assertIn("IDF_PATH", result.stderr)
        self.assertEqual((device / "factory_cal.csv").read_text(), before)
        self.assertFalse((device / "profile.bin").exists())

    def test_the_tool_never_flashes(self):
        """It prints the esptool command; it must not be able to run one."""
        source = TOOL.read_text()
        self.assertNotIn("esptool\"", source.replace("-m esptool", ""))
        self.assertNotIn("subprocess", source)


def idf_nvs_generator_available() -> bool:
    """nvs_partition_gen.py in ESP-IDF v5.5 wraps a pip package that lives only
    in the IDF python env; IDF_PATH alone does not make it runnable."""
    return bool(os.environ.get("IDF_PATH")) and importlib.util.find_spec("esp_idf_nvs_partition_gen") is not None


@unittest.skipUnless(idf_nvs_generator_available(),
                     "not under the IDF python env (IDF_PATH set and esp_idf_nvs_partition_gen "
                     "importable): the image build needs ESP-IDF's nvs_partition_gen.py")
class TestImageBuild(unittest.TestCase):
    """The merge through provision_credentials.build_image(), and the image read
    back through the same reader --dump uses. Runs only under the IDF python env."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.device = fake_device_dir(self.root)
        self.blob = write_profile.pack(sample())

    def test_the_built_image_carries_the_schema_and_the_profile_and_nothing_else(self):
        write_profile.merge_into_device_dir(self.device, self.blob)
        image = write_profile.build_device_image(self.device, self.blob)
        raw = image.read_bytes()
        self.assertEqual(len(raw), write_profile.provision_credentials.IMAGE_SIZE)
        self.assertEqual(sorted(write_profile.namespace_keys(raw, "cal")), ["profile", "schema"])
        self.assertEqual(write_profile.extract_blob(raw, "cal", "profile"), self.blob)
        self.assertEqual(write_profile.unpack(write_profile.extract_blob(raw, "cal", "profile"))["crossover_hz"],
                         write_profile.float32(sample()["crossover_hz"]))

    def test_a_legacy_directory_keeps_its_rows_in_the_built_image(self):
        device = legacy_device_dir(self.root)
        write_profile.merge_into_device_dir(device, self.blob)
        image = write_profile.build_device_image(device, self.blob)
        raw = image.read_bytes()
        self.assertEqual(sorted(write_profile.namespace_keys(raw, "cal")),
                         ["ap_pass", "profile", "prov_salt", "prov_verif", "schema"])
        self.assertEqual(write_profile.extract_blob(raw, "cal", "profile"), self.blob)

    def test_dump_reads_the_built_image(self):
        write_profile.merge_into_device_dir(self.device, self.blob)
        image = write_profile.build_device_image(self.device, self.blob)
        result = subprocess.run([sys.executable, str(TOOL), "--dump", str(image)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("an NVS image", result.stdout)
        self.assertIn("profile, schema", result.stdout)
        self.assertIn("verdict: ok", result.stdout)
        self.assertNotIn("legacy", result.stdout)

    def test_end_to_end_from_provision_credentials_writes_no_secret(self):
        """The whole path an operator runs: provision_credentials.py writes the
        device directory and its image, this tool merges the record's own
        values file through the command line, and --dump reads the result. On
        the way, nothing that looks like a secret may appear in the directory:
        no .bin but the profile and the image, no restricted file, and no
        password or PoP word in any text file."""
        out = self.root / "devices"
        gen = subprocess.run([sys.executable, str(TOOL.parent / "provision_credentials.py"),
                              "--device", "E2E1", "--image", "--out", str(out)],
                             capture_output=True, text=True)
        self.assertEqual(gen.returncode, 0, gen.stderr)
        device = out / "E2E1"
        self.assertEqual(sorted(path.name for path in device.iterdir()),
                         ["factory_cal.bin", "factory_cal.csv", "label.txt", "qr.txt"])

        result = subprocess.run([sys.executable, str(TOOL), str(TONIGHT), "--device-dir", str(device)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("every row kept", result.stdout)
        self.assertNotIn("legacy", result.stdout)
        self.assertIn("write_flash", result.stdout)

        self.assertEqual(sorted(path.name for path in device.iterdir()),
                         ["factory_cal.bin", "factory_cal.csv", "label.txt", "profile.bin", "qr.txt"])
        for path in device.iterdir():
            self.assertNotEqual(path.stat().st_mode & 0o777, 0o600, f"{path.name} is restricted, so it claims a secret")
        for name in ("factory_cal.csv", "label.txt", "qr.txt"):
            text = (device / name).read_text(encoding="utf-8").lower()
            for word in ("password", "pop", "username", "salt", "verif"):
                self.assertNotIn(word, text, f"{name} carries {word!r}")

        raw = (device / "factory_cal.bin").read_bytes()
        self.assertEqual(len(raw), write_profile.provision_credentials.IMAGE_SIZE)
        self.assertEqual(sorted(write_profile.namespace_keys(raw, "cal")), ["profile", "schema"])

        dumped = subprocess.run([sys.executable, str(TOOL), "--dump", str(device / "factory_cal.bin")],
                                capture_output=True, text=True)
        self.assertEqual(dumped.returncode, 0, dumped.stderr)
        self.assertIn("'provisional-2026-09-12'", dumped.stdout)
        self.assertIn("verdict: ok", dumped.stdout)


if __name__ == "__main__":
    unittest.main()
