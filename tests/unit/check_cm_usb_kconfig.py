#!/usr/bin/env python3
"""Check command mode's USB port (doc/interfaces/platform.md 3.10).

Which of the SoC's ports command mode uses is a Kconfig choice of the
platform's, made in the board's defconfig, and plat_cm_init() sets that port
up. Kconfig is where that is decided, so this reads Kconfig:

  check_cm_usb_kconfig.py <source dir>

For each board: its defconfig chooses the port that its cm_init hook set up
before the platform did, the choice has one port, the drivers that port's set
up calls are in the build whatever the defconfig says of them, and a build
without command mode has no port.
"""

from __future__ import annotations

import importlib
import os
import pathlib
import sys

USB_STACK = ["DRIVER_USB_DEVICE", "DRIVER_USB_PB_CLS"]
CHIPIDEA = [*USB_STACK, "DRIVER_IMX_USB2_PHY", "DRIVER_IMX_CI_UDC"]
DWC3 = [*USB_STACK, "DRIVER_IMX8M_PHY", "DRIVER_IMX_DWC3_UDC"]

# The board, its port, and the drivers plat_cm_init() calls for that port
BOARDS = [
    ("jiffy", "IMX6UL_CM_USB_OTG1", CHIPIDEA),
    ("imx8mevk", "IMX8M_CM_USB1", DWC3),
    ("imx8mpevk", "IMX8M_CM_USB1", DWC3),
]


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def check_board(kconfiglib, root: pathlib.Path, board: str, port: str, drivers: list[str]) -> int:  # noqa: ANN001
    """Check one board's defconfig and return the number of problems."""
    defconfig = str(root / "configs" / f"{board}_defconfig")
    problems = 0

    kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
    kconf.load_config(defconfig)
    sym = kconf.syms.get(port)

    if sym is None or sym.choice is None:
        say(f"{board}: {port} is not a port of a choice")
        return 1

    chosen = [s.name for s in sym.choice.syms if s.str_value == "y"]
    if chosen != [port]:
        say(f"{board}: the port is {chosen}, expected {port}")
        problems += 1

    # The port brings its drivers, also when the defconfig turned them off
    for driver in drivers:
        kconf.syms[driver].set_value("n")
    for driver in drivers:
        if kconf.syms[driver].str_value != "y":
            say(f"{board}: {port} without {driver}")
            problems += 1

    kconf = kconfiglib.Kconfig("src/Kconfig", warn=False)
    kconf.load_config(defconfig)
    kconf.syms["CM"].set_value("n")
    if kconf.syms[port].str_value != "n":
        say(f"{board}: {port} without command mode")
        problems += 1

    return problems


def main() -> int:
    """Check every board and return 1 when there is a problem."""
    root = pathlib.Path(sys.argv[1]).resolve()
    # Kconfig's 'source' lines are relative to the source tree
    os.chdir(root)
    sys.path.insert(0, str(root / "scripts"))
    kconfiglib = importlib.import_module("kconfiglib")

    problems = 0

    for board, port, drivers in BOARDS:
        problems += check_board(kconfiglib, root, board, port, drivers)

    say(f"{problems} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
