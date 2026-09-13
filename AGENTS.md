# Merzarkabul Airplay Speakers agent contract

## Mission

Build one safe, measurable, mains-powered active speaker from Harman Kardon Nova drivers: a single cabinet holding eight of them (four woofers, four tweeters), playing one mono programme, fed by a 24 V / 2.9 A DC desktop adapter. The product name is **Merzarkabul Airplay Speakers**. Never present unverified driver impedance as confirmed.

## Source of truth

Before work, read `docs/Home.md`, the relevant accepted ADRs under `docs/07-decisions/`, and the domain plan. Keep user-facing plans and decisions in `docs/`; chat history is not the project record.

## One orchestrator

- One primary orchestrator owns scope, task division, integration and final reporting.
- Delegate only concrete, independent work that benefits from parallelism.
- Give every writer an explicit, non-overlapping file scope. One file has one writer at a time.
- Read-heavy exploration, research and verification are preferred parallel tasks.
- Workers return changed files, validation evidence, risks and the next step. The orchestrator reviews before integration.
- Record an agent transfer with `docs/templates/handoff.md`.

## Change discipline

- Preserve unrelated user changes and inspect files before editing.
- An architectural, safety, product-identity or protocol decision requires an ADR.
- Update `docs/08-development-log/` for material work and `docs/06-testing/` for evidence.
- Use current primary documentation for unstable APIs/protocols; procurement links are candidates until rechecked on purchase day.
- Credential files live in `docs/credentials/` and nowhere else. That folder is public by construction, because this repository is public, so everything in it is treated as burned: it holds development material and the record of what exists — which key, its public fingerprint, which release it signed, what to do if it is lost. Never put a user's own secrets there: no Wi-Fi credentials, no API tokens. The design has had no provisioning password or proof-of-possession value since ADR-0023; should a superseding ADR bring one back, it falls under the same rule. The location rule is enforced by `scripts/check_no_private_keys.py`, which inspects file contents rather than names and runs in CI and as a pre-commit hook (`git config core.hooksPath .githooks`). A key committed there can never sign a release: the publish job refuses it. See `docs/credentials/README.md` for what must change before a real release is signed.

## Hardware safety

- Never energize an unknown driver at full level. G0 and the relevant G1-G2 checks come first.
- Tweeter output requires a verified HPF, limiter and safe boot/mute sequence.
- The adapter's no-load output is measured before it is ever connected and must read below 25.5 V: 24 V sits 2 V under the amplifier's 26 V maximum, so an adapter that rises off-load is refused, not derated (ADR-0020).
- First power-up goes through a current-limited bench supply, on the dummy load, with the barrel jack polarity (centre-positive) verified with a meter before the adapter is connected (ADR-0020).
- Staging: the first energised path is one amplifier, one woofer and one tweeter -- on the dummy load first, then on real drivers after G0-G2 pass on that pair. The other three amplifiers are wired to drivers only after that.
- The supply budget the DSP enforces (the profile's `supply_budget_sq` / `supply_window_ms`, an average over both branches summed) is derived from the 2.9 A adapter budget with all four amplifiers driven, on 4 ohm-class dummy loads, and verified by VIN sag in G1: eight BTL channels can draw far more than 70 W, and a sagging VIN resets the ESP32-S3 mid-song (ADR-0020, ADR-0022). The peak ceilings are G2's driver-protection numbers and stay beneath that budget; they do not carry it.
- Agents cannot claim a physical test passed without recorded operator measurements.
- BTL amplifier speaker negatives are not chassis ground.

## Verification

- Every task defines measurable acceptance criteria before implementation.
- Run the smallest relevant automated checks, then the broader gate when risk warrants it.
- Report PASS, FAIL or BLOCKED honestly with command/measurement evidence.
- Manual acoustic, visual and functional acceptance belongs to the user unless explicitly delegated.

## Current blockers

These are the open `Kritik` risks from `docs/01-planning/risk-register.md` that still block a gate: each one is an unverified fact, not a hazard with a mitigation already in place. The register is the complete list and the detailed source. It carries `Kritik` rows that are deliberately not repeated here, because they are not of this kind: hazards whose standing mitigations are already in `Hardware safety` above, and accepted risks recorded with the reason they are accepted. Do not restate the count of those rows in this file — a number here goes stale the moment a row is added there, and a contract that misdescribes its own source is worse than one that points at it. Adding a blocker here means adding its row to the register too.

- Individual Nova driver impedance is **partly** confirmed. DC resistance is measured -- woofer 4.0 ohm, tweeter 3.5 ohm, both 4 ohm class -- which settles nominal impedance and `C_SAFE`. Still open, and still blocking: the impedance curve and both drivers' `Fs`. The tweeter's `Fs` is what sets the minimum safe high-pass corner, so the crossover corner remains a conservative guess rather than a measurement. Follow `docs/02-hardware/driver-measurements.md`. Blocks G0, the crossover corner and safe amplifier level.
- The amplifier is uncharacterised at the 24 V / 4 ohm operating point. The TPA3110D2 datasheet's Absolute Maximum Ratings (SLOS528F) give a minimum BTL load of 4.8 ohm for `PVCC > 15 V`, while both Nova drivers measure 4 ohm-class (Re 4.0 / 3.5 ohm) and the datasheet's 4 ohm output-power curves do not reach 24 V. Blocks connecting any driver at 24 V until G0 gives `Z_min` and G1 records the amplifier's thermal and protection behaviour into 4 ohm-class dummy loads at 24 V (ADR-0022; the matching `Kritik` row is in the register).

## Locked decisions agents must not re-open silently

- Board: ESP32-S3 `N16R8`, 16 MB flash + 8 MB PSRAM (ADR-0010). The GPIO assignment is still a candidate.
- Signal chain: one PCM5102A; `LOUT` carries the woofer band and `ROUT` the tweeter band; `LOUT` is paralleled into the L input of four identical XH-A232 and `ROUT` into their R inputs; each amp's L output drives one woofer and its R output one tweeter through that tweeter's own `C_SAFE` (ADR-0002).
- Output backend: the DSP chain (`hk_dsp`, in `firmware/components/hk_airplay/output/hk_airplay_output_i2s.c`) is the product's and the release's output backend (ADR-0022); the vendored passthrough is selectable only on the bring-up devkit (ADR-0012) or a bench-exception build, and a release that selects it or any `HK_BENCH_*` symbol is refused. One judge, `hk_profile_load()`, decides whether a stored profile is valid, for the boot gate and the backend alike; without a valid `factory_cal` profile the product plays nothing. The profile's supply budget is a G1 number, never a default; the amplifier gain reading (bench item C3) is carried beside it, 0 until read, and C3 is read before any driver is listened to at 24 V.
- Power: a 24 V / 2.9 A DC desktop adapter through a 5.5 x 2.1 mm centre-positive barrel jack feeds all four XH-A232 (8-26 V input) directly as `VIN`; two MP1584-class bucks from `VIN`, one 5 V for the ESP32-S3 and one 5 V for the PCM5102A, kept separate because a shared buck put audible hiss into the DAC. No power switch (ADR-0020).
- One cabinet, eight drivers, one mono programme, one device on the network; stereo and multi-device playback are out of scope. The cabinet is a reflex alignment by the Nova's own passive radiators (ADR-0021).
- AirPlay receiver: `rbouteiller/airplay-esp32`, vendored at a pinned commit (ADR-0007). Its licence permits non-commercial use only, which binds the whole project.
- Provisioning: protocomm Security 1 without proof of possession on BLE and SoftAP, open setup network, PROV_-prefixed setup names (ADR-0023); a stronger mode returns only through a superseding ADR.

## Documentation integrity

Run `python3 scripts/check_docs.py` before integrating. It checks wiki links, frontmatter, ADR status vocabulary and canonical-term drift. It does not replace physical gates.
