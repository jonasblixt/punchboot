"""Partition table handling (ported from tests/test_part.sh)."""

from __future__ import annotations

import uuid
from typing import TYPE_CHECKING

import punchboot
import pytest

from .conftest import DISK_UUID, SYSTEM_A_UUID, SYSTEM_B_UUID

if TYPE_CHECKING:
    from .conftest import Device


def test_blank_disk_lists_only_the_disk(device: Device) -> None:
    with device.session() as s:
        parts = s.part_get_partitions()

    assert [p.uuid for p in parts] == [uuid.UUID(DISK_UUID)]


def test_install_partition_table(device: Device) -> None:
    with device.session() as s:
        s.part_table_install(DISK_UUID)

    # The new table is used after the next boot
    device.reboot()

    with device.session() as s:
        parts = {str(p.uuid): p for p in s.part_get_partitions()}

    assert parts[SYSTEM_A_UUID].description == "System A"
    assert parts[SYSTEM_A_UUID].first_block == 34
    assert parts[SYSTEM_B_UUID].description == "System B"


def test_disable_boot_without_partition_table_fails(device: Device) -> None:
    # The A/B state lives on the disk, so it is not available until a
    # partition table has been installed.
    with device.session() as s, pytest.raises(punchboot.GenericError):
        s.boot_set_boot_part(None)


def test_disable_boot(device: Device) -> None:
    with device.session() as s:
        s.part_table_install(DISK_UUID)

    device.reboot()

    with device.session() as s:
        s.boot_set_boot_part(None)
