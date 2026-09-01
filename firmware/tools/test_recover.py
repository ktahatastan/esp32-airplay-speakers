#!/usr/bin/env python3
"""Tests for recover.py.

The script exists to make one mistake hard, so the tests are mostly about
whether that mistake is actually prevented: a recovery flash must never reach
`factory_cal`, because the measurements in it cost bench time and nothing in
software can produce them again.
"""
import csv
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import recover  # noqa: E402

TOOL = Path(__file__).resolve().parent / "recover.py"
FIRMWARE = Path(__file__).resolve().parents[1]


class TestOffsets(unittest.TestCase):
    def test_offsets_come_from_the_partition_table(self):
        """Hard-coding them here is how a repartition silently starts writing
        into the wrong place."""
        offsets = recover.read_offsets()
        self.assertEqual(offsets["otadata"], 0xF000)
        self.assertEqual(offsets["ota_0"], 0x20000)
        self.assertEqual(offsets["factory_cal"], 0x13000)

    def test_the_table_and_the_script_agree_on_every_preserved_name(self):
        offsets = recover.read_offsets()
        for name in recover.PRESERVE:
            self.assertIn(name, offsets, f"{name} is guarded but not in partitions.csv")

    def test_calibration_is_guarded(self):
        """The whole point. If this list ever loses factory_cal, a recovery
        becomes a way to destroy a G0/G2 afternoon."""
        self.assertIn("factory_cal", recover.PRESERVE)
        self.assertIn("nvs", recover.PRESERVE)


class TestOverlapGuard(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.offsets = recover.read_offsets()

    def _file(self, name, size):
        path = Path(self.tmp.name) / name
        path.write_bytes(b"\0" * size)
        return path

    def test_a_normal_plan_passes(self):
        items = [
            (0x0, self._file("boot.bin", 0x5800)),
            (0x8000, self._file("part.bin", 0xC00)),
            (self.offsets["otadata"], self._file("otadata.bin", 0x2000)),
            (self.offsets["ota_0"], self._file("app.bin", 0x111000)),
        ]
        recover.check_no_overlap(items, self.offsets)   # must not raise

    def test_an_oversized_bootloader_that_would_reach_nvs_is_refused(self):
        """A bootloader larger than the gap before nvs would take the user's
        settings with it, and then keep going into factory_cal."""
        items = [(0x0, self._file("boot.bin", 0x20000))]
        with self.assertRaises(recover.RecoveryError) as caught:
            recover.check_no_overlap(items, self.offsets)
        self.assertIn("nvs", str(caught.exception))

    def test_an_image_written_over_calibration_is_refused(self):
        items = [(0x12000, self._file("wrong.bin", 0x4000))]
        with self.assertRaises(recover.RecoveryError) as caught:
            recover.check_no_overlap(items, self.offsets)
        self.assertIn("factory_cal", str(caught.exception))

    def test_an_application_that_runs_past_its_slot_into_storage_is_refused(self):
        """ota_0 is 0x6e0000 and ota_1 follows it, but an image big enough to
        cross both would reach the spiffs partition."""
        items = [(self.offsets["ota_0"], self._file("huge.bin", 0xE00000))]
        with self.assertRaises(recover.RecoveryError) as caught:
            recover.check_no_overlap(items, self.offsets)
        self.assertIn("storage", str(caught.exception))


class TestCommand(unittest.TestCase):
    def test_dry_run_writes_four_regions_and_names_what_it_spares(self):
        result = subprocess.run(
            [sys.executable, str(TOOL), "--dry-run"],
            capture_output=True, text=True, cwd=str(FIRMWARE))
        if "missing" in result.stderr:
            self.skipTest("firmware not built")
        self.assertEqual(result.returncode, 0, result.stderr)

        # every preserved partition is named to the operator
        for name in recover.PRESERVE:
            self.assertIn(name, result.stdout)

        # the four boot regions, and nothing else
        self.assertIn("0x0 ", result.stdout)
        self.assertIn("0x8000 ", result.stdout)
        self.assertIn("0xf000 ", result.stdout)
        self.assertIn("0x20000 ", result.stdout)

    def test_it_never_offers_to_erase_the_whole_chip(self):
        """`esptool erase_flash` is one command away from this one and takes
        the calibration with it."""
        source = TOOL.read_text()
        self.assertNotIn("erase_flash", source.replace("erase_flash        <-", ""))
        self.assertNotIn("--erase-all", source)

    def test_a_missing_build_is_reported_rather_than_flashed(self):
        result = subprocess.run(
            [sys.executable, str(TOOL), "--dry-run", "--build", "no-such-build"],
            capture_output=True, text=True, cwd=str(FIRMWARE))
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing", result.stderr)


if __name__ == "__main__":
    unittest.main()
