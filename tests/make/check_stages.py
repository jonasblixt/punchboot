#!/usr/bin/env python3
"""Check the image stages as a board out of the tree gets them.

What `all` makes after pb.bin is the list image-y, and FINAL_OUTPUT names the
last file: both are set while the makefiles are read, the board's before the
platform's (platform.md 6.2, F-240). This builds a board that is not in the
tree, a directory in the work directory with jiffy's board.c and a makefile.mk
of its own, and checks what its build makes:

  check_stages.py <source dir> <work dir>

The cases, each in a build directory of its own, at the levels at which a
board overrides the platform's stages (platform.md 6.2):

- Level 0: the board's makefile.mk has its sources and memory, the defconfig turns the
  i.MX6UL's stages on: pb.imx, the two descriptions and the signed images.
- Level 1: its own signing tool (CST_TOOL in its makefile.mk), which gets
  the descriptions and writes the signatures.
- Level 2: signing off in the defconfig, its own stage after imx6ul_image in
  image-y, with its FINAL_OUTPUT.
- Level 3: the platform's stages off in the defconfig, and the board's own
  stage from pb.bin in image-y, with its FINAL_OUTPUT.
- A board that still fills plat-y, the list's name before 3.0.0, is refused:
  it would build without its image.
- Each board in the tree names the last file of its stages as the final
  output (make -n).
- No recipe in the Makefile or a makefile.mk has an $(eval): what a recipe
  works out is the shell's, and FINAL_OUTPUT is set when the makefiles are
  read.

It needs the firmware's tools (arm-none-eabi-gcc, U-Boot's mkimage and NXP's
cst, all in the dev container) and is skipped without them.
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
TOOLS = ("make", f"{CROSS_COMPILE}gcc", "mkimage", "cst")
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

# A board's makefile.mk: its sources and its memory (jiffy's), and nothing else
SOURCES = """src-y += $(BOARD)/board.c
PB_ENTRY = 0x80001000
PB_SIZE = 0x100000
BOARD_DRAM_BASE = 0x80000000
BOARD_DRAM_SIZE = 0x40000000
PB_BUFFERS_BASE = 0x81021000
PB_BUFFERS_SIZE = 0xfdf000
"""

# Level 1: the board's own signing tool, called as cst is
OWN_TOOL = """
CST_TOOL ?= $(BOARD)/sign
"""

# The tool: it signs with a line before the description
SIGN_TOOL = """#!/bin/sh
# sign --o <signature> --i <description>
(echo "SIGNED"; cat "$4") > "$2"
"""

# Level 2: signing off, the board's own stage after the platform's pb.imx
OWN_SIGNING = """
image-y += oot_sign
FINAL_OUTPUT = $(BUILD_DIR)/oot_signed.imx

oot_sign: imx6ul_image
\t$(Q)(echo SIGNED; cat $(BUILD_DIR)/pb.imx) > $(BUILD_DIR)/oot_signed.imx
"""

# Level 3: the board's own tooling from pb.bin, here a header before it
OWN_IMAGE = """
image-y += oot_image
FINAL_OUTPUT = $(BUILD_DIR)/oot.img

oot_image: $(BUILD_DIR)/pb.bin
\t$(Q)(printf 'OOT %s\\n' $(PB_ENTRY); cat $<) > $(BUILD_DIR)/oot.img
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
        return {
            name
            for name in names
            if name.endswith((".imx", ".img", ".csf")) or name.startswith("pb_csf")
        }

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


# What the i.MX6UL's stages make with signing on
SIGNED = {
    "pb.imx",
    "pb.csf",
    "pb_uuu.csf",
    "pb_csf.bin",
    "pb_csf_uuu.bin",
    "pb_signed.imx",
    "pb_signed_uuu.imx",
}


def check_signed(case: Case) -> list[str]:
    """Check the i.MX6UL's descriptions and signed images against pb.imx.

    pb.csf signs pb.imx from the IVT at PB_ENTRY - 0xC00 to its end,
    pb_uuu.csf also the DCD at the serial downloader's 0x00910000, as long as
    its header says. pb_signed.imx is two blocks of zeros, pb.imx and its
    signature, padded to a block; pb_signed_uuu.imx pb.imx and its signature.
    """
    problems = []
    imx = case.read("pb.imx")
    path = case.build / "pb.imx"
    blocks = f'Blocks = 0x80000400 0x000 0x{len(imx):X} "{path}"'
    uuu = f'Blocks = 0x00910000 0x02c 0x{imx[45:47].hex()} "{path}"'
    csf = case.read("pb.csf").decode()
    csf_uuu = case.read("pb_uuu.csf").decode()
    if blocks not in csf.splitlines():
        problems.append(f"pb.csf hasn't '{blocks}'")
    if blocks not in csf_uuu.splitlines() or uuu not in csf_uuu.splitlines():
        problems.append(f"pb_uuu.csf hasn't '{blocks}' and '{uuu}'")
    keys = (
        "pki/imx6ul_hab_testkeys/SRK_1_2_3_4_table.bin",
        "pki/imx6ul_hab_testkeys/CSF1_1_sha256_4096_65537_v3_usr_crt.pem",
        "pki/imx6ul_hab_testkeys/IMG1_1_sha256_4096_65537_v3_usr_crt.pem",
    )
    for text, name in ((csf, "pb.csf"), (csf_uuu, "pb_uuu.csf")):
        missing = [key for key in keys if f'"{key}"' not in text]
        if missing or "__" in text:
            problems.append(f"{name}: the keys {missing} or a placeholder left")
    signed = bytes(1024) + imx + case.read("pb_csf.bin")
    signed += bytes(-len(signed) % 512)
    if case.read("pb_signed.imx") != signed:
        problems.append("pb_signed.imx is not the zeros, pb.imx and pb_csf.bin, padded")
    if case.read("pb_signed_uuu.imx") != imx + case.read("pb_csf_uuu.bin"):
        problems.append("pb_signed_uuu.imx is not pb.imx and pb_csf_uuu.bin")
    return problems


def check_platform(root: pathlib.Path, work: pathlib.Path) -> str:
    """Level 0: the platform's stages, the board gives its sources."""
    case = Case(root, work, "level0")
    case.defconfig()
    result = case.make(SOURCES)
    if result.returncode != 0:
        return case.line("the platform's stages", [f"make: exit {result.returncode}"])
    problems = []
    if case.made() != SIGNED:
        problems.append(f"the build made {sorted(case.made())}, expected {sorted(SIGNED)}")
    else:
        problems += check_signed(case)
        # A HABv4 CSF starts with its header's tag, 0xd4
        if not case.read("pb_csf.bin").startswith(b"\xd4"):
            problems.append("pb_csf.bin is not a CSF (cst's)")
    problems += case.final_output("pb_signed.imx")
    return case.line("the platform's stages make pb.imx and sign it", problems)


def check_own_signing_tool(root: pathlib.Path, work: pathlib.Path) -> str:
    """Level 1: the board's own signing tool, the platform's descriptions."""
    case = Case(root, work, "level1")
    tool = case.board / "sign"
    tool.write_text(SIGN_TOOL)
    tool.chmod(0o755)
    case.defconfig()
    result = case.make(SOURCES + OWN_TOOL)
    if result.returncode != 0:
        return case.line("the board's signing tool", [f"make: exit {result.returncode}"])
    problems = []
    if case.made() != SIGNED:
        problems.append(f"the build made {sorted(case.made())}, expected {sorted(SIGNED)}")
    else:
        problems += check_signed(case)
        for csf, signature in (("pb.csf", "pb_csf.bin"), ("pb_uuu.csf", "pb_csf_uuu.bin")):
            if case.read(signature) != b"SIGNED\n" + case.read(csf):
                problems.append(f"{signature} is not the tool's signature of {csf}")
    problems += case.final_output("pb_signed.imx")
    return case.line("the board's signing tool signs the platform's descriptions", problems)


def check_own_signing(root: pathlib.Path, work: pathlib.Path) -> str:
    """Level 2: signing off, the board's own stage after the platform's image."""
    case = Case(root, work, "level2")
    case.defconfig(off=(SIGN, *SIGN_INPUTS))
    result = case.make(SOURCES + OWN_SIGNING)
    if result.returncode != 0:
        return case.line("the board's signing", [f"make: exit {result.returncode}"])
    problems = []
    if case.made() != {"pb.imx", "oot_signed.imx"}:
        problems.append(f"the build made {sorted(case.made())}, expected oot_signed.imx")
    elif case.read("oot_signed.imx") != b"SIGNED\n" + case.read("pb.imx"):
        problems.append("oot_signed.imx is not the board's signature and pb.imx")
    problems += case.final_output("oot_signed.imx")
    return case.line("signing off, the board's own stage after imx6ul_image", problems)


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


# The boards in the tree and the last file their defconfig's stages make
IN_TREE = {
    "jiffy": "pb_signed.imx",
    "imxrt1060evk": "pb_flash.bin",
    "imx8qxmek": "pb_signed.imx",
    "imx8dxlevk": "pb.imx",
    "imx8mevk": "pb_signed.imx",
    "imx8mpevk": "pb.imx",
}


def check_in_tree(root: pathlib.Path, work: pathlib.Path) -> str:
    """Check what `all` names as the final output of each board in the tree.

    make -n: nothing is built, so neither the vendors' tools and firmware nor
    the aarch64 compiler are needed. The DDR firmware is left out (blobs-y).
    """
    env = {k: v for k, v in os.environ.items() if k not in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL")}
    problems = []
    for board, name in IN_TREE.items():
        build = work / "in-tree" / board
        cmd = [
            "make",
            "-n",
            f"BOARD=src/board/{board}",
            f"KCONFIG_CONFIG=configs/{board}_defconfig",
            f"BUILD_DIR={build}",
            "blobs-y=",
            "all",
        ]
        result = subprocess.run(  # noqa: S603
            cmd, cwd=root, env=env, capture_output=True, text=True, check=False
        )
        said = f"Success, final output: {build / name}"
        if result.returncode != 0:
            problems.append(f"{board}: make -n: exit {result.returncode}: {result.stderr[-500:]}")
        elif said not in result.stdout:
            problems.append(f"{board}: all didn't say '{said}'")
    if problems:
        return f"FAILED: in-tree: the boards' final output: {'; '.join(problems)}"
    return "ok: in-tree: every board's final output is the last file of its stages"


def check_no_eval(root: pathlib.Path, work: pathlib.Path) -> str:  # noqa: ARG001
    """Check that no recipe of the Makefile or a makefile.mk has an $(eval)."""
    found = []
    for path in [root / "Makefile", *sorted(root.glob("src/**/makefile.mk"))]:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if line.startswith("\t") and "$(eval" in line:
                found.append(f"{path.relative_to(root)}:{number}")
    if found:
        return f"FAILED: no-eval: $(eval) in a recipe: {', '.join(found)}"
    return "ok: no-eval: no recipe has an $(eval)"


CHECKS = (
    check_platform,
    check_own_signing_tool,
    check_own_signing,
    check_own_tooling,
    check_old_list,
    check_in_tree,
    check_no_eval,
)


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
