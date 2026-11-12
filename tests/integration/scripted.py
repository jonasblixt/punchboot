"""A scripted device for the host's own tests, in the same process.

ScriptedDevice is a Transport: a Session talks to it through its transport
argument. It answers each command from a script, by default with OK, and
records what the host sent. The host reads a result after each command and
after each data transfer, so OK is served whenever nothing else is queued.
An exception in the queue is raised by the read that would have got it, like
a transfer that fails.
"""

from __future__ import annotations

import ctypes

from punchboot import wire
from punchboot.transport import Transport
from punchboot.wire import Command

from .pbtest import SYSTEM_A_UUID

PART_SIZE = 512 * 1024
CHUNK = 4096


def result(
    code: int = 0, response: wire.Struct | bytes = b"", magic: int = wire.PB_WIRE_MAGIC
) -> bytes:
    """Return a pb_result frame."""
    data = response.pack() if isinstance(response, wire.Struct) else response
    frame = wire.pb_result(magic=magic, result_code=code)
    ctypes.memmove(frame.response, data, len(data))
    return frame.pack()


def caps(operation_timeout_ms: int = 0) -> bytes:
    """Return a caps result: two buffers of CHUNK bytes."""
    return result(
        response=wire.pb_result_device_caps(
            stream_no_of_buffers=2,
            stream_buffer_size=CHUNK,
            chunk_transfer_max_bytes=CHUNK,
            operation_timeout_ms=operation_timeout_ms,
        )
    )


def part_table(flags: int = 0, entries: int = 1) -> list[bytes]:
    """Return a partition table's result and its entries: System A, 'entries' times."""
    entry = wire.pb_result_part_table_entry(
        uuid=(ctypes.c_uint8 * 16)(*SYSTEM_A_UUID.bytes),
        description=b"System A",
        first_block=0,
        last_block=PART_SIZE // 512 - 1,
        block_size=512,
        flags=flags,
    )
    return [
        result(response=wire.pb_result_part_table_read(no_of_entries=entries)),
        entry.pack() * entries,
    ]


Frame = bytes | Exception


class ScriptedDevice(Transport):
    """Answers commands from a script and records what the host sends."""

    def __init__(self) -> None:
        """Answer the caps and the partition table, and everything else with OK."""
        self.script: dict[Command, list[Frame]] = {
            Command.PB_CMD_DEVICE_READ_CAPS: [caps()],
            Command.PB_CMD_PART_TBL_READ: [*part_table()],
        }
        self.queue: list[Frame] = []
        # The commands the host sent, with their requests, and the sizes of
        # its data transfers
        self.commands: list[tuple[Command, bytes]] = []
        self.data: list[int] = []
        self.closed = False

    def on(self, cmd: Command, *frames: Frame) -> None:
        """Answer 'cmd' with 'frames', then with OK for each further read."""
        self.script[cmd] = list(frames)

    def sent(self) -> list[Command]:
        """Return the commands the host sent."""
        return [cmd for cmd, _ in self.commands]

    def write(self, data: bytes) -> None:
        """Take a command or a data transfer."""
        assert not self.closed, "the host sent on a closed transport"
        frame = wire.pb_command.unpack(data)
        if len(data) != wire.pb_command.sizeof() or frame.magic != wire.PB_WIRE_MAGIC:
            self.data.append(len(data))
            return
        cmd = Command(frame.command)
        self.commands.append((cmd, bytes(frame.request)))
        self.queue += self.script.get(cmd, [])

    def read(self, size: int) -> bytes:
        """Serve what was queued, or OK."""
        assert not self.closed, "the host read from a closed transport"
        frame = self.queue.pop(0) if self.queue else result()
        if isinstance(frame, Exception):
            raise frame
        assert len(frame) == size
        return frame

    def close(self) -> None:
        """Close the connection."""
        self.closed = True
