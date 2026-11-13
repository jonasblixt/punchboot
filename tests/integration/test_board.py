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
    BLOCK_SIZE,
    DISK_BLOCKS,
    DISK_UUID,
    NULL_UUID,
    PB_CMD_BOARD_COMMAND,
    PB_CMD_BOOTLOADER_VERSION_READ,
    READABLE_UUID,
    SYSTEM_A_UUID,
    make_bpak,
)
from .test_stream import (
    BUFFER_SIZE as STREAM_BUFFER_SIZE,
)
from .test_stream import (
    READABLE_LBA,
    stream_init,
    stream_prepare,
    stream_write,
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
    # board's test-command reads the request up to request_size.
    with device.wire() as wire:
        wire.send_command(PB_CMD_BOARD_COMMAND, struct.pack("<III", TEST_COMMAND, request_size, 0))
        assert wire.result() == 0
        wire.send(bytes(1024))
        assert wire.result() == -PB_RESULT_INVALID_ARGUMENT
        assert wire.command(PB_CMD_BOOTLOADER_VERSION_READ) == 0


def test_board_response_fits_the_hosts_buffer(device: Device) -> None:
    # command-mode.md 5.6 (F-095): the response is at most what the host said
    # it can take. The punchboot package always says 4096; a host that says 8
    # gets 8 bytes of the test-command's answer, and stays in step.
    with device.wire() as wire:
        wire.send_command(PB_CMD_BOARD_COMMAND, struct.pack("<III", TEST_COMMAND, 4, 8))
        assert wire.result() == 0
        wire.send(b"abcd".ljust(1024, b"\0"))
        _, code, size = struct.unpack_from("<Ib3xI", wire.recv(512))
        assert (code, size) == (0, 8)
        assert wire.recv(size) == b"Hello t\0"
        assert wire.result() == 0
        assert wire.command(PB_CMD_BOOTLOADER_VERSION_READ) == 0


def test_board_command_writes_nothing_past_its_request(device: Device) -> None:
    # command-mode.md 5.6 (F-413): the request is the host's, read into the command
    # buffer, the stream's buffer 0. The host board's test-command put a NUL
    # after the request, a byte past a 1024-byte one: what the buffer held
    # there was gone
    data = b"\xa5" * STREAM_BUFFER_SIZE
    request = b"x" * 1024
    with device.wire() as wire:
        assert stream_init(wire, READABLE_UUID) == 0
        assert stream_prepare(wire, 0, data) == 0
        wire.send_command(
            PB_CMD_BOARD_COMMAND, struct.pack("<III", TEST_COMMAND, len(request), 4096)
        )
        assert wire.result() == 0
        wire.send(request)
        _, code, size = struct.unpack_from("<Ib3xI", wire.recv(512))
        assert code == 0
        assert wire.recv(size) == b"Hello test-command: " + request + b"\n"
        assert wire.result() == 0
        assert stream_write(wire, 0, 0, len(data)) == 0

    written = device.read_blocks(READABLE_LBA, len(data) // BLOCK_SIZE)
    assert written == request + data[len(request) :]


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
    # A/B's init failed before it looked its state partitions up. 'boot
    # activate' reaches the policy (F-178), which answers an error and writes
    # no state (F-393): before that, the state went to LBA 0 of the disk
    before = no_policy_device.read_blocks(0, DISK_BLOCKS)

    with no_policy_device.session() as s:
        for part in (SYSTEM_A_UUID, None):
            with contextlib.suppress(punchboot.Error):
                s.boot_set_boot_part(part)

    assert no_policy_device.read_blocks(0, DISK_BLOCKS) == before
