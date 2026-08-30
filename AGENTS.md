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

- Individual Nova woofer/tweeter impedance is not confirmed; follow `docs/02-hardware/driver-measurements.md`.
- AirPlay 2 group synchronization is not confirmed; follow `docs/01-architecture/audio-network-feasibility.md` and ADR-0007.
