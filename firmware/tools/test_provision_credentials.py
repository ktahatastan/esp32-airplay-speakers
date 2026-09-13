#!/usr/bin/env python3
"""Tests for provision_credentials.py.

Since ADR-0023 the tool's job is to write a device directory that holds no
secret: a bare `factory_cal.csv`, the two QR payloads and a label. So the cases
are mostly about what must NOT be there -- no password, no proof of
possession, no username, no restricted file, no salt or verifier -- and about
the two things the phone apps read: the setup name (the `PROV_` prefix that
lets the stock apps list the device without a QR, and the same string for BLE
and the SSID) and the QR payload's fields. The name is cross-checked against
the host test of hk_identity, so the label and the firmware cannot drift apart.

The image build needs ESP-IDF's nvs_partition_gen.py under the IDF python env
and is skipped with a message otherwise; everything else runs bare.
"""
import contextlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import provision_credentials  # noqa: E402

TOOL = Path(__file__).resolve().parent / "provision_credentials.py"
FIRMWARE = Path(__file__).resolve().parents[1]
IDENTITY_TEST = FIRMWARE / "test" / "test_identity.c"
IDENTITY_HEADER = FIRMWARE / "components" / "hk_identity" / "include" / "hk_identity.h"

#: Words that would mean a secret, or the retired design, got written.
SECRET_WORDS = ("password", "passwd", "pop", "username", "salt", "verif", "srp")


def write_device(root: Path, device_id: str) -> Path:
    """write_device() without its progress line, so a test run reads clean."""
    with contextlib.redirect_stdout(None):
        return provision_credentials.write_device(root, device_id)


class TestSetupName(unittest.TestCase):
    def test_one_name_for_ble_and_the_ssid(self):
        self.assertEqual(provision_credentials.setup_name("A1B2"), "PROV_Merzarkabul-A1B2")
        ble = json.loads(provision_credentials.qr_payload("A1B2", "ble"))["name"]
        softap = json.loads(provision_credentials.qr_payload("A1B2", "softap"))["name"]
        self.assertEqual(ble, softap)
        self.assertEqual(ble, "PROV_Merzarkabul-A1B2")

    def test_the_prefix_is_the_one_the_stock_apps_filter_by(self):
        """Both Espressif sample apps list only BLE devices whose name starts
        with PROV_ until the user changes a setting; the prefix is what makes
        'no QR' work without that step."""
        self.assertTrue(provision_credentials.setup_name("A1B2").startswith("PROV_"))

    def test_the_name_fits_a_ble_advertisement_and_an_ssid(self):
        """29 octets for a BLE local name in one advertising packet (31 minus
        the 2-octet AD header), 32 for an SSID -- the same limits hk_identity.h
        asserts at compile time."""
        name = provision_credentials.setup_name("A1B2")
        self.assertLessEqual(len(name.encode("ascii")), 29)
        self.assertLessEqual(len(name.encode("ascii")), 32)

    def test_the_name_matches_the_firmware_host_test(self):
        """test_identity.c pins the names hk_identity derives for the MAC
        ending A1:B2; the label must print the same string, or the QR names a
        device that does not exist."""
        source = IDENTITY_TEST.read_text(encoding="utf-8")
        expected = f'"{provision_credentials.setup_name("A1B2")}"'
        self.assertIn(expected, source,
                      f"{IDENTITY_TEST.name} does not expect {expected}: the tool and hk_identity disagree")

    def test_the_prefix_is_spelled_in_the_identity_header(self):
        source = IDENTITY_HEADER.read_text(encoding="utf-8")
        self.assertRegex(source, r'#define HK_NAME_PREFIX_SETUP\s+"PROV_"',
                         "hk_identity.h no longer defines the setup prefix as PROV_")


class TestQrPayload(unittest.TestCase):
    def test_fields_are_exactly_ver_name_transport_security(self):
        for transport in ("ble", "softap"):
            payload = json.loads(provision_credentials.qr_payload("A1B2", transport))
            self.assertEqual(sorted(payload), ["name", "security", "transport", "ver"], transport)
            self.assertEqual(payload["ver"], "v1")
            self.assertEqual(payload["transport"], transport)
            self.assertEqual(payload["name"], "PROV_Merzarkabul-A1B2")

    def test_security_is_1_the_level_the_device_advertises(self):
        """Security 1 with no proof of possession (ADR-0023). Written because
        both Espressif libraries assume 2 when the field is absent."""
        payload = json.loads(provision_credentials.qr_payload("A1B2", "ble"))
        self.assertEqual(payload["security"], 1)
        self.assertEqual(provision_credentials.SECURITY, 1)

    def test_no_pop_no_username_no_password(self):
        for transport in ("ble", "softap"):
            payload = json.loads(provision_credentials.qr_payload("A1B2", transport))
            for field in ("pop", "username", "password"):
                self.assertNotIn(field, payload, transport)

    def test_the_payload_is_compact_json(self):
        """One line, no spaces: the same shape ESP-IDF prints for its own QR,
        which is what the apps were written against."""
        text = provision_credentials.qr_payload("A1B2", "ble")
        self.assertEqual(text, '{"ver":"v1","name":"PROV_Merzarkabul-A1B2","transport":"ble","security":1}')

    def test_an_unknown_transport_is_refused(self):
        with self.assertRaises(ValueError):
            provision_credentials.qr_payload("A1B2", "wifi")


class TestDeviceDirectory(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def _texts(self, device: Path) -> dict:
        return {path.name: path.read_text(encoding="utf-8") for path in device.iterdir()}

    def test_exactly_three_files_and_no_bin(self):
        device = write_device(self.root, "A1B2")
        self.assertEqual(device, self.root / "A1B2")
        self.assertEqual(sorted(path.name for path in device.iterdir()),
                         ["factory_cal.csv", "label.txt", "qr.txt"])

    def test_the_csv_is_the_namespace_and_the_schema_row_only(self):
        device = write_device(self.root, "A1B2")
        self.assertEqual((device / "factory_cal.csv").read_text(encoding="utf-8"),
                         "key,type,encoding,value\n"
                         "cal,namespace,,\n"
                         "schema,data,u32,1\n")

    def test_qr_txt_is_the_two_payloads_one_per_line(self):
        device = write_device(self.root, "A1B2")
        lines = (device / "qr.txt").read_text(encoding="utf-8").splitlines()
        self.assertEqual(lines, [provision_credentials.qr_payload("A1B2", "ble"),
                                 provision_credentials.qr_payload("A1B2", "softap")])

    def test_the_label_carries_the_name_and_the_qr_text(self):
        device = write_device(self.root, "A1B2")
        label = (device / "label.txt").read_text(encoding="utf-8")
        self.assertIn("Merzarkabul Airplay Speakers A1B2", label)
        self.assertIn("PROV_Merzarkabul-A1B2", label)
        self.assertIn(provision_credentials.qr_payload("A1B2", "ble"), label)
        self.assertIn(provision_credentials.qr_payload("A1B2", "softap"), label)

    def test_no_secret_is_written(self):
        """The point of the change. No file may carry a password, a proof of
        possession, a username, a salt or a verifier, and no file may be
        restricted -- a 600 mode would tell the reader there is something
        to protect."""
        device = write_device(self.root, "A1B2")
        for name, text in self._texts(device).items():
            lowered = text.lower()
            for word in SECRET_WORDS:
                self.assertNotIn(word, lowered, f"{name} carries {word!r}")
        for path in device.iterdir():
            self.assertNotEqual(path.stat().st_mode & 0o777, 0o600, f"{path.name} is restricted")

    def test_writing_twice_is_the_same_directory(self):
        first = self._texts(write_device(self.root, "A1B2"))
        second = self._texts(write_device(self.root, "A1B2"))
        self.assertEqual(first, second)

    def test_an_existing_csv_with_its_own_rows_is_kept(self):
        """A merged profile row, or the legacy rows of a board set up before
        ADR-0023, survive a second run: only the name files are rewritten."""
        device = write_device(self.root, "A1B2")
        csv_path = device / "factory_cal.csv"
        merged = csv_path.read_text(encoding="utf-8") + "profile,file,binary,profile.bin\n"
        csv_path.write_text(merged, encoding="utf-8")
        write_device(self.root, "A1B2")
        self.assertEqual(csv_path.read_text(encoding="utf-8"), merged)
        legacy = ("key,type,encoding,value\ncal,namespace,,\nschema,data,u32,1\n"
                  "prov_salt,file,binary,prov_salt.bin\nprov_verif,file,binary,prov_verif.bin\n")
        csv_path.write_text(legacy, encoding="utf-8")
        write_device(self.root, "A1B2")
        self.assertEqual(csv_path.read_text(encoding="utf-8"), legacy)

    def test_the_tool_has_no_way_to_make_a_secret(self):
        """Not by construction alone: the source must not import a random
        source or ESP-IDF's SRP module, and must not chmod anything."""
        source = TOOL.read_text(encoding="utf-8")
        self.assertNotIn("import secrets", source)
        self.assertNotIn("srp6a", source.lower())
        self.assertNotIn("chmod", source)
        self.assertNotIn("0o600", source)


class TestCommandLine(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def _run(self, *args, env=None):
        return subprocess.run([sys.executable, str(TOOL), *args],
                              capture_output=True, text=True, env=env)

    def test_a_directory_and_a_label_need_no_idf(self):
        env = {key: value for key, value in os.environ.items() if key != "IDF_PATH"}
        result = self._run("--device", "A1B2", "--out", str(self.root), env=env)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.root / "A1B2" / "label.txt").is_file())
        self.assertIn("PROV_Merzarkabul-A1B2", result.stdout)

    def test_nothing_printed_looks_like_a_secret(self):
        result = self._run("--device", "A1B2", "--out", str(self.root))
        self.assertEqual(result.returncode, 0, result.stderr)
        lowered = result.stdout.replace(str(self.root), "<out>").lower()
        for word in ("password", "only copy", "pop", "salt", "verif"):
            self.assertNotIn(word, lowered, word)
        self.assertIn("no pin", lowered)

    def test_count_writes_placeholder_ids(self):
        result = self._run("--count", "2", "--out", str(self.root))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(sorted(path.name for path in self.root.iterdir()), ["DEV1", "DEV2"])

    def test_image_without_idf_is_an_error_after_the_directory_is_written(self):
        env = {key: value for key, value in os.environ.items() if key != "IDF_PATH"}
        result = self._run("--device", "A1B2", "--image", "--out", str(self.root), env=env)
        self.assertEqual(result.returncode, 1)
        self.assertIn("IDF_PATH", result.stderr)
        self.assertTrue((self.root / "A1B2" / "factory_cal.csv").is_file())
        self.assertFalse((self.root / "A1B2" / "factory_cal.bin").exists())


def idf_nvs_generator_available() -> bool:
    return bool(os.environ.get("IDF_PATH")) and importlib.util.find_spec("esp_idf_nvs_partition_gen") is not None


@unittest.skipUnless(idf_nvs_generator_available(),
                     "not under the IDF python env (IDF_PATH set and esp_idf_nvs_partition_gen "
                     "importable): the image build needs ESP-IDF's nvs_partition_gen.py")
class TestImage(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def test_the_image_is_the_partition_size_and_carries_only_the_schema(self):
        sys.path.insert(0, str(TOOL.parent))
        import write_profile  # the reader --dump uses; the two tools share the format
        device = write_device(self.root, "A1B2")
        image = provision_credentials.build_image(device)
        raw = image.read_bytes()
        self.assertEqual(len(raw), provision_credentials.IMAGE_SIZE)
        self.assertEqual(len(raw), 0xD000)
        self.assertEqual(sorted(write_profile.namespace_keys(raw, "cal")), ["schema"])

    def test_the_command_line_builds_it(self):
        result = subprocess.run([sys.executable, str(TOOL), "--device", "A1B2", "--image", "--out", str(self.root)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("factory_cal.bin", result.stdout)
        self.assertEqual((self.root / "A1B2" / "factory_cal.bin").stat().st_size, 0xD000)


if __name__ == "__main__":
    unittest.main()
