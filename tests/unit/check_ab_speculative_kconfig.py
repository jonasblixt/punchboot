#!/usr/bin/env python3
"""Check the one switch of A/B's speculative mode (doc/architecture/boot.md 3.6).

CONFIG_BOOT_AB_SPECULATIVE is off unless a configuration asks for it, it is
A/B's, and it depends on the watchdog (rule 6): without one a hang stays
hung, and the alternation that was written before the load leads nowhere.
Kconfig is where that is decided, so this reads Kconfig:

  check_ab_speculative_kconfig.py <source dir>

It also checks that no configuration in configs/ has the mode: a device that
boots today boots the same with this build. The host's second configuration
for the tests of the mode (host_speculative_defconfig) is the one that has it.
"""

from __future__ import annotations

import importlib
import os
import pathlib
import sys

SYMBOL = "BOOT_AB_SPECULATIVE"

# The configurations that have the mode: pb-host-speculative's
TEST_CONFIGS = {"host_speculative_defconfig"}

# What a configuration sets, and what SYMBOL then is
CASES = [
    ("off by default", {"BOOT_POLICY_AB": "y", "ENABLE_WATCHDOG": "y"}, "n"),
    (
        "on with A/B and the watchdog",
        {"BOOT_POLICY_AB": "y", "ENABLE_WATCHDOG": "y", SYMBOL: "y"},
        "y",
    ),
    (
        "not without the watchdog",
        {"BOOT_POLICY_AB": "y", "ENABLE_WATCHDOG": "n", SYMBOL: "y"},
        "n",
    ),
    (
        "not without A/B",
        {"BOOT_POLICY_FIXED": "y", "ENABLE_WATCHDOG": "y", SYMBOL: "y"},
        "n",
    ),
]


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    # Kconfig's 'source' lines are relative to the source tree
    os.chdir(root)
    sys.path.insert(0, str(root / "scripts"))
    kconfiglib = importlib.import_module("kconfiglib")

    problems = 0

    for name, values, expected in CASES:
        kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
        for symbol, value in values.items():
            kconf.syms[symbol].set_value(value)
        got = kconf.syms[SYMBOL].str_value
        if got != expected:
            say(f"{SYMBOL} {name}: is {got}, expected {expected}")
            problems += 1

    for defconfig in sorted(root.glob("configs/*_defconfig")):
        # The host's second configuration, for the tests of the mode (F-341):
        # no device boots it, and it must have the mode
        expected = "y" if defconfig.name in TEST_CONFIGS else "n"
        kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
        kconf.load_config(str(defconfig))
        if kconf.syms[SYMBOL].str_value != expected:
            say(f"{defconfig.name}: {SYMBOL} is {kconf.syms[SYMBOL].str_value}, not {expected}")
            problems += 1

    say(f"{problems} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
