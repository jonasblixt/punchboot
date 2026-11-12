"""Partitions that are booted in place (boot.md 4.1, board.md 2.4).

The host board's memory mapped flash (pb-host --nor) has a static partition
table: "NOR In place" and "NOR Not bootable" have the in place flag, "NOR
Loaded" doesn't. pb maps the flash read only, so an image that is verified
in place can't have been copied or patched there. Its parts are linked for
where the partition is mapped: the load address is the partition's address
plus the part's offset in the payload.

A boot of "NOR In place" ends with the host board writing 'X', one of "NOR
Loaded" with 'N'.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    BLOCK_SIZE,
    BPAK_HEADER_SIZE,
    DEV_KEY_ID,
    NOR_BASE,
    NOR_IN_PLACE_OFFSET,
    NOR_IN_PLACE_SIZE,
    NOR_IN_PLACE_UUID,
    NOR_LOADED_OFFSET,
    NOR_LOADED_SIZE,
    NOR_LOADED_UUID,
    NOR_NOT_BOOTABLE_UUID,
    NOR_UUID,
    SYSTEM_A_UUID,
    make_bpak,
)
from .test_boot import boot_partition

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

# Where the first part of an image in "NOR In place" is, and the room for
# the parts: the partition without the header in its last 4 KiB
IN_PLACE_ADDR = NOR_BASE + NOR_IN_PLACE_OFFSET
IN_PLACE_ROOM = NOR_IN_PLACE_SIZE - BPAK_HEADER_SIZE
NOT_BOOTABLE_ADDR = NOR_BASE + NOR_LOADED_OFFSET + NOR_LOADED_SIZE


def assert_not_booted(device: Device) -> None:
    assert device.running
    assert device.boot_status is None


def test_flash_partitions_are_listed(nor_device: Device) -> None:
    with nor_device.session() as s:
        parts = {p.uuid: p for p in s.part_get_partitions()}

    assert parts[NOR_UUID].first_block == 0
    assert parts[NOR_IN_PLACE_UUID].description == "NOR In place"
    assert parts[NOR_IN_PLACE_UUID].first_block == NOR_IN_PLACE_OFFSET // BLOCK_SIZE
    assert parts[NOR_LOADED_UUID].first_block == NOR_LOADED_OFFSET // BLOCK_SIZE
    # The disk's partitions are there as before
    assert parts[SYSTEM_A_UUID].description == "System A"


def test_boot_in_place(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        s.part_verify(img, NOR_IN_PLACE_UUID)

    assert boot_partition(nor_device, NOR_IN_PLACE_UUID) == "X"


def test_boot_in_place_after_power_cycle(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
    nor_device.reboot()

    assert boot_partition(nor_device, NOR_IN_PLACE_UUID) == "X"


def test_image_that_fills_the_partition_boots(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(IN_PLACE_ROOM), load_addr=IN_PLACE_ADDR)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)

    assert boot_partition(nor_device, NOR_IN_PLACE_UUID) == "X"


def test_part_that_reaches_into_the_header_is_refused(
    nor_device: Device, tmp_path: pathlib.Path
) -> None:
    # One block further up than where it is: the image fits the partition,
    # its part's load address range doesn't
    img = make_bpak(
        tmp_path / "img.bpak", os.urandom(IN_PLACE_ROOM), load_addr=IN_PLACE_ADDR + BLOCK_SIZE
    )

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        with pytest.raises(punchboot.MemError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


@pytest.mark.parametrize(
    "load_addr",
    [
        pytest.param(0x49000000, id="ram"),
        pytest.param(NOR_BASE + NOR_LOADED_OFFSET, id="another-partition"),
        pytest.param(IN_PLACE_ADDR - BLOCK_SIZE, id="starts-below"),
        pytest.param(NOR_BASE, id="start-of-the-flash"),
        pytest.param(2**64 - 4096, id="wraps"),
    ],
)
def test_image_for_another_address_is_refused(
    nor_device: Device, tmp_path: pathlib.Path, load_addr: int
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=load_addr)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        with pytest.raises(punchboot.MemError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_changed_payload_is_refused(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)
    offset = NOR_IN_PLACE_OFFSET + 5000

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)

        # In the flash, behind pb's back: pb hashes what is there at the boot
        byte = nor_device.read_nor(offset, 1)
        nor_device.write_nor(offset, bytes([byte[0] ^ 1]))
        with pytest.raises(punchboot.GenericError):
            s.boot_partition(NOR_IN_PLACE_UUID)
        assert_not_booted(nor_device)

        nor_device.write_nor(offset, byte)

    assert boot_partition(nor_device, NOR_IN_PLACE_UUID) == "X"


def test_part_is_hashed_at_its_load_address(nor_device: Device, tmp_path: pathlib.Path) -> None:
    # Linked for one block further up than where 'part write' puts it. The
    # load address range is inside the partition, so placement has no say
    data = os.urandom(16 * 1024)
    img = make_bpak(tmp_path / "img.bpak", data, load_addr=IN_PLACE_ADDR + BLOCK_SIZE)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)

        # The part is at the partition's start, and pb hashes what is at its
        # load address: other bytes
        assert nor_device.read_nor(NOR_IN_PLACE_OFFSET, len(data)) == data
        with pytest.raises(punchboot.GenericError):
            s.boot_partition(NOR_IN_PLACE_UUID)
        assert_not_booted(nor_device)

        # The same image and header, with the part where it is linked for
        nor_device.write_nor(NOR_IN_PLACE_OFFSET + BLOCK_SIZE, data)

    assert boot_partition(nor_device, NOR_IN_PLACE_UUID) == "X"


def test_changed_header_is_refused(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)
    # A byte inside the header, which the signature covers
    offset = NOR_IN_PLACE_OFFSET + IN_PLACE_ROOM + 1024

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        byte = nor_device.read_nor(offset, 1)
        nor_device.write_nor(offset, bytes([byte[0] ^ 1]))
        with pytest.raises(punchboot.AuthenticationError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_erased_partition_is_refused(nor_device: Device) -> None:
    with nor_device.session() as s, pytest.raises(punchboot.GenericError):
        s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_image_signed_with_wrong_key_is_refused(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(
        tmp_path / "img.bpak",
        os.urandom(16 * 1024),
        load_addr=IN_PLACE_ADDR,
        sign_key="pki/secp256r1-key-pair2.pem",
    )

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        with pytest.raises(punchboot.AuthenticationError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_image_with_unknown_key_id_is_refused(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(
        tmp_path / "img.bpak",
        os.urandom(16 * 1024),
        load_addr=IN_PLACE_ADDR,
        key_id="invalid-key-id",
    )

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        with pytest.raises(punchboot.GenericError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_image_with_revoked_key_is_refused(nor_device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)

    with nor_device.session() as s:
        s.part_write(img, NOR_IN_PLACE_UUID)
        s.slc_revoke_key(DEV_KEY_ID)
        with pytest.raises(punchboot.KeyRevokedError):
            s.boot_partition(NOR_IN_PLACE_UUID)

    assert_not_booted(nor_device)


def test_partition_that_is_not_bootable_is_refused(
    nor_device: Device, tmp_path: pathlib.Path
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), load_addr=NOT_BOOTABLE_ADDR)

    with nor_device.session() as s:
        s.part_write(img, NOR_NOT_BOOTABLE_UUID)
        with pytest.raises(punchboot.PartNotBootableError):
            s.boot_partition(NOR_NOT_BOOTABLE_UUID)

    assert_not_booted(nor_device)


def test_partition_without_the_flag_is_loaded(nor_device: Device, tmp_path: pathlib.Path) -> None:
    # An image for RAM, as on the disk: pb reads it from the flash and copies it
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with nor_device.session() as s:
        s.part_write(img, NOR_LOADED_UUID)

    assert boot_partition(nor_device, NOR_LOADED_UUID) == "N"


def test_disk_boots_as_before_with_the_flash(nor_device: Device, tmp_path: pathlib.Path) -> None:
    flash_img = make_bpak(tmp_path / "flash.bpak", os.urandom(16 * 1024), load_addr=IN_PLACE_ADDR)
    disk_img = make_bpak(tmp_path / "disk.bpak", os.urandom(16 * 1024))

    with nor_device.session() as s:
        s.part_write(flash_img, NOR_IN_PLACE_UUID)
        s.part_write(disk_img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    nor_device.reset()

    # The policy's candidate is the disk's, and it is loaded
    assert nor_device.boot() == "A"
