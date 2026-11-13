#!/usr/bin/env python3
"""Check that every include directory the firmware build names exists (F-031).

A -I that names a directory that is gone (src/libc/include/... after the
libraries moved) says nothing and hides where the headers are. This reads the
cflags lines of the Makefile and of every makefile.mk under src/; a path with
a make variable in it ($(BOARD)/include) is a board's and isn't checked:

  check_include_dirs.py <source dir>
"""

from __future__ import annotations

import pathlib
import re
import sys

INCLUDE_RE = re.compile(r"-I\s*(\S+)")


def main() -> int:
    """Return the number of include directories that don't exist."""
    root = pathlib.Path(sys.argv[1]).resolve()
    problems = 0
    for makefile in [root / "Makefile", *sorted((root / "src").rglob("makefile.mk"))]:
        for number, line in enumerate(makefile.read_text().splitlines(), 1):
            if not line.startswith("cflags"):
                continue
            for path in INCLUDE_RE.findall(line.split("#", 1)[0]):
                if "$" not in path and not (root / path).is_dir():
                    where = makefile.relative_to(root)
                    sys.stdout.write(f"{where}:{number}: -I {path}: no such directory\n")
                    problems += 1
    return problems


if __name__ == "__main__":
    sys.exit(main())
