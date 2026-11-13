"""The punchboot command line tool.

Ported from test_cli and test_reset. The other tests use the Python API,
these cover the CLI on top of it.
"""

from __future__ import annotations

import subprocess
from typing import TYPE_CHECKING

import punchboot
import pytest
from punchboot import wire
from punchboot.wire import Command

from .pbtest import SYSTEM_A_UUID
from .scripted import ScriptedDevice, part_table, result, run_cli

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


def test_an_error_says_what_went_wrong(device: Device, tmp_path: pathlib.Path) -> None:
    # The exception's message follows the fixed text (F-225)
    data = tmp_path / "data"
    data.write_bytes(bytes(8192))

    result = punchboot_cli(device, "boot", "bpak", str(data))

    assert result.returncode == 1
    assert result.stderr.strip() == "Error: Generic error: Invalid BPAK header"


def test_a_missing_file_is_an_error(device: Device, tmp_path: pathlib.Path) -> None:
    result = punchboot_cli(device, "part", "write", str(tmp_path / "missing"), str(SYSTEM_A_UUID))

    assert result.returncode == 1
    assert result.stderr.startswith("Error: [Errno 2] No such file or directory"), result.stderr


# What a newer device may send. The CLI runs in this process on a scripted
# device: a traceback is an exception that isn't a SystemExit.


def test_unknown_partition_flags_are_kept(monkeypatch: pytest.MonkeyPatch) -> None:
    dev = ScriptedDevice()
    # Bootable, writable and the two bits no release defines
    dev.on(Command.PB_CMD_PART_TBL_READ, *part_table(flags=0x95))

    run = run_cli(monkeypatch, dev, "part", "list")

    assert run.exit_code == 0, run.output
    assert "System A" in run.output
    (part,) = punchboot.Session(transport=dev).part_get_partitions()
    assert part.partition_flags.value == 0x95
    assert part.bootable
    assert part.writable
    assert not part.readable


def test_an_unknown_life_cycle_is_an_error(monkeypatch: pytest.MonkeyPatch) -> None:
    dev = ScriptedDevice()
    keys = bytes(wire.pb_result_slc_key_status.sizeof())
    dev.on(Command.PB_CMD_SLC_READ, result(response=wire.pb_result_slc(slc=5)), keys)

    run = run_cli(monkeypatch, dev, "slc", "show")

    assert run.exit_code == 1
    assert isinstance(run.exception, SystemExit), run.exception
    assert "Error: 5" in run.output


def test_a_version_that_isnt_semantic_is_an_error(monkeypatch: pytest.MonkeyPatch) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOTLOADER_VERSION_READ, result(response=b"2.8"))

    run = run_cli(monkeypatch, dev, "dev", "show")

    assert run.exit_code == 1
    assert isinstance(run.exception, SystemExit), run.exception
    assert "Error: 2.8" in run.output


def test_a_binary_board_response_is_printed(monkeypatch: pytest.MonkeyPatch) -> None:
    dev = ScriptedDevice()
    response = b"\xff\xfeok"
    size = wire.pb_result_board(size=len(response))
    dev.on(Command.PB_CMD_BOARD_COMMAND, result(), result(response=size), response)

    run = run_cli(monkeypatch, dev, "board", "command", "test-command")

    assert run.exit_code == 0, run.output
    assert "ok" in run.output


def test_boot_enable_needs_a_partition(device: Device) -> None:
    # Without one it sent the null UUID, which disables boot (F-224)
    with device.session() as s:
        s.boot_set_boot_part(SYSTEM_A_UUID)

    result = punchboot_cli(device, "boot", "enable")

    assert result.returncode == 2
    assert "Missing argument" in result.stderr
    with device.session() as s:
        assert s.boot_status()[0] == SYSTEM_A_UUID


def test_dev_reset(device: Device) -> None:
    result = punchboot_cli(device, "dev", "reset")

    assert result.returncode == 0, result.stderr
    assert device.wait_exit() == 0
