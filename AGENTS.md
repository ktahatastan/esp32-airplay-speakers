# Harman Kardom agent contract

## Mission

Build four safe, measurable, battery-powered active speakers from Harman Kardon Nova drivers. The product name is **Harman Kardom**. Never present unverified driver impedance or AirPlay multiroom support as confirmed.

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
- Do not commit generated secrets, Wi-Fi credentials, provisioning PoP values or private keys.

## Hardware safety

- Never energize an unknown driver at full level. G0 and the relevant G1-G2 checks come first.
- Tweeter output requires a verified HPF, limiter and safe boot/mute sequence.
- Li-ion packs use matched new cells, professional spot welding, balanced BMS, fuse and temperature monitoring.
- Battery/charger tests start outside the enclosure on a non-flammable surface with current limiting and supervision.
- Do not scale to four packs until G0-G5 pass. Agents cannot claim a physical test passed without recorded operator measurements.
- BTL amplifier speaker negatives are not chassis ground.

## Verification

- Every task defines measurable acceptance criteria before implementation.
- Run the smallest relevant automated checks, then the broader gate when risk warrants it.
- Report PASS, FAIL or BLOCKED honestly with command/measurement evidence.
- Manual acoustic, visual and functional acceptance belongs to the user unless explicitly delegated.

## Current blockers

These are the open `Kritik` risks from `docs/01-planning/risk-register.md` that still block a gate: each one is an unverified fact, not a hazard with a mitigation already in place. The register is the complete list and the detailed source; the two `Kritik` rows not repeated here (tweeter DC exposure and cell thermal runaway) are hazards whose standing mitigations are in `Hardware safety` above. Adding a blocker here means adding its row to the register too.

- Individual Nova woofer/tweeter impedance is not confirmed; follow `docs/02-hardware/driver-measurements.md`. Blocks G0, `C_SAFE`, crossover and safe amplifier level.
- AirPlay 2 group synchronization is not measured. The stack is chosen and its AirPlay 2 and PTP capability is verified in source (ADR-0007), but no four-device measurement exists. Follow `docs/01-architecture/audio-network-feasibility.md`. Blocks G7 and PRD-002.
- The XL4015 charge stage has no guaranteed charge termination; follow ADR-0009. No unattended or overnight charging before the G4 termination measurement.
- The KM103 / DC-132A switch has no documented 16.8 VDC contact rating; it stays off the main battery line until written vendor data and the G3 load test exist.
- BMS real continuous current, balance threshold/current and NTC behaviour are vendor claims only; they are not verified.

## Locked decisions agents must not re-open silently

- Board: ESP32-S3 `N16R8`, 16 MB flash + 8 MB PSRAM (ADR-0010). The GPIO assignment is still a candidate.
- V1 charge chain: USB-C PD -> 20 V trigger -> XL4015 16.80 V / 2.00 A CC/CV -> 4S BMS (ADR-0009).
- V1 does not play audio while charging (ADR-0004).
- AirPlay receiver: `rbouteiller/airplay-esp32`, vendored at a pinned commit (ADR-0007). Its licence permits non-commercial use only, which binds the whole project.

## Documentation integrity

Run `python3 scripts/check_docs.py` before integrating. It checks wiki links, frontmatter, ADR status vocabulary and canonical-term drift. It does not replace physical gates.
