#!/usr/bin/env python3
"""Check that the firmware build makes again what a changed input makes.

keystore.c is made from the board's keys (VERIFY_KEY<n>) and
scripts/gen-keystore.py, and every object can use PB_VERSION (F-183). Every
object is compiled with the compiler flags, and pb is linked with the linker
flags and the link scripts (F-398); the banner's PB_BUILD_VERSION remakes only what
includes pb_build_version.h (F-231). A build directory that was built before
must follow a change of one, in the configuration or on the command line, or
pb keeps the old keys or the old flags. This builds two of jiffy's objects,
and then all of pb, in build directories of its own and changes each input:

  check_rebuild.py <source dir> <work dir>

It needs the firmware's toolchain (arm-none-eabi-gcc, in the dev container) and
is skipped without it.
"""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys

SKIP = 77
CROSS_COMPILE = "arm-none-eabi-"
BOARD = "src/board/jiffy"
DEFCONFIG = "configs/jiffy_defconfig"
# An mtime before any build: a copied input is older than what was made from
# the original, so only the variable that names it changed
OLD = 1_000_000_000


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


class Build:
    """jiffy's keystore.o and src/console.o in a build directory."""

    def __init__(self, root: pathlib.Path, work: pathlib.Path) -> None:
        """Start with an empty work directory."""
        self.root = root
        self.work = work
        self.build = work / "build"
        self.problems = 0
        shutil.rmtree(work, ignore_errors=True)
        work.mkdir(parents=True)
        # Not the jobserver or the variables of a make that runs the tests
        self.env = {
            k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")
        }
        self.version = (root / "version.txt").read_text().strip()

    def make(self, *args: str, question: bool = False, targets: list[str] | None = None) -> int:
        """Run make for the two objects with the default arguments and args."""
        names = targets or ["keystore.o", "src/console.o"]
        targets = [f"{self.build}/{t}" for t in names]
        cmd = [
            "make",
            *(["-q"] if question else []),
            f"BOARD={BOARD}",
            f"KCONFIG_CONFIG={DEFCONFIG}",
            f"CROSS_COMPILE={CROSS_COMPILE}",
            f"BUILD_DIR={self.build}",
            *args,
            *targets,
        ]
        result = subprocess.run(  # noqa: S603
            cmd, cwd=self.root, env=self.env, capture_output=True, text=True, check=False
        )
        # make -q: 0 when there is nothing to make, 1 when there is
        if result.returncode not in (0, 1) or (result.returncode and not question):
            say(f"'{' '.join(cmd)}' failed:\n{result.stdout}{result.stderr}")
            self.problems += 1
        return result.returncode

    def check(self, what: str, *, keys: int = 3, version: str | None = None) -> None:
        """Check what the two objects were made from: the defaults unless given."""
        version = version or self.version
        keystore = (self.build / "keystore.c").read_text()
        console = (self.build / "src/console.o").read_bytes()
        found = (keystore.count(".slot = "), f"PB {version}".encode() in console)
        expected = (keys, True)
        if found == expected:
            say(f"ok: {what}")
        else:
            say(f"FAILED: {what}: {found}, expected {expected}")
            self.problems += 1

    def check_up_to_date(self, what: str, *args: str, targets: list[str] | None = None) -> None:
        """Check that a build with the default arguments and args has nothing to make."""
        if self.make(*args, question=True, targets=targets) == 0:
            say(f"ok: {what}")
        else:
            say(f"FAILED: {what}: make has something to make")
            self.problems += 1

    def check_out_of_date(self, what: str, *args: str, targets: list[str] | None = None) -> None:
        """Check that a build with the default arguments and args has something to make."""
        if self.make(*args, question=True, targets=targets) == 1:
            say(f"ok: {what}")
        else:
            say(f"FAILED: {what}: make has nothing to make")
            self.problems += 1

    def read(self, name: str) -> bytes:
        """Return a file of the build directory."""
        return (self.build / name).read_bytes()

    def check_made(self, what: str, made: dict[str, bool], before: dict[str, bytes]) -> None:
        """Check which files changed from before: made[name] says whether it should."""
        found = {name: self.read(name) != before[name] for name in made}
        if found == made:
            say(f"ok: {what}")
        else:
            say(f"FAILED: {what}: changed {found}, expected {made}")
            self.problems += 1


def check_flags(root: pathlib.Path, work: pathlib.Path) -> int:
    """Build all of pb, change the flags and the link's inputs (F-398)."""
    b = Build(root, work)
    pb = ["pb"]

    b.make(targets=pb)
    first = {name: b.read(name) for name in ("src/console.o", "src/cm/cm_device.o", "pb")}
    b.check_up_to_date("pb: a build without a change has nothing to make", targets=pb)

    # A compiler flag: every object and the link
    b.make("CONFIG_OPTIMIZE=0", targets=pb)
    b.check_made("CONFIG_OPTIMIZE on the command line", {"src/console.o": True, "pb": True}, first)
    b.check_up_to_date("and then nothing to make", "CONFIG_OPTIMIZE=0", targets=pb)

    b.make(targets=pb)
    b.check_made("back to the defconfig's", {"src/console.o": False, "pb": False}, first)

    # The banner's version: the objects that include pb_build_version.h, not
    # the others (F-231)
    b.make("PB_BUILD_VERSION=1.2.3-dev+test", targets=pb)
    b.check_made(
        "PB_BUILD_VERSION on the command line: the banner only",
        {"src/console.o": True, "src/cm/cm_device.o": False, "pb": True},
        first,
    )
    b.check_up_to_date("and then nothing to make", "PB_BUILD_VERSION=1.2.3-dev+test", targets=pb)

    b.make(targets=pb)
    b.check_made(
        "back to the build's",
        {"src/console.o": False, "src/cm/cm_device.o": False, "pb": False},
        first,
    )

    # A linker flag: the link only
    b.make("CONFIG_STACK_SIZE_KiB=4", targets=pb)
    b.check_made("a linker flag", {"src/console.o": False, "pb": True}, first)

    b.make(targets=pb)
    b.check_made("back to the defconfig's", {"src/console.o": False, "pb": False}, first)

    # The link's inputs (-W: as if changed)
    for name in ("src/link.lds", "src/arch/armv7a/link.lds", "src/plat/imx6ul/link.lds"):
        b.check_out_of_date(f"{name} changed: pb is linked again", "-W", name, targets=pb)
        b.check_up_to_date(
            f"{name} changed: no object is made again",
            "-W",
            name,
            targets=["src/console.o", "keystore.o"],
        )
    b.check_up_to_date("and then nothing to make", targets=pb)

    # A blob in a directory of its own, as the i.MX8M boards' DDR firmware:
    # once it is linked, nothing to make (F-431)
    blob = work / "fw" / "blob.bin"
    blob.parent.mkdir()
    blob.write_bytes(bytes(64))
    b.make(f"blobs-y={blob}", targets=pb)
    b.check_up_to_date("a blob: then nothing to make", f"blobs-y={blob}", targets=pb)

    return b.problems


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    missing = [t for t in ("make", f"{CROSS_COMPILE}gcc") if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    b = Build(root, work)

    # Two of the development keys on the command line, the first a copy
    key0 = work / "key0.der"
    shutil.copy(root / "pki/secp256r1-pub-key.der", key0)
    os.utime(key0, (OLD, OLD))
    two_keys = (
        f"VERIFY_KEY0_PATH={key0}",
        "VERIFY_KEY0_ID=pb-development",
        "VERIFY_KEY1_PATH=pki/secp384r1-pub-key.der",
        "VERIFY_KEY1_ID=pb-development2",
    )

    b.make()
    b.check("the first build")
    b.check_up_to_date("a build without a change has nothing to make")

    # Each change follows a build with the defaults: the configuration is the
    # same, so config.h, which most objects depend on, is too
    b.make("PB_VERSION=9.9.9")
    b.check("PB_VERSION on the command line", version="9.9.9")

    b.make()
    b.check("back to version.txt")

    b.make(*two_keys)
    b.check("two keys on the command line", keys=2)
    b.check_up_to_date("and then nothing to make", *two_keys)

    # A key file that changes, at the same path
    key0.write_bytes((root / "pki/secp521r1-pub-key.der").read_bytes())
    b.check_out_of_date("a key's file changed: keystore.o is made again", *two_keys)
    b.make(*two_keys)
    b.check_up_to_date("and then nothing to make", *two_keys)

    b.make()
    b.check("back to the development keys")
    b.check_out_of_date(
        "scripts/gen-keystore.py changed: keystore.o is made again",
        "-W",
        "scripts/gen-keystore.py",
        targets=["keystore.o"],
    )

    b.check_up_to_date("and then nothing to make")

    return b.problems + check_flags(root, work / "flags")


if __name__ == "__main__":
    sys.exit(main())
