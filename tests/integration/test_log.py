"""The RAM log (CONFIG_RAMLOG) and the log command.

pb-host sends its stdout through the console, like the UART on target, so the
RAM log holds the same bytes as the pb-host log.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import punchboot
import pytest
from punchboot.wire import PB_LOG_ID_PB

from .test_cli import punchboot_cli

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

# configs/host_defconfig
CM_BUF_SIZE = 4 * 1024
RAMLOG_SIZE = 16 * 1024


def log_head(s: punchboot.Session) -> int:
    """Bytes written to the device's log so far."""
    _, _, head = s.log_read_chunk(0, 0)
    return int(head)


def fill_log(s: punchboot.Session, size: int) -> None:
    """Run commands until the device has logged at least 'size' bytes."""
    while log_head(s) < size:
        s.part_get_partitions()


def test_log_read(device: Device) -> None:
    with device.session() as s:
        log = s.log_read()

    assert log.startswith(b"\n\rPB ")
    assert b"Initializing command mode" in log
    assert log in device.log.read_bytes()


def test_log_read_in_chunks(device: Device) -> None:
    with device.session() as s:
        fill_log(s, 3 * CM_BUF_SIZE)
        log = s.log_read()

    assert len(log) > 2 * CM_BUF_SIZE
    assert log.startswith(b"\n\rPB ")
    assert log in device.log.read_bytes()


def test_log_read_wrapped(device: Device) -> None:
    with device.session() as s:
        fill_log(s, RAMLOG_SIZE + CM_BUF_SIZE)
        log = s.log_read()

    assert len(log) == RAMLOG_SIZE
    assert not log.startswith(b"\n\rPB ")
    assert log in device.log.read_bytes()


def test_cli_log(device: Device) -> None:
    result = punchboot_cli(device, "log")

    assert result.returncode == 0, result.stderr
    assert "Initializing command mode" in result.stdout
    assert "\r" not in result.stdout


def test_cli_log_to_file(device: Device, tmp_path: pathlib.Path) -> None:
    output = tmp_path / "log.txt"
    result = punchboot_cli(device, "log", "-o", str(output))

    assert result.returncode == 0, result.stderr
    assert result.stdout == ""
    assert output.read_bytes().startswith(b"\n\rPB ")


def test_log_list(device: Device) -> None:
    with device.session() as s:
        assert s.log_list() == [("pb", "pb's console output")]


def test_log_read_by_id(device: Device) -> None:
    # pb_id("pb") reads the same log as 0, which the host sends for "pb"
    with device.session() as s:
        _, by_name, _ = s.log_read_chunk(0, 64)
        _, by_id, _ = s.log_read_chunk(0, 64, PB_LOG_ID_PB)

    assert by_name == by_id
    assert by_name.startswith(b"\n\rPB ")


def test_unknown_log(device: Device) -> None:
    with device.session() as s, pytest.raises(punchboot.NotFoundError):
        s.log_read("rom")


def test_cli_log_list(device: Device) -> None:
    result = punchboot_cli(device, "log", "--list")

    assert result.returncode == 0, result.stderr
    assert result.stdout.split() == ["pb", "pb's", "console", "output"]


def test_cli_unknown_log(device: Device) -> None:
    result = punchboot_cli(device, "log", "rom")

    assert result.returncode != 0
