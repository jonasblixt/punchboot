"""Partition table handling and partition I/O.

Ported from test_part, test_part_flash, test_part_verify_fh, test_verify_bpak,
test_part_dump and test_part_dump2.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    DISK_UUID,
    READABLE_UUID,
    SYSTEM_A_LBA,
    SYSTEM_A_UUID,
    SYSTEM_B_LBA,
    SYSTEM_B_UUID,
    SYSTEM_SIZE,
    make_bpak,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device


def test_blank_disk_lists_only_the_disk(blank_device: Device) -> None:
    with blank_device.session() as s:
        parts = s.part_get_partitions()

    assert [p.uuid for p in parts] == [DISK_UUID]


def test_install_partition_table(blank_device: Device) -> None:
    with blank_device.session() as s:
        s.part_table_install(DISK_UUID)

    # The new table is used after the next boot
    blank_device.reboot()

    with blank_device.session() as s:
        parts = {p.uuid: p for p in s.part_get_partitions()}

    assert parts[SYSTEM_A_UUID].description == "System A"
    assert parts[SYSTEM_A_UUID].first_block == SYSTEM_A_LBA
    assert parts[SYSTEM_B_UUID].description == "System B"
    assert parts[SYSTEM_B_UUID].first_block == SYSTEM_B_LBA


def test_disable_boot_without_partition_table_fails(blank_device: Device) -> None:
    # The A/B state lives on the disk, so it is not available until a
    # partition table has been installed.
    with blank_device.session() as s, pytest.raises(punchboot.GenericError):
        s.boot_set_boot_part(None)


def test_disable_boot(device: Device) -> None:
    with device.session() as s:
        s.boot_set_boot_part(None)


def test_write_partitions(device: Device, tmp_path: pathlib.Path) -> None:
    data_a = os.urandom(SYSTEM_SIZE)
    data_b = os.urandom(SYSTEM_SIZE)
    (tmp_path / "a").write_bytes(data_a)
    (tmp_path / "b").write_bytes(data_b)

    with device.session() as s:
        s.part_write(tmp_path / "a", SYSTEM_A_UUID)
        s.part_write(tmp_path / "b", SYSTEM_B_UUID)

    assert device.read_blocks(SYSTEM_A_LBA, SYSTEM_SIZE // 512) == data_a
    assert device.read_blocks(SYSTEM_B_LBA, SYSTEM_SIZE // 512) == data_b


def test_verify_partition(device: Device, tmp_path: pathlib.Path) -> None:
    data = tmp_path / "data"
    other = tmp_path / "other"
    data.write_bytes(os.urandom(64 * 1024))
    other.write_bytes(os.urandom(64 * 1024))

    with device.session() as s:
        s.part_write(data, SYSTEM_A_UUID)
        s.part_verify(data, SYSTEM_A_UUID)
        with pytest.raises(punchboot.PartVerifyError):
            s.part_verify(other, SYSTEM_A_UUID)


def test_verify_partition_from_file_handle_at_eof(device: Device, tmp_path: pathlib.Path) -> None:
    """part_verify must rewind file handles, otherwise 0 bytes are verified."""
    data = tmp_path / "data"
    other = tmp_path / "other"
    data.write_bytes(os.urandom(64 * 1024))
    other.write_bytes(os.urandom(64 * 1024))

    with device.session() as s:
        s.part_write(data, SYSTEM_A_UUID)

        with data.open("rb") as f:
            f.read()
            s.part_verify(f, SYSTEM_A_UUID)

        with other.open("rb") as f:
            f.read()
            with pytest.raises(punchboot.PartVerifyError):
                s.part_verify(f, SYSTEM_A_UUID)


def test_verify_bpak_image(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(64 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.part_verify(img, SYSTEM_A_UUID)


def test_read_partition(device: Device, tmp_path: pathlib.Path) -> None:
    data = os.urandom(1024 * 1024)
    (tmp_path / "in").write_bytes(data)

    with device.session() as s:
        s.part_write(tmp_path / "in", READABLE_UUID)
        s.part_read(tmp_path / "out", READABLE_UUID)

    assert (tmp_path / "out").read_bytes() == data


def test_read_non_readable_partition_fails(device: Device, tmp_path: pathlib.Path) -> None:
    with device.session() as s, pytest.raises(punchboot.Error):
        s.part_read(tmp_path / "out", SYSTEM_A_UUID)


@pytest.mark.parametrize(
    ("payload_size", "fits"),
    [
        pytest.param(SYSTEM_SIZE - 4096, True, id="exactly-fits"),
        pytest.param(SYSTEM_SIZE, False, id="header-overlaps-payload"),
        pytest.param(2 * SYSTEM_SIZE, False, id="much-too-large"),
    ],
)
def test_write_bpak_image_size_limit(
    device: Device, tmp_path: pathlib.Path, payload_size: int, *, fits: bool
) -> None:
    """The payload and the 4 KiB header (written to the end) must both fit (F-050)."""
    img = make_bpak(tmp_path / "img.bpak", os.urandom(payload_size))
    before = device.read_blocks(SYSTEM_A_LBA, SYSTEM_SIZE // 512)

    with device.session() as s:
        if fits:
            s.part_write(img, SYSTEM_A_UUID)
            s.part_verify(img, SYSTEM_A_UUID)
        else:
            with pytest.raises(punchboot.ArgumentError):
                s.part_write(img, SYSTEM_A_UUID)

    if not fits:
        # Rejected before anything was written
        assert device.read_blocks(SYSTEM_A_LBA, SYSTEM_SIZE // 512) == before


def test_write_file_larger_than_partition_fails(device: Device, tmp_path: pathlib.Path) -> None:
    data = tmp_path / "data"
    data.write_bytes(os.urandom(SYSTEM_SIZE + 512))

    with device.session() as s, pytest.raises(punchboot.ArgumentError):
        s.part_write(data, SYSTEM_A_UUID)
