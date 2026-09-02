#!/usr/bin/env python3
"""Keep private key material to the one folder that is allowed to hold it.

`docs/credentials/` is the declared home for credentials, per the rule in
AGENTS.md. In practice nothing secret needs to live there: the release signing
key's private half is a `release` environment secret on GitHub, and the folder
holds records — public fingerprints, and the list of keys that must never sign
again. The allowance exists so the rule has one answer rather than two.

Everywhere else is refused. The .gitignore names the files a key is *likely* to
be called, which is a guess rather than a guard — a key copied as `key.pem`,
`backup.pem` or `meeting-notes.md` sails straight past it. So this checks
content, not names, and it checks location: a key outside the declared folder
is a mistake by definition, because the folder is where someone would put one
on purpose.

This is a location check and nothing more. It does not know whether a key has
been exposed, and it would not stop a compromised key from signing. That is a
different failure with a different guard: the publish job in
.github/workflows/release.yml refuses any key whose fingerprint appears in
docs/credentials/burned-keys.txt, and any key that does not match the pinned
public half in firmware/certs/.

The repository is public. It was private for one day and is public again, which
is exactly why this does not relax: history is permanent, visibility changes,
and the first signing key was burned by that sequence.

Usage:
    check_no_private_keys.py              # every tracked file
    check_no_private_keys.py --staged     # only what is about to be committed
"""
from __future__ import annotations

import argparse
import subprocess
import sys

# Built rather than written out, so that this file does not trip its own check
# and so that grepping the repository for a key header finds keys, not this.
_B = b"-" * 5 + b"BEGIN "
_E = b" PRIVATE KEY" + b"-" * 5

MARKERS: list[tuple[bytes, str]] = [
    (_B + b"RSA" + _E, "RSA private key"),
    (_B + b"EC" + _E, "EC private key"),
    (_B + b"DSA" + _E, "DSA private key"),
    (_B + b"OPENSSH" + _E, "OpenSSH private key"),
    (_B + b"PGP" + _E, "PGP private key"),
    (_B + b"ENCRYPTED" + _E, "encrypted private key"),
    (_B + _E.lstrip(b" "), "PKCS#8 private key"),
]

# A PKCS#12 / PFX bundle is binary and carries no header to match, so it is
# caught by extension. Unlike .pem, these extensions have no legitimate
# non-secret use.
BINARY_KEY_SUFFIXES = (".p12", ".pfx", ".jks", ".keystore")

SKIP_PREFIXES = ("build/", "firmware/build/", "firmware/build-release/",
                 "firmware/managed_components/")

# The one place a private key may be committed. Everything under it is public
# by construction; see docs/credentials/README.md for why that is accepted for
# now and what has to change before a real release is signed.
ALLOWED_PREFIX = "docs/credentials/"


def tracked_files(staged: bool) -> list[str]:
    if staged:
        cmd = ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"]
    else:
        cmd = ["git", "ls-files"]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    return [line for line in out.splitlines() if line]


def scan(paths: list[str]) -> list[str]:
    problems: list[str] = []
    for path in paths:
        if path.startswith(SKIP_PREFIXES) or path.startswith(ALLOWED_PREFIX):
            continue
        if path.endswith(BINARY_KEY_SUFFIXES):
            problems.append(f"{path}: key store file ({path.rsplit('.', 1)[-1]})")
            continue
        try:
            with open(path, "rb") as handle:
                blob = handle.read()
        except (FileNotFoundError, IsADirectoryError, PermissionError):
            continue
        for marker, description in MARKERS:
            if marker in blob:
                problems.append(f"{path}: contains a {description}")
                break
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged", action="store_true",
                        help="check only files staged for commit")
    args = parser.parse_args(argv)

    paths = tracked_files(args.staged)
    problems = scan(paths)

    scope = "staged" if args.staged else "tracked"
    print(f"scanned {len(paths)} {scope} files")

    if problems:
        print(f"\nPrivate key material belongs in {ALLOWED_PREFIX} and nowhere else.")
        print("This repository is public and its history cannot be withdrawn:\n")
        for problem in problems:
            print(f"  {problem}")
        print(f"\nMove it under {ALLOWED_PREFIX} if it is a development key that")
        print("is meant to be public, or out of the working tree entirely if it")
        print("is not. See docs/credentials/README.md. If this file was already")
        print("pushed, treat the key as burned: generate a new one and re-sign.")
        print(f"\ncheck_no_private_keys: {len(problems)} problem(s)")
        return 1

    print("check_no_private_keys: 0 problems")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
