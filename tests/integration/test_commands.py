"""Commands that the other modules don't cover: boot status, erase, end of life and bpak read.

The command mode error paths are covered by the unit tests (tests/unit/test_cm.c).
"""

from __future__ import annotations

import os
import struct
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    AUTH_TOKEN,
    BLOCK_SIZE,
    DEV_KEY_ID,
    NULL_UUID,
    SYSTEM_A_LBA,
    SYSTEM_A_UUID,
    SYSTEM_B_UUID,
    SYSTEM_SIZE,
    make_bpak,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

PB_CMD_PART_BPAK_READ = 14
PB_RESULT_NOT_FOUND = 11
BPAK_HEADER_SIZE = 4096
BPAK_HEADER_MAGIC = 0x42504132

# pb-development3, the last key in the host board's key map
LAST_KEY_ID = 0x52C1EDA0


def test_boot_status(device: Device) -> None:
    with device.session() as s:
        s.boot_set_boot_part(SYSTEM_B_UUID)
        assert s.boot_status()[0] == SYSTEM_B_UUID
        s.boot_set_boot_part(None)
        assert s.boot_status()[0] == NULL_UUID


def test_erase_is_not_supported_by_the_host_disk(device: Device) -> None:
    with device.session() as s, pytest.raises(punchboot.NotSupportedError):
        s.part_erase(SYSTEM_A_UUID)


def test_end_of_life(device: Device) -> None:
    with device.session() as s:
        s.slc_set_configuration()
        s.slc_set_end_of_life()
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_EOL
    device.reboot()
    with device.session() as s:
        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_EOL


def test_slc_read_with_last_key_revoked(device: Device) -> None:
    with device.session() as s:
        s.slc_revoke_key(LAST_KEY_ID)
        assert LAST_KEY_ID in s.slc_get_revoked_keys()


def test_bpak_read(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(8 * 1024))
    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
    header = img.read_bytes()[:BPAK_HEADER_SIZE]

    with device.wire() as w:
        w.send_command(PB_CMD_PART_BPAK_READ, SYSTEM_A_UUID.bytes)
        assert w.result() == 0
        assert w.recv(BPAK_HEADER_SIZE) == header
        assert w.result() == 0

        # System B is empty
        assert w.command(PB_CMD_PART_BPAK_READ, SYSTEM_B_UUID.bytes) == -PB_RESULT_NOT_FOUND

    # The header is read from the end of the partition
    last_blocks = device.read_blocks(
        SYSTEM_A_LBA + (SYSTEM_SIZE - BPAK_HEADER_SIZE) // BLOCK_SIZE,
        BPAK_HEADER_SIZE // BLOCK_SIZE,
    )
    assert last_blocks == header
    assert struct.unpack_from("<I", header)[0] == BPAK_HEADER_MAGIC
