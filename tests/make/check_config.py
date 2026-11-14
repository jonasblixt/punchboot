#!/usr/bin/env python3
"""Check the firmware build's configuration (F-229, F-147).

A KCONFIG_CONFIG that doesn't exist would build the Kconfig defaults, and a
failed scripts/genconfig.py the configuration of the build before. Both stop
the build. The goals that need no configuration (clean, menuconfig,
olddefconfig) run without one, and olddefconfig updates a configuration.
jiffy is built as an out-of-tree board, a copy in the work directory, so its
Kconfig can be broken:

  check_config.py <source dir> <work dir>

It needs the firmware's tools (arm-none-eabi-gcc and the bpak CLI, both in the
dev container) and is skipped without them.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

SKIP = 77
CROSS_COMPILE = "arm-none-eabi-"


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    missing = [t for t in ("make", f"{CROSS_COMPILE}gcc", "bpak") if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    board = work / "jiffy"
    shutil.copytree(root / "src/board/jiffy", board)
    build = work / "build"
    env = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
    problems = 0

    def make(config: str, *goals: str) -> subprocess.CompletedProcess[str]:
        cmd = [
            "make",
            f"BOARD={board}",
            f"KCONFIG_CONFIG={config}",
            f"CROSS_COMPILE={CROSS_COMPILE}",
            f"BUILD_DIR={build}",
            *goals,
        ]
        return subprocess.run(  # noqa: S603
            cmd, cwd=root, env=env, capture_output=True, text=True, check=False
        )

    def check(what: str, *, ok: bool, detail: str = "") -> None:
        nonlocal problems
        if ok:
            say(f"ok: {what}")
        else:
            say(f"FAILED: {what}{detail}")
            problems += 1

    # A configuration that isn't there: nothing is configured or built
    result = make("configs/jiffy_defconfg", f"{build}/keystore.o")
    check(
        "a mistyped KCONFIG_CONFIG stops the build",
        ok=result.returncode != 0 and not (build / ".config").exists(),
        detail=f": exit {result.returncode}, {build}/.config written",
    )
    for goal in ("clean", "menuconfig", "olddefconfig"):
        result = make(str(work / "none_defconfig"), "-n", goal)
        check(f"{goal} runs without one", ok=result.returncode == 0, detail=result.stderr)

    # olddefconfig gives a symbol that the configuration lacks its default (F-147)
    old = work / "old_defconfig"
    lines = (root / "configs/jiffy_defconfig").read_text().splitlines()
    old.write_text("".join(f"{x}\n" for x in lines if not x.startswith("CONFIG_STACK_SIZE_KiB=")))
    result = make(str(old), "olddefconfig")
    check(
        "olddefconfig fills in a missing symbol",
        ok=result.returncode == 0 and "CONFIG_STACK_SIZE_KiB=" in old.read_text(),
        detail=f": exit {result.returncode}{result.stdout}{result.stderr}",
    )

    # A build, then genconfig.py fails: the build doesn't go on with its .config
    result = make("configs/jiffy_defconfig", f"{build}/keystore.o")
    check("a build with the defconfig", ok=result.returncode == 0, detail=result.stderr)
    (board / "Kconfig").write_text('config BROKEN\n    bool "unterminated\n')
    result = make("configs/jiffy_defconfig", f"{build}/keystore.o")
    check(
        "a failed genconfig.py stops the build",
        ok=result.returncode != 0,
        detail=f": exit {result.returncode}",
    )
    return problems


if __name__ == "__main__":
    sys.exit(main())
