"""Security life cycle, authentication and key revocation.

Ported from test_device_setup, test_authentication and test_revoke_key.
"""

from __future__ import annotations

import struct
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import (
    AUTH_TOKEN,
    DEV_KEY_ID,
    NULL_UUID,
    PB_COMMAND_SIZE,
    PB_WIRE_MAGIC,
    SYSTEM_A_UUID,
    SYSTEM_B_UUID,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

DEVICE_UUID = "02e49231-756e-35ee-a982-378e5ba866a9"

# include/pb-tools/wire.h and error.h
PB_CMD_DEVICE_IDENTIFIER_READ = 2
PB_CMD_AUTHENTICATE = 16
PB_CMD_END = 30
PB_RESULT_NOT_AUTHENTICATED = 3
PB_RESULT_INVALID_COMMAND = 6


def test_device_identity(device: Device) -> None:
    with device.session() as s:
        assert str(s.device_get_uuid()) == DEVICE_UUID
        assert s.device_get_boardname() == "host test"
        assert str(s.device_get_punchboot_version())


def test_configure_device(device: Device) -> None:
    with device.session() as s:
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION
        s.slc_set_configuration()
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION


@pytest.fixture
def locked_device(device: Device) -> Device:
    """Device in the 'configuration locked' life cycle state."""
    with device.session() as s:
        s.slc_set_configuration()
        s.slc_set_configuration_lock()
        # Reading the life cycle requires authentication once locked
        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
        assert s.slc_get_lifecycle() == punchboot.SLC.SLC_CONFIGURATION_LOCKED
    device.reboot()
    return device


def test_locked_device_requires_authentication(
    locked_device: Device, tmp_path: pathlib.Path
) -> None:
    img = tmp_path / "img"
    img.write_bytes(b"\0" * 4096)

    with locked_device.session() as s:
        # The device identity is always available
        assert str(s.device_get_uuid()) == DEVICE_UUID

        not_authenticated = pytest.raises(punchboot.NotAuthenticatedError)
        with not_authenticated:
            s.part_get_partitions()
        with not_authenticated:
            s.device_reset()
        with not_authenticated:
            s.boot_partition(SYSTEM_A_UUID, verbose=True)
        with not_authenticated:
            s.boot_partition(SYSTEM_B_UUID, verbose=True)
        with not_authenticated:
            s.boot_set_boot_part(SYSTEM_A_UUID)
        with not_authenticated:
            s.boot_set_boot_part(SYSTEM_B_UUID)
        with not_authenticated:
            s.boot_bpak(img, NULL_UUID)

        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
        assert s.part_get_partitions()


def test_locked_device_refuses_every_frame(locked_device: Device) -> None:
    # command-mode.md 3.2 (F-275): before authentication a closed device
    # answers NOT_AUTHENTICATED to a number it doesn't define and to a frame
    # that isn't a command, so that it doesn't tell which commands it has.
    # After authentication they get what an open device answers.
    unknown = (0, PB_CMD_END, 0xFF)
    bad_magic = struct.pack("<IB3x", PB_WIRE_MAGIC ^ 0xFFFFFFFF, PB_CMD_DEVICE_IDENTIFIER_READ)
    bad_magic = bad_magic.ljust(PB_COMMAND_SIZE, b"\0")
    token = AUTH_TOKEN.read_bytes()

    with locked_device.wire() as w:
        for command in unknown:
            assert w.command(command) == -PB_RESULT_NOT_AUTHENTICATED
        w.send(bad_magic)
        assert w.result() == -PB_RESULT_NOT_AUTHENTICATED
        assert w.command(PB_CMD_DEVICE_IDENTIFIER_READ) == 0

        w.send_command(PB_CMD_AUTHENTICATE, struct.pack("<BHI", 1, len(token), DEV_KEY_ID))
        assert w.result() == 0
        w.send(token.ljust(1024, b"\0"))
        assert w.result() == 0

        for command in unknown:
            assert w.command(command) == -PB_RESULT_INVALID_COMMAND
        w.send(bad_magic)
        assert w.result() == -PB_RESULT_INVALID_COMMAND


def test_open_device_answers_invalid_command(device: Device) -> None:
    bad_magic = struct.pack("<IB3x", PB_WIRE_MAGIC ^ 0xFFFFFFFF, PB_CMD_DEVICE_IDENTIFIER_READ)

    with device.wire() as w:
        for command in (0, PB_CMD_END, 0xFF):
            assert w.command(command) == -PB_RESULT_INVALID_COMMAND
        w.send(bad_magic.ljust(PB_COMMAND_SIZE, b"\0"))
        assert w.result() == -PB_RESULT_INVALID_COMMAND


def test_authentication_lasts_until_reset(locked_device: Device) -> None:
    # Authentication survives reconnects, so that separate CLI invocations
    # can authenticate and then run commands, but not a reset.
    with locked_device.session() as s:
        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)

    with locked_device.session() as s:
        s.part_get_partitions()

    locked_device.reset()
    locked_device.start()

    with locked_device.session() as s, pytest.raises(punchboot.NotAuthenticatedError):
        s.part_get_partitions()


def test_authentication_with_wrong_key_fails(locked_device: Device) -> None:
    with locked_device.session() as s, pytest.raises(punchboot.AuthenticationError):
        s.authenticate_dsa_token(AUTH_TOKEN, "pb-development2")


def test_revoked_key_can_not_authenticate(device: Device) -> None:
    with device.session() as s:
        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
        s.slc_revoke_key(DEV_KEY_ID)
        assert DEV_KEY_ID in s.slc_get_revoked_keys()

        with pytest.raises(punchboot.KeyRevokedError):
            s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
