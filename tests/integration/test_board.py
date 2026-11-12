"""Board specific commands and status, and a failed step of the board's setup.

Ported from test_board and test_board_status.
"""

from __future__ import annotations

import contextlib
import os
import struct
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    DISK_BLOCKS,
    DISK_UUID,
    NULL_UUID,
    PB_CMD_BOARD_COMMAND,
    PB_CMD_BOOTLOADER_VERSION_READ,
    SYSTEM_A_UUID,
    make_bpak,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

TEST_COMMAND = 0xC72B6E9E  # pb_id("test-command")
PB_RESULT_INVALID_ARGUMENT = 5


def test_board_command(device: Device) -> None:
    with device.session() as s:
        response = s.board_run_command("test-command", b"0x11223344")

    assert response == b"Hello test-command: 0x11223344\n"


def test_unknown_board_command_fails(device: Device) -> None:
    with device.session() as s, pytest.raises(punchboot.Error):
        s.board_run_command("test-unknown", b"0x00112233")


def test_failing_board_command(device: Device) -> None:
    # 'test-command2' is a board command that returns an error (-128)
    with device.session() as s, pytest.raises(punchboot.Error):
        s.board_run_command("test-command2", b"0x00112233")


@pytest.mark.parametrize("request_size", [1025, 0xFFFFFFFF])
def test_board_command_request_too_large(device: Device, request_size: int) -> None:
    # F-063: the punchboot package never sends more than 1024 bytes. The host
    # board's test-command terminates the request at request_size.
    with device.wire() as wire:
        wire.send_command(PB_CMD_BOARD_COMMAND, struct.pack("<III", TEST_COMMAND, request_size, 0))
        assert wire.result() == 0
        wire.send(bytes(1024))
        assert wire.result() == -PB_RESULT_INVALID_ARGUMENT
        assert wire.command(PB_CMD_BOOTLOADER_VERSION_READ) == 0


def test_board_status(device: Device) -> None:
    with device.session() as s:
        assert s.board_read_status() == "Board status OK!"


@pytest.fixture
def no_policy_device(blank_device: Device) -> Device:
    """Start a device whose boot source policy can't be set up.

    The table's variant 2 has System A's partition and no System B, so A/B's
    init fails and board_setup() with it: pb stops in command mode.
    """
    with blank_device.session() as s:
        s.part_table_install(DISK_UUID, 2)
    blank_device.reboot()
    run = blank_device.log.read_text(errors="replace").rpartition("=== start:")[2]
    assert "Boot policy init failed" in run
    return blank_device


def test_boot_from_ram_after_a_failed_policy_init(
    no_policy_device: Device, tmp_path: pathlib.Path
) -> None:
    # board.md 2.1 (F-178): the handover is set up all the same, and a boot
    # from RAM needs neither the table nor the policy
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with no_policy_device.session() as s:
        s.boot_bpak(img, NULL_UUID)

    assert no_policy_device.wait_exit() == 0
    assert no_policy_device.boot_status == "?"


def test_boot_activate_after_a_failed_policy_init_writes_no_state(
    no_policy_device: Device,
) -> None:
    # A/B's init failed before it looked its state partitions up. Whatever
    # 'boot activate' answers then (F-178), it writes no state: with the
    # policy called, the state went to LBA 0 of the disk
    before = no_policy_device.read_blocks(0, DISK_BLOCKS)

    with no_policy_device.session() as s:
        for part in (SYSTEM_A_UUID, None):
            with contextlib.suppress(punchboot.Error):
                s.boot_set_boot_part(part)

    assert no_policy_device.read_blocks(0, DISK_BLOCKS) == before
