"""The pbstate tool's command line, on the A/B state of pb-host's disk.

Like from Linux on a target (pbtest.Device.pbstate). Scripts in the field
call the tool: an invocation that worked keeps working, with its exit code.
One that silently did the wrong thing fails now, and writes nothing (F-221).
Products link its library, libpbstate.so.0, next to others (F-222).
"""

from __future__ import annotations

import ctypes
import ctypes.util
import errno
import os
import pathlib
import re
import shutil
import struct
import subprocess
from typing import TYPE_CHECKING

import pytest

from .pbtest import BACKUP_STATE_LBA, PRIMARY_STATE_LBA, REPO_ROOT

if TYPE_CHECKING:
    from .pbtest import Device

# struct pb_boot_state: magic, enable, verified, attempts, error (u32 each)
STATE_A = 1
STATE_B = 2


def _state(device: Device) -> tuple[int, int, int, int]:
    """Enable, verified, remaining attempts and error of the primary state."""
    raw = device.read_blocks(PRIMARY_STATE_LBA)
    enable, verified, attempts, error = struct.unpack_from("<4I", raw, 4)
    return enable, verified, attempts, error


def _copies(device: Device) -> tuple[bytes, bytes]:
    """Read the primary and the backup state, as they are on the disk."""
    return device.read_blocks(PRIMARY_STATE_LBA), device.read_blocks(BACKUP_STATE_LBA)


@pytest.mark.parametrize(
    ("system", "enable"),
    [("a", STATE_A), ("A", STATE_A), ("b", STATE_B), ("B", STATE_B), ("none", 0), ("NONE", 0)],
)
def test_switch_takes_a_b_or_none(device: Device, system: str, enable: int) -> None:
    device.pbstate("-s", "b" if enable == STATE_A else "a")

    assert device.pbstate_run("-s", system).returncode == 0
    assert _state(device)[0] == enable


@pytest.mark.parametrize("system", ["x", "c", "0", "", "off"])
def test_switch_refuses_what_is_no_system(device: Device, system: str) -> None:
    # It disabled both systems, and exited 0
    device.pbstate("-s", "a")
    copies = _copies(device)

    result = device.pbstate_run("-s", system)
    assert result.returncode == errno.EINVAL
    assert "-s takes a, b or none" in result.stderr
    assert _copies(device) == copies


@pytest.mark.parametrize("system", ["x", "none", "", "0"])
def test_verified_refuses_what_is_no_system(device: Device, system: str) -> None:
    # F-408: it exited 1, with "operation failed (-1)"
    device.pbstate("-s", "a", "-c", "3")
    copies = _copies(device)

    result = device.pbstate_run("-v", system)
    assert result.returncode == errno.EINVAL
    assert "-v takes a or b" in result.stderr
    assert _copies(device) == copies


@pytest.mark.parametrize(("system", "verified"), [("a", STATE_A), ("B", STATE_B)])
def test_verified_marks_a_system(device: Device, system: str, verified: int) -> None:
    device.pbstate("-s", "a", "-c", "3")

    assert device.pbstate_run("-v", system).returncode == 0
    assert _state(device)[1] == verified


@pytest.mark.parametrize("count", ["1", "255", "256", "300", "0x100", "4294967295"])
def test_count_is_the_trial_s_attempts(device: Device, count: str) -> None:
    # A trial of that many attempts, A not verified. It was a byte: 256 was
    # 0, which means verified, and 300 was 44
    device.pbstate("-s", "a", "-c", count)
    assert _state(device) == (STATE_A, 0, int(count, 0), 0)


@pytest.mark.parametrize(
    "args", [("-c", "3"), ("-c", "0"), ("-c", "3", "-i"), ("-v", "a", "-c", "3")]
)
def test_count_without_switch_is_refused(device: Device, args: tuple[str, ...]) -> None:
    # F-408: the count is the switch's (-s), alone it was ignored
    device.pbstate("-s", "a", "-c", "5")
    copies = _copies(device)

    result = device.pbstate_run(*args)
    assert result.returncode == errno.EINVAL
    assert "-c goes with -s" in result.stderr
    assert result.stdout == ""
    assert _copies(device) == copies


@pytest.mark.parametrize(
    "args",
    [
        ("-i", "-s", "b"),
        ("-s", "b", "-i"),
        ("-i", "-v", "a"),
        ("-i", "-w", "0", "7"),
        ("-s", "b", "-v", "a"),
        ("-v", "a", "-s", "b"),
        ("-s", "b", "-c", "3", "-v", "a"),
        ("-s", "b", "-w", "0", "7"),
        ("-v", "a", "-w", "0", "7"),
        ("--info", "--switch", "none"),
    ],
)
def test_two_commands_are_refused(device: Device, args: tuple[str, ...]) -> None:
    # F-421: -i was done and the others ignored, -s beat -v and -w, -v beat -w
    device.pbstate("-s", "a", "-c", "5")
    copies = _copies(device)

    result = device.pbstate_run(*args)
    assert result.returncode == errno.EINVAL
    assert "one command at a time" in result.stderr
    assert result.stdout == ""
    assert _copies(device) == copies


@pytest.mark.parametrize("count", ["-1", "4294967296", "x"])
def test_count_outside_32_bits_is_refused(device: Device, count: str) -> None:
    copies = _copies(device)

    result = device.pbstate_run("-s", "a", "-c", count)
    assert result.returncode == (errno.EINVAL if count == "x" else errno.ERANGE)
    assert _copies(device) == copies


@pytest.mark.parametrize("value", ["zzz", "", "x1"])
def test_board_register_value_must_be_a_number(device: Device, value: str) -> None:
    # It wrote 0
    device.pbstate("-w", "0", "5")
    copies = _copies(device)

    result = device.pbstate_run("-w", "0", value)
    assert result.returncode == errno.EINVAL
    assert _copies(device) == copies
    assert "0: 00000005" in device.pbstate("-i")


@pytest.mark.parametrize(
    "args",
    [
        ("-s", "a", "-c", "3x"),
        ("-s", "a", "-c", "0x1g"),
        ("-w", "0", "1zz"),
        ("-w", "0x", "1"),
        ("-w", "1z", "5"),
    ],
)
def test_a_number_with_characters_after_it_is_refused(
    device: Device, args: tuple[str, ...]
) -> None:
    # F-408: what followed the number was ignored. '-c 3x' was a trial of 3
    # attempts, '-w 0 1zz' wrote 1
    device.pbstate("-w", "0", "5")
    device.pbstate("-s", "b")
    copies = _copies(device)

    result = device.pbstate_run(*args)
    assert result.returncode == errno.EINVAL
    assert _copies(device) == copies


def test_a_number_may_have_white_space_around_it(device: Device) -> None:
    device.pbstate("-s", "a", "-c", " 3 ")
    device.pbstate("-w", " 1\t", "\n0x12 ")
    assert _state(device) == (STATE_A, 0, 3, 0)
    assert "1: 00000012" in device.pbstate("-i")


def test_board_register_takes_32_bits(device: Device) -> None:
    # strtol() stopped at 0x7fffffff where a long is 32 bits
    device.pbstate("-w", "0", "0xffffffff")
    device.pbstate("-w", "1", "0x80000000")
    info = device.pbstate("-i")
    assert "0: FFFFFFFF" in info
    assert "1: 80000000" in info


def test_info_prints_the_state(device: Device) -> None:
    # What scripts parse: the same, line for line, as long as the state can
    # be read
    device.pbstate("-s", "a")
    device.pbstate("-s", "b", "-c", "3")
    device.pbstate("-w", "0", "5")
    device.pbstate("-w", "3", "0xcafe")

    assert device.pbstate("-i") == (
        "Punchboot status:\n"
        "\n"
        "System A is disabled and verified\n"
        "System B is enabled and not verified\n"
        "Errors : 0x00000000\n"
        "Remaining boot attempts: 3\n"
        "\n"
        "Board registers:\n"
        "    0: 00000005\n"
        "    1: 00000000\n"
        "    2: 00000000\n"
        "    3: 0000CAFE\n"
    )


def test_info_fails_without_printing_a_state_it_could_not_read(device: Device) -> None:
    # F-408: with both copies bad, the systems were printed as disabled and
    # not verified before the error
    device.write_blocks(PRIMARY_STATE_LBA, bytes(512))
    device.write_blocks(BACKUP_STATE_LBA, bytes(512))

    result = device.pbstate_run("-i")
    assert result.returncode == errno.EIO
    assert "System A" not in result.stdout
    assert "Punchboot status" not in result.stdout
    assert "Error: operation failed (-5)" in result.stdout


def test_the_library_keeps_its_crc32_next_to_zlib(device: Device) -> None:
    # zlib's crc32() has the library's name for its own and other results.
    # The library exported its own: with zlib loaded first it got zlib's, and
    # every state failed its CRC check (and a program that loaded the library
    # first got the library's for zlib's)
    zlib = ctypes.util.find_library("z")
    assert zlib is not None
    device.pbstate("-s", "a")

    result = device.pbstate_run("-i", env={**os.environ, "LD_PRELOAD": zlib})
    assert "preload" not in result.stderr
    assert result.returncode == 0
    assert "CRC failed" not in result.stdout
    assert "System A is enabled and verified" in result.stdout


def test_the_library_exports_its_header() -> None:
    """The library's ABI is the functions of its public header, and only those."""
    header = (REPO_ROOT / "tools" / "pbstate" / "src" / "pbstate.h").read_text()
    functions = re.findall(r"^int (pbstate_\w+)\(", header, re.MULTILINE)
    assert len(functions) == 13

    lib = ctypes.CDLL("libpbstate.so.0")
    for function in functions:
        assert hasattr(lib, function), function
    assert not hasattr(lib, "crc32")


def test_the_build_s_default_partitions(device: Device, tmp_path: pathlib.Path) -> None:
    """A build with PB_STATE_PRIMARY_PART and PB_STATE_BACKUP_PART runs without -p and -b.

    Each is its copy's: the backup's was the primary's (F-010), so the tool
    wrote the primary twice and never the backup. The install installs each
    file once.
    """
    cmake = shutil.which("cmake")
    assert cmake is not None
    primary = tmp_path / "primary"
    backup = tmp_path / "backup"
    primary.write_bytes(device.read_blocks(PRIMARY_STATE_LBA))
    backup.write_bytes(device.read_blocks(BACKUP_STATE_LBA))
    state = primary.read_bytes()
    build = tmp_path / "build"
    for args in (
        [
            "-S",
            str(REPO_ROOT / "tools" / "pbstate"),
            "-B",
            str(build),
            f"-DCMAKE_INSTALL_PREFIX={tmp_path / 'prefix'}",
            f"-DPB_STATE_PRIMARY_PART={primary}",
            f"-DPB_STATE_BACKUP_PART={backup}",
        ],
        ["--build", str(build)],
        ["--install", str(build)],
    ):
        subprocess.run([cmake, *args], check=True, capture_output=True)  # noqa: S603

    installed = (build / "install_manifest.txt").read_text().split()
    assert len(installed) == len(set(installed))
    tool = next(path for path in installed if path.endswith("/bin/pbstate"))
    lib = next(path for path in installed if path.endswith("/libpbstate.so.0"))

    subprocess.run(  # noqa: S603
        [tool, "-w", "0", "0x12345678"],
        check=True,
        capture_output=True,
        env={**os.environ, "LD_LIBRARY_PATH": str(pathlib.Path(lib).parent)},
    )
    assert primary.read_bytes() != state
    assert backup.read_bytes() == primary.read_bytes()
