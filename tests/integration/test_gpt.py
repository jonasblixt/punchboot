"""GPT recovery and table variants, verified with sgdisk.

Ported from test_corrupt_gpt{,2,3,4,5}, test_corrupt_backup_gpt and
test_gpt_variants.

Note: test_corrupt_gpt3/4 intended to corrupt the GPT header and the
partition entries, but used dd 'skip' instead of 'seek' and corrupted the
start of the disk instead. Both cases are covered properly here.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import DISK_BLOCKS, DISK_UUID, SYSTEM_A_UUID, sgdisk_verify

if TYPE_CHECKING:
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
