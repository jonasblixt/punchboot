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
BUFFERS_NOT_IN = "pb's buffers (CONFIG_PB_BUFFERS_BASE, _SIZE) are not in "
PB_SIZE_AFTER = "CONFIG_PB_SIZE ends pb's region after the memory the ROM loads pb to"
IMX6UL_ENTRY = "CONFIG_PB_ENTRY is not where the i.MX6UL's ROM loads pb to"

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
    # The i.MX6UL's windows are the board's DRAM and the OCRAM from 0x908000:
    # below the DRAM, at its end, and in the OCRAM's part that is the ROM's
    ("jiffy", {"CONFIG_PB_ENTRY": "0x7ffff000"}, IMX6UL_ENTRY),
    ("jiffy", {"CONFIG_PB_ENTRY": "0xc0000000"}, IMX6UL_ENTRY),
    ("jiffy", {"CONFIG_PB_ENTRY": "0x00907000"}, IMX6UL_ENTRY),
    # A region of pb's that ends after its window, or is empty
    ("jiffy", {"CONFIG_PB_SIZE": "0x40000000"}, PB_SIZE_AFTER),
    ("jiffy", {"CONFIG_PB_ENTRY": "0x00908000", "CONFIG_PB_SIZE": "0x19000"}, PB_SIZE_AFTER),
    ("imx8mpevk", {"CONFIG_PB_SIZE": "0x61000"}, PB_SIZE_AFTER),
    ("imxrt1060evk", {"CONFIG_PB_SIZE": "0x78000"}, PB_SIZE_AFTER),
    (
        "imx8qxmek",
        {"CONFIG_PB_SIZE": "0xc0001000"},
        "pb (CONFIG_PB_ENTRY, CONFIG_PB_SIZE) is not in the board's DRAM",
    ),
    (
        "imx8qxmek",
        {"CONFIG_PB_SIZE": "0x20000"},
        "pb (CONFIG_PB_ENTRY, CONFIG_PB_SIZE) is not in the board's DRAM",
    ),
    # Buffers that aren't in memory that works after plat_init(): over the
    # DRAM's end, below it, in no memory, and over the on-chip RAM's end
    (
        "jiffy",
        {"CONFIG_PB_BUFFERS_BASE": "0xbf001000"},
        BUFFERS_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "jiffy",
        {"CONFIG_PB_BUFFERS_BASE": "0x00910000"},
        BUFFERS_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "imx8mpevk",
        {"CONFIG_PB_BUFFERS_BASE": "0x3f800000"},
        BUFFERS_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    ("imx8qxmek", {"CONFIG_PB_BUFFERS_BASE": "0x70000000"}, BUFFERS_NOT_IN + "the board's DRAM"),
    (
        "imx8qxmek",
        {"CONFIG_PB_BUFFERS_BASE": "0x13f001000"},
        BUFFERS_NOT_IN + "the board's DRAM",
    ),
    (
        "imxrt1060evk",
        {"CONFIG_PB_BUFFERS_BASE": "0x2028a000"},
        BUFFERS_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
    (
        "imxrt1060evk",
        {"CONFIG_PB_BUFFERS_BASE": "0x80000000"},
        BUFFERS_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
]

# The regions the boards' defconfigs give, (origin, length), as pb.map has
# them. A length that ld can't work out is every address, without a message.
REGIONS = {
    "jiffy": {"ram": (0x80001000, 0x100000), "buffers": (0x81021000, 0x1000000)},
    "imxrt1060evk": {"ram": (0x20249000, 0x40000), "buffers": (0x20289000, 0x37000)},
    "imx8qxmek": {
        "reset": (0x80000000, 0x20000),
        "ram": (0x80020000, 0x1000000),
        "buffers": (0x85000000, 0x1000000),
    },
    "imx8mpevk": {
        "ram": (0x920000, 0x60000),
        "umctl2": (0x40000000, 0x20000),
        "buffers": (0x40060000, 0x1000000),
    },
}

# (board, the values that are changed, the regions they give): other places
# the checks allow. The link fails when pb or its buffers don't fit there,
# with the linker's "region ... overflowed", but not with a message of the
# checks
ALLOWED = [
    # The buffers in the on-chip RAM. On the RT pb then has its window to the end
    (
        "jiffy",
        {"CONFIG_PB_BUFFERS_BASE": "0x00900000", "CONFIG_PB_BUFFERS_SIZE": "0x20000"},
        {"ram": (0x80001000, 0x100000), "buffers": (0x00900000, 0x20000)},
    ),
    (
        "imxrt1060evk",
        {"CONFIG_PB_BUFFERS_BASE": "0x20000000", "CONFIG_PB_BUFFERS_SIZE": "0x20000"},
        {"ram": (0x20249000, 0x77000), "buffers": (0x20000000, 0x20000)},
    ),
    # pb in the i.MX6UL's OCRAM, with the region platform.md 5.1 derives:
    # to the window's end
    (
        "jiffy",
        {"CONFIG_PB_ENTRY": "0x00908000", "CONFIG_PB_SIZE": "0x0"},
        {"ram": (0x00908000, 0x18000)},
    ),
    # pb's region derived in the DRAM: to the buffers that follow it
    ("jiffy", {"CONFIG_PB_SIZE": "0x0"}, {"ram": (0x80001000, 0x1020000)}),
    ("imx8qxmek", {"CONFIG_PB_SIZE": "0x0"}, {"ram": (0x80020000, 0x4FE0000)}),
    # and to the DRAM's end when they are below it
    (
        "jiffy",
        {"CONFIG_PB_ENTRY": "0x83000000", "CONFIG_PB_SIZE": "0x0"},
        {"ram": (0x83000000, 0x3D000000)},
    ),
    # A board that gives pb less
    ("imx8mpevk", {"CONFIG_PB_SIZE": "0x50000"}, {"ram": (0x920000, 0x50000)}),
    ("imxrt1060evk", {"CONFIG_PB_SIZE": "0x30000"}, {"ram": (0x20249000, 0x30000)}),
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

    def regions(self, board: str) -> dict[str, tuple[int, int]]:
        """Return the regions of the board's last link, from its map."""
        regions = {}
        lines = (self.work / board / "pb.map").read_text().splitlines()
        for line in lines[lines.index("Memory Configuration") + 3 :]:
            fields = line.split()
            if not fields or fields[0] == "*default*":
                break
            regions[fields[0]] = (int(fields[1], 16), int(fields[2], 16))
        return regions

    def check_links(self, what: str) -> None:
        """Check that every board links with its defconfig, with its regions."""
        for board in BOARDS:
            result = self.make(board, {})
            if result.returncode != 0:
                say(f"FAILED: {board}: {what}:\n{result.stdout}{result.stderr}")
                self.problems += 1
            elif self.regions(board) != REGIONS[board]:
                say(f"FAILED: {board}: {what}: with the regions {self.regions(board)}")
                self.problems += 1
            else:
                say(f"ok: {board}: {what}")

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

    def check_allowed(
        self, board: str, values: dict[str, str], expected: dict[str, tuple[int, int]]
    ) -> None:
        """Check that no check refuses the board's link with the values, and its regions."""
        (self.work / board / "pb.map").unlink()
        result = self.make(board, values)
        what = ", ".join(f"{name}={value}" for name, value in values.items())
        refused = [line for line in result.stderr.splitlines() if "CONFIG_" in line]
        regions = self.regions(board)
        found = {name: regions.get(name) for name in expected}
        if refused:
            say(f"FAILED: {board}: {what}: refused:\n{result.stderr}")
            self.problems += 1
        elif found != expected:
            say(f"FAILED: {board}: {what}: the regions {found}, expected {expected}")
            self.problems += 1
        else:
            fits = "links" if result.returncode == 0 else "is allowed (pb doesn't fit there)"
            say(f"ok: {board}: {what}: {fits}")


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
    for board, values, regions in ALLOWED:
        links.check_allowed(board, values, regions)
    links.check_links("and its defconfig links again")

    return links.problems


if __name__ == "__main__":
    sys.exit(main())
