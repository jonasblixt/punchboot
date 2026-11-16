"""The wire protocol's packing and unpacking in punchboot.wire, without a device.

The values and the layout are checked against tests/compat/abi.txt by
tests/integration/test_compat.py. These tests are about what wire.py does
with them: a command frame, a struct to bytes and back, a C string.
"""

from __future__ import annotations

import struct

import pytest
from punchboot import wire


def test_command_frame() -> None:
    frame = wire.command(wire.Command.PB_CMD_DEVICE_READ_CAPS)

    assert len(frame) == 512
    assert struct.unpack_from("<IB3x", frame) == (wire.PB_WIRE_MAGIC, 3)
    assert frame[8:] == bytes(wire.PB_COMMAND_REQUEST_MAX_SIZE)


def test_command_request_from_bytes_and_struct() -> None:
    frame = wire.command(wire.Command.PB_CMD_PART_ERASE, b"\x01\x02")
    assert frame[8:10] == b"\x01\x02"
    assert frame[10:] == bytes(502)

    req = wire.pb_command_erase_part(start_lba=0x11223344, block_count=5)
    req.uuid[:] = list(range(16))
    frame = wire.command(wire.Command.PB_CMD_PART_ERASE, req)
    assert frame[4] == wire.Command.PB_CMD_PART_ERASE
    assert frame[8:24] == bytes(range(16))
    assert struct.unpack_from("<II", frame, 24) == (0x11223344, 5)


def test_command_request_size() -> None:
    frame = wire.command(wire.Command.PB_CMD_BOARD_COMMAND, b"\xff" * 504)
    assert frame[8:] == b"\xff" * 504

    with pytest.raises(ValueError, match="Request too large"):
        wire.command(wire.Command.PB_CMD_BOARD_COMMAND, b"\xff" * 505)


def test_structs_are_packed_little_endian() -> None:
    caps = wire.pb_result_device_caps(
        stream_no_of_buffers=2,
        stream_buffer_size=0x01020304,
        operation_timeout_ms=0x0506,
        part_erase_timeout_ms=0x0708,
        bpak_stream_support=1,
        chunk_transfer_max_bytes=0x0A0B0C0D,
    )
    data = caps.pack()

    assert len(data) == wire.pb_result_device_caps.sizeof() == 32
    assert data[:14] == bytes.fromhex("02 04030201 0605 0807 01 0d0c0b0a")
    assert data[14:] == bytes(18)


def test_unpack_round_trip() -> None:
    entry = wire.pb_result_part_table_entry(
        description=b"System A", first_block=34, last_block=0x1_0000_0000, block_size=512, flags=5
    )
    back = wire.pb_result_part_table_entry.unpack(entry.pack() + b"trailing")

    assert back.description == b"System A"
    assert back.first_block == 34
    assert back.last_block == 0x1_0000_0000
    assert back.block_size == 512
    assert back.flags == 5


def test_unpack_takes_the_start() -> None:
    log = wire.pb_result_log_read(position=1, size=2, head=3, log=wire.PB_LOG_ID_PB)
    data = log.pack() + bytes(wire.PB_RESULT_RESPONSE_MAX_SIZE)
    assert wire.pb_result_log_read.unpack(data).log == wire.PB_LOG_ID_PB


def test_unpack_short_data_is_an_error() -> None:
    with pytest.raises(ValueError, match="too small"):
        wire.pb_result_slc.unpack(bytes(31))


def test_result_code_is_signed() -> None:
    data = struct.pack("<Ib3x", wire.PB_WIRE_MAGIC, -wire.Result.PB_RESULT_NOT_FOUND)
    result = wire.pb_result.unpack(data + bytes(wire.PB_RESULT_RESPONSE_MAX_SIZE))

    assert result.magic == wire.PB_WIRE_MAGIC
    assert result.result_code == -11


def test_c_string() -> None:
    assert wire.c_string(b"board\0junk\0") == "board"
    assert wire.c_string(b"no terminator") == "no terminator"
    assert wire.c_string(b"") == ""
    # Not UTF-8: replaced, not an exception
    assert wire.c_string(b"\xffx\0") == "�x"


def test_every_struct_is_listed_once() -> None:
    names = [s.__name__ for s in wire.STRUCTS]
    assert len(names) == len(set(names))
    assert all(issubclass(s, wire.Struct) for s in wire.STRUCTS)


def test_commands_only_grow() -> None:
    # Appended, never renumbered (D-008): the values are 0..n without a gap
    assert [c.value for c in wire.Command] == list(range(len(wire.Command)))
    assert [r.value for r in wire.Result] == list(range(len(wire.Result)))
