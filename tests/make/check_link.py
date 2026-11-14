#!/usr/bin/env python3
"""Check that the link refuses a board's addresses that can't work.

Where pb is linked, where its buffers are and the board's DRAM come from the
board's makefile.mk, and the platform's link script checks them against what
the SoC allows (platform.md 5.1, F-239). A check that doesn't fail when its
condition is broken checks nothing, so this links a board of each platform
with its values, and then with each value broken, and reads the linker's
message:

  check_link.py <source dir> <work dir>

A value is changed on make's command line, so only the link is made again.
The C sees the same values as the link, and a board's own checks of them
(board.c's static asserts) refuse some of the cases before the link. The
cases are the platform's, so they link a copy of the board whose board.c is
the board's without its static asserts, with the same makefile.mk; the board
itself links with its values.
The Firmware Handoff region is checked too: only the RT1060 EVK has one, so
the other boards are also built with a defconfig that has it ("<board>-fh").
It needs the firmware's tools (arm-none-eabi-gcc, aarch64-linux-gnu-gcc and
the bpak CLI, all in the dev container) and is skipped without them.
"""

from __future__ import annotations

import concurrent.futures
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

# A Firmware Handoff region that links, for the boards whose defconfig has none
FH_REGION = {
    "jiffy": "0x81020000",
    "imx8qxmek": "0x84000000",
    "imx8mpevk": "0x42000000",
}

# What a board's C is with the cases: the board's, without its static asserts
UNCHECKED_BOARD_C = """#define _Static_assert(condition, message)
#include "{path}"
"""

FH_NOT_IN = "The Firmware Handoff region (CONFIG_BOOT_FH_REGION_BASE, _SIZE) is not in "
BUFFERS_OVERLAP = "pb's buffers (PB_BUFFERS_BASE, _SIZE) overlap pb"
BUFFERS_NOT_IN = "pb's buffers (PB_BUFFERS_BASE, _SIZE) are not in "
PB_SIZE_AFTER = "PB_SIZE ends pb's region after the memory the ROM loads pb to"
IMX6UL_ENTRY = "PB_ENTRY is not where the i.MX6UL's ROM loads pb to"

# (board, the values that are changed, what the build says)
CASES = [
    ("jiffy", {"PB_ENTRY": ""}, "PB_ENTRY is not set"),
    (
        "imx8qxmek",
        {"PB_ENTRY": "0x80020000"},
        "PB_ENTRY is not 0x80000000, where the i.MX8X starts pb",
    ),
    # pb outside the ROM's load window: in the ROM's own part of the RAM,
    # and one byte after the window
    (
        "imx8mpevk",
        {"PB_ENTRY": "0x91f000"},
        "PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to",
    ),
    (
        "imx8mpevk",
        {"PB_ENTRY": "0x980000"},
        "PB_ENTRY is not in the on-chip RAM the i.MX8M's ROM loads pb to",
    ),
    (
        "imxrt1060evk",
        {"PB_ENTRY": "0x20207000"},
        "PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to",
    ),
    (
        "imxrt1060evk",
        {"PB_ENTRY": "0x202c0000"},
        "PB_ENTRY is not in the on-chip RAM the i.MX RT's ROM loads pb to",
    ),
    # The buffers where pb is: at its start, over its end, and around it
    ("jiffy", {"PB_BUFFERS_BASE": "0x80001000"}, BUFFERS_OVERLAP),
    ("jiffy", {"PB_BUFFERS_BASE": "0x80100000"}, BUFFERS_OVERLAP),
    (
        "jiffy",
        {"PB_BUFFERS_BASE": "0x80000000", "PB_BUFFERS_SIZE": "0x2000000"},
        BUFFERS_OVERLAP,
    ),
    ("imxrt1060evk", {"PB_BUFFERS_BASE": "0x20249000"}, BUFFERS_OVERLAP),
    ("imxrt1060evk", {"PB_BUFFERS_BASE": "0x20240000"}, BUFFERS_OVERLAP),
    # The i.MX8X: in the reset stub's region, and in pb's after it
    ("imx8qxmek", {"PB_BUFFERS_BASE": "0x80010000"}, BUFFERS_OVERLAP),
    ("imx8qxmek", {"PB_BUFFERS_BASE": "0x80030000"}, BUFFERS_OVERLAP),
    # The i.MX8M's are in the DRAM, after the DDR controller's state there
    (
        "imx8mpevk",
        {"PB_BUFFERS_BASE": "0x930000", "PB_BUFFERS_SIZE": "0x10000"},
        BUFFERS_NOT_IN + "the board's DRAM, after the DDR controller's state",
    ),
    (
        "imx8mpevk",
        {"PB_BUFFERS_BASE": "0x4001f000"},
        BUFFERS_NOT_IN + "the board's DRAM, after the DDR controller's state",
    ),
    # The i.MX6UL's windows are the board's DRAM and the OCRAM from 0x908000:
    # below the DRAM, at its end, and in the OCRAM's part that is the ROM's
    ("jiffy", {"PB_ENTRY": "0x7ffff000"}, IMX6UL_ENTRY),
    ("jiffy", {"PB_ENTRY": "0xc0000000"}, IMX6UL_ENTRY),
    ("jiffy", {"PB_ENTRY": "0x00907000"}, IMX6UL_ENTRY),
    # A region of pb's that ends after its window, or is empty
    ("jiffy", {"PB_SIZE": "0x40000000"}, PB_SIZE_AFTER),
    ("jiffy", {"PB_ENTRY": "0x00908000", "PB_SIZE": "0x19000"}, PB_SIZE_AFTER),
    ("imx8mpevk", {"PB_SIZE": "0x61000"}, PB_SIZE_AFTER),
    ("imxrt1060evk", {"PB_SIZE": "0x78000"}, PB_SIZE_AFTER),
    (
        "imx8qxmek",
        {"PB_SIZE": "0xc0001000"},
        "pb (PB_ENTRY, PB_SIZE) is not in the board's DRAM",
    ),
    (
        "imx8qxmek",
        {"PB_SIZE": "0x20000"},
        "pb (PB_ENTRY, PB_SIZE) is not in the board's DRAM",
    ),
    # Buffers that aren't in memory that works after plat_init(): over the
    # DRAM's end, below it, in no memory, and over the on-chip RAM's end
    (
        "jiffy",
        {"PB_BUFFERS_BASE": "0xbf022000"},
        BUFFERS_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "jiffy",
        {"PB_BUFFERS_BASE": "0x00910000"},
        BUFFERS_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "imx8mpevk",
        {"PB_BUFFERS_BASE": "0x3f800000"},
        BUFFERS_NOT_IN + "the board's DRAM, after the DDR controller's state",
    ),
    (
        "imx8mpevk",
        {"PB_BUFFERS_BASE": "0xbf800000"},
        BUFFERS_NOT_IN + "the board's DRAM, after the DDR controller's state",
    ),
    ("imx8qxmek", {"PB_BUFFERS_BASE": "0x70000000"}, BUFFERS_NOT_IN + "the board's DRAM"),
    (
        "imx8qxmek",
        {"PB_BUFFERS_BASE": "0x13f001000"},
        BUFFERS_NOT_IN + "the board's DRAM",
    ),
    (
        "imxrt1060evk",
        {"PB_BUFFERS_BASE": "0x2028a000"},
        BUFFERS_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
    (
        "imxrt1060evk",
        {"PB_BUFFERS_BASE": "0x80000000"},
        BUFFERS_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
    # The linker holds the regions' ends, where a board's image data region
    # starts (F-391): a pb or buffers that need more than the board gives them
    ("jiffy", {"PB_SIZE": "0x30000"}, "region `ram' overflowed"),
    ("jiffy", {"PB_BUFFERS_SIZE": "0x800000"}, "is not within region `buffers'"),
    ("imx8qxmek", {"PB_SIZE": "0x50000"}, "region `ram' overflowed"),
    ("imx8mpevk", {"PB_SIZE": "0x3f000"}, "region `ram' overflowed"),
    ("imx8mpevk", {"PB_BUFFERS_SIZE": "0x800000"}, "is not within region `buffers'"),
    # The stack right above .ro_data, which is read only with the MMU on: a
    # link script that has something between the two (F-209)
    (
        "jiffy",
        {"CONFIG_LINK_FILE": "{work}/link_gap.lds"},
        "The stack doesn't start where .ro_data ends",
    ),
    (
        "imx8qxmek",
        {"CONFIG_LINK_FILE": "{work}/link_gap.lds"},
        "The stack doesn't start where .ro_data ends",
    ),
    # A Firmware Handoff region that isn't in such memory: in the ROM's data,
    # in the flash, in no memory, over the DRAM's end and below it, over the
    # OCRAM's end
    (
        "imxrt1060evk",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x20207000"},
        FH_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
    (
        "imxrt1060evk",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x60000000"},
        FH_NOT_IN + "the OCRAM, the DTCM or the board's DRAM",
    ),
    (
        "jiffy-fh",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x10000000"},
        FH_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "jiffy-fh",
        {"CONFIG_BOOT_FH_REGION_BASE": "0xbffff800"},
        FH_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "imx8qxmek-fh",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x7ffff000"},
        FH_NOT_IN + "the board's DRAM",
    ),
    (
        "imx8mpevk-fh",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x3ffff000"},
        FH_NOT_IN + "the board's DRAM or the OCRAM",
    ),
    (
        "imx8mpevk-fh",
        {"CONFIG_BOOT_FH_REGION_BASE": "0x98f800"},
        FH_NOT_IN + "the board's DRAM or the OCRAM",
    ),
]

# What the checks say, and not the linker: the cases that the linker refuses
# say "region"
REFUSALS = {message for _, _, message in CASES if "region `" not in message}

# The regions the boards' makefile.mk give, (origin, length), as pb.map has
# them. A length that ld can't work out is every address, without a message.
# pb's region and the buffers' end where the board's next image data region
# starts (F-391): jiffy's at 0x80101000 and 0x82000000, the MEK's at
# 0x80080000 and 0x86000000, the 8MP EVK's at 0x970000 (BL31's on-chip RAM)
# and 0x41060000, the RT1060 EVK's below pb.
REGIONS = {
    "jiffy": {"ram": (0x80001000, 0x100000), "buffers": (0x81021000, 0xFDF000)},
    "imxrt1060evk": {"ram": (0x20249000, 0x40000), "buffers": (0x20289000, 0x37000)},
    "imx8qxmek": {
        "reset": (0x80000000, 0x20000),
        "ram": (0x80020000, 0x60000),
        "buffers": (0x85000000, 0x1000000),
    },
    "imx8mpevk": {
        "ram": (0x920000, 0x50000),
        "buffers": (0x40000000, 0x1060000),
    },
}

# The ARMv7-M's entry and exception handlers are in a section the script
# places, with content, not in an orphan that ld puts where it sees fit (F-209)
SCRIPT_SECTION = ".vector_handlers"

# (board, the values that are changed, the regions they give): other places
# the checks allow. The link fails when pb or its buffers don't fit there,
# with the linker's "region ... overflowed", but not with a message of the
# checks
ALLOWED = [
    # The buffers in the on-chip RAM. On the RT pb then has its window to the end
    (
        "jiffy",
        {"PB_BUFFERS_BASE": "0x00900000", "PB_BUFFERS_SIZE": "0x20000"},
        {"ram": (0x80001000, 0x100000), "buffers": (0x00900000, 0x20000)},
    ),
    (
        "imxrt1060evk",
        {"PB_BUFFERS_BASE": "0x20000000", "PB_BUFFERS_SIZE": "0x20000"},
        {"ram": (0x20249000, 0x77000), "buffers": (0x20000000, 0x20000)},
    ),
    # pb in the i.MX6UL's OCRAM, with the region platform.md 5.1 derives:
    # to the window's end
    (
        "jiffy",
        {"PB_ENTRY": "0x00908000", "PB_SIZE": "0x0"},
        {"ram": (0x00908000, 0x18000)},
    ),
    # pb's region derived in the DRAM: to the buffers that follow it
    ("jiffy", {"PB_SIZE": "0x0"}, {"ram": (0x80001000, 0x1020000)}),
    ("imx8qxmek", {"PB_SIZE": "0x0"}, {"ram": (0x80020000, 0x4FE0000)}),
    # and in the i.MX8M's on-chip RAM: to the window's end
    ("imx8mpevk", {"PB_SIZE": "0x0"}, {"ram": (0x920000, 0x60000)}),
    # and to the DRAM's end when they are below it
    (
        "jiffy",
        {"PB_ENTRY": "0x83000000", "PB_SIZE": "0x0"},
        {"ram": (0x83000000, 0x3D000000)},
    ),
    # A board that gives pb less
    ("imx8mpevk", {"PB_SIZE": "0x58000"}, {"ram": (0x920000, 0x58000)}),
    ("imxrt1060evk", {"PB_SIZE": "0x30000"}, {"ram": (0x20249000, 0x30000)}),
    # The i.MX8M's buffers right after the DDR controller's state
    (
        "imx8mpevk",
        {"PB_BUFFERS_BASE": "0x40020000"},
        {"buffers": (0x40000000, 0x1020000)},
    ),
    # The ARMv7-M has no MMU: the stack may be anywhere
    ("imxrt1060evk", {"CONFIG_LINK_FILE": "{work}/link_gap.lds"}, {}),
    # The Firmware Handoff region in the on-chip RAM
    ("jiffy-fh", {"CONFIG_BOOT_FH_REGION_BASE": "0x00900000"}, {}),
    ("imx8mpevk-fh", {"CONFIG_BOOT_FH_REGION_BASE": "0x00980000"}, {}),
    ("imxrt1060evk", {"CONFIG_BOOT_FH_REGION_BASE": "0x20000000"}, {}),
]


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


class Links:
    """The boards' builds, a build directory each in the work directory.

    A check returns its line of the report, which starts with "ok" or "FAILED".
    """

    def __init__(self, root: pathlib.Path, work: pathlib.Path) -> None:
        """Start with an empty work directory."""
        self.root = root
        self.work = work
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
        # src/link.lds with a page between .ro_data and the stack
        script = (root / "src/link.lds").read_text()
        stack = "     .stack (NOLOAD):\n"
        gap = "    .gap (NOLOAD) : { . = . + 4096; } > ram\n\n"
        (work / "link_gap.lds").write_text(script.replace(stack, gap + stack))
        # The unchecked boards: the board's directory, its board.c without
        # the static asserts
        for board in BOARDS:
            unchecked = work / "boards" / board
            shutil.copytree(root / f"src/board/{board}", unchecked)
            path = root / f"src/board/{board}/board.c"
            (unchecked / "board.c").write_text(UNCHECKED_BOARD_C.format(path=path))
        # The boards' defconfigs with a Firmware Handoff region
        for board, base in FH_REGION.items():
            lines = (root / f"configs/{board}_defconfig").read_text().splitlines()
            lines = [line for line in lines if "CONFIG_BOOT_FH" not in line]
            lines += [
                "CONFIG_BOOT_FH=y",
                f"CONFIG_BOOT_FH_REGION_BASE={base}",
                "CONFIG_BOOT_FH_REGION_SIZE=0x1000",
            ]
            (work / f"{board}_fh_defconfig").write_text("\n".join(lines) + "\n")

    @staticmethod
    def build_dir(build: str, *, unchecked: bool) -> str:
        """Return the name of the build's directory: the unchecked board's has its own."""
        return f"{build}-unchecked" if unchecked else build

    def make(
        self, build: str, values: dict[str, str], *, unchecked: bool = False
    ) -> subprocess.CompletedProcess[str]:
        """Link pb with the values: a board's, or "<board>-fh" with the region.

        unchecked: the board's copy without its static asserts.
        """
        board = build.removesuffix("-fh")
        config = f"configs/{board}_defconfig"
        if build != board:
            config = str(self.work / f"{board}_fh_defconfig")
        if board == "imx8mpevk":
            values = {**self.firmware, **values}
        path = self.work / "boards" / board if unchecked else f"src/board/{board}"
        out = self.work / self.build_dir(build, unchecked=unchecked)
        cmd = [
            "make",
            f"-j{os.cpu_count() or 1}",
            f"BOARD={path}",
            f"KCONFIG_CONFIG={config}",
            f"CROSS_COMPILE={BOARDS[board]}",
            f"BUILD_DIR={out}",
            *(f"{name}={value.format(work=self.work)}" for name, value in values.items()),
            f"{out}/pb",
        ]
        return subprocess.run(  # noqa: S603
            cmd, cwd=self.root, env=self.env, capture_output=True, text=True, check=False
        )

    def regions(self, build: str, *, unchecked: bool = False) -> dict[str, tuple[int, int]] | None:
        """Return the regions of the build's last link, from its map, None without one."""
        regions = {}
        path = self.work / self.build_dir(build, unchecked=unchecked) / "pb.map"
        if not path.exists():
            return None
        lines = path.read_text().splitlines()
        for line in lines[lines.index("Memory Configuration") + 3 :]:
            fields = line.split()
            if not fields or fields[0] == "*default*":
                break
            regions[fields[0]] = (int(fields[1], 16), int(fields[2], 16))
        return regions

    def check_section(self, build: str, name: str) -> str:
        """Check that the section has content and is one the link script places."""
        lines = (self.work / build / "pb.map").read_text().splitlines()
        # The output section's line, with its address and size after the name
        # or on the next line; the script's pattern follows only when the
        # script placed it
        at = max(i for i, line in enumerate(lines) if line.startswith(name))
        size = int((lines[at].split()[1:] or lines[at + 1].split())[1], 16)
        if size and any(line.strip().startswith(f"*({name}") for line in lines[at : at + 3]):
            return f"ok: {build}: {name} is a section of the link script's, {size} bytes"
        return f"FAILED: {build}: {name} is an orphan section or empty:\n" + "\n".join(
            lines[at : at + 3]
        )

    def check_links(self, build: str, what: str) -> str:
        """Check that the build links with the board's values, with its regions."""
        result = self.make(build, {})
        if result.returncode != 0:
            return f"FAILED: {build}: {what}:\n{result.stdout}{result.stderr}"
        if self.regions(build) != REGIONS[build.removesuffix("-fh")]:
            return f"FAILED: {build}: {what}: with the regions {self.regions(build)}"
        return f"ok: {build}: {what}"

    def check_refused(self, build: str, values: dict[str, str], message: str) -> str:
        """Check that the build's link with the values fails with the message."""
        result = self.make(build, values, unchecked=True)
        what = ", ".join(f"{name}={value}" for name, value in values.items())
        if result.returncode != 0 and message in result.stderr:
            return f"ok: {build}: {what}: {message}"
        return (
            f"FAILED: {build}: {what}: exit {result.returncode}, expected '{message}':\n"
            f"{result.stderr}"
        )

    def check_allowed(
        self, build: str, values: dict[str, str], expected: dict[str, tuple[int, int]]
    ) -> str:
        """Check that no check refuses the build's link with the values, and its regions."""
        (self.work / self.build_dir(build, unchecked=True) / "pb.map").unlink(missing_ok=True)
        result = self.make(build, values, unchecked=True)
        what = ", ".join(f"{name}={value}" for name, value in values.items())
        refused = [message for message in REFUSALS if message in result.stderr]
        regions = self.regions(build, unchecked=True)
        if refused or regions is None:
            return f"FAILED: {build}: {what}: refused:\n{result.stderr}"
        # A link that fails may fail only where pb or its buffers don't fit
        if result.returncode != 0 and not any(
            message in result.stderr for message in ("overflowed", "is not within region")
        ):
            return f"FAILED: {build}: {what}: exit {result.returncode}:\n{result.stderr}"
        found = {name: regions.get(name) for name in expected}
        if found != expected:
            return f"FAILED: {build}: {what}: the regions {found}, expected {expected}"
        fits = "links" if result.returncode == 0 else "is allowed (pb doesn't fit there)"
        return f"ok: {build}: {what}: {fits}"

    def check_board(self, board: str) -> list[str]:
        """Run the board's cases, and those of its build with the region."""
        report = []
        for build in (board, f"{board}-fh") if board in FH_REGION else (board,):
            report.append(self.check_links(build, "its values link"))
            if board == "imxrt1060evk":
                report.append(self.check_section(build, SCRIPT_SECTION))
            report += [self.check_refused(*case) for case in CASES if case[0] == build]
            report += [self.check_allowed(*case) for case in ALLOWED if case[0] == build]
            report.append(self.check_links(build, "and its values link again"))
        return report


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    tools = ("make", f"{ARM}gcc", f"{AARCH64}gcc", "bpak")
    missing = [t for t in tools if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    # The boards at once, each in build directories of its own. A board's two
    # builds run after each other: the i.MX8M's both copy its firmware to blobs/
    links = Links(root, work)
    with concurrent.futures.ThreadPoolExecutor() as pool:
        reports = list(pool.map(links.check_board, BOARDS))

    lines = [line for report in reports for line in report]
    for line in lines:
        say(line)
    return sum(not line.startswith("ok: ") for line in lines)


if __name__ == "__main__":
    sys.exit(main())
