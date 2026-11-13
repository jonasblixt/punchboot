"""The pbstate tool's command line, on the A/B state of pb-host's disk.

Like from Linux on a target (pbtest.Device.pbstate). Scripts in the field
call the tool: an invocation that worked keeps working, with its exit code.
One that silently did the wrong thing fails now, and writes nothing (F-221).
"""

from __future__ import annotations

import errno
import struct
from typing import TYPE_CHECKING

import pytest

from .pbtest import BACKUP_STATE_LBA, PRIMARY_STATE_LBA

if TYPE_CHECKING:
    from .pbtest import Device

# struct pb_boot_state: magic, enable, verified, attempts, error (u32 each)
STATE_A = 1
STATE_B = 2


def _state(device: Device) -> tuple[int, int, int, int]:
    """Enable, verified, remaining attempts and error of the primary state."""
    raw = device.read_blocks(PRIMARY_STATE_LBA)
    enable, verified, attempts, error = struct.unpack_from("<4I", raw, 4)
    return enable, verified, attempts, error


def _copies(device: Device) -> tuple[bytes, bytes]:
    """Read the primary and the backup state, as they are on the disk."""
    return device.read_blocks(PRIMARY_STATE_LBA), device.read_blocks(BACKUP_STATE_LBA)


@pytest.mark.parametrize(
    ("system", "enable"),
    [("a", STATE_A), ("A", STATE_A), ("b", STATE_B), ("B", STATE_B), ("none", 0), ("NONE", 0)],
)
def test_switch_takes_a_b_or_none(device: Device, system: str, enable: int) -> None:
    device.pbstate("-s", "b" if enable == STATE_A else "a")

    assert device.pbstate_run("-s", system).returncode == 0
    assert _state(device)[0] == enable


@pytest.mark.parametrize("system", ["x", "c", "0", "", "off"])
def test_switch_refuses_what_is_no_system(device: Device, system: str) -> None:
    # It disabled both systems, and exited 0
    device.pbstate("-s", "a")
    copies = _copies(device)

    result = device.pbstate_run("-s", system)
    assert result.returncode == errno.EINVAL
    assert "-s takes a, b or none" in result.stderr
    assert _copies(device) == copies
