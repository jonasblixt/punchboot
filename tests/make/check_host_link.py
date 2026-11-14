#!/usr/bin/env python3
"""Check that pb-host has every file of the firmware core (F-407).

The firmware links every object of the build, so a task (pb/task.h) in a file
that nothing else refers to runs. pb-host must too: an object of a static
library is linked only when something refers to it, and such a task would be
missing on the host. This checks that every global symbol that an object of
pb_core defines is defined in pb-host:

  check_host_link.py <nm> <pb-host> <object>[;<object>...]...
"""

from __future__ import annotations

import subprocess
import sys


def defined(nm: str, path: str) -> set[str]:
    """Return the global symbols that the file defines."""
    out = subprocess.run(  # noqa: S603
        [nm, "--defined-only", "--extern-only", "--format=posix", path],
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return {line.split()[0] for line in out.splitlines() if line.strip()}


def main() -> int:
    """Check each object and return 1 when one is missing from pb-host."""
    nm, host, *args = sys.argv[1:]
    # CMake gives the objects as one list, separated by ';'
    objects = [obj for arg in args for obj in arg.split(";") if obj]
    in_host = defined(nm, host)
    problems = 0

    for obj in objects:
        missing = sorted(defined(nm, obj) - in_host)
        if missing:
            sys.stdout.write(f"not in pb-host: {obj}: {', '.join(missing)}\n")
            problems += 1

    sys.stdout.write(f"{len(objects)} objects, {problems} not linked\n")
    return 1 if problems or not objects else 0


if __name__ == "__main__":
    sys.exit(main())
