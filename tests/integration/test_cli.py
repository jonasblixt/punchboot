"""The punchboot command line tool.

Ported from test_cli and test_reset. The other tests use the Python API,
these cover the CLI on top of it.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

import pytest

from .pbtest import SYSTEM_A_UUID

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device


def punchboot_cli(device: Device, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(  # noqa: S603
        ["punchboot", "-t", "socket", "-s", str(device.socket), *args],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
    )


@pytest.mark.parametrize(
    "args", [[], ["boot"], ["dev"], ["part"]], ids=["-", "boot", "dev", "part"]
)
def test_group_without_command_prints_help(device: Device, args: list[str]) -> None:
    # click >= 8.2 treats a missing command as a usage error
    result = punchboot_cli(device, *args)

    assert result.returncode == 2
    assert "Usage:" in result.stdout + result.stderr


def test_dev_show(device: Device) -> None:
    result = punchboot_cli(device, "dev", "show")

    assert result.returncode == 0, result.stderr
    assert "host test" in result.stdout


def test_part_list(device: Device) -> None:
    result = punchboot_cli(device, "part", "list")

    assert result.returncode == 0, result.stderr
    assert str(SYSTEM_A_UUID) in result.stdout


def test_error_exit_code(device: Device) -> None:
    result = punchboot_cli(device, "board", "command", "test-unknown")

    assert result.returncode == 1


def test_not_supported_message(device: Device) -> None:
    # The host board has no password auth. NOT_SUPPORTED is also returned for
    # image hashes and key types, so the message doesn't blame the command.
    result = punchboot_cli(device, "auth", "password", "secret")

    assert result.returncode == 1
    assert result.stderr.strip() == "Error: Not supported"


def test_not_found_messages(device: Device, tmp_path: pathlib.Path) -> None:
    # NOT_FOUND from a command
    result = punchboot_cli(device, "boot", "partition", "00000000-0000-0000-0000-000000000001")

    assert result.returncode == 1
    assert result.stderr.strip() == "Error: Not found"

    # NOT_FOUND from the transport
    result = subprocess.run(  # noqa: S603
        ["punchboot", "-t", "socket", "-s", str(tmp_path / "no-such.sock"), "dev", "show"],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 1
    assert result.stderr.strip() == "Error: Could not connect to device"


def test_dev_reset(device: Device) -> None:
    result = punchboot_cli(device, "dev", "reset")

    assert result.returncode == 0, result.stderr
    assert device.wait_exit() == 0
