"""The host's verdict on a partition write (command-mode.md 5.4).

After a complete write STREAM_FINALIZE runs the device's last step (on eMMC
it makes a boot partition bootable, F-082), so its result is the write's.
After a failed write hosts finalize too, and the write's error stays the
verdict. A scripted device answers, see scripted.py.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import punchboot
import pytest
from click.testing import CliRunner
from punchboot import __main__ as main
from punchboot.wire import Command, Result

from .pbtest import SYSTEM_A_UUID
from .scripted import ScriptedDevice, result

if TYPE_CHECKING:
    import pathlib

DATA = bytes(range(256)) * 40  # 10 KiB, not a bpak file


@pytest.fixture
def data(tmp_path: pathlib.Path) -> pathlib.Path:
    path = tmp_path / "data"
    path.write_bytes(DATA)
    return path


def test_finalize_after_a_complete_write_is_its_verdict(data: pathlib.Path) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_STREAM_FINALIZE, result(-Result.PB_RESULT_IO_ERROR))

    with pytest.raises(punchboot.IOError):
        punchboot.Session(transport=dev).part_write(data, SYSTEM_A_UUID)
    assert dev.sent()[-1] == Command.PB_CMD_STREAM_FINALIZE
    assert sum(dev.data) == len(DATA)


def test_finalize_after_a_failed_write_keeps_its_error(data: pathlib.Path) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_STREAM_WRITE_BUFFER, result(-Result.PB_RESULT_IO_ERROR))
    dev.on(Command.PB_CMD_STREAM_FINALIZE, result(-Result.PB_RESULT_ERROR))

    with pytest.raises(punchboot.IOError):
        punchboot.Session(transport=dev).part_write(data, SYSTEM_A_UUID)
    # Hosts finalize after a failure too
    assert dev.sent()[-2:] == [Command.PB_CMD_STREAM_WRITE_BUFFER, Command.PB_CMD_STREAM_FINALIZE]


def test_part_write_exits_1_when_finalize_fails(
    data: pathlib.Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_STREAM_FINALIZE, result(-Result.PB_RESULT_IO_ERROR))
    monkeypatch.setattr(main, "_open_session", lambda **_: punchboot.Session(transport=dev))

    # The socket transport, so that nothing looks for USB devices
    args = ["-t", "socket", "part", "write", str(data), str(SYSTEM_A_UUID)]
    run = CliRunner().invoke(main.cli, args)

    assert run.exit_code == 1
    assert isinstance(run.exception, SystemExit), run.exception
    assert "I/O error" in run.output
