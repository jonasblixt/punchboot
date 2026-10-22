"""Stream commands with requests the punchboot package never sends (F-053, F-054).

The punchboot package always sends valid buffer ids, sizes and offsets, so
these tests use the raw wire protocol (pbtest.Wire).
"""

from __future__ import annotations

import struct
from typing import TYPE_CHECKING

import pytest

from .pbtest import (
    BLOCK_SIZE,
    PB_CMD_BOOTLOADER_VERSION_READ,
    PB_CMD_STREAM_INITIALIZE,
    PB_CMD_STREAM_PREPARE_BUFFER,
    PB_CMD_STREAM_READ_BUFFER,
    PB_CMD_STREAM_WRITE_BUFFER,
    READABLE_UUID,
    Wire,
)

if TYPE_CHECKING:
    import uuid
    from collections.abc import Iterator

    from .pbtest import Device

# The 'Readable' partition of the default table is readable and writable
READABLE_LBA = 2085
READABLE_BLOCKS = 2048

BUFFER_SIZE = 4096  # CONFIG_CM_BUF_SIZE_KiB in configs/host_defconfig
N_BUFFERS = 2

PB_RESULT_INVALID_ARGUMENT = 5
PB_RESULT_NO_MEMORY = 9


def stream_init(wire: Wire, part: uuid.UUID) -> int:
    return wire.command(PB_CMD_STREAM_INITIALIZE, part.bytes)


def stream_prepare(wire: Wire, buffer_id: int, data: bytes) -> int:
    wire.send_command(PB_CMD_STREAM_PREPARE_BUFFER, struct.pack("<IB", len(data), buffer_id))
    rc = wire.result()
    if rc != 0:
        return rc
    wire.send(data)
    return wire.result()


def stream_write(wire: Wire, buffer_id: int, offset: int, size: int) -> int:
    return wire.command(PB_CMD_STREAM_WRITE_BUFFER, struct.pack("<IQB", size, offset, buffer_id))


def stream_read(wire: Wire, buffer_id: int, offset: int, size: int) -> tuple[int, bytes]:
    wire.send_command(PB_CMD_STREAM_READ_BUFFER, struct.pack("<IQB", size, offset, buffer_id))
    rc = wire.result()
    if rc != 0:
        return rc, b""
    data = wire.recv(size)
    return wire.result(), data


def assert_responsive(wire: Wire) -> None:
    """Check that the device is alive and the protocol is still in sync."""
    assert wire.command(PB_CMD_BOOTLOADER_VERSION_READ) == 0


@pytest.fixture
def wire(device: Device) -> Iterator[Wire]:
    with device.wire() as w:
        assert stream_init(w, READABLE_UUID) == 0
        yield w


def test_stream_roundtrip(device: Device, wire: Wire) -> None:
    """Sanity check of the raw client: write a buffer and read it back."""
    data = bytes(range(256)) * (BUFFER_SIZE // 256)
    assert stream_prepare(wire, 0, data) == 0
    assert stream_write(wire, 0, 8 * BLOCK_SIZE, len(data)) == 0

    assert stream_read(wire, 1, 8 * BLOCK_SIZE, len(data)) == (0, data)
    assert device.read_blocks(READABLE_LBA + 8, len(data) // BLOCK_SIZE) == data


@pytest.mark.parametrize("buffer_id", [N_BUFFERS, 255])
def test_stream_write_rejects_bad_buffer_id(wire: Wire, buffer_id: int) -> None:
    assert stream_write(wire, buffer_id, 0, BLOCK_SIZE) == -PB_RESULT_NO_MEMORY
    assert_responsive(wire)


@pytest.mark.parametrize("buffer_id", [N_BUFFERS, 255])
def test_stream_read_rejects_bad_buffer_id(wire: Wire, buffer_id: int) -> None:
    assert stream_read(wire, buffer_id, 0, BLOCK_SIZE)[0] == -PB_RESULT_NO_MEMORY
    assert_responsive(wire)


@pytest.mark.parametrize("buffer_id", [0, 1])
def test_stream_write_rejects_oversized_transfer(wire: Wire, buffer_id: int) -> None:
    assert stream_write(wire, buffer_id, 0, BUFFER_SIZE + BLOCK_SIZE) == -PB_RESULT_NO_MEMORY
    assert_responsive(wire)


@pytest.mark.parametrize("buffer_id", [0, 1])
def test_stream_read_rejects_oversized_transfer(wire: Wire, buffer_id: int) -> None:
    assert stream_read(wire, buffer_id, 0, 2 * BUFFER_SIZE)[0] == -PB_RESULT_NO_MEMORY
    assert_responsive(wire)


def test_stream_write_offset_cannot_wrap_to_start_of_disk(device: Device, wire: Wire) -> None:
    """An offset whose block number wraps around to disk LBA 0."""
    mbr = device.read_blocks(0)
    offset = ((1 << 32) - READABLE_LBA) * BLOCK_SIZE

    assert stream_prepare(wire, 0, b"\xa5" * BLOCK_SIZE) == 0
    assert stream_write(wire, 0, offset, BLOCK_SIZE) == -PB_RESULT_INVALID_ARGUMENT
    assert_responsive(wire)
    assert device.read_blocks(0) == mbr


def test_stream_read_offset_cannot_wrap_to_start_of_disk(wire: Wire) -> None:
    """An offset whose block number wraps around to the GPT header at LBA 1."""
    offset = ((1 << 32) - READABLE_LBA + 1) * BLOCK_SIZE

    assert stream_read(wire, 0, offset, BLOCK_SIZE)[0] == -PB_RESULT_INVALID_ARGUMENT
    assert_responsive(wire)


def test_stream_write_rejects_offset_beyond_32_bit_lba(device: Device, wire: Wire) -> None:
    """The block number must not be truncated to a smaller, valid one."""
    before = device.read_blocks(READABLE_LBA + 8)
    offset = ((1 << 32) + 8) * BLOCK_SIZE

    assert stream_prepare(wire, 0, b"\xa5" * BLOCK_SIZE) == 0
    assert stream_write(wire, 0, offset, BLOCK_SIZE) == -PB_RESULT_INVALID_ARGUMENT
    assert_responsive(wire)
    assert device.read_blocks(READABLE_LBA + 8) == before


def test_stream_write_past_end_of_partition_fails(device: Device, wire: Wire) -> None:
    after = device.read_blocks(READABLE_LBA + READABLE_BLOCKS)

    assert stream_prepare(wire, 0, b"\xa5" * (2 * BLOCK_SIZE)) == 0
    # The last block is fine, one more is not
    assert stream_write(wire, 0, (READABLE_BLOCKS - 1) * BLOCK_SIZE, BLOCK_SIZE) == 0
    assert (
        stream_write(wire, 0, (READABLE_BLOCKS - 1) * BLOCK_SIZE, 2 * BLOCK_SIZE)
        == -PB_RESULT_INVALID_ARGUMENT
    )
    assert_responsive(wire)
    assert device.read_blocks(READABLE_LBA + READABLE_BLOCKS) == after
