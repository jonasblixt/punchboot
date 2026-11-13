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

# The boards that are linked, with their toolchain
BOARDS = {
    "jiffy": ARM,
    "imx8qxmek": AARCH64,
}

# (board, the values that are changed, what the build says)
CASES = [
    ("jiffy", {"CONFIG_PB_ENTRY": ""}, "CONFIG_PB_ENTRY is not set"),
    (
        "imx8qxmek",
        {"CONFIG_PB_ENTRY": "0x80020000"},
        "CONFIG_PB_ENTRY is not 0x80000000, where the i.MX8X starts pb",
    ),
]


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    tools = ("make", f"{ARM}gcc", f"{AARCH64}gcc", "bpak")
    missing = [t for t in tools if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    # Not the jobserver or the variables of a make that runs the tests
    env = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
    problems = 0

    def make(board: str, values: dict[str, str]) -> subprocess.CompletedProcess[str]:
        build = work / board
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
            cmd, cwd=root, env=env, capture_output=True, text=True, check=False
        )

    def check_links(board: str, what: str) -> None:
        nonlocal problems
        result = make(board, {})
        if result.returncode == 0:
            say(f"ok: {board}: {what}")
        else:
            say(f"FAILED: {board}: {what}:\n{result.stdout}{result.stderr}")
            problems += 1

    for board in BOARDS:
        check_links(board, "its defconfig links")

    for board, values, message in CASES:
        result = make(board, values)
        what = ", ".join(f"{name}={value}" for name, value in values.items())
        if result.returncode != 0 and message in result.stderr:
            say(f"ok: {board}: {what}: {message}")
        else:
            say(
                f"FAILED: {board}: {what}: exit {result.returncode}, expected '{message}':\n"
                f"{result.stderr}"
            )
            problems += 1

    for board in BOARDS:
        check_links(board, "and its defconfig links again")

    return problems


if __name__ == "__main__":
    sys.exit(main())
