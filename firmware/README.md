# Harman Kardom firmware

ESP32-S3 firmware for one speaker. Four speakers run the same image and are told
apart by a device identity derived from their MAC.

**Stage F0.** This build comes up, reports what it is, and proves the skeleton
links. It plays no audio, joins no network and drives no GPIO. What comes next,
in order and with acceptance criteria, is in
[docs/03-firmware/firmware-plan.md](../docs/03-firmware/firmware-plan.md).

## Locked inputs

| Input | Value | Source |
|---|---|---|
| Board | ESP32-S3, 16 MB flash + 8 MB octal PSRAM (`N16R8`) | ADR-0010 |
| ESP-IDF | `v5.5.1`, pinned | this file and `.github/workflows/firmware-ci.yml` |
| Audio topology | mono program, bi-amp: left path woofer, right path tweeter | ADR-0002 |
| Distribution | SemVer tag, GitHub Releases, signed A/B OTA | ADR-0008 |
| AirPlay stack | **not chosen** | ADR-0007 is open |

The GPIO assignment is a *candidate*, not accepted: it holds until the purchased
board's own schematic and a boot test confirm it.

## Build

```bash
git clone --branch v5.5.1 --depth 1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
~/esp/esp-idf/install.sh esp32s3
. ~/esp/esp-idf/export.sh
idf.py -C firmware build
```

Flash and watch the boot report:

```bash
idf.py -C firmware -p /dev/tty.usbmodem* flash monitor
```

`PROJECT_VER` comes from `version.txt`. It must stay strict SemVer, because the
OTA client compares it numerically and the release pipeline checks it against the
Git tag.

## Verify

Three checks run without any hardware, and all three run in CI.

```bash
# 1. Host unit tests: pure logic, no ESP-IDF needed
cmake -S firmware/test -B build/host-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure

# 2. Partition layout, and the image size gate once a build exists
python3 firmware/tools/check_partitions.py
python3 firmware/tools/check_partitions.py --app-size firmware/build/harman-kardom.bin

# 3. The partition validator's own tests, so a gate that accepts everything
#    cannot pass unnoticed
python3 firmware/tools/test_check_partitions.py

# 4. Documentation integrity
python3 scripts/check_docs.py
```

### What has been verified, and what has not

Verified on 2026-08-31 with ESP-IDF v5.5.1 on macOS: the project builds clean
with no warnings in project sources, the host suite passes, the partition gate
and its own tests pass, and `PROJECT_VER` in the built image matches
`version.txt`.

The button, LED and provisioning modules are complete policy, fully covered by
host tests. What is missing is the layer beneath them: no GPIO is configured,
no PWM runs, no radio is started. `app_main` prints what those policies decide
and acts on none of it.

**Not verified: nothing has run on hardware.** No board has been flashed, so the
boot report, the GPIO assignment and the PSRAM detection are unexercised. The
pin table stays a candidate until that happens.

## Layout

```text
firmware/
  CMakeLists.txt        project definition; PROJECT_VER comes from version.txt
  sdkconfig.defaults    board, partition, PSRAM and rollback settings
  partitions.csv        16 MB layout: dual OTA slots + isolated calibration
  version.txt           strict SemVer, compared by the OTA client
  main/                 app_main: boot report only at F0
  components/
    hk_pins/            GPIO assignment; the compiler enforces the constraints
    hk_identity/        every user-visible name, derived from the MAC
    hk_version/         SemVer parsing and the OTA update decision
    hk_button/          function button: debounce, hold levels, what commits
    hk_led/             which status wins the single LED, and how it looks
    hk_provision/       when the setup radios are open, and when they shut
  test/                 host unit tests, built with plain CMake
  tools/                partition and size validation
```

Components with no ESP-IDF dependency are deliberately pure C. That is what
makes them testable on a laptop, years before a driver is safe to energise.

## What the compiler enforces

`components/hk_pins/include/hk_pins.h` is the single source of truth for GPIO
assignment, and it is not merely documentation:

- no two functions may share a pin
- no pin may land on a strapping pin (`GPIO0/3/45/46`) or on the native USB pair
  (`GPIO19/20`), which the documented USB/UART recovery path needs
- no pin may exceed the highest ESP32-S3 GPIO

Each is a `_Static_assert`, so a violation fails the build with a readable
message instead of producing a board that boots into the wrong mode. The host
test additionally checks the table against the pin table published in
[the wiring plan](../docs/02-hardware/circuit-and-wiring-plan.md), function by
function, so a documentation change and a code change cannot drift apart
silently.

## Safety

This firmware will eventually drive an amplifier connected to drivers whose
impedance has not been measured. Until the relevant gate passes:

- no code path may raise output level on a real driver (G0, G2)
- the tweeter path stays muted without a verified high-pass and limiter
- a user reset must never erase `factory_cal`
- OTA must not start on low battery, high temperature or during playback

An automated test can show that logic behaves. It cannot show that a gate
passed; only a recorded operator measurement can.
