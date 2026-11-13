"""pb's information in the device tree the image handover prepared (boot.md 5.5, 5.6).

The device runs with --dt: its handover names the images' device tree part,
and a boot writes the tree it handed over, which the tests read as the next
stage would (Device.chosen). The properties every release wrote keep their
bytes (boot.md 7): test_image.c pins them, these read the same end to end.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import punchboot
import pytest

from . import fdt
from .pbtest import (
    AUTH_TOKEN,
    DEV_KEY_ID,
    DT,
    NULL_UUID,
    SYSTEM_A_UUID,
    SYSTEM_B_UUID,
    make_bpak,
)

if TYPE_CHECKING:
    import pathlib

    from .pbtest import Device

DEVICE_UUID = "02e49231-756e-35ee-a982-378e5ba866a9"
OTHER_KEY_ID = 0x25C6DD36  # pb-development2


@pytest.fixture
def img(tmp_path: pathlib.Path) -> pathlib.Path:
    """Make an image with a kernel and a device tree part, signed with the development key."""
    return make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024), dt=DT)


def boot_a(device: Device, img: pathlib.Path) -> dict[str, bytes]:
    """Write 'img' to System A, activate it, boot, and return /chosen."""
    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()
    assert device.boot() == "A"
    chosen = device.chosen
    assert chosen is not None
    return chosen


def test_pb_s_information(dt_device: Device, img: pathlib.Path) -> None:
    with dt_device.session() as s:
        version = s.device_get_punchboot_version()

    chosen = boot_a(dt_device, img)

    # Released: the strings and cells as every release wrote them
    assert chosen["pb,device-uuid"] == DEVICE_UUID.encode() + b"\0"
    assert chosen["pb,slc"] == int(punchboot.SLC.SLC_CONFIGURATION.value).to_bytes(4, "big")
    assert chosen["pb,slc-active-key"] == DEV_KEY_ID.to_bytes(4, "big")
    assert DEV_KEY_ID in fdt.u32_list(chosen["pb,slc-available-keys"])
    assert chosen["pb,active-system"] == b"A\0"
    # New in 3.0.0
    assert chosen["pb,boot-partition"] == str(SYSTEM_A_UUID).encode() + b"\0"
    assert chosen["pb,boot-mode"] == b"normal\0"
    assert fdt.string(chosen["pb,version"]) == version


def test_the_board_s_hook_runs_first(dt_device: Device, img: pathlib.Path) -> None:
    # boot.md 5.5: the board's hook, then pb's properties. The host's hook
    # writes bootargs, which is the board's, and a pb,version, which pb's
    # own replaces
    chosen = boot_a(dt_device, img)

    assert chosen["bootargs"] == b"console=pb-host\0"
    assert fdt.string(chosen["pb,version"]) != "the board's"


def test_a_revoked_key_is_not_available(dt_device: Device, img: pathlib.Path) -> None:
    before = fdt.u32_list(boot_a(dt_device, img)["pb,slc-available-keys"])
    assert OTHER_KEY_ID in before

    dt_device.start()
    with dt_device.session() as s:
        s.authenticate_dsa_token(AUTH_TOKEN, DEV_KEY_ID)
        s.slc_revoke_key(OTHER_KEY_ID)

    # The others, in the keystore's order
    after = fdt.u32_list(boot_a(dt_device, img)["pb,slc-available-keys"])
    assert after == [key for key in before if key != OTHER_KEY_ID]


def test_the_life_cycle_of_a_locked_device(dt_device: Device, img: pathlib.Path) -> None:
    with dt_device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
        s.slc_set_configuration()
        s.slc_set_configuration_lock()
    dt_device.reset()

    assert dt_device.boot() == "A"
    chosen = dt_device.chosen
    assert chosen is not None
    locked = int(punchboot.SLC.SLC_CONFIGURATION_LOCKED.value)
    assert chosen["pb,slc"] == locked.to_bytes(4, "big")


def test_a_boot_from_ram_names_no_partition(dt_device: Device, img: pathlib.Path) -> None:
    # boot.md 5.6: 'boot bpak' without a UUID has no pb,boot-partition, and
    # the system is "?"; with one, the UUID is taken as given
    with dt_device.session() as s:
        s.boot_bpak(img, NULL_UUID)
    assert dt_device.wait_exit() == 0
    chosen = dt_device.chosen
    assert chosen is not None
    assert "pb,boot-partition" not in chosen
    assert chosen["pb,active-system"] == b"?\0"
    assert chosen["pb,boot-mode"] == b"normal\0"

    dt_device.start()
    with dt_device.session() as s:
        s.boot_bpak(img, SYSTEM_B_UUID)
    assert dt_device.wait_exit() == 0
    chosen = dt_device.chosen
    assert chosen is not None
    assert chosen["pb,boot-partition"] == str(SYSTEM_B_UUID).encode() + b"\0"
    assert chosen["pb,active-system"] == b"B\0"


def test_an_image_without_the_part_does_not_boot(
    dt_device: Device, tmp_path: pathlib.Path
) -> None:
    # The board names a device tree part: an image without one fails its
    # prepare, and pb stays in command mode with nothing handed over
    img = make_bpak(tmp_path / "no-dt.bpak", os.urandom(16 * 1024))
    with dt_device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        with pytest.raises(punchboot.Error):
            s.boot_partition(SYSTEM_A_UUID)
    assert dt_device.running
    assert dt_device.chosen is None
