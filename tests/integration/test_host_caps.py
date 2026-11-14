"""The host with devices that report 0 for capabilities (F-077, D-008).

Devices that predate a caps field report 0 for it, and the host must handle
that. These tests talk to a scripted device in the same process, through
Session's transport argument.
"""

from __future__ import annotations

import ctypes
import uuid
from typing import TYPE_CHECKING

import punchboot
import pytest
from punchboot import wire
from punchboot.transport import Transport
from punchboot.wire import Command

from .pbtest import REPO_ROOT, SYSTEM_A_UUID

if TYPE_CHECKING:
    import pathlib

GOLDEN_IMAGE = REPO_ROOT / "tests" / "compat" / "bpak" / "v0.9.3" / "two-parts.bpak"
STREAM_BUFFER_SIZE = 4096
PART_SIZE = 512 * 1024
MAX_WRITES = 10000


def _result(response: wire.Struct | bytes = b"") -> bytes:
    data = response.pack() if isinstance(response, wire.Struct) else response
    result = wire.pb_result(magic=wire.PB_WIRE_MAGIC)
    ctypes.memmove(result.response, data, len(data))
    return result.pack()


class OldDevice(Transport):
    """Answers OK to everything, and reports caps with only stream_buffer_size set.

    The host reads a result after each command and data transfer, so 'OK'
    is served whenever nothing else is queued.
    """

    def __init__(self, buffers: int = 0, chunk: int = 0) -> None:
        """Report 'buffers' stream buffers and 'chunk' as chunk_transfer_max_bytes."""
        self.caps = wire.pb_result_device_caps(
            stream_no_of_buffers=buffers,
            stream_buffer_size=STREAM_BUFFER_SIZE,
            chunk_transfer_max_bytes=chunk,
        )
        self.queue: list[bytes] = []
        self.commands: list[tuple[Command, bytes]] = []
        self.data: list[int] = []

    def write(self, data: bytes) -> None:
        """Take a command or a data transfer."""
        if len(self.commands) + len(self.data) > MAX_WRITES:
            msg = "The host keeps writing"
            raise RuntimeError(msg)
        frame = wire.pb_command.unpack(data)
        if len(data) != wire.pb_command.sizeof() or frame.magic != wire.PB_WIRE_MAGIC:
            self.data.append(len(data))
            return
        cmd = Command(frame.command)
        self.commands.append((cmd, bytes(frame.request)))
        if cmd == Command.PB_CMD_DEVICE_READ_CAPS:
            self.queue.append(_result(self.caps))
        elif cmd == Command.PB_CMD_PART_TBL_READ:
            entry = wire.pb_result_part_table_entry(
                uuid=(ctypes.c_uint8 * 16)(*SYSTEM_A_UUID.bytes),
                first_block=0,
                last_block=PART_SIZE // 512 - 1,
                block_size=512,
            )
            self.queue += [
                _result(wire.pb_result_part_table_read(no_of_entries=1)),
                entry.pack(),
            ]

    def read(self, size: int) -> bytes:
        """Serve what was queued, or OK."""
        data = self.queue.pop(0) if self.queue else _result()
        assert len(data) == size
        return data

    def buffer_ids(self) -> list[int]:
        """Return the buffer ids of the prepare buffer commands."""
        return [
            wire.pb_command_stream_prepare_buffer.unpack(request).id
            for cmd, request in self.commands
            if cmd == Command.PB_CMD_STREAM_PREPARE_BUFFER
        ]


def test_part_write_falls_back_to_the_stream_buffer_size(tmp_path: pathlib.Path) -> None:
    dev = OldDevice()
    data = tmp_path / "data"
    data.write_bytes(bytes(range(256)) * 40)  # 10 KiB, not a bpak file

    punchboot.Session(transport=dev).part_write(data, SYSTEM_A_UUID)

    assert dev.data == [4096, 4096, 2048]
    assert dev.buffer_ids() == [0, 0, 0]


def test_ram_boot_falls_back_to_the_stream_buffer_size() -> None:
    dev = OldDevice(buffers=2)

    punchboot.Session(transport=dev).boot_bpak(GOLDEN_IMAGE, uuid.UUID(int=0))

    payload = GOLDEN_IMAGE.stat().st_size - 4096
    assert dev.data[0] == 4096  # the header
    assert max(dev.data[1:]) == STREAM_BUFFER_SIZE
    assert sum(dev.data[1:]) == payload


def test_no_transfer_size_is_not_supported(tmp_path: pathlib.Path) -> None:
    dev = OldDevice()
    dev.caps.stream_buffer_size = 0
    data = tmp_path / "data"
    data.write_bytes(bytes(4096))

    with pytest.raises(punchboot.NotSupportedError):
        punchboot.Session(transport=dev).part_write(data, SYSTEM_A_UUID)
    with pytest.raises(punchboot.NotSupportedError):
        punchboot.Session(transport=dev).boot_bpak(GOLDEN_IMAGE, uuid.UUID(int=0))
    assert not dev.data
