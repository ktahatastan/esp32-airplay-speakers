#!/usr/bin/env python3
"""Detect a vendored output backend that has drifted away from the file shadowing it.

firmware/components/hk_airplay/output/hk_airplay_output_i2s.c is a SHADOW of
firmware/components/hk_airplay/vendor/audio/audio_output.c. It is not a patch:
ADR-0013 forbids changing a line of the vendored tree, so the DSP backend is a
separate implementation compiled in the vendored one's place, deliberately
sharing its DMA geometry, its playout cursor, its latency model, its task
priority and its underrun pacing.

Two files holding the same reasoning is a maintenance debt with exactly one
failure mode: a future upstream update changes audio_output.c -- a DMA
descriptor count, the latency midpoint model, the meaning of a cursor -- the
copy is not reviewed, and the shadow silently keeps the old behaviour. Nothing
fails to compile. Nothing fails to boot. The timing engine just gets a slightly
wrong number from one backend and the right one from the other, and the
difference shows up as drift that nobody can attribute.

So the vendored file's hash is recorded beside the shadow. If they disagree,
upstream moved and the shadow has not been looked at, and this says so.

This is a REVIEW trigger, not a correctness check. It cannot tell whether the
shadow is right; it can only tell that nobody has claimed to have checked. The
claim is `--record`, and it should be made after reading the diff, not to make
this quiet.

Usage:
    check_vendor_output_shadow.py            # report, non-zero if unreviewed
    check_vendor_output_shadow.py --staged   # the same, against what is about
                                             # to be committed
    check_vendor_output_shadow.py --record   # after reviewing the shadow
"""
from __future__ import annotations

import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPONENT = ROOT / "firmware/components/hk_airplay"
VENDOR = COMPONENT / "vendor/audio/audio_output.c"
SHADOW = COMPONENT / "output/hk_airplay_output_i2s.c"
STAMP = COMPONENT / "output/.shadowed-sha256"

VENDOR_REL = VENDOR.relative_to(ROOT).as_posix()
SHADOW_REL = SHADOW.relative_to(ROOT).as_posix()
STAMP_REL = STAMP.relative_to(ROOT).as_posix()


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def staged_paths() -> set[str]:
    out = subprocess.run(
        ["git", "-C", str(ROOT), "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        capture_output=True, text=True, check=True).stdout
    return {line.strip() for line in out.splitlines() if line.strip()}


def staged_bytes(path: str) -> bytes | None:
    """The content git would commit for `path`, or None if it is not staged."""
    result = subprocess.run(["git", "-C", str(ROOT), "show", f":{path}"],
                            capture_output=True)
    return result.stdout if result.returncode == 0 else None


def explain(current: str, recorded: str | None, also_untouched: bool) -> None:
    print(f"{VENDOR_REL} has changed, and")
    print(f"{SHADOW_REL}")
    print("has not been reviewed against the new version.\n")
    print(f"  vendored file now : {current[:12]}")
    print(f"  shadow reviewed at: {recorded[:12] if recorded else '(never recorded)'}\n")
    if also_untouched:
        print("The shadow is not part of this commit either, so nothing has been")
        print("looked at at all.\n")
    print("The shadow shares the vendored file's DMA geometry, playout cursor,")
    print("latency model, task priority and underrun pacing. Read the diff and")
    print("decide, for each of those, whether the shadow has to move too:\n")
    print(f"  git diff -- {VENDOR_REL}\n")
    print("Then record that it was reviewed:")
    print("  python3 scripts/check_vendor_output_shadow.py --record\n")
    print("check_vendor_output_shadow: 1 problem")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", action="store_true",
                        help="record the vendored file's hash as reviewed against the shadow")
    parser.add_argument("--staged", action="store_true",
                        help="check what is about to be committed rather than the worktree")
    args = parser.parse_args(argv)

    if not VENDOR.exists():
        print(f"error: {VENDOR_REL} is missing. The vendored tree is incomplete;")
        print("this check cannot say anything about a file that is not there.")
        return 1

    if args.record:
        if not SHADOW.exists():
            print("error: refusing to record a review of a shadow that does not exist.")
            return 1
        current = sha256(VENDOR.read_bytes())
        STAMP.write_text(current + "\n", encoding="utf-8")
        print(f"recorded: the shadow was reviewed against vendored {current[:12]}")
        return 0

    if not SHADOW.exists():
        # Nothing shadows the vendored file, so there is nothing to drift.
        print("check_vendor_output_shadow: no shadow backend, nothing to keep in step")
        return 0

    if args.staged:
        staged = staged_paths()
        blob = staged_bytes(VENDOR_REL)
        if blob is None:
            # Not tracked in the index at all; fall back to the worktree.
            blob = VENDOR.read_bytes()
        current = sha256(blob)
        stamp_blob = staged_bytes(STAMP_REL)
        recorded = (stamp_blob.decode("utf-8").strip() if stamp_blob is not None
                    else (STAMP.read_text(encoding="utf-8").strip()
                          if STAMP.exists() else None))
        if recorded != current:
            explain(current, recorded, also_untouched=SHADOW_REL not in staged)
            return 1
        print("check_vendor_output_shadow: the staged shadow matches its vendored original")
        return 0

    current = sha256(VENDOR.read_bytes())

    if not STAMP.exists():
        print(f"{SHADOW_REL} shadows")
        print(f"{VENDOR_REL}, but no reviewed hash is recorded, so whether it is")
        print("current cannot be established.\n")
        print("Read both files, then:")
        print("  python3 scripts/check_vendor_output_shadow.py --record")
        print("\ncheck_vendor_output_shadow: 1 problem")
        return 1

    recorded = STAMP.read_text(encoding="utf-8").strip()
    if recorded != current:
        explain(current, recorded, also_untouched=False)
        return 1

    print("check_vendor_output_shadow: the shadow backend matches its vendored original")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
