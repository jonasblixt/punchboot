"""The host with devices that are older than the log field (D-008).

Those devices ignore pb_command_log_read.log, read pb's log and answer with
a log of 0. The host must take that as "no such log", and stay in step with
the device. A scripted device in the same process answers like them.
"""

from __future__ import annotations

import ctypes

import punchboot
import pytest
from punchboot import wire
from punchboot.transport import Transport
from punchboot.wire import Command

LOG = b"PB 2.8.0\n"


def _result(response: wire.Struct | bytes = b"") -> bytes:
    data = response.pack() if isinstance(response, wire.Struct) else response
    result = wire.pb_result(magic=wire.PB_WIRE_MAGIC)
    ctypes.memmove(result.response, data, len(data))
    return result.pack()


class OldDevice(Transport):
    """Reads pb's log for any log, and answers other commands with OK."""

    def __init__(self) -> None:
        """Start with nothing to send."""
        self.queue: list[bytes] = []
        self.logs: list[int] = []

    def write(self, data: bytes) -> None:
        """Take a command."""
        frame = wire.pb_command.unpack(data)
        assert frame.magic == wire.PB_WIRE_MAGIC
        if frame.command != Command.PB_CMD_LOG_READ:
            self.queue.append(_result())
            return
        request = wire.pb_command_log_read.unpack(bytes(frame.request))
        self.logs.append(request.log)
        size = min(len(LOG) - min(request.position, len(LOG)), request.size)
        result = wire.pb_result_log_read(position=request.position, size=size, head=len(LOG))
        self.queue.append(_result(result))
        if size:
            self.queue += [LOG[request.position : request.position + size], _result()]

    def read(self, size: int) -> bytes:
        """Send what was queued."""
        data = self.queue.pop(0)
        assert len(data) == size
        return data


@pytest.mark.parametrize("log", ["rom", "list"])
def test_other_logs_are_not_supported(log: str) -> None:
    dev = OldDevice()
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.NotSupportedError):
        s.log_read(log)
    # The log the device sent was read, the next command is in step
    assert not dev.queue
    s.slc_set_configuration()
    assert dev.logs == [punchboot.pb_id(log)]


def test_pb_log_is_read() -> None:
    dev = OldDevice()
    s = punchboot.Session(transport=dev)

    assert s.log_read("pb") == LOG
    assert s.log_read(wire.PB_LOG_ID_PB) == LOG
    # "pb" goes as 0, like from released hosts
    assert dev.logs == [0, wire.PB_LOG_ID_PB]
