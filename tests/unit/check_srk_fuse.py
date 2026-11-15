#!/usr/bin/env python3
"""Check scripts/check-srk-fuse.py (F-213).

  check_srk_fuse.py <source dir>

The test keys' fuse files are the hashes of their tables, under HABv4 and
AHAB; a fuse file of other keys, a changed table and a table of the other
scheme are refused; without a fuse file there is nothing to check. Every
defconfig that names a fuse file names it for the table its platform signs
with by default or by its own setting.
"""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile

HAB = "pki/imx6ul_hab_testkeys/SRK_1_2_3_4"
AHAB = "pki/imx8x_ahab/crts/SRK_1_2_3_4"

# The platform's scheme and its table option, with the default of the
# platform's Kconfig where it has one
PLATFORMS = {
    "CONFIG_PLAT_IMX6UL": ("hab", "CONFIG_IMX6UL_SRK_TABLE"),
    "CONFIG_PLAT_IMX8M_COMMON": ("hab", "CONFIG_IMX8M_SRK_TABLE"),
    "CONFIG_PLAT_IMX8X_COMMON": ("ahab", "CONFIG_IMX8X_SRK_TABLE"),
}
DEFAULT_TABLE = {"CONFIG_IMX8X_SRK_TABLE": f"{AHAB}_table.bin"}


def say(text: str) -> None:
    """Print a line of the report."""
    sys.stdout.write(text + "\n")


def run(root: pathlib.Path, *args: str) -> int:
    """Run the script from 'root' and return its exit code."""
    script = root / "scripts" / "check-srk-fuse.py"
    return subprocess.run(  # noqa: S603
        [sys.executable, str(script), *args],
        cwd=root,
        capture_output=True,
        check=False,
    ).returncode


def check_script(root: pathlib.Path) -> int:
    """Run the script on the test keys; return the number of problems."""
    problems = 0
    cases = [
        ("the HABv4 test keys", ["hab", f"{HAB}_table.bin", f"{HAB}_fuse.bin"], 0),
        ("the AHAB test keys", ["ahab", f"{AHAB}_table.bin", f"{AHAB}_fuse.bin"], 0),
        ("no fuse file", ["hab", f"{HAB}_table.bin"], 0),
        ("no fuse file, an empty argument", ["ahab", f"{AHAB}_table.bin", ""], 0),
        ("the AHAB fuse for the HABv4 table", ["hab", f"{HAB}_table.bin", f"{AHAB}_fuse.bin"], 1),
        ("the HABv4 fuse for the AHAB table", ["ahab", f"{AHAB}_table.bin", f"{HAB}_fuse.bin"], 1),
        ("an AHAB table as HABv4's", ["hab", f"{AHAB}_table.bin", f"{HAB}_fuse.bin"], 1),
        ("no scheme", [f"{HAB}_table.bin", f"{HAB}_fuse.bin"], 2),
    ]

    for name, args, expected in cases:
        rc = run(root, *args)
        if rc != expected:
            say(f"{name}: exit {rc}, expected {expected}")
            problems += 1

    # One byte of a key changed: the fuse no longer matches
    with tempfile.TemporaryDirectory() as tmp:
        for scheme, base in (("hab", HAB), ("ahab", AHAB)):
            table = bytearray((root / f"{base}_table.bin").read_bytes())
            table[-1] ^= 1
            changed = pathlib.Path(tmp) / f"{scheme}_table.bin"
            changed.write_bytes(bytes(table))
            rc = run(root, scheme, str(changed), f"{base}_fuse.bin")
            if rc != 1:
                say(f"a changed {scheme} table: exit {rc}, expected 1")
                problems += 1

    return problems


def config_values(defconfig: pathlib.Path) -> dict[str, str]:
    """Return the defconfig's string and bool settings."""
    values = {}

    for line in defconfig.read_text().splitlines():
        if line.startswith("CONFIG_") and "=" in line:
            key, value = line.split("=", 1)
            values[key] = value.strip('"')

    return values


def check_defconfigs(root: pathlib.Path) -> int:
    """Check each defconfig's fuse file against its table; return the number of problems."""
    problems = 0

    for defconfig in sorted((root / "configs").glob("*_defconfig")):
        values = config_values(defconfig)
        fuse = values.get("CONFIG_SRK_FUSE", "")

        if not fuse:
            continue

        for plat, (scheme, option) in PLATFORMS.items():
            if values.get(plat) != "y":
                continue

            table = values.get(option, DEFAULT_TABLE.get(option, ""))

            if not table:
                say(f"{defconfig.name}: a fuse file and no {option}")
                problems += 1
            elif run(root, scheme, table, fuse) != 0:
                say(f"{defconfig.name}: {fuse} isn't the hash of {table}")
                problems += 1

    return problems


def main() -> int:
    """Run the checks and return 1 when there is a problem."""
    root = pathlib.Path(sys.argv[1]).resolve()
    problems = check_script(root) + check_defconfigs(root)
    say(f"{problems} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
