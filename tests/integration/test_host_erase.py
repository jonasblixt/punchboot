"""The host's side of a partition erase (command-mode.md 5.3).

PART_ERASE carries the first block in 32 bits. The host erases in chunks of
64 blocks from the partition's first block, and a partition whose chunks
start past LBA 2^32 - 1 is refused before an erase is sent: the block went
masked, and an old device erased a block near the start of the disk
(F-401). A scripted device answers, see scripted.py.
"""

from __future__ import annotations

import ctypes
import struct

import punchboot
import pytest
from punchboot import wire
from punchboot.wire import Command

from .pbtest import SYSTEM_A_UUID
from .scripted import ScriptedDevice, result

LBA_MAX = 0xFFFFFFFF


def device_with(first_block: int, last_block: int) -> ScriptedDevice:
    """Return a device whose one partition, System A, has these blocks."""
    dev = ScriptedDevice()
    entry = wire.pb_result_part_table_entry(
        uuid=(ctypes.c_uint8 * 16)(*SYSTEM_A_UUID.bytes),
        description=b"System A",
        first_block=first_block,
        last_block=last_block,
        block_size=512,
        flags=0,
    )
    dev.on(
        Command.PB_CMD_PART_TBL_READ,
        result(response=wire.pb_result_part_table_read(no_of_entries=1)),
        entry.pack(),
    )
    return dev


def erases(dev: ScriptedDevice) -> list[tuple[int, int]]:
    """Return the first block and the count of each erase the host sent."""
    return [
        struct.unpack_from("<II", request, 16)
        for cmd, request in dev.commands
        if cmd == Command.PB_CMD_PART_ERASE
    ]


@pytest.mark.parametrize(
    ("first", "last"),
    [
        (LBA_MAX + 1, LBA_MAX + 64),
        (LBA_MAX - 9, LBA_MAX + 100),
        (LBA_MAX - 63, LBA_MAX + 1),  # the second chunk starts at 2^32
    ],
)
def test_an_erase_the_command_cant_carry_is_refused(first: int, last: int) -> None:
    dev = device_with(first, last)
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.ArgumentError):
        s.part_erase(SYSTEM_A_UUID)
    assert not erases(dev)


def test_an_erase_whose_chunks_start_below_2_32_is_sent() -> None:
    # The last chunk starts at 2^32 - 6 and its blocks go past 2^32: the
    # command carries its first block and its count, as it always has
    dev = device_with(LBA_MAX - 69, LBA_MAX + 6)
    s = punchboot.Session(transport=dev)

    s.part_erase(SYSTEM_A_UUID)
    assert erases(dev) == [(LBA_MAX - 69, 64), (LBA_MAX - 5, 12)]


def test_an_erase_is_sent_in_chunks_of_64_blocks() -> None:
    dev = device_with(100, 100 + 64 + 64 + 9)
    s = punchboot.Session(transport=dev)

    s.part_erase(SYSTEM_A_UUID)
    assert erases(dev) == [(100, 64), (164, 64), (228, 10)]
