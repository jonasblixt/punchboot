"""Board specific commands and status.

Ported from test_board and test_board_status.
"""

from __future__ import annotations

import struct
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import PB_CMD_BOARD_COMMAND, PB_CMD_BOOTLOADER_VERSION_READ

if TYPE_CHECKING:
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
