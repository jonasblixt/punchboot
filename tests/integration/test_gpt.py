"""GPT recovery and table variants, verified with sgdisk.

Ported from test_corrupt_gpt{,2,3,4,5}, test_corrupt_backup_gpt and
test_gpt_variants.

Note: test_corrupt_gpt3/4 intended to corrupt the GPT header and the
partition entries, but used dd 'skip' instead of 'seek' and corrupted the
start of the disk instead. Both cases are covered properly here.
"""

from __future__ import annotations

import os
import struct
import uuid
import zlib
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    DISK_BLOCKS,
    DISK_UUID,
    READABLE_UUID,
    SYSTEM_A_LBA,
    SYSTEM_A_UUID,
    SYSTEM_B_LBA,
    SYSTEM_B_UUID,
    SYSTEM_SIZE,
    sgdisk,
    sgdisk_verify,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

GPT_OK = "No problems found"


def assert_partitions_present(device: Device) -> None:
    with device.session() as s:
        uuids = [p.uuid for p in s.part_get_partitions()]
    assert SYSTEM_A_UUID in uuids


def test_installed_table_is_valid(device: Device) -> None:
    assert GPT_OK in sgdisk_verify(device.disk)


@pytest.mark.parametrize(
    ("lba", "blocks"),
    [
        pytest.param(0, 64, id="primary-table-and-more"),
        pytest.param(1, 1, id="primary-header"),
        pytest.param(2, 3, id="primary-entries"),
    ],
)
def test_recover_primary_from_backup(device: Device, lba: int, blocks: int) -> None:
    garbage = os.urandom(blocks * 512)
    device.write_blocks(lba, garbage)

    device.reboot()

    # The corrupted blocks have been rewritten and the table is valid again
    assert device.read_blocks(lba, blocks) != garbage
    assert GPT_OK in sgdisk_verify(device.disk)
    assert_partitions_present(device)


def test_recover_backup_from_primary(device: Device) -> None:
    device.write_blocks(DISK_BLOCKS - 34, os.urandom(34 * 512))
    assert "CRC for the backup" in sgdisk_verify(device.disk)

    device.reboot()

    assert GPT_OK in sgdisk_verify(device.disk)
    assert_partitions_present(device)


@pytest.mark.parametrize("variant", [1, 2, 0])
def test_install_table_variant(blank_device: Device, variant: int) -> None:
    with blank_device.session() as s:
        s.part_table_install(DISK_UUID, variant)

    blank_device.reboot()

    assert GPT_OK in sgdisk_verify(blank_device.disk)
    assert_partitions_present(blank_device)


def test_install_too_large_table_variant_fails(blank_device: Device) -> None:
    with blank_device.session() as s, pytest.raises(punchboot.Error):
        s.part_table_install(DISK_UUID, 3)


def partitions(device: Device) -> dict[uuid.UUID, tuple[int, int]]:
    """Return the partitions pb lists, with their first and last block."""
    with device.session() as s:
        return {p.uuid: (p.first_block, p.last_block) for p in s.part_get_partitions()}


def test_empty_entry_is_skipped(device: Device) -> None:
    """bio.md 4: the partitions after a deleted one are made (F-192)."""
    device.stop()
    sgdisk(device.disk, "-d", "1")
    before = device.read_blocks(0, DISK_BLOCKS)

    device.start()

    parts = partitions(device)
    assert SYSTEM_A_UUID not in parts
    assert parts[SYSTEM_B_UUID] == (SYSTEM_B_LBA, SYSTEM_B_LBA + SYSTEM_SIZE // 512 - 1)
    device.stop()
    assert device.read_blocks(0, DISK_BLOCKS) == before
    device.start()


def test_table_with_fewer_entries(blank_device: Device) -> None:
    """A valid table of 32 entries is used as it is, boot after boot (F-191)."""
    blank_device.stop()
    sgdisk(blank_device.disk, "-o", "-S", "32", "-a", "1", "-n", "1:10:1087", "-n", "2:1088:65526")
    before = blank_device.read_blocks(0, DISK_BLOCKS)

    for _ in range(2):
        blank_device.start()
        assert sorted(partitions(blank_device).values()) == [
            (0, DISK_BLOCKS - 1),
            (10, 1087),
            (1088, 65526),
        ]
        blank_device.stop()
        assert blank_device.read_blocks(0, DISK_BLOCKS) == before

    blank_device.start()


def test_copies_that_differ(device: Device) -> None:
    """Two valid copies with other entries: the primary's are used, and it is logged (F-006)."""
    device.stop()
    # The primary loses its first entry, the backup keeps it
    array = bytearray(device.read_blocks(2, 32))
    array[:128] = bytes(128)
    header = bytearray(device.read_blocks(1, 1))
    struct.pack_into("<I", header, 88, zlib.crc32(array))
    struct.pack_into("<I", header, 16, 0)
    struct.pack_into("<I", header, 16, zlib.crc32(header[:92]))
    device.write_blocks(1, bytes(header) + bytes(array))
    before = device.read_blocks(0, DISK_BLOCKS)

    device.start()

    parts = partitions(device)
    assert SYSTEM_A_UUID not in parts
    assert SYSTEM_B_UUID in parts
    assert "backup GPT's entries differ" in device.log.read_text().rpartition("=== start:")[2]
    device.stop()
    assert device.read_blocks(0, DISK_BLOCKS) == before
    device.start()


# An installed table is in effect at once (command-mode.md 5.3, bio.md 4,
# D-070, F-194). The host board's layouts (src/board/host/board.c): variant 1
# is the default with "Readable" grown from 1 to 8 MiB, variant 2 is one
# partition with System A's UUID over the disk, variant 3 doesn't fit.
READABLE_LBA = 2085
READABLE_BLOCKS = 2048
READABLE_BLOCKS_VAR1 = 16384
LARGE_BLOCKS = 65468
FUSE_ARRAY_UUID = uuid.UUID("44acdcbe-dcb0-4d89-b0ad-8f96967f8c95")

Listing = dict[uuid.UUID, tuple[int, int, punchboot.PartitionFlags]]


def listing(s: punchboot.Session) -> Listing:
    """Return the partitions pb lists: first and last block, and the flags."""
    return {
        p.uuid: (p.first_block, p.last_block, p.partition_flags) for p in s.part_get_partitions()
    }


def blocks(parts: Listing) -> dict[uuid.UUID, tuple[int, int]]:
    return {part: (first, last) for part, (first, last, _) in parts.items()}


def test_installed_table_is_in_effect_at_once(
    blank_device: Device, tmp_path: pathlib.Path
) -> None:
    """A device without a table has its partitions when the install returned.

    Until F-194 they were there after a reset only: the host listed the disk
    alone, and nothing could be written or activated.
    """
    data = os.urandom(64 * 1024)
    (tmp_path / "data").write_bytes(data)

    with blank_device.session() as s:
        assert set(listing(s)) == {DISK_UUID}
        s.part_table_install(DISK_UUID)

        at_once = listing(s)
        assert blocks(at_once)[SYSTEM_A_UUID] == (SYSTEM_A_LBA, SYSTEM_A_LBA + 1023)
        assert blocks(at_once)[SYSTEM_B_UUID] == (SYSTEM_B_LBA, SYSTEM_B_LBA + 1023)
        # The partitions are a host's to write, as after a reset; the device
        # the table is on isn't
        assert at_once[SYSTEM_B_UUID][2] & punchboot.PartitionFlags.FLAG_WRITABLE
        assert not at_once[DISK_UUID][2] & punchboot.PartitionFlags.FLAG_WRITABLE

        s.part_write(tmp_path / "data", SYSTEM_B_UUID)
        s.part_verify(tmp_path / "data", SYSTEM_B_UUID)
        assert blank_device.read_blocks(SYSTEM_B_LBA, len(data) // 512) == data

        # The boot source policy's init ran again: its state's partitions
        # are there, and the systems are its candidates
        s.boot_set_boot_part(SYSTEM_B_UUID)
        assert s.boot_status()[0] == SYSTEM_B_UUID
        assert at_once[SYSTEM_A_UUID][2] & punchboot.PartitionFlags.FLAG_BOOTABLE

    # What a reset gives is the same table
    blank_device.reboot()
    with blank_device.session() as s:
        assert blocks(listing(s)) == blocks(at_once)
        assert s.boot_status()[0] == SYSTEM_B_UUID
        s.part_verify(tmp_path / "data", SYSTEM_B_UUID)


def test_another_table_replaces_the_partitions_at_once(
    device: Device, tmp_path: pathlib.Path
) -> None:
    """A partition that changed is the new one at once, the others stay.

    Until F-194 the old partitions were listed and written at their old
    blocks until the reset.
    """
    data = os.urandom((READABLE_BLOCKS + 16) * 512)
    (tmp_path / "data").write_bytes(data)

    with device.session() as s:
        before = listing(s)
        assert blocks(before)[READABLE_UUID] == (READABLE_LBA, READABLE_LBA + READABLE_BLOCKS - 1)

        s.part_table_install(DISK_UUID, 1)
        var1 = listing(s)

        # "Readable" grew. The others didn't change: the same blocks and the
        # same flags, the policy's BOOTABLE too
        assert blocks(var1)[READABLE_UUID] == (
            READABLE_LBA,
            READABLE_LBA + READABLE_BLOCKS_VAR1 - 1,
        )
        unchanged = set(before) - {READABLE_UUID}
        assert {part: var1[part] for part in unchanged} == {
            part: before[part] for part in unchanged
        }
        assert set(var1) == set(before)

        # More than the old partition held, written where the new one is
        s.part_write(tmp_path / "data", READABLE_UUID)
        s.part_verify(tmp_path / "data", READABLE_UUID)
        assert device.read_blocks(READABLE_LBA, len(data) // 512) == data

        # The board's handle of a partition that didn't change is good: the
        # host board's fuses are in "Fuse array" (bio.md 4, item 3)
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION

        # Variant 2: one partition, with System A's UUID, over the disk
        s.part_table_install(DISK_UUID, 2)
        var2 = listing(s)
        assert blocks(var2) == {
            DISK_UUID: (0, DISK_BLOCKS - 1),
            SYSTEM_A_UUID: (SYSTEM_A_LBA, SYSTEM_A_LBA + LARGE_BLOCKS - 1),
        }
        s.part_write(tmp_path / "data", SYSTEM_A_UUID)
        s.part_verify(tmp_path / "data", SYSTEM_A_UUID)
        # A partition that is gone isn't there for any command
        with pytest.raises(punchboot.NotFoundError):
            s.part_verify(tmp_path / "data", SYSTEM_B_UUID)
        with pytest.raises(punchboot.NotFoundError):
            s.part_write(tmp_path / "data", READABLE_UUID)

    assert device.read_blocks(SYSTEM_A_LBA, len(data) // 512) == data
    device.reboot()
    with device.session() as s:
        assert blocks(listing(s)) == blocks(var2)


def test_the_fuses_stay_when_their_old_partition_goes(device: Device) -> None:
    """bio.md 4, item 3: no hook of the board's runs again, and none needs to.

    The host board's fuses were on "Fuse array", looked up in late_init(),
    and an install that removed it left them failing until the reset. They
    are a file of their own now (F-217): the life cycle commands work, and
    the old blocks, now inside another partition, aren't written.
    """
    with device.session() as s:
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION
        s.part_table_install(DISK_UUID, 2)
        assert FUSE_ARRAY_UUID not in listing(s)
        fuses = device.read_blocks(READABLE_LBA - 1)
        s.slc_set_configuration()
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION
    assert device.read_blocks(READABLE_LBA - 1) == fuses


def test_the_same_table_keeps_every_partition(device: Device, tmp_path: pathlib.Path) -> None:
    """Installing the table that is there changes no partition and no flag."""
    data = os.urandom(SYSTEM_SIZE)
    (tmp_path / "data").write_bytes(data)

    with device.session() as s:
        s.part_write(tmp_path / "data", SYSTEM_A_UUID)
        before = listing(s)
        # The board's late_init() made it readable: a flag no install sets
        assert before[READABLE_UUID][2] & punchboot.PartitionFlags.FLAG_READABLE

        for _ in range(3):
            s.part_table_install(DISK_UUID)
            assert listing(s) == before

        s.part_verify(tmp_path / "data", SYSTEM_A_UUID)
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION

    device.reboot()
    with device.session() as s:
        assert listing(s) == before


@pytest.mark.parametrize("variant", [3, 42], ids=["too-large", "unknown"])
def test_an_install_that_writes_nothing_changes_nothing(
    device: Device, tmp_path: pathlib.Path, variant: int
) -> None:
    """A layout that is refused before the device is written leaves the table in effect."""
    data = os.urandom(SYSTEM_SIZE)
    (tmp_path / "data").write_bytes(data)
    table = device.read_blocks(0, SYSTEM_A_LBA)

    with device.session() as s:
        before = listing(s)
        with pytest.raises(punchboot.Error):
            s.part_table_install(DISK_UUID, variant)
        assert listing(s) == before
        s.part_write(tmp_path / "data", SYSTEM_A_UUID)
        s.part_verify(tmp_path / "data", SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)

    assert device.read_blocks(0, SYSTEM_A_LBA) == table


def test_installs_until_the_pool_is_full(device: Device) -> None:
    """A released partition's handle isn't given out again before the reset.

    Each change of the table takes new handles (bio.md 4). When the pool
    has none left the table is on the device all the same, the install
    answers the error, and a reset uses the table.
    """
    with device.session() as s:
        full = None
        for n in range(40):
            variant = 2 if n % 2 == 0 else 0
            try:
                s.part_table_install(DISK_UUID, variant)
            except punchboot.MemError:
                full = variant
                break
            expected = 2 if variant == 2 else 7
            assert len(listing(s)) == expected
        assert full is not None

    device.reboot()
    with device.session() as s:
        assert len(listing(s)) == (2 if full == 2 else 7)
    assert GPT_OK in sgdisk_verify(device.disk)
