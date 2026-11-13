#!/usr/bin/env python3
"""Check that the link refuses a board's addresses that can't work.

Where pb is linked, where its buffers are and the board's DRAM come from the
board's defconfig, and the platform's link script checks them against what
the SoC allows (platform.md 5.1, F-239). A check that doesn't fail when its
condition is broken checks nothing, so this links a board of each platform
with its defconfig, and then with each value broken, and reads the linker's
message:

  check_link.py <source dir> <work dir>

A value is changed on make's command line, so only the link is made again.
It needs the firmware's tools (arm-none-eabi-gcc, aarch64-linux-gnu-gcc and
the bpak CLI, all in the dev container) and is skipped without them.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

SKIP = 77
ARM = "arm-none-eabi-"
AARCH64 = "aarch64-linux-gnu-"

# The boards that are linked, one of each platform, with their toolchain
BOARDS = {
    "jiffy": ARM,
    "imxrt1060evk": ARM,
    "imx8qxmek": AARCH64,
    "imx8mpevk": AARCH64,
}

# The i.MX8M's DDR training firmware isn't in the repository: placeholders,
# as scripts/ci/build-firmware.sh makes them
IMX8M_FIRMWARE = {
    "IMX8M_UMCTL_TRAIN_1D_I": "lpddr4_pmu_train_1d_imem.bin",
    "IMX8M_UMCTL_TRAIN_1D_D": "lpddr4_pmu_train_1d_dmem.bin",
    "IMX8M_UMCTL_TRAIN_2D_I": "lpddr4_pmu_train_2d_imem.bin",
    "IMX8M_UMCTL_TRAIN_2D_D": "lpddr4_pmu_train_2d_dmem.bin",
}

BUFFERS_OVERLAP = "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) overlap pb"

# (board, the values that are changed, what the build says)
CASES = [
    ("jiffy", {"CONFIG_PB_ENTRY": ""}, "CONFIG_PB_ENTRY is not set"),
    (
        "imx8qxmek",
        {"CONFIG_PB_ENTRY": "0x80020000"},
        "CONFIG_PB_ENTRY is not 0x80000000, where the i.MX8X starts pb",
    ),
    # pb outside the ROM's load window: in the ROM's own part of the RAM,
    # and one byte after the window
    (
        "imx8mpevk",
        {"CONFIG_PB_ENTRY": "0x91f000"},
        "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to",
    ),
    (
        "imx8mpevk",
        {"CONFIG_PB_ENTRY": "0x980000"},
        "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to",
    ),
    (
        "imxrt1060evk",
        {"CONFIG_PB_ENTRY": "0x20207000"},
        "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to",
    ),
    (
        "imxrt1060evk",
        {"CONFIG_PB_ENTRY": "0x202c0000"},
        "CONFIG_PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to",
    ),
    # The buffers where pb is: at its start, over its end, and around it
    ("jiffy", {"CONFIG_PB_BUFFERS_BASE": "0x80001000"}, BUFFERS_OVERLAP),
    ("jiffy", {"CONFIG_PB_BUFFERS_BASE": "0x80100000"}, BUFFERS_OVERLAP),
    (
        "jiffy",
        {"CONFIG_PB_BUFFERS_BASE": "0x80000000", "CONFIG_PB_BUFFERS_SIZE": "0x2000000"},
        BUFFERS_OVERLAP,
    ),
    ("imxrt1060evk", {"CONFIG_PB_BUFFERS_BASE": "0x20249000"}, BUFFERS_OVERLAP),
    ("imxrt1060evk", {"CONFIG_PB_BUFFERS_BASE": "0x20240000"}, BUFFERS_OVERLAP),
    # The i.MX8X: in the reset stub's region, and in pb's after it
    ("imx8qxmek", {"CONFIG_PB_BUFFERS_BASE": "0x80010000"}, BUFFERS_OVERLAP),
    ("imx8qxmek", {"CONFIG_PB_BUFFERS_BASE": "0x80030000"}, BUFFERS_OVERLAP),
    ("imx8mpevk", {"CONFIG_PB_BUFFERS_BASE": "0x930000"}, BUFFERS_OVERLAP),
]


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


class Links:
    """The boards' builds, a build directory each in the work directory."""

    def __init__(self, root: pathlib.Path, work: pathlib.Path) -> None:
        """Start with an empty work directory."""
        self.root = root
        self.work = work
        self.problems = 0
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        # Not the jobserver or the variables of a make that runs the tests
        self.env = {
            k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")
        }
        self.firmware = {}
        for name, file in IMX8M_FIRMWARE.items():
            (work / file).write_bytes(bytes(1024))
            self.firmware[name] = str(work / file)

    def make(self, board: str, values: dict[str, str]) -> subprocess.CompletedProcess[str]:
        """Link the board's pb with its defconfig and the values."""
        build = self.work / board
        if board == "imx8mpevk":
            values = {**self.firmware, **values}
        cmd = [
            "make",
            f"-j{os.cpu_count() or 1}",
            f"BOARD=src/board/{board}",
            f"KCONFIG_CONFIG=configs/{board}_defconfig",
            f"CROSS_COMPILE={BOARDS[board]}",
            f"BUILD_DIR={build}",
            *(f"{name}={value}" for name, value in values.items()),
            f"{build}/pb",
        ]
        return subprocess.run(  # noqa: S603
            cmd, cwd=self.root, env=self.env, capture_output=True, text=True, check=False
        )

    def check_links(self, what: str) -> None:
        """Check that every board links with its defconfig."""
        for board in BOARDS:
            result = self.make(board, {})
            if result.returncode == 0:
                say(f"ok: {board}: {what}")
            else:
                say(f"FAILED: {board}: {what}:\n{result.stdout}{result.stderr}")
                self.problems += 1

    def check_refused(self, board: str, values: dict[str, str], message: str) -> None:
        """Check that the board's link with the values fails with the message."""
        result = self.make(board, values)
        what = ", ".join(f"{name}={value}" for name, value in values.items())
        if result.returncode != 0 and message in result.stderr:
            say(f"ok: {board}: {what}: {message}")
        else:
            say(
                f"FAILED: {board}: {what}: exit {result.returncode}, expected '{message}':\n"
                f"{result.stderr}"
            )
            self.problems += 1


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    tools = ("make", f"{ARM}gcc", f"{AARCH64}gcc", "bpak")
    missing = [t for t in tools if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    links = Links(root, work)
    links.check_links("its defconfig links")
    for board, values, message in CASES:
        links.check_refused(board, values, message)
    links.check_links("and its defconfig links again")

    return links.problems


if __name__ == "__main__":
    sys.exit(main())
