#!/usr/bin/env python3
"""Check the watchdog's time-out against what the watchdogs take (F-416).

CONFIG_WATCHDOG_TIMEOUT has a Kconfig range that every platform's watchdog
takes as it is: the i.MX WDOG's eight bits of half seconds (6UL, 8M, RT),
which clamped a longer time-out to 128 s and took 0 as 0.5 s, and the 8X's
SCFW. Kconfig drops a value outside the range from a defconfig with a
warning and builds with the default, so this reads Kconfig:

  check_watchdog_kconfig.py <source dir>

The range is one, from 1, and its end fits the WDOG's WT field without its
clamp (imx_wdog_init() writes 2 * n). Every configs/*_defconfig that has the
watchdog sets a value inside it, and the build has that value, not the
default in its place.
"""

from __future__ import annotations

import importlib
import os
import pathlib
import re
import sys

SYMBOL = "WATCHDOG_TIMEOUT"


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def wt_max(root: pathlib.Path) -> int:
    """Return WDOG_WT_MAX from the i.MX WDOG's header."""
    header = (root / "include/drivers/wdog/imx_wdog.h").read_text()
    match = re.search(r"#define\s+WDOG_WT_MAX\s+(0x[0-9a-fA-F]+|\d+)", header)
    if match is None:
        msg = "WDOG_WT_MAX not found"
        raise RuntimeError(msg)
    return int(match.group(1), 0)


def check_range(kconfiglib, root: pathlib.Path) -> tuple[int, int, int]:  # noqa: ANN001
    """Check the symbol's range and return it with the number of problems."""
    kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
    ranges = kconf.syms[SYMBOL].ranges

    if len(ranges) != 1:
        say(f"{SYMBOL}: {len(ranges)} ranges, expected one")
        return 0, 0, 1

    low, high = int(ranges[0][0].str_value), int(ranges[0][1].str_value)
    problems = 0

    if low < 1:
        say(f"{SYMBOL}: the range starts at {low}: 0 is the WDOG's 0.5 s")
        problems += 1
    if 2 * high > wt_max(root):
        say(f"{SYMBOL}: the range ends at {high}, the WDOG takes {wt_max(root) // 2} s at most")
        problems += 1

    return low, high, problems


def check_defconfig(kconfiglib, defconfig: pathlib.Path, low: int, high: int) -> int:  # noqa: ANN001
    """Check one defconfig's value and return the number of problems."""
    raw = None

    for line in defconfig.read_text().splitlines():
        match = re.fullmatch(rf"CONFIG_{SYMBOL}=(\S+)", line.strip())
        if match is not None:
            raw = match.group(1)

    kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
    kconf.load_config(str(defconfig))

    if kconf.syms["ENABLE_WATCHDOG"].str_value != "y":
        return 0

    built = kconf.syms[SYMBOL].str_value

    if raw is None:
        # The default: inside the range, as the range's check has it
        return 0 if low <= int(built) <= high else 1

    if not low <= int(raw, 0) <= high:
        say(f"{defconfig.name}: {SYMBOL}={raw} is outside [{low}, {high}]")
        return 1

    if int(built) != int(raw, 0):
        say(f"{defconfig.name}: {SYMBOL}={raw}, the build has {built}")
        return 1

    return 0


def main() -> int:
    """Check the range and every defconfig, return 1 when there is a problem."""
    root = pathlib.Path(sys.argv[1]).resolve()
    # Kconfig's 'source' lines are relative to the source tree
    os.chdir(root)
    sys.path.insert(0, str(root / "scripts"))
    kconfiglib = importlib.import_module("kconfiglib")

    low, high, problems = check_range(kconfiglib, root)

    if problems == 0:
        defconfigs = sorted((root / "configs").glob("*_defconfig"))
        if not defconfigs:
            say("no configs/*_defconfig")
            problems += 1
        for defconfig in defconfigs:
            problems += check_defconfig(kconfiglib, defconfig, low, high)

    say(f"{problems} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
