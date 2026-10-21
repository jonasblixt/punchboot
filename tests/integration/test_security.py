"""Security life cycle, authentication and key revocation.

Ported from test_device_setup, test_authentication and test_revoke_key.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import AUTH_TOKEN, DEV_KEY_ID, NULL_UUID, SYSTEM_A_UUID, SYSTEM_B_UUID

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

DEVICE_UUID = "02e49231-756e-35ee-a982-378e5ba866a9"


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
