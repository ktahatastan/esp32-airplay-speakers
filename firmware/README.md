# Merzarkabul Airplay Speakers firmware

ESP32-S3 firmware for the speaker: one cabinet, one board, one device on the
network (ADR-0021). Every user-visible name carries a suffix derived from the MAC
so it is unique on any network.

**F0 is closed, and F4/F5 have run on real silicon.** On the N8R2 bring-up
devkit (ADR-0012) this firmware joins Wi-Fi, answers mDNS, provisions over both
SoftAP and BLE, drives the button and the LED, and runs the vendored AirPlay
receiver: on 2026-09-05 an iPhone streamed to it and the audio was heard.

The product's own audio path has played too. On 2026-09-08 the bench build (the
product profile plus `sdkconfig.bench`) carried AirPlay → I2S → PCM5102A →
XH-A232 → sound, mono and clean, with a DSP chain in front of the amplifier: mono
sum, per-band EQ, subsonic high-pass, LR4 crossover, a limiter per branch. That
chain is not finished and its numbers are placeholders: two are measured (the
drivers' DC resistances), the rest are reasoned from an impedance sweep that located
neither driver's `Fs`, so the product profile still refuses audio until a
measured calibration profile exists in `factory_cal` (`G0`). The bench record is
[docs/06-testing/bench-measurement-order.md](../docs/06-testing/bench-measurement-order.md);
what comes next, in order and with acceptance criteria, is in
[docs/03-firmware/firmware-plan.md](../docs/03-firmware/firmware-plan.md).

## Locked inputs

| Input | Value | Source |
|---|---|---|
| Board | ESP32-S3, 16 MB flash + 8 MB octal PSRAM (`N16R8`) | ADR-0010 |
| ESP-IDF | `v5.5.1`, pinned | this file and `.github/workflows/firmware-ci.yml` |
| Audio topology | mono programme, bi-amp: DAC left to four woofers and DAC right to four tweeters through four identical XH-A232 amplifiers | ADR-0002 |
| Supply | 24 V / 2.9 A DC adapter on VIN; `CONFIG_HK_SUPPLY_MV` defaults to 24000 | ADR-0020 |
| Distribution | SemVer tag, GitHub Releases, signed A/B OTA | ADR-0008 |
| AirPlay stack | `rbouteiller/airplay-esp32`, vendored at `38027441ff43` | ADR-0007, ADR-0013 |

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

> Changing `sdkconfig.defaults` does **not** update an existing `sdkconfig`.
> ESP-IDF applies the defaults only when it generates the file, so a local build
> keeps the old settings and silently omits whatever you just enabled. Delete
> `firmware/sdkconfig` and rebuild after editing the defaults. CI is immune,
> since it starts from a fresh checkout.

`PROJECT_VER` comes from `version.txt`. It must stay strict SemVer, because the
OTA client compares it numerically and the release pipeline checks it against the
Git tag.

### The bring-up devkit

The product board is not the only target. While it is unavailable, the firmware
also builds for an ESP32-S3 **N8R2** development kit — 8 MB flash, 2 MB quad
PSRAM — so the network and control paths can be exercised on real silicon
(ADR-0012). It is a bring-up target and cannot ship.

```bash
idf.py -C firmware -B firmware/build-devkit \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.devkit" \
  -D SDKCONFIG="$PWD/firmware/build-devkit/sdkconfig" \
  build
```

The separate `-B` and `-D SDKCONFIG` are not tidiness. The two profiles disagree
about flash size, PSRAM mode and the partition table, and ESP-IDF applies
defaults only when it first writes an `sdkconfig` — sharing one would silently
keep whichever profile was configured first, and that shows up as a board that
does not boot rather than as a build error.

#### Putting the devkit on a network

The board has no button, so the 5-second press that clears Wi-Fi credentials
cannot be given; and joining its SoftAP from the machine that is driving it
takes that machine off the network it is being told to join. So the bench gets a
third way in, over USB:

```bash
python3 firmware/tools/set_bench_wifi.py
```

It asks for the SSID and reads the password with a hidden prompt, then writes
`firmware/sdkconfig.devkit.local` — ignored by git, mode 600. Add it to the
build's `SDKCONFIG_DEFAULTS` (third entry) and the board joins on boot.

The password ends up inside the built image. That is the honest cost, and the
reason this is limited to a board with nothing attached to it. Run the tool with
`--clear` when you are done. Existing stored credentials are never overwritten:
a preload that silently replaced a provisioned network would make every bench
result ambiguous.

Four settings define a board: flash size, PSRAM mode, partition table and OTA
hardware revision. `CMakeLists.txt` refuses to configure a build in which they
disagree, and refuses to sign a devkit build at all. On the board itself the boot
report opens with the revision it was built for:

```text
hk: board       devkit-n8r2
hk: flash       8 MB detected
hk: psram       2 MB
```

### The bench profile

The product profile compiles the AirPlay receiver in, and it refuses to drive
the audio path until a driver-protection profile exists in `factory_cal`. Until
`G0` has been measured no such profile can be written, so on the product path
the receiver could never be exercised.

`sdkconfig.bench` lifts that gate and runs the DSP backend on a provisional
profile that says on every boot that it was not measured:

```bash
idf.py -C firmware -B firmware/build-bench \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench" \
  -D SDKCONFIG="$PWD/firmware/build-bench/sdkconfig" \
  build
```

It announces itself twice in the boot report, because an exception nobody can
see is indistinguishable from a defect — which is how this one started life:
until 2026-09-08 writing provisioning credentials put a schema version into the
calibration namespace, and that alone was read as "calibrated".

**Look at what is wired to the amplifier output before flashing this build.** It
is defensible into a dummy load, a scope or nothing; the staging rule in
`AGENTS.md` says when one amplifier, one woofer and one tweeter may follow.

## ESP-IDF traps

Four toolchain behaviours cost time on 2026-09-05, during devkit bring-up. None
of them is a decision of this project and all four will be hit again, so each
one is written symptom first — the symptom is what you have when you are stuck.
The session they came from is in
[docs/06-testing/devkit-bring-up.md](../docs/06-testing/devkit-bring-up.md).

**"My edit did nothing."** `SDKCONFIG_DEFAULTS` is read only when ESP-IDF
*generates* an `sdkconfig`. Afterwards the generated file wins, so editing a
defaults fragment and rebuilding changes nothing — no warning, no diff, the old
value still in the image. Delete the generated `sdkconfig` and configure again.
This is the same cause as the note under Build, and the reason the devkit build
gets its own `-B` and `-D SDKCONFIG`.

**A dozen missing system headers in a component that compiled yesterday.** A
component's `CMakeLists.txt` is processed twice: an early pass that only
collects requirements, in which `CONFIG_` symbols do not exist yet, then the
real build. So `REQUIRES` / `PRIV_REQUIRES` must be listed unconditionally.
Wrapping `idf_component_register()` in `if(CONFIG_...)` makes that early pass
record an empty requirement list, and the failure then surfaces far from its
cause, as `esp_*.h` includes that cannot be found.

**A run that produced a broken file.** `nvs_partition_gen.py` resolves the file
paths named *inside* a CSV against the working directory, not against the CSV.
Started from anywhere else it fails to find them and leaves a short or empty
output behind, and that file flashed at the `factory_cal` offset erases the
credentials and replaces nothing — leaving a device that behaves as if it
had never been given any, which this firmware correctly refuses to provision.
Run the tool from the directory the CSV's paths are written against.

**A hard configure error before anything compiles.** A file listed in
`SDKCONFIG_DEFAULTS` that does not exist stops the configure step outright.
Check every entry in the list, `sdkconfig.devkit.local` included: it exists only
after `tools/set_bench_wifi.py` has been run.

## Verify

Three checks run without any hardware, and all three run in CI.

```bash
# 1. Host unit tests: pure logic, no ESP-IDF needed
cmake -S firmware/test -B build/host-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure

# 2. Partition layout, and the image size gate once a build exists
python3 firmware/tools/check_partitions.py
python3 firmware/tools/check_partitions.py --app-size firmware/build/merzarkabul-airplay-speakers.bin

# 3. The partition validator's own tests, so a gate that accepts everything
#    cannot pass unnoticed
python3 firmware/tools/test_check_partitions.py

# 4. A user reset must not be able to erase the calibration store (PRD-008)
python3 firmware/tools/check_storage_isolation.py

# 5. No log statement may print a credential
python3 firmware/tools/check_no_credential_logs.py

# 6. Documentation integrity
python3 scripts/check_docs.py
```

### What has been verified, and what has not

Verified without hardware, and re-verified by CI on every change: the project
builds clean with no warnings in project sources, the host suite passes, the
partition gate and its own tests pass, and `PROJECT_VER` in the built image
matches `version.txt`.

Verified on real silicon on 2026-09-05, on the N8R2 bring-up devkit, with the
image built from this repository. The record and its raw logs are in
[docs/06-testing/devkit-bring-up.md](../docs/06-testing/devkit-bring-up.md):
the boot report and PSRAM detection, the 8 MB partition table, a Wi-Fi join with
DHCP and mDNS, provisioning end to end over both SoftAP and BLE, the short and
5-second button presses, the LED, and the vendored AirPlay receiver carrying a
real iPhone session whose audio was heard through the bench S/PDIF output.

Provisioning will not open on a device whose per-device credentials have not
been written, and that is on purpose: the firmware refuses rather than falling
back to a weaker security mode. Generate them with

```bash
. $IDF_PATH/export.sh
python3 firmware/tools/provision_credentials.py --device A1B2 --image --out ~/hk-credentials
```

`--image` is not optional in practice: it builds the partition image from the
directory the CSV's paths are written against, and checks the result. Doing it
by hand is the trap described under ESP-IDF traps. `--out` belongs outside the
repository — three of the files it writes hold the password.

The password is random and generated per board. The speaker stores only an SRP6a salt
and verifier, from which the password cannot be recovered, so reading the flash
off a speaker does not yield the credential. The generated `label.txt` is the
only copy of the password; it is written owner-only and must not be committed. The transport is chosen by the situation, not by the caller: SoftAP with
nothing stored, BLE from a button press on a configured device. ADR-0005
option C, because ESP-IDF cannot run both in one session.

Verified on the PRODUCT board on 2026-09-08 — an N16R8 with 16 MB flash and
8 MB octal PSRAM, the board ADR-0010 locks. Octal PSRAM comes up and passes its
memory test, the 16 MB partition table loads, the identity derives from the MAC,
and with no calibration written the device refuses audio and refuses to open
provisioning rather than weaken it. Two boots were identical down to the free-heap
byte. Record: [docs/06-testing/product-board-bring-up.md](../docs/06-testing/product-board-bring-up.md).

**The GPIO assignment is still a *candidate*.** A board that boots has not proved
its pin table: what sits on the pins is a property of the board, not of the
image. ADR-0011 wants the purchased board's own schematic before the table is
`accepted`, and it has not been checked against one.

**No physical gate has passed.** `G0` is partly measured — both drivers' DC
resistances, neither driver's `Fs` — and `G1`, `G2`, `G6` and `G8` are all
untouched; the twelve-second button press has never been exercised.

## Layout

```text
firmware/
  CMakeLists.txt        project definition; PROJECT_VER comes from version.txt
  sdkconfig.defaults    board, partition, PSRAM and rollback settings
  sdkconfig.devkit      overlay for the N8R2 bring-up board (ADR-0012)
  partitions.csv        16 MB layout: dual OTA slots + isolated calibration
  partitions-devkit.csv the same rows at 8 MB, identical below 0x20000
  version.txt           strict SemVer, compared by the OTA client
  main/                 app_main: boot report, gates, network, AirPlay, OTA
  components/
    hk_pins/            GPIO assignment; the compiler enforces the constraints
    hk_identity/        every user-visible name, derived from the MAC
    hk_version/         SemVer parsing and the OTA update decision
    hk_button/          function button: debounce, hold levels, what commits
    hk_led/             which status wins the single LED, and how it looks
    hk_provision/       when the setup radios are open, and when they shut
    hk_portal/          the app-less captive-portal setup path (ADR-0015)
    hk_ui/              button GPIO and RGB PWM, on its own low-priority task
    hk_network/         Wi-Fi, mDNS and the provisioning transport
    hk_airplay/         the vendored AirPlay 2 receiver and its output backends
                        (I2S passthrough, the DSP chain, S/PDIF for the bench)
    hk_audio/           the mute sequence: clocks, DAC and amplifier in order
    hk_settings/        what the user may change, and its stored bounds
    hk_schema/          what to do when stored data does not match this build
    hk_storage/         the two stores, and the wall between them
    hk_sched/           when to look for an update, and how to back off
    hk_manifest/        whether a published release belongs on this device
    hk_ota/             checking the image that arrived against its manifest
    hk_gate/            when an update may start (the gate table)
    hk_health/          whether a freshly installed image has earned its place
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
- a user reset must never erase `factory_cal`: enforced by a partition boundary,
  a read-only open, and `tools/check_storage_isolation.py` in CI
- OTA must not start during playback or without Wi-Fi

An automated test can show that logic behaves. It cannot show that a gate
passed; only a recorded operator measurement can.
