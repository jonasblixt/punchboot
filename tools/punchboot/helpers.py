"""Various punchboot helper functions."""

from __future__ import annotations

import time
import uuid
import zlib
from importlib.metadata import version
from typing import TYPE_CHECKING

import semver  # type: ignore[import-not-found]

from . import transport
from .errors import Error

if TYPE_CHECKING:
    from collections.abc import Sequence


def library_version() -> semver.Version:
    """Return the version of the punchboot package."""
    return semver.Version.parse(version("punchboot"))


def pb_id(cmd: str) -> int:
    """Convert a string to a BPAK ID.

    This is used when calling a board specific command or when addressing
    keys.

    The id is a CRC32 of the name without the initial and final inversion,
    as pb's and bpak's C code compute it: zlib's crc32 with both undone.
    It is over the name's UTF-8 bytes, the bytes of the C string the device
    and the bpak CLI have for it.

    Keyword Arguments:
    cmd -- String input

    Returns a checksum result as an integer.
    """
    return zlib.crc32(cmd.encode(), 0xFFFFFFFF) ^ 0xFFFFFFFF


def wait_for_device(timeout: int = -1) -> None:
    """Wait for a punchboot device to enumerate.

    Keyword arguments:
    timeout -- Timeout in seconds

    Exceptions:
    TimeoutError -- The builtin one: no device found in time
    """
    # Late import, session.py imports this module
    from .session import Session  # noqa: PLC0415

    while True:
        try:
            s = Session()
        except Error:
            pass
        else:
            try:
                s.device_get_uuid()
            except Error:
                pass
            else:
                return
            finally:
                s.close()
        if timeout <= 0:
            msg = "No device found"
            raise TimeoutError(msg)
        time.sleep(1)
        timeout -= 1


def list_usb_devices() -> Sequence[uuid.UUID]:
    """Find all punchboot USB devices.

    Returns a list of device UUID's
    """
    return [uuid.UUID(x) for x in transport.list_usb_devices()]


def valid_bpak_magic(data: bytes) -> bool:
    """Check for a valid BPAK magic."""
    punchboot_magic_len = 4
    return len(data) >= punchboot_magic_len and data[0:4] == b"2APB"
