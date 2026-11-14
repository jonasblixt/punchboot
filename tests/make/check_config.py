#!/usr/bin/env python3
"""Check the firmware build's configuration (F-229, F-147) and its keys (D-083).

A KCONFIG_CONFIG that doesn't exist would build the Kconfig defaults, and a
failed scripts/genconfig.py the configuration of the build before. Both stop
the build. The goals that need no configuration (clean, menuconfig,
olddefconfig) run without one, and olddefconfig updates a configuration. The
keys are the board's makefile.mk's: all of the development keys or only the
board's, and a key named in part, a slot above 15 or a 2.x CONFIG_KEYSTORE
stops the build (board.md 7). jiffy is built as an out-of-tree board, a copy in
the work directory, so its Kconfig and makefile.mk can be changed:

  check_config.py <source dir> <work dir>

It needs the firmware's toolchain (arm-none-eabi-gcc, in the dev container) and
is skipped without it.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys
from collections.abc import Callable

SKIP = 77
CROSS_COMPILE = "arm-none-eabi-"


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")



Make = Callable[..., subprocess.CompletedProcess[str]]


def check_keys(
    root: pathlib.Path, work: pathlib.Path, board: pathlib.Path, make: Make
) -> list[tuple[str, bool, str]]:
    """Check the board's keys (board.md 7): each case, whether it passed and what went wrong."""
    results = []
    defconfig = "configs/jiffy_defconfig"
    keystore = work / "build" / "keystore.c"
    target = f"{work}/build/keystore.o"

    # The development keys, all three, and the build says so
    result = make(defconfig, target)
    results.append(
        (
            "a board without keys gets the development keys",
            result.returncode == 0
            and "development keys" in result.stdout
            and keystore.read_text().count("keystore_pb_key") == 3 * 2,
            f": {result.stdout}{result.stderr}",
        )
    )

    makefile = board / "makefile.mk"
    original = makefile.read_text()
    p384 = root / "pki/secp384r1-pub-key.der"
    rsa = root / "pki/test_rsa_public.der"
    for what, lines, error in (
        (
            "a board with a key gets only its own",
            f"VERIFY_KEY3_PATH = {p384}\nVERIFY_KEY3_ID = 0x1234\n",
            "",
        ),
        ("a key without an id stops the build", f"VERIFY_KEY3_PATH = {p384}\n", "VERIFY_KEY3_ID"),
        ("an id without a key stops the build", "VERIFY_KEY3_ID = k\n", "VERIFY_KEY3_PATH"),
        ("slot 16 stops the build", f"VERIFY_KEY16_PATH = {p384}\nVERIFY_KEY16_ID = k\n", "to 15"),
        ("an RSA key stops the build", f"VERIFY_KEY0_PATH = {rsa}\nVERIFY_KEY0_ID = k\n", "ECDSA"),
    ):
        makefile.write_text(original + lines)
        result = make(defconfig, target)
        if error:
            ok = result.returncode != 0 and error in result.stderr
        else:
            text = keystore.read_text()
            ok = (
                result.returncode == 0
                and "development keys" not in result.stdout
                and text.count("keystore_pb_key") == 1 * 2
                and ".id = 0x1234," in text
                and "    3,\n" in text
            )
        results.append((what, ok, f": exit {result.returncode}{result.stdout}{result.stderr}"))
    makefile.write_text(original)

    # A 2.x configuration's keystore stops the build, in the configuration or
    # on the command line: it would build with the development keys
    old = work / "keystore_defconfig"
    old.write_text((root / defconfig).read_text() + 'CONFIG_KEYSTORE="product.bpak"\n')
    for what, config, args in (
        ("CONFIG_KEYSTORE in the configuration stops the build", str(old), ()),
        (
            "CONFIG_KEYSTORE_SLOTS on the command line stops the build",
            defconfig,
            ("CONFIG_KEYSTORE_SLOTS=a90f9680",),
        ),
    ):
        result = make(config, *args, target)
        results.append(
            (
                what,
                result.returncode != 0 and "VERIFY_KEY" in result.stderr,
                f": exit {result.returncode}{result.stderr}",
            )
        )
    return results


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    missing = [t for t in ("make", f"{CROSS_COMPILE}gcc") if shutil.which(t) is None]
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

    for what, ok, detail in check_keys(root, work, board, make):
        check(what, ok=ok, detail=detail)

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
