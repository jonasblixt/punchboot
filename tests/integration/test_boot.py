"""Booting bpak images, from RAM and from partitions.

Ported from test_boot_bpak{,2..10}, test_invalid_key_index,
test_overlapping_region, test_all_sig_formats and test_gpt_boot_activate{,_step2..5}.

A successful boot ends with the host board writing the name of the booted
partition ('A', 'B', or '?' when booting from RAM) and resetting.

Note: test_boot_bpak9 intended to check that an image larger than the
partition is rejected, but never wrote it. That is covered by
test_write_image_larger_than_partition_fails. The test_gpt_boot_activate
steps always ran in command mode and never checked a boot, the activation
tests here do.
"""

from __future__ import annotations

import os
from typing import TYPE_CHECKING

import punchboot
import pytest

from .pbtest import NULL_UUID, SYSTEM_A_UUID, SYSTEM_B_UUID, make_bpak

if TYPE_CHECKING:
    import pathlib
    import uuid

    from .pbtest import Device


def boot_partition(device: Device, part: uuid.UUID) -> str | None:
    """Boot a partition over command mode, returns the boot status."""
    with device.session() as s:
        s.boot_partition(part)
    assert device.wait_exit() == 0
    return device.boot_status


def boot_ram(device: Device, img: pathlib.Path) -> str | None:
    """Load an image to RAM over command mode and run it, returns the boot status."""
    with device.session() as s:
        s.boot_bpak(img, NULL_UUID)
    assert device.wait_exit() == 0
    return device.boot_status


def test_boot_from_ram(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(1023 * 64))

    with device.session() as s:
        s.boot_set_boot_part(None)

    assert boot_ram(device, img) == "?"


@pytest.mark.parametrize(
    ("part", "name"), [(SYSTEM_A_UUID, "A"), (SYSTEM_B_UUID, "B")], ids=["A", "B"]
)
def test_boot_partition(
    device: Device, tmp_path: pathlib.Path, part: uuid.UUID, name: str
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(128 * 1024))

    with device.session() as s:
        s.part_write(img, part)

    assert boot_partition(device, part) == name


def test_write_image_larger_than_partition_fails(device: Device, tmp_path: pathlib.Path) -> None:
    small = make_bpak(tmp_path / "small.bpak", os.urandom(128 * 1024))
    large = make_bpak(tmp_path / "large.bpak", os.urandom(1024 * 1024))

    with device.session() as s:
        s.part_write(small, SYSTEM_B_UUID)
        with pytest.raises(punchboot.ArgumentError):
            s.part_write(large, SYSTEM_A_UUID)

    # System B is still intact
    assert boot_partition(device, SYSTEM_B_UUID) == "B"


# punchboot itself occupies the start of RAM, 0x40000000 - 0x40120000 on the
# host board (see CMakeLists.txt)
@pytest.mark.parametrize(
    ("load_addr", "size"),
    [
        pytest.param(0x3FFFFF00, 508 * 1024, id="below-and-into-pb"),
        pytest.param(0x40011000, 500 * 1024, id="pb-code-1"),
        pytest.param(0x40013000, 256 * 1024, id="pb-code-2"),
        pytest.param(0x4001B6A8, 500 * 1024, id="pb-code-3"),
        pytest.param(0x40050000, 500 * 1024, id="pb-code-4"),
    ],
)
def test_image_overlapping_punchboot_is_rejected(
    device: Device, tmp_path: pathlib.Path, load_addr: int, size: int
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(size), load_addr=load_addr)

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        with pytest.raises(punchboot.Error):
            s.boot_partition(SYSTEM_A_UUID)
        with pytest.raises(punchboot.Error):
            s.boot_bpak(img, NULL_UUID)

    assert device.running
    assert device.boot_status is None


def test_image_signed_with_wrong_key_is_rejected(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(
        tmp_path / "img.bpak", os.urandom(64 * 1024), sign_key="pki/secp256r1-key-pair2.pem"
    )

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        with pytest.raises(punchboot.Error):
            s.boot_partition(SYSTEM_A_UUID)

    assert device.boot_status is None


def test_image_with_unknown_key_id_is_rejected(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(64 * 1024), key_id="invalid-key-id")

    with device.session() as s, pytest.raises(punchboot.Error):
        s.boot_bpak(img, NULL_UUID)

    assert device.boot_status is None


@pytest.mark.parametrize(
    ("hash_kind", "signature_kind", "key_id", "key"),
    [
        ("sha256", "prime256v1", "pb-development", "pki/secp256r1-key-pair.pem"),
        ("sha384", "secp384r1", "pb-development2", "pki/secp384r1-key-pair.pem"),
        ("sha512", "secp521r1", "pb-development3", "pki/secp521r1-key-pair.pem"),
    ],
    ids=["secp256r1", "secp384r1", "secp521r1"],
)
def test_signature_formats(
    device: Device,
    tmp_path: pathlib.Path,
    hash_kind: str,
    signature_kind: str,
    key_id: str,
    key: str,
) -> None:
    img = make_bpak(
        tmp_path / "img.bpak",
        os.urandom(64 * 1024),
        hash_kind=hash_kind,
        signature_kind=signature_kind,
        key_id=key_id,
        sign_key=key,
    )

    assert boot_ram(device, img) == "?"


def test_activated_partition_boots(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_B_UUID)
        s.boot_set_boot_part(SYSTEM_B_UUID)
    device.reset()

    assert device.boot() == "B"


def test_activated_partition_with_invalid_image_enters_command_mode(
    device: Device, tmp_path: pathlib.Path
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(500 * 1024), key_id="invalid-key-id")

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()

    device.start(command_mode=False)
    device.wait_command_mode()
    assert device.boot_status is None


def test_disabled_boot_enters_command_mode(device: Device, tmp_path: pathlib.Path) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
        s.boot_set_boot_part(None)
    device.reset()

    device.start(command_mode=False)
    device.wait_command_mode()
    assert device.boot_status is None
