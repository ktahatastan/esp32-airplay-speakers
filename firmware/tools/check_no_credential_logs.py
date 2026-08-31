#!/usr/bin/env python3
"""Check that no log statement can print a credential.

docs/03-firmware/security-and-recovery.md requires that a Wi-Fi password or a
provisioning secret never appears in a log, a crash dump or a portal response.
Serial output is the least private surface a speaker has: it is read over USB in
a room with other people, pasted into issues, and captured verbatim in coredumps
that get shared while debugging.

The rule this enforces is narrow enough to be trustworthy: logging the LENGTH of
a secret is fine, logging its VALUE is not. So `salt %u B` passes and
`password %s` does not. Identifiers ending in a size-like suffix are treated as
lengths.

    python3 firmware/tools/check_no_credential_logs.py

This is a source check. It cannot see a secret that reaches a log through a
variable named something innocuous, so it lowers the chance of an accident
rather than proving the absence of one.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

FIRMWARE = Path(__file__).resolve().parent.parent

#: An ESP_LOG call and everything up to the closing parenthesis of the statement.
LOG_CALL = re.compile(r"\bESP_LOG[EWIDV]\s*\((.*?)\);", re.DOTALL)

#: Words that name a secret rather than a fact about one.
SECRET = re.compile(
    r"\b\w*(password|passwd|passphrase|pwd|verifier|secret|token|credential)\w*\b",
    re.IGNORECASE,
)

#: A name ending like this holds a size, not the thing itself.
SIZE_SUFFIX = re.compile(r"(_len|_length|_size|_bytes|_count|_max|_min)$", re.IGNORECASE)

#: A C string literal, so message text can be told apart from arguments.
STRING_LITERAL = re.compile(r'"(?:[^"\\]|\\.)*"')


def sources() -> list[Path]:
    return sorted(
        path for path in FIRMWARE.rglob("*.c")
        if "build" not in path.parts and "managed_components" not in path.parts
    )


def offending_names(arguments: str) -> list[str]:
    """Secret-looking identifiers passed as VALUES to a log call.

    String literals are stripped first. A message that says "credentials
    received" mentions the word without printing anything; an argument called
    wifi_password prints the thing itself. Only the second is a defect, and
    conflating them produces a checker noisy enough that people stop reading it.

    A literal that itself contained a secret would slip through. Telling
    "password is hunter2" apart from "waiting for password" is not something a
    regular expression can do, and guessing would cost more in false alarms than
    it buys.
    """
    without_text = STRING_LITERAL.sub('""', arguments)

    found = []
    for match in SECRET.finditer(without_text):
        name = match.group(0)
        if SIZE_SUFFIX.search(name):
            continue  # a length, not the value
        found.append(name)
    return found


def check() -> list[str]:
    problems: list[str] = []
    calls = 0

    for path in sources():
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(FIRMWARE)

        for match in LOG_CALL.finditer(text):
            calls += 1
            arguments = match.group(1)
            line = text[: match.start()].count("\n") + 1

            names = offending_names(arguments)
            if names:
                problems.append(
                    f"{relative}:{line}: log call mentions {sorted(set(names))}. "
                    "Log the length of a secret, never its value "
                    "(docs/03-firmware/security-and-recovery.md)")

    if calls == 0:
        problems.append(
            "no ESP_LOG call found at all: either logging disappeared or this checker "
            "no longer recognises it")

    print(f"scanned {len(sources())} sources, {calls} log call(s)")
    return problems


def main() -> int:
    problems = check()
    for problem in problems:
        print(f"ERROR: {problem}", file=sys.stderr)
    print(f"check_no_credential_logs: {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    raise SystemExit(main())
