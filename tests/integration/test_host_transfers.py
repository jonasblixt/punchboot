"""A failed transfer ends the session; the caps' timeout (command-mode.md 4.4, 4.6).

After a transfer that failed or timed out, or an exchange the host leaves
before its last transfer, the host and the device no longer agree on where a
frame starts. The host then sends nothing more on that transport, so it
can't read a late result as the next command's (F-226). The caps carry pb's
timeout for a transfer, and a device that reports 0 leaves the host its own.
A scripted device answers, see scripted.py; the USB device is a fake.
"""

from __future__ import annotations

from typing import TYPE_CHECKING, Any

import punchboot
import pytest
from punchboot import transport, wire
from punchboot.session import BOARD_RESPONSE_SIZE, BOARD_STATUS_SIZE
from punchboot.wire import Command, Result

from .pbtest import SYSTEM_A_UUID
from .scripted import ScriptedDevice, caps, part_table, result

if TYPE_CHECKING:
    import pathlib
    from collections.abc import Callable

DATA = bytes(range(256)) * 40  # 10 KiB, not a bpak file


@pytest.fixture
def data(tmp_path: pathlib.Path) -> pathlib.Path:
    path = tmp_path / "data"
    path.write_bytes(DATA)
    return path


def _ended(s: punchboot.Session, dev: ScriptedDevice) -> None:
    """Check that the session ended: the transport is closed, and a call raises."""
    sent = len(dev.commands)
    assert dev.closed
    with pytest.raises(OSError, match="invalidated"):
        s.device_get_uuid()
    assert len(dev.commands) == sent


@pytest.mark.parametrize(
    "error", [punchboot.TimeoutError(), punchboot.TransferError()], ids=["timeout", "failed"]
)
def test_a_failed_transfer_ends_the_session(error: punchboot.Error) -> None:
    dev = ScriptedDevice()
    # The result comes after the host gave up on it
    dev.on(Command.PB_CMD_DEVICE_IDENTIFIER_READ, error, result())
    s = punchboot.Session(transport=dev)

    with pytest.raises(type(error)):
        s.device_get_uuid()
    _ended(s, dev)
    assert dev.queue == [result()]


def test_a_failed_write_sends_nothing_more(data: pathlib.Path) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_STREAM_WRITE_BUFFER, punchboot.TimeoutError())
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.TimeoutError):
        s.part_write(data, SYSTEM_A_UUID)
    # Not even the finalize
    assert dev.sent()[-1] == Command.PB_CMD_STREAM_WRITE_BUFFER
    _ended(s, dev)


def _part_table(dev: ScriptedDevice) -> Callable[[punchboot.Session], object]:
    dev.on(Command.PB_CMD_PART_TBL_READ, *part_table(entries=129))
    return lambda s: s.part_get_partitions()


def _board_command(dev: ScriptedDevice) -> Callable[[punchboot.Session], object]:
    size = BOARD_RESPONSE_SIZE + 1
    dev.on(
        Command.PB_CMD_BOARD_COMMAND,
        result(),
        result(response=wire.pb_result_board(size=size)),
        bytes(size),
    )
    return lambda s: s.board_run_command("test")


def _board_status(dev: ScriptedDevice) -> Callable[[punchboot.Session], object]:
    size = BOARD_STATUS_SIZE + 1
    dev.on(
        Command.PB_CMD_BOARD_STATUS_READ,
        result(response=wire.pb_result_board_status(size=size)),
        bytes(size),
    )
    return lambda s: s.board_read_status()


def _log(dev: ScriptedDevice) -> Callable[[punchboot.Session], object]:
    dev.on(
        Command.PB_CMD_LOG_READ,
        result(response=wire.pb_result_log_read(size=32, head=32)),
        bytes(32),
    )
    return lambda s: s.log_read_chunk(0, 16)


def _magic(dev: ScriptedDevice) -> Callable[[punchboot.Session], object]:
    dev.on(Command.PB_CMD_DEVICE_IDENTIFIER_READ, result(magic=0))
    return lambda s: s.device_get_uuid()


@pytest.mark.parametrize(
    "exchange",
    [_part_table, _board_command, _board_status, _log, _magic],
    ids=["part-table", "board-command", "board-status", "log", "magic"],
)
def test_an_exchange_left_before_its_end_ends_the_session(
    exchange: Callable[[ScriptedDevice], Callable[[punchboot.Session], object]],
) -> None:
    dev = ScriptedDevice()
    call = exchange(dev)
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.Error):
        call(s)
    _ended(s, dev)


def test_an_error_in_step_keeps_the_session() -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_BOOT_STATUS, result(-Result.PB_RESULT_NOT_SUPPORTED))
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.NotSupportedError):
        s.boot_status()
    s.device_get_uuid()
    assert not dev.closed


@pytest.mark.parametrize(
    ("operation_timeout_ms", "timeouts"), [(30000, [30000]), (0, [])], ids=["pb", "released"]
)
def test_transfers_take_the_caps_timeout(
    data: pathlib.Path, operation_timeout_ms: int, timeouts: list[int]
) -> None:
    dev = ScriptedDevice()
    dev.on(Command.PB_CMD_DEVICE_READ_CAPS, caps(operation_timeout_ms))

    punchboot.Session(transport=dev).part_write(data, SYSTEM_A_UUID)

    assert dev.timeouts == timeouts


class FakeUsbDevice:
    """Takes bulk transfers and records their timeouts."""

    def __init__(self) -> None:
        """Start with no transfers."""
        self.timeouts: list[int] = []

    def is_kernel_driver_active(self, _interface: int) -> bool:
        """No kernel driver has the interface."""
        return False

    def write(self, _endpoint: int, data: bytes, timeout: int) -> int:
        """Take a transfer."""
        self.timeouts.append(timeout)
        return len(data)

    def read(self, _endpoint: int, size: int, timeout: int) -> bytes:
        """Send a transfer of zeros."""
        self.timeouts.append(timeout)
        return bytes(size)


def test_usb_transfers_take_the_timeout(monkeypatch: pytest.MonkeyPatch) -> None:
    usb_util: Any = pytest.importorskip("usb.util")
    for name in ("claim_interface", "release_interface", "dispose_resources"):
        monkeypatch.setattr(usb_util, name, lambda *_: None)
    dev = FakeUsbDevice()
    usb = transport.UsbTransport(dev=dev)

    usb.write(b"x")
    usb.read(1)
    usb.set_timeout(30000)
    usb.write(b"x")
    usb.read(1)
    usb.close()

    assert dev.timeouts == [transport.USB_TIMEOUT_MS] * 2 + [30000] * 2
