"""A/B boot state: switching, rollback, redundancy and board registers.

Ported from test_switch, test_rollback, test_corrupt_primary_config,
test_corrupt_backup_config and test_board_regs.

The state is manipulated with the pbstate tool, like from Linux on a target.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import pytest

from .pbtest import (
    BACKUP_STATE_LBA,
    PRIMARY_STATE_LBA,
    SYSTEM_A_UUID,
    SYSTEM_B_UUID,
    make_bpak,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device


@pytest.fixture
def ab_device(device: Device, tmp_path: pathlib.Path) -> Device:
    """Device with bootable images in both System A and System B."""
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))
    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.part_write(img, SYSTEM_B_UUID)
    return device


def test_switch_system(ab_device: Device) -> None:
    ab_device.pbstate("-s", "a")
    ab_device.reset()
    assert ab_device.boot() == "A"

    ab_device.pbstate("-s", "b")
    assert ab_device.boot() == "B"


def test_rollback_after_failed_boot_attempts(ab_device: Device) -> None:
    # B is verified. A is activated but not verified, with three attempts.
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a", "-c", "3")
    ab_device.reset()

    # A is never marked verified, so after three attempts it rolls back to B
    assert [ab_device.boot() for _ in range(4)] == ["A", "A", "A", "B"]


@pytest.mark.parametrize(
    ("corrupt_lba", "good_lba"),
    [(PRIMARY_STATE_LBA, BACKUP_STATE_LBA), (BACKUP_STATE_LBA, PRIMARY_STATE_LBA)],
    ids=["primary", "backup"],
)
def test_recover_corrupt_state(device: Device, corrupt_lba: int, good_lba: int) -> None:
    state = device.read_blocks(PRIMARY_STATE_LBA)
    assert device.read_blocks(BACKUP_STATE_LBA) == state

    device.write_blocks(corrupt_lba, os.urandom(512))
    device.reboot()

    assert device.read_blocks(good_lba) == state
    assert device.read_blocks(corrupt_lba) == state


def test_board_registers(device: Device) -> None:
    # The host board sets bit 0 in register 3 on boot when bit 0 in
    # register 0 is set
    device.pbstate("--write-board-reg", "0", "0x00000001")
    for reg in ("1", "2", "3"):
        device.pbstate("--write-board-reg", reg, "0x00000000")

    device.reboot()

    assert "3: 00000001" in device.pbstate("--info")
