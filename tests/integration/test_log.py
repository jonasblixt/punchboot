"""The RAM log (CONFIG_RAMLOG) and the log command.

pb-host sends its stdout through the console, like the UART on target, so the
RAM log holds the same bytes as the pb-host log.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from .test_cli import punchboot_cli

if TYPE_CHECKING:
    import pathlib

    import punchboot

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
