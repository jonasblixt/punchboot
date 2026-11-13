"""The punchboot command line tool.

Ported from test_cli and test_reset. The other tests use the Python API,
these cover the CLI on top of it.
"""

from __future__ import annotations

import os
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


def punchboot_cli(
    device: Device, *args: str, stdin: str | None = None
) -> subprocess.CompletedProcess[str]:
    # stdin is /dev/null unless the test pipes something: 'command' reads a
    # stdin that isn't a terminal
    return subprocess.run(  # noqa: S603
        ["punchboot", "-t", "socket", "-s", str(device.socket), *args],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
        input=stdin,
        stdin=subprocess.DEVNULL if stdin is None else None,
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


def test_command_list(device: Device) -> None:
    # command-mode.md 5.6: the device's named commands, a name and a help
    # line each
    result = punchboot_cli(device, "command", "list")

    assert result.returncode == 0, result.stderr
    lines = result.stdout.splitlines()
    assert "test-command    Echoes its request" in lines
    assert "test-command2   Fails with -128" in lines
    assert any(line.startswith("test-boot-stop  Stops the next boot") for line in lines)


def test_command_runs_by_name(device: Device) -> None:
    result = punchboot_cli(device, "command", "test-command", "abc")

    assert result.returncode == 0, result.stderr
    assert result.stdout == "Hello test-command: abc\n"


def test_command_reads_a_piped_stdin(device: Device) -> None:
    # D-076: echo -n data | punchboot command name
    result = punchboot_cli(device, "command", "test-command", stdin="piped")

    assert result.returncode == 0, result.stderr
    assert result.stdout == "Hello test-command: piped\n"


def test_command_takes_args_before_stdin(device: Device) -> None:
    # A piped stdin is read only when there is no other request
    result = punchboot_cli(device, "command", "test-command", "abc", stdin="piped")

    assert result.returncode == 0, result.stderr
    assert result.stdout == "Hello test-command: abc\n"


def test_command_text_ends_without_its_nul(monkeypatch: pytest.MonkeyPatch) -> None:
    # jiffy's and imx8mevk's 'test' send their text with a newline and a NUL:
    # one line is printed, not a second one with the NUL
    dev = ScriptedDevice()
    response = b"Test command hello 0xf93ba110\n\0"
    size = wire.pb_result_board(size=len(response))
    dev.on(Command.PB_CMD_BOARD_COMMAND, result(), result(response=size), response)

    run = run_cli(monkeypatch, dev, "command", "test", "")

    assert run.exit_code == 0, run.output
    assert run.output == "Test command hello 0xf93ba110\n"


def test_command_request_from_a_file(device: Device, tmp_path: pathlib.Path) -> None:
    request = tmp_path / "request"
    request.write_bytes(b"from a file")

    result = punchboot_cli(device, "command", "test-command", "--args-from-file", str(request))

    assert result.returncode == 0, result.stderr
    assert result.stdout == "Hello test-command: from a file\n"


def test_command_with_two_requests_is_an_error(device: Device, tmp_path: pathlib.Path) -> None:
    request = tmp_path / "request"
    request.write_bytes(b"x")

    result = punchboot_cli(
        device, "command", "test-command", "abc", "--args-from-file", str(request)
    )

    assert result.returncode == 2
    assert "Use either ARGS or --args-from-file" in result.stderr


def test_command_request_too_large(device: Device) -> None:
    # The host checks the 1024 bytes before it sends anything
    result = punchboot_cli(device, "command", "test-command", stdin="x" * 1025)

    assert result.returncode == 2
    assert "The request is 1025 bytes, at most 1024" in result.stderr


def test_unknown_command(device: Device) -> None:
    result = punchboot_cli(device, "command", "test-unknown")

    assert result.returncode == 1
    assert result.stderr.strip() == "Error: Not supported"


def _complete(device: Device | None, socket: str, words: str) -> list[str]:
    """Return what bash would be offered for the last word of 'words'."""
    comp_words = f"punchboot -t socket -s {socket} {words}"
    env = {
        **os.environ,
        "_PUNCHBOOT_COMPLETE": "bash_complete",
        "COMP_WORDS": comp_words,
        "COMP_CWORD": str(len(comp_words.split()) - (0 if words.endswith(" ") else 1)),
    }
    del device
    result = subprocess.run(
        ["punchboot"],  # noqa: S607
        check=False,
        capture_output=True,
        text=True,
        env=env,
        stdin=subprocess.DEVNULL,
    )
    assert result.returncode == 0, result.stderr
    # bash_complete prints 'type,value' per item
    items = [line.split(",", 1) for line in result.stdout.splitlines()]
    return [value for kind, value in items if kind == "plain"]


def test_command_names_complete(device: Device) -> None:
    # <tab> after 'command': 'list' and the device's named commands
    socket = str(device.socket)

    assert set(_complete(device, socket, "command ")) == {
        "list",
        "test-command",
        "test-command2",
        "test-boot-stop",
    }
    assert set(_complete(device, socket, "command test-c")) == {"test-command", "test-command2"}
    assert _complete(device, socket, "command l") == ["list"]


def test_command_completion_without_a_device(tmp_path: pathlib.Path) -> None:
    # Nothing to ask: 'list' only, and no error
    assert _complete(None, str(tmp_path / "no.sock"), "command ") == ["list"]


def test_command_list_on_an_old_device(monkeypatch: pytest.MonkeyPatch) -> None:
    # A device before 3.0.0 doesn't have COMMAND_LIST
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_COMMAND_LIST, result(code=-wire.Result.PB_RESULT_INVALID_COMMAND))

    run = run_cli(monkeypatch, dev, "command", "list")

    assert run.exit_code == 1
    assert "can't list its commands" in run.output


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
