#!/usr/bin/env python3
"""Check the image stages as a board out of the tree gets them.

What `all` makes after pb.bin is the list image-y, and FINAL_OUTPUT names the
last file: both are set while the makefiles are read, the board's before the
platform's (platform.md 6.2, F-240). This builds a board that is not in the
tree, a directory in the work directory with jiffy's board.c and a makefile.mk
of its own, and checks what its build makes:

  check_stages.py <source dir> <work dir>

The cases, each in a build directory of its own:

- Level 3 of platform.md 6.2: the platform's stages off in the defconfig, and
  the board's own stage from pb.bin in image-y, with its FINAL_OUTPUT.
- A board that still fills plat-y, the list's name before 3.0.0, is refused:
  it would build without its image.

It needs the firmware's tools (arm-none-eabi-gcc and the bpak CLI, both in the
dev container) and is skipped without them.
"""

from __future__ import annotations

import concurrent.futures
import os
import pathlib
import shutil
import subprocess
import sys

SKIP = 77
CROSS_COMPILE = "arm-none-eabi-"
TOOLS = ("make", f"{CROSS_COMPILE}gcc", "bpak")
# The board in the tree whose sources and defconfig the board is made from
FROM = "jiffy"
# The board's data: its sources, and nothing of the in-tree board's makefile.mk
BOARD_FILES = ("board.c", "partitions.h", "imximage.cfg")

# The platform's stages, as the options of its Kconfig turn them on
CREATE = "CONFIG_IMX6UL_CREATE_IMX_IMAGE"
SIGN = "CONFIG_IMX6UL_SIGN_IMAGE"
SIGN_INPUTS = (
    "CONFIG_IMX6UL_SRK_TABLE",
    "CONFIG_IMX6UL_KEY_INDEX",
    "CONFIG_IMX6UL_SIGN_CERT",
    "CONFIG_IMX6UL_IMAGE_SIGN_CERT",
)

# A board's makefile.mk: its sources, and nothing else
SOURCES = "src-y += $(BOARD)/board.c\n"

# Level 3: the board's own tooling from pb.bin, here a header before it
OWN_IMAGE = """
image-y += oot_image
FINAL_OUTPUT = $(BUILD_DIR)/oot.img

oot_image: $(BUILD_DIR)/pb.bin
\t$(Q)(printf 'OOT %s\\n' $(CONFIG_PB_ENTRY); cat $<) > $(BUILD_DIR)/oot.img
"""

# The list's name before 3.0.0
OLD_LIST = """
plat-y += oot_image

oot_image: $(BUILD_DIR)/pb.bin
\t$(Q)cp $< $(BUILD_DIR)/oot.img
"""


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


class Case:
    """A board out of the tree, its defconfig and its build directory.

    A check returns its line of the report, which starts with "ok" or "FAILED".
    """

    def __init__(self, root: pathlib.Path, work: pathlib.Path, name: str) -> None:
        """Make the board's directory with the in-tree board's sources."""
        self.root = root
        self.name = name
        self.board = work / name / "board"
        self.build = work / name / "build"
        self.config = work / name / "defconfig"
        self.board.mkdir(parents=True)
        for file in BOARD_FILES:
            shutil.copy(root / f"src/board/{FROM}/{file}", self.board / file)
        # Not the jobserver or the variables of a make that runs the tests
        self.env = {
            k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")
        }
        self.result: subprocess.CompletedProcess[str] | None = None

    def defconfig(self, *, off: tuple[str, ...] = ()) -> None:
        """Write the in-tree board's defconfig with the options turned off."""
        lines = []
        for line in (self.root / f"configs/{FROM}_defconfig").read_text().splitlines():
            key = line.split("=", 1)[0]
            lines.append(f"# {key} is not set" if key in off else line)
        self.config.write_text("\n".join(lines) + "\n")

    def make(self, makefile: str, *args: str) -> subprocess.CompletedProcess[str]:
        """Build `all` with the board's makefile.mk and the arguments."""
        (self.board / "makefile.mk").write_text(makefile)
        cmd = [
            "make",
            f"-j{os.cpu_count() or 1}",
            f"BOARD={self.board}",
            f"KCONFIG_CONFIG={self.config}",
            f"CROSS_COMPILE={CROSS_COMPILE}",
            f"BUILD_DIR={self.build}",
            *args,
            "all",
        ]
        self.result = subprocess.run(  # noqa: S603
            cmd, cwd=self.root, env=self.env, capture_output=True, text=True, check=False
        )
        return self.result

    def read(self, name: str) -> bytes:
        """Return a file of the build directory."""
        return (self.build / name).read_bytes()

    def made(self) -> set[str]:
        """Return the images in the build directory."""
        names = {path.name for path in self.build.iterdir() if path.is_file()}
        return {name for name in names if name.endswith((".imx", ".img", ".csf", "_csf.bin"))}

    def line(self, what: str, problems: list[str]) -> str:
        """Return the case's line of the report."""
        if not problems:
            return f"ok: {self.name}: {what}"
        output = f"{self.result.stdout[-2000:]}{self.result.stderr[-2000:]}" if self.result else ""
        return f"FAILED: {self.name}: {what}: {'; '.join(problems)}\n{output}"

    def final_output(self, name: str) -> list[str]:
        """Check what `all` named as the build's output."""
        said = f"Success, final output: {self.build / name}"
        assert self.result is not None
        return [] if said in self.result.stdout else [f"all didn't say '{said}'"]


def check_own_tooling(root: pathlib.Path, work: pathlib.Path) -> str:
    """Level 3: the platform's stages off, the board's own stage from pb.bin."""
    case = Case(root, work, "level3")
    case.defconfig(off=(CREATE, SIGN, *SIGN_INPUTS))
    result = case.make(SOURCES + OWN_IMAGE)
    problems = []
    if result.returncode != 0:
        return case.line("the board's own stage", [f"make: exit {result.returncode}"])
    if case.made() != {"oot.img"}:
        problems.append(f"the build made {sorted(case.made())}, expected oot.img only")
    elif case.read("oot.img") != b"OOT 0x80001000\n" + case.read("pb.bin"):
        problems.append("oot.img is not the board's header and pb.bin")
    problems += case.final_output("oot.img")
    return case.line("the platform's stages off, the board's own from pb.bin, is all's", problems)


def check_old_list(root: pathlib.Path, work: pathlib.Path) -> str:
    """Check that a board that fills plat-y is refused, and told the list's name."""
    case = Case(root, work, "plat-y")
    case.defconfig(off=(CREATE, SIGN, *SIGN_INPUTS))
    result = case.make(SOURCES + OLD_LIST)
    problems = []
    if result.returncode == 0:
        problems.append("make: exit 0")
    if "plat-y (oot_image) is image-y since 3.0.0" not in result.stderr:
        problems.append("make didn't say that plat-y is image-y")
    return case.line("a board that fills plat-y is refused", problems)


CHECKS = (check_own_tooling, check_old_list)


def main() -> int:
    """Run the cases and return the number of problems."""
    root = pathlib.Path(sys.argv[1]).resolve()
    work = pathlib.Path(sys.argv[2]).resolve()

    missing = [t for t in TOOLS if shutil.which(t) is None]
    if missing:
        say(f"skipped: no {', '.join(missing)}")
        return SKIP

    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)

    # The cases at once, each with a board and a build directory of its own
    with concurrent.futures.ThreadPoolExecutor() as pool:
        lines = list(pool.map(lambda check: check(root, work), CHECKS))

    for line in lines:
        say(line)
    return sum(not line.startswith("ok: ") for line in lines)


if __name__ == "__main__":
    sys.exit(main())
