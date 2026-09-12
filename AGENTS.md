# Merzarkabul Airplay Speakers agent contract

## Mission

Build four safe, measurable, mains-powered active speakers from Harman Kardon Nova drivers, each fed by a 19 V DC desktop adapter. The product name is **Merzarkabul Airplay Speakers**. Never present unverified driver impedance or AirPlay multiroom support as confirmed.

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
- Credential files live in `docs/credentials/` and nowhere else. That folder is public by construction, because this repository is public, so everything in it is treated as burned: it holds development material and the record of what exists — which key, its public fingerprint, which release it signed, what to do if it is lost. Never put a user's own secrets there: no Wi-Fi credentials, provisioning passwords or PoP values, no API tokens. The location rule is enforced by `scripts/check_no_private_keys.py`, which inspects file contents rather than names and runs in CI and as a pre-commit hook (`git config core.hooksPath .githooks`). A key committed there can never sign a release: the publish job refuses it. See `docs/credentials/README.md` for what must change before a real release is signed.

## Hardware safety

- Never energize an unknown driver at full level. G0 and the relevant G1-G2 checks come first.
- Tweeter output requires a verified HPF, limiter and safe boot/mute sequence.
- First power-up of the amplifier goes through a current-limited bench supply, on the dummy load, with the barrel jack polarity (centre-positive) verified with a meter before the adapter is connected (ADR-0020).
- Do not replicate to four units until G0-G2 pass. Agents cannot claim a physical test passed without recorded operator measurements.
- BTL amplifier speaker negatives are not chassis ground.

## Verification

- Every task defines measurable acceptance criteria before implementation.
- Run the smallest relevant automated checks, then the broader gate when risk warrants it.
- Report PASS, FAIL or BLOCKED honestly with command/measurement evidence.
- Manual acoustic, visual and functional acceptance belongs to the user unless explicitly delegated.

## Current blockers

These are the open `Kritik` risks from `docs/01-planning/risk-register.md` that still block a gate: each one is an unverified fact, not a hazard with a mitigation already in place. The register is the complete list and the detailed source. It carries `Kritik` rows that are deliberately not repeated here, because they are not of this kind: hazards whose standing mitigations are already in `Hardware safety` above, and accepted risks recorded with the reason they are accepted. Do not restate the count of those rows in this file — a number here goes stale the moment a row is added there, and a contract that misdescribes its own source is worse than one that points at it. Adding a blocker here means adding its row to the register too.

- Individual Nova driver impedance is **partly** confirmed. DC resistance is measured -- woofer 4.0 ohm, tweeter 3.5 ohm, both 4 ohm class -- which settles nominal impedance and `C_SAFE`. Still open, and still blocking: the impedance curve and both drivers' `Fs`. The tweeter's `Fs` is what sets the minimum safe high-pass corner, so the crossover corner remains a conservative guess rather than a measurement. Follow `docs/02-hardware/driver-measurements.md`. Blocks G0, the crossover corner and safe amplifier level.
- AirPlay 2 group synchronization is not measured. The stack is chosen and its AirPlay 2 and PTP capability is verified in source (ADR-0007), but no four-device measurement exists. Follow `docs/01-architecture/audio-network-feasibility.md`. Blocks G7 and PRD-002.

## Locked decisions agents must not re-open silently

- Board: ESP32-S3 `N16R8`, 16 MB flash + 8 MB PSRAM (ADR-0010). The GPIO assignment is still a candidate.
- Power: a 19 V DC desktop adapter through a 5.5 x 2.1 mm centre-positive barrel jack feeds the XH-A232 (8-26 V input) directly as `VIN`; the 5 V buck feeds the ESP32-S3 and the PCM5102A. No power switch in V1 (ADR-0020).
- AirPlay receiver: `rbouteiller/airplay-esp32`, vendored at a pinned commit (ADR-0007). Its licence permits non-commercial use only, which binds the whole project.

## Documentation integrity

Run `python3 scripts/check_docs.py` before integrating. It checks wiki links, frontmatter, ADR status vocabulary and canonical-term drift. It does not replace physical gates.
