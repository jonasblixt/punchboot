"""The host's side of a partition verify (command-mode.md 5.3).

PART_VERIFY carries the size in 32 bits, so the host refuses a larger file
before it sends anything: the size went masked, and the device hashed fewer
bytes than the host (F-227). A scripted device answers, see scripted.py.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import SYSTEM_A_UUID
from .scripted import ScriptedDevice

if TYPE_CHECKING:
    import pathlib

SIZE_MAX = 0xFFFFFFFF


@pytest.fixture
def large(tmp_path: pathlib.Path) -> pathlib.Path:
    path = tmp_path / "large"
    with path.open("wb") as f:
        f.truncate(SIZE_MAX + 1)  # sparse
    return path


def test_a_verify_size_the_command_cant_carry_is_refused(large: pathlib.Path) -> None:
    dev = ScriptedDevice()
    s = punchboot.Session(transport=dev)

    with pytest.raises(punchboot.ArgumentError):
        s.part_verify(large, SYSTEM_A_UUID)
    with large.open("rb") as f, pytest.raises(punchboot.ArgumentError):
        s.part_verify(f, SYSTEM_A_UUID)
    assert not dev.commands
