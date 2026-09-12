#!/usr/bin/env python3
"""Merzarkabul Airplay Speakers repository integrity checker.

Validates what the project claims about itself: that wiki links resolve, that
notes carry the frontmatter the contract requires, that ADR statuses use the
agreed vocabulary, that decisions locked by an ADR are not silently
contradicted somewhere else in the vault, and that no file in the tree names a
part, a supply voltage or a product shape the product does not have.

This is a documentation check. It never asserts that a physical gate passed.

Usage:
    python3 scripts/check_docs.py            # report problems, exit 1 if any
    python3 scripts/check_docs.py --quiet    # only print the summary
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterator

ROOT = Path(__file__).resolve().parent.parent
# Third-party and generated trees. managed_components holds vendored ESP-IDF
# components whose documentation is not ours to police.
SKIP_DIRS = {".git", ".obsidian", "node_modules", "generated",
             "managed_components", "build",
             # Virtual environments carry their own packaged READMEs, which are
             # not this project's notes and would be judged against a
             # frontmatter contract they never agreed to.
             ".venv", "venv", "site-packages", "__pycache__"}
# Build trees (build, build-devkit, build-release, ...) hold generated
# sdkconfigs and compile databases: output, not record.
SKIP_DIR_PREFIXES = ("build",)
# idf.py writes these next to the project from the tracked sdkconfig.* inputs
# and Git ignores them. A stale one carries whatever value was current when it
# was generated, which is a reason to regenerate it, not a finding about the
# record; the inputs it was made from are scanned.
SKIP_FILES = {"sdkconfig", "sdkconfig.old"}

# The vault frontmatter contract applies to notes under docs/ only. Agent and
# skill definitions follow their own tool-defined schema, checked separately.
VAULT_ROOT = "docs"
FRONTMATTER_EXEMPT = {"README.md"}
# Templates ship YYYY-MM-DD placeholders on purpose.
TEMPLATE_DIR = "docs/templates"

# Tool-defined agent and skill files: these need name + description, nothing else.
AGENT_GLOBS = (".claude/agents/*.md", ".cursor/agents/*.md")
SKILL_GLOB = ".agents/skills/*/SKILL.md"

REQUIRED_FRONTMATTER = ("status", "owner", "updated")
ADR_STATUSES = {"proposed", "accepted", "superseded", "rejected"}

# ADR numbers that stay vacant. A number is a permanent identifier: once
# issued it is never issued again, so the index check refuses a file that
# takes one of these.
RETIRED_ADR_NUMBERS = {3, 4, 9, 17, 18, 19}
ADR_NUMBER = re.compile(r"^ADR-(\d{4})")

# `\|` is the Obsidian escape for an alias pipe inside a markdown table cell.
WIKILINK = re.compile(r"\[\[([^\]|#\\]+)\\?(?:#[^\]|\\]*)?\\?(?:\|[^\]]*)?\]\]")
FRONTMATTER = re.compile(r"\A---\n(.*?)\n---\n", re.DOTALL)
DATE = re.compile(r"^\d{4}-\d{2}-\d{2}$")

# Canonical values locked by an accepted ADR, and terms for things this
# product does not contain. A hit outside `allowed` means the record
# contradicts a decision, which is exactly how an agent gets misled.
#
# Case sensitivity is part of each pattern (`(?i)` where wanted), not a global
# flag: the removed-subsystem rule hunts the exact identifiers, and a lowercase
# "overheat" in a note about the amplifier on the dummy load is a legitimate G1
# concern that must stay writable.
@dataclass(frozen=True)
class Drift:
    label: str
    pattern: str
    allowed: tuple[str, ...]
    hint: str
    # "vault": the markdown notes, where a locked value can be misquoted.
    # "repo": every text file in the tree, for terms that must not exist
    # anywhere — a stray one in a workflow, a generator or a code comment is
    # as misleading as one in a note.
    scope: str = "vault"


DRIFT_RULES = (
    Drift(
        label="board-variant",
        pattern=r"(?i)\bN8R8\b",
        allowed=(
            "docs/07-decisions/ADR-0010-esp32-s3-n16r8-board.md",
            "docs/07-decisions/ADR-0012-n8r2-bringup-target.md",
            "docs/02-hardware/board-and-pin-selection.md",
            "docs/05-procurement/bom.md",
            "docs/05-procurement/suppliers.md",
            "docs/05-procurement/research-log.md",
            "docs/08-development-log/",
            "scripts/check_docs.py",
        ),
        hint="ADR-0010 locks the board to N16R8. N8R8 may only appear as an explicit rejected/backup alternative.",
    ),
    Drift(
        label="devkit-variant",
        pattern=r"(?i)\bN8R2\b",
        allowed=(
            "docs/07-decisions/ADR-0012-n8r2-bringup-target.md",
            "docs/07-decisions/ADR-0013-airplay-integration-shape.md",
            "docs/07-decisions/README.md",
            "docs/06-testing/devkit-bring-up.md",
            "docs/06-testing/test-log.md",
            "docs/08-development-log/",
            "firmware/README.md",
            "scripts/check_docs.py",
        ),
        hint=("N8R2 is the bring-up devkit of ADR-0012, never the product board. "
              "It may only appear where that distinction is being made."),
    ),
    Drift(
        label="product-identity",
        pattern=r"(?i)kardom|HarmanKardom|harman-kardom",
        allowed=("scripts/check_docs.py",),
        hint=("The product is Merzarkabul Airplay Speakers. 'Harman Kardon' is the "
              "driver brand and stays; 'Kardom' is not a name this project uses."),
        scope="repo",
    ),
    Drift(
        label="removed-subsystem",
        # Identifiers are matched case-insensitively (gc9a01 in a lock file is
        # still the display driver); OVERHEAT stays exact so prose about the
        # amplifier's heat on the dummy load in G1 remains writable.
        pattern=(r"(?i:\bBMS\b|XL4015|INA219|GC9A01|LVGL|\bNTC\b|batarya|"
                 r"Li-ion|4S1P|hk_power|hk_display|KM103|DC-132A)|OVERHEAT"),
        allowed=("scripts/check_docs.py",),
        hint=("The speaker runs from a 24 V DC adapter (ADR-0020): it has no battery, "
              "charger, display, cell thermistor or current sensor, so nothing in the "
              "tree may describe one."),
        scope="repo",
    ),
    Drift(
        label="multi-device",
        # `\bG7\b` sees the bare gate number only. A range written "G6-G8"
        # would step over it unseen, hence the hint's spelling.
        pattern=r"(?i)multiroom|çoklu oda|\bG7\b|dört hoparlör|four speakers|dört cihaz|four devices",
        allowed=("scripts/check_docs.py",),
        hint=("The product is one cabinet and one device on the network (ADR-0021): "
              "there is no multiroom group and no multi-device sync gate, so nothing "
              "in the tree may describe one. G7 is a vacant gate number; write the "
              "range as 'G6, G8', never 'G6-G8'."),
        scope="repo",
    ),
    Drift(
        label="amp-mute-line",
        # The XH-A232 has no mute or shutdown input (ADR-0011, owner's
        # inspection 2026-09-12), so the firmware drives one mute line, the
        # DAC's XSMT. The old amplifier line is matched by its symbol names and
        # by its GPIO, written with or without a space; the test records and
        # the dated development-log entries keep the history and are allowed.
        pattern=r"HK_PIN_AMP_MUTE|\bAMP_MUTE\b|\bamp_enabled\b|\bGPIO ?21\b|`R7`-`R10`",
        allowed=(
            "docs/06-testing/",
            "docs/07-decisions/ADR-0011-audio-side-gpio-reservation.md",
            "docs/08-development-log/",
            "docs/01-planning/risk-register.md",
            "scripts/check_docs.py",
        ),
        hint=("The amplifier boards have no mute input (ADR-0011): the DAC's XSMT on "
              "GPIO13 is the only mute line, hk_audio drives two lines (I2S clock and "
              "XSMT), and there is no AMP_MUTE, no GPIO21 and no R7-R10. Only the "
              "dated records may mention them."),
        scope="repo",
    ),
    Drift(
        label="supply-voltage",
        # Case-sensitive on purpose: `19 V` and `19V` are how a supply figure
        # is written, and `\b` keeps a year like 2019 or a larger number out.
        pattern=r"\b19\s?V\b|\b19000\b",
        allowed=(
            "docs/07-decisions/ADR-0020-dc-adapter-power.md",
            "scripts/check_docs.py",
        ),
        hint=("ADR-0020 locks the supply to a 24 V / 2.9 A DC adapter and "
              "CONFIG_HK_SUPPLY_MV to 24000. A 19 V figure belongs only in "
              "ADR-0020's rejected options."),
        scope="repo",
    ),
    Drift(
        label="passthrough-backend",
        # The symbol of upstream's output stage, the one with no crossover and
        # no limiter in it. It is matched by name because the way it comes
        # back is a tracked sdkconfig fragment that sets it, or a note that
        # still calls it the product's output. The generated sdkconfig is
        # skipped (SKIP_FILES): a stale one is a reason to reconfigure, not a
        # finding about the record. The dated logs keep the history; the
        # Kconfig that defines it, the CMake that compiles it, the workflows
        # that refuse it and the README that explains the three backends are
        # where the name belongs.
        pattern=r"HK_AIRPLAY_OUTPUT_I2S\b",
        allowed=(
            "firmware/components/hk_airplay/Kconfig",
            "firmware/components/hk_airplay/CMakeLists.txt",
            ".github/workflows/",
            "docs/08-development-log/",
            "docs/07-decisions/ADR-0022-dsp-product-output-backend.md",
            "firmware/README.md",
            "scripts/check_docs.py",
        ),
        hint=("The product's output backend is the DSP chain (ADR-0022): the "
              "passthrough is upstream's stage, selectable only on the devkit or a "
              "bench-exception build, and no tracked sdkconfig fragment may select "
              "it. Write 'the vendored passthrough' where the stage is meant."),
        scope="repo",
    ),
)

# Claims the contract forbids stating as fact.
#
# Applied case-insensitively to every vault note. Each row is a regex and the
# reason it is forbidden; a hit is an error, so a row has to be tight enough
# that the honest sentence stays writable -- the second row's negative
# lookahead exists for exactly that: 'kesin değildir' is the phrasing the
# record uses for a placeholder, 'kesin olmayan', 'kesin olmaktan uzak' and
# 'kesin sayılmaz' are its neighbours, and none of them may trip the rule
# against calling one certain.
FORBIDDEN_CLAIMS = (
    (r"(woofer|tweeter)[^.\n]{0,40}\b8\s*(ohm|Ω)\b[^.\n]{0,20}(olduğu|doğrulandı|kesin)",
     "The driver impedance curve and Fs are not measured (G0); the measured DC "
     "resistance puts both Nova drivers in the 4 ohm class, not 8."),
    # The provisional profile's numbers -- crossover 3500 Hz (2800 before
    # 2026-09-12), subsonic 55 Hz,
    # ceilings 0.70 and 0.35 -- are placeholders until G0/G2, and the likeliest
    # new false claim is one of them written as measured. The middle group
    # keeps the number whole (`\b` on both sides), the last group takes the
    # past-tense and perfect forms of 'measured', 'verified' and 'certain',
    # and the lookahead on `kesin` lets the honest negations through: 'kesin
    # değil(dir)', 'kesin olmayan', 'kesin olmaktan uzak', 'kesin sayılmaz'.
    (r"(crossover|subsonic|tavan|köşe)[^.\n]{0,40}\b(3500|2800|55|0[.,]70|0[.,]35)\b[^.\n]{0,40}"
     r"\b(ölçüldü|ölçülmüş(?:tür)?|doğrulandı|doğrulanmış(?:tır)?|"
     r"kesin(?!\s+(?:değil|olmayan|olmaktan|sayılm))(?:dir|leşti|leşmiş(?:tir)?)?)\b",
     "The provisional crossover and subsonic corners and the limiter ceilings "
     "are placeholders until G0/G2 produce measured values; none of them was "
     "measured or verified, and the record may not say so."),
)


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    def error(self, path: Path, message: str) -> None:
        self.errors.append(f"{path.relative_to(ROOT)}: {message}")

    def warn(self, path: Path, message: str) -> None:
        self.warnings.append(f"{path.relative_to(ROOT)}: {message}")


def skipped(path: Path) -> bool:
    if path.name in SKIP_FILES:
        return True
    parts = path.relative_to(ROOT).parts[:-1]
    return any(part in SKIP_DIRS or part.startswith(SKIP_DIR_PREFIXES)
               for part in parts)


def markdown_files() -> list[Path]:
    return sorted(p for p in ROOT.rglob("*.md") if not skipped(p))


def read_text(path: Path) -> str | None:
    """None when the file vanished between being listed and being read.

    Writers work in parallel on this tree; a note deleted mid-run is not a
    finding, it is simply no longer a note.
    """
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return None


def text_files() -> Iterator[tuple[Path, str]]:
    """Every file in the tree that reads as UTF-8 text, outside skipped trees.

    Binary artefacts (images, firmware images, key material) are recognised by
    content, not by extension, so a file called anything at all is either
    scanned or skipped for what it is.
    """
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or skipped(path):
            continue
        try:
            raw = path.read_bytes()
        except OSError:
            continue
        if b"\0" in raw:
            continue
        try:
            yield path, raw.decode("utf-8")
        except UnicodeDecodeError:
            continue


def parse_frontmatter(text: str) -> dict[str, str] | None:
    match = FRONTMATTER.match(text)
    if not match:
        return None
    fields: dict[str, str] = {}
    for line in match.group(1).splitlines():
        if line.startswith((" ", "-", "\t")) or ":" not in line:
            continue
        key, _, value = line.partition(":")
        fields[key.strip()] = value.strip()
    return fields


def check_wikilinks(path: Path, text: str, stems: dict[str, list[Path]], report: Report) -> None:
    for match in WIKILINK.finditer(text):
        target = match.group(1).strip()
        if (path.parent / f"{target}.md").exists():
            continue
        if (ROOT / f"{target}.md").exists():
            continue
        if (ROOT / "docs" / f"{target}.md").exists():
            continue
        if target.split("/")[-1] in stems:
            continue
        report.error(path, f"wiki link target not found: [[{target}]]")


def check_frontmatter(path: Path, text: str, report: Report) -> None:
    relative = path.relative_to(ROOT)
    if relative.parts[0] != VAULT_ROOT:
        return
    if path.name in FRONTMATTER_EXEMPT:
        return
    fields = parse_frontmatter(text)
    if fields is None:
        report.warn(path, "no frontmatter block")
        return
    for key in REQUIRED_FRONTMATTER:
        if key not in fields:
            report.error(path, f"frontmatter missing '{key}'")
            continue
    updated = fields.get("updated", "")
    if str(relative.parent) == TEMPLATE_DIR:
        return
    if updated and not DATE.match(updated):
        report.error(path, f"frontmatter 'updated' is not YYYY-MM-DD: {updated!r}")
    if path.parent.name == "07-decisions" and path.name.startswith("ADR-"):
        status = fields.get("status", "")
        if status not in ADR_STATUSES:
            report.error(path, f"ADR status {status!r} outside {sorted(ADR_STATUSES)}")


def check_adr_index(report: Report) -> None:
    index = ROOT / "docs/07-decisions/README.md"
    listed = set(WIKILINK.findall(index.read_text(encoding="utf-8")))
    for adr in sorted((ROOT / "docs/07-decisions").glob("ADR-*.md")):
        match = ADR_NUMBER.match(adr.stem)
        if match and int(match.group(1)) in RETIRED_ADR_NUMBERS:
            report.error(adr, f"ADR number {match.group(1)} stays vacant; "
                              "file the decision under the next free number")
        if adr.stem not in listed:
            report.error(index, f"{adr.stem} is not listed in the ADR index")


def check_drift(path: Path, text: str, report: Report, scope: str) -> None:
    relative = str(path.relative_to(ROOT))
    for rule in DRIFT_RULES:
        if rule.scope != scope:
            continue
        if any(relative.startswith(prefix) for prefix in rule.allowed):
            continue
        if re.search(rule.pattern, text):
            report.error(path, f"[{rule.label}] {rule.hint}")


def check_forbidden_claims(path: Path, text: str, report: Report) -> None:
    for pattern, message in FORBIDDEN_CLAIMS:
        if re.search(pattern, text, re.IGNORECASE):
            report.error(path, f"unverified claim: {message}")


def check_agent_definitions(report: Report) -> None:
    """Agent and skill files use the tool schema: a name and a description."""
    targets: list[Path] = []
    for pattern in AGENT_GLOBS:
        targets.extend(sorted(ROOT.glob(pattern)))
    targets.extend(sorted(ROOT.glob(SKILL_GLOB)))
    for path in targets:
        text = read_text(path)
        if text is None:
            continue
        fields = parse_frontmatter(text)
        if fields is None:
            report.error(path, "agent/skill definition has no frontmatter")
            continue
        for key in ("name", "description"):
            if not fields.get(key):
                report.error(path, f"agent/skill frontmatter missing '{key}'")
        name = fields.get("name", "")
        if name and name != path.stem and path.name != "SKILL.md":
            report.error(path, f"frontmatter name {name!r} does not match filename {path.stem!r}")


def check_ambiguous_stems(texts: dict[Path, str], stems: dict[str, list[Path]], report: Report) -> None:
    """Only complain about a duplicated stem if something links to it bare."""
    used: set[str] = set()
    for text in texts.values():
        for target in WIKILINK.findall(text):
            if "/" not in target:
                used.add(target.strip())
    for stem in sorted(used):
        paths = stems.get(stem, [])
        if len(paths) > 1:
            joined = ", ".join(str(p.relative_to(ROOT)) for p in paths)
            report.warn(paths[0], f"bare wiki link {stem!r} is ambiguous between: {joined}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quiet", action="store_true", help="only print the summary line")
    args = parser.parse_args()

    texts: dict[Path, str] = {}
    for path in markdown_files():
        text = read_text(path)
        if text is not None:
            texts[path] = text
    files = list(texts)
    stems: dict[str, list[Path]] = {}
    for path in files:
        stems.setdefault(path.stem, []).append(path)

    report = Report()
    for path, text in texts.items():
        check_wikilinks(path, text, stems, report)
        check_frontmatter(path, text, report)
        check_drift(path, text, report, scope="vault")
        check_forbidden_claims(path, text, report)
    check_adr_index(report)

    scanned = 0
    for path, text in text_files():
        scanned += 1
        check_drift(path, text, report, scope="repo")

    check_agent_definitions(report)
    check_ambiguous_stems(texts, stems, report)

    if not args.quiet:
        for line in report.errors:
            print(f"ERROR   {line}")
        for line in report.warnings:
            print(f"WARN    {line}")

    print(f"check_docs: {len(files)} notes, {scanned} text files, "
          f"{len(report.errors)} errors, {len(report.warnings)} warnings")
    return 1 if report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
