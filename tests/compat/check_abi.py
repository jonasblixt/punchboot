#!/usr/bin/env python3
"""Check the frozen wire protocol and bpak layout (D-008, doc/dev/compatibility.md).

abi.txt lists struct sizes, field offsets, enum values, flags and ids, one per
line, as printed by abi_dump. The file may only grow: a line that changes or
disappears means old hosts, old devices or old images break.

  check_abi.py check <abi_dump> <abi.txt> [--update]
      Compare abi_dump's output with abi.txt. --update adds new lines, but
      never removes or changes one.

  check_abi.py history <abi.txt> [--against <git ref>]
      Check that every line in abi.txt at <ref> (default HEAD) is still there.
      CI runs it against the PR base, pre-commit against HEAD.
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def entries(text: str) -> list[str]:
    """Return the lines that carry a frozen value (no comments, no blanks)."""
    return [line for line in text.splitlines() if line.strip() and not line.startswith("#")]


def key(line: str) -> str:
    """Return the name part of a line, e.g. 'value PB_CMD_DEVICE_RESET'."""
    return " ".join(line.split()[:2])


def lost(old: list[str], new: list[str]) -> list[str]:
    """Return a report line for each old line that is missing from new."""
    current = {key(line): line for line in new}
    present = set(new)
    report = []
    for line in old:
        if line in present:
            continue
        now = current.get(key(line))
        report.append(f"  {line}  ->  {now}" if now else f"  {line}  ->  (removed)")
    return report


def cmd_check(dump: pathlib.Path, baseline: pathlib.Path, *, update: bool) -> int:
    run = subprocess.run([str(dump)], check=True, capture_output=True, text=True)  # noqa: S603
    output = run.stdout
    new = entries(output)
    old = entries(baseline.read_text())

    broken = lost(old, new)
    if broken:
        say(f"{baseline}: frozen values changed or removed, this breaks compatibility (D-008):")
        say("\n".join(broken))
        return 1

    added = [line for line in new if line not in set(old)]
    if not added:
        return 0
    if update:
        baseline.write_text(output)
        say(f"{baseline}: added {len(added)} line(s)")
        return 0
    say(f"{baseline}: new values, not frozen yet. Check them, then run with --update:")
    say("\n".join(f"  {line}" for line in added))
    return 1


def cmd_history(baseline: pathlib.Path, ref: str) -> int:
    git = subprocess.run(  # noqa: S603
        ["git", "show", f"{ref}:{baseline.as_posix()}"],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
    )
    if git.returncode != 0:
        say(f"{baseline}: not in {ref}, nothing to compare")
        return 0

    broken = lost(entries(git.stdout), entries(baseline.read_text()))
    if broken:
        say(f"{baseline}: lines from {ref} changed or removed, this breaks compatibility (D-008):")
        say("\n".join(broken))
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="cmd", required=True)
    check = sub.add_parser("check")
    check.add_argument("dump", type=pathlib.Path)
    check.add_argument("baseline", type=pathlib.Path)
    check.add_argument("--update", action="store_true")
    history = sub.add_parser("history")
    history.add_argument("baseline", type=pathlib.Path)
    history.add_argument("--against", default="HEAD")
    args = parser.parse_args()

    if args.cmd == "check":
        return cmd_check(args.dump, args.baseline, update=args.update)
    return cmd_history(args.baseline, args.against)


if __name__ == "__main__":
    sys.exit(main())
