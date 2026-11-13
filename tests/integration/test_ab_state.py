"""A/B boot state: switching, rollback, redundancy and board registers.

Ported from test_switch, test_rollback, test_corrupt_primary_config,
test_corrupt_backup_config and test_board_regs.

The state is manipulated with the pbstate tool, like from Linux on a target.
"""

from __future__ import annotations

import os
import struct
import uuid
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    BACKUP_STATE_LBA,
    NULL_UUID,
    PRIMARY_STATE_LBA,
    READABLE_UUID,
    SYSTEM_A_LBA,
    SYSTEM_A_UUID,
    SYSTEM_B_LBA,
    SYSTEM_B_UUID,
    make_bpak,
)

# struct pb_boot_state: magic, enable, verified, attempts, error (u32 each)
STATE_A = 1
STATE_B = 2

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


def _state(device: Device) -> tuple[int, int, int, int]:
    """Enable, verified, remaining attempts and error of the primary state."""
    raw = device.read_blocks(PRIMARY_STATE_LBA)
    enable, verified, attempts, error = struct.unpack_from("<4I", raw, 4)
    return enable, verified, attempts, error


def _flip_payload_byte(device: Device, lba: int) -> None:
    """Change one byte of the image's payload: it no longer verifies."""
    block = bytearray(device.read_blocks(lba))
    block[0] ^= 0xFF
    device.write_blocks(lba, bytes(block))


def test_fallback_when_enabled_system_fails_to_verify(ab_device: Device) -> None:
    # Both systems are confirmed by the OS, A is the enabled one
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a")
    ab_device.pbstate("-v", "a")
    ab_device.reset()
    assert _state(ab_device) == (STATE_A, STATE_A | STATE_B, 0, 0)
    primary = ab_device.read_blocks(PRIMARY_STATE_LBA)
    backup = ab_device.read_blocks(BACKUP_STATE_LBA)

    _flip_payload_byte(ab_device, SYSTEM_A_LBA)

    # A doesn't verify: B boots, for this boot. Nothing is written: A is
    # still the enabled system, and it is tried first on every boot
    assert [ab_device.boot() for _ in range(2)] == ["B", "B"]
    assert ab_device.read_blocks(PRIMARY_STATE_LBA) == primary
    assert ab_device.read_blocks(BACKUP_STATE_LBA) == backup

    # A is whole again: it boots
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)
    assert ab_device.boot() == "A"


def test_fallback_when_trial_fails_to_verify(ab_device: Device) -> None:
    # B is confirmed. A is on trial with three attempts, and doesn't verify
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a", "-c", "3")
    ab_device.reset()
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)

    # Each boot spends one of A's attempts, then B boots for that boot
    for attempts_left in (2, 1, 0):
        assert ab_device.boot() == "B"
        assert _state(ab_device) == (STATE_A, STATE_B, attempts_left, 0)

    # Out of attempts: the rollback to B, which is written
    assert ab_device.boot() == "B"
    assert _state(ab_device) == (STATE_B, STATE_B, 0, STATE_A)


def test_confirming_the_fallback_keeps_the_trial(ab_device: Device) -> None:
    """boot.md 3.5, rule 3: the trial is tried again until its attempts run out.

    Also when the OS confirms the system it runs in, the fallback, after each
    boot (F-180: that ended the trial, and the next boot rolled back).
    """
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a", "-c", "3")
    ab_device.reset()
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)

    for attempts_left in (2, 1, 0):
        assert ab_device.boot() == "B"
        ab_device.pbstate("-v", "b")
        assert _state(ab_device) == (STATE_A, STATE_B, attempts_left, 0)

    assert ab_device.boot() == "B"
    assert _state(ab_device) == (STATE_B, STATE_B, 0, STATE_A)


def test_no_fallback_to_a_system_that_is_not_confirmed(ab_device: Device) -> None:
    # 'boot activate A' leaves only A verified: B is no fallback
    with ab_device.session() as s:
        s.boot_set_boot_part(SYSTEM_A_UUID)
    ab_device.reset()
    assert _state(ab_device) == (STATE_A, STATE_A, 0, 0)
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)

    ab_device.start(command_mode=False)
    ab_device.wait_command_mode()
    assert ab_device.boot_status is None
    assert _state(ab_device) == (STATE_A, STATE_A, 0, 0)


def test_boot_part_without_a_uuid_falls_back(ab_device: Device) -> None:
    # 'boot part' with the null UUID is the policy's boot, as without
    # command mode. Both systems are confirmed, A is enabled and doesn't verify
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a")
    ab_device.pbstate("-v", "a")
    ab_device.reset()
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)
    primary = ab_device.read_blocks(PRIMARY_STATE_LBA)

    ab_device.start()
    with ab_device.session() as s:
        # The host gets OK: an image was loaded and verified, B's
        s.boot_partition(NULL_UUID)
    assert ab_device.wait_exit() == 0
    assert ab_device.boot_status == "B"
    assert ab_device.read_blocks(PRIMARY_STATE_LBA) == primary


def test_boot_part_without_a_uuid_and_no_fallback(ab_device: Device) -> None:
    # Only A is verified ('boot activate A'), and it doesn't verify: the host
    # gets the error of the one candidate, as before there was a fallback
    with ab_device.session() as s:
        s.boot_set_boot_part(SYSTEM_A_UUID)
    ab_device.reset()
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)
    primary = ab_device.read_blocks(PRIMARY_STATE_LBA)

    ab_device.start()
    with ab_device.session() as s, pytest.raises(punchboot.GenericError):
        s.boot_partition(NULL_UUID)
    # Still in command mode, nothing booted and nothing written
    assert ab_device.boot_status is None
    ab_device.reset()
    assert ab_device.read_blocks(PRIMARY_STATE_LBA) == primary

    # A boots once it is whole again

    _flip_payload_byte(ab_device, SYSTEM_A_LBA)
    ab_device.start()
    with ab_device.session() as s:
        s.boot_partition(NULL_UUID)
    assert ab_device.wait_exit() == 0
    assert ab_device.boot_status == "A"


def test_boot_mode(ab_device: Device, tmp_path: pathlib.Path) -> None:
    """The boot's mode in pb's information (boot.md 5.6, 6), for each way to boot."""
    # Both systems are confirmed by the OS, A is the enabled one
    ab_device.pbstate("-v", "b")
    ab_device.pbstate("-s", "a")
    ab_device.pbstate("-v", "a")
    ab_device.reset()

    # The policy's first candidate
    assert ab_device.boot() == "A"
    assert ab_device.boot_mode == "normal"

    # A doesn't verify: the policy's second candidate, for this boot
    _flip_payload_byte(ab_device, SYSTEM_A_LBA)
    assert ab_device.boot() == "B"
    assert ab_device.boot_mode == "fallback"

    # 'boot part' without a UUID is the policy's boot too
    ab_device.start()
    with ab_device.session() as s:
        s.boot_partition(NULL_UUID)
    assert ab_device.wait_exit() == 0
    assert (ab_device.boot_status, ab_device.boot_mode) == ("B", "fallback")

    # A partition the host names is a normal boot: the policy isn't asked
    ab_device.start()
    with ab_device.session() as s:
        s.boot_partition(SYSTEM_B_UUID)
    assert ab_device.wait_exit() == 0
    assert (ab_device.boot_status, ab_device.boot_mode) == ("B", "normal")

    # B doesn't verify either: the fallback fails and pb enters command mode
    # with nothing prepared. The policy's last answer is "fallback"; the
    # host's stream, booted after it, is a normal boot
    _flip_payload_byte(ab_device, SYSTEM_B_LBA)
    ab_device.start(command_mode=False)
    ab_device.wait_command_mode()
    assert ab_device.boot_mode is None
    img = make_bpak(tmp_path / "ram.bpak", os.urandom(16 * 1024))
    with ab_device.session() as s:
        s.boot_bpak(img, SYSTEM_B_UUID)
    assert ab_device.wait_exit() == 0
    assert (ab_device.boot_status, ab_device.boot_mode) == ("B", "normal")


def test_activate_of_a_partition_that_is_no_system_changes_nothing(ab_device: Device) -> None:
    """`boot activate` with a UUID that isn't a slot's (boot.md 3.4, F-175)."""
    with ab_device.session() as s:
        s.boot_set_boot_part(SYSTEM_A_UUID)

        # A partition that is no system, and a UUID that is no partition: refused
        for other in (READABLE_UUID, uuid.UUID(int=1)):
            with pytest.raises(punchboot.PartNotBootableError):
                s.boot_set_boot_part(other)

        # For the rest of command mode A is still the enabled system, and
        # the policy's boot finds it
        assert s.boot_status()[0] == SYSTEM_A_UUID
        s.boot_partition(NULL_UUID)
    assert ab_device.wait_exit() == 0
    assert ab_device.boot_status == "A"
    assert _state(ab_device) == (STATE_A, STATE_A, 0, 0)
