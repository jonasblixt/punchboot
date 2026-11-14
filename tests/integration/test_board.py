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
from .test_security import DEVICE_UUID
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


def test_command_list(device: Device) -> None:
    # command-mode.md 5.6: the host board's named commands, with the ids it
    # answered before 3.0.0, and the core's 'info' (D-077)
    with device.session() as s:
        commands = s.command_list()

    assert {c.name for c in commands} == {"info", "test-command", "test-command2", "test-boot-stop"}
    ids = {c.name: c.id for c in commands}
    assert ids["test-command"] == TEST_COMMAND
    assert ids["test-command2"] == 0xDFA7C4AD
    assert ids["test-boot-stop"] == 0x70C3E5F7
    assert all(c.help for c in commands)


def test_info(device: Device) -> None:
    # platform.md 3.11: the host platform's line, with its NUL
    with device.session() as s:
        info = s.command_run("info")

    assert info == b"SoC host rev unknown, CPU unknown, memory none set up\n\0"


def test_command_run(device: Device) -> None:
    with device.session() as s:
        assert s.command_run("test-command", b"abc") == b"Hello test-command: abc\n"
        with pytest.raises(punchboot.NotSupportedError):
            s.command_run("test-unknown")
        with pytest.raises(punchboot.NoMemoryError):
            s.command_run("test-command", bytes(1025))


def test_unknown_command_is_not_supported(device: Device) -> None:
    # An id no command has: NOT_SUPPORTED, as on every board (command-mode.md
    # 5.6). The host board answered ERROR before 3.0.0
    with device.session() as s, pytest.raises(punchboot.NotSupportedError):
        s.board_run_command(0xDEAD)


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


def test_identity_after_a_failed_init(no_disk_device: Device) -> None:
    # platform.md 2.2 (F-247): the crypto backend is set up by the platform,
    # before the board's init(). The device's identity is a hash (platform.md
    # 3.7), and without it there is no command mode (F-416): a failed init()
    # leaves both
    run = no_disk_device.log.read_text(errors="replace").rpartition("=== start:")[2]
    assert "Board init failed" in run

    with no_disk_device.session() as s:
        assert str(s.device_get_uuid()) == DEVICE_UUID


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


def test_boot_from_ram_after_a_failed_init(
    no_disk_device: Device, tmp_path: pathlib.Path
) -> None:
    """board.md 2.1 and 2.3 (F-394): a failed init() leaves what a RAM boot needs.

    The board's disk can't be opened, so its init() fails and pb stops in
    command mode. The platform sets up Mbed TLS in plat_init(), before the
    board's init(), as the real ports set up their crypto engine (F-247), so
    the image from RAM verifies.
    """
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with no_disk_device.session() as s:
        # No disk, so no partitions: the host's library answers that with an error
        with pytest.raises(punchboot.GenericError):
            s.part_get_partitions()
        s.boot_bpak(img, NULL_UUID)

    assert no_disk_device.wait_exit() == 0
    assert no_disk_device.boot_status == "?"
