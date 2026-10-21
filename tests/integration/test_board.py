"""Board specific commands and status.

Ported from test_board and test_board_status.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import punchboot
import pytest

if TYPE_CHECKING:
    from .pbtest import Device


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


def test_board_status(device: Device) -> None:
    with device.session() as s:
        assert s.board_read_status() == "Board status OK!"
