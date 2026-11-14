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


def test_payload_that_differs_from_its_hash_has_its_own_result(
    device: Device, tmp_path: pathlib.Path
) -> None:
    """A payload whose hash differs is PB_RESULT_BAD_PAYLOAD (command-mode.md 4.5, F-087).

    A release's device answered PB_RESULT_ERROR, "Generic error" for the
    host. The host's exception is a GenericError, as that was.
    """
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))
    changed = bytearray(img.read_bytes())
    changed[4096 + 100] ^= 1  # In the part, after the header
    bad = tmp_path / "bad.bpak"
    bad.write_bytes(changed)

    with device.session() as s:
        s.part_write(bad, SYSTEM_A_UUID)
        with pytest.raises(punchboot.BadPayloadError):
            s.boot_partition(SYSTEM_A_UUID)
        # The last result of the boot from the host
        with pytest.raises(punchboot.BadPayloadError):
            s.boot_bpak(bad, NULL_UUID)
        with pytest.raises(punchboot.GenericError):
            s.boot_bpak(bad, NULL_UUID)
        # Command mode goes on, and the image as it was signed boots
        assert s.device_get_boardname()
        assert device.boot_status is None

    assert boot_ram(device, img) == "?"


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


# The host board's image data region is its RAM above punchboot, 0x40120000 -
# 0x60000000 (src/board/host/board.c): the only memory a part is loaded into.
# Nothing is mapped outside the RAM, so a load there would end the process
@pytest.mark.parametrize(
    ("load_addr", "size"),
    [
        pytest.param(0x00000000, 16 * 1024, id="address-0"),
        pytest.param(0x30000000, 16 * 1024, id="below-the-ram"),
        pytest.param(0x40120000 - 512, 16 * 1024, id="starts-below-the-region"),
        pytest.param(0x60000000 - 512, 16 * 1024, id="ends-above-the-region"),
        pytest.param(0x60000000, 16 * 1024, id="above-the-ram"),
        pytest.param(0x70000000, 16 * 1024, id="where-a-flash-is-mapped"),
        pytest.param(0x149000000, 16 * 1024, id="above-4-gib"),
        pytest.param(2**64 - 4096, 16 * 1024, id="wraps"),
    ],
)
def test_image_outside_the_image_regions_is_refused(
    device: Device, tmp_path: pathlib.Path, load_addr: int, size: int
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(size), load_addr=load_addr)

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        with pytest.raises(punchboot.MemError):
            s.boot_partition(SYSTEM_A_UUID)
        with pytest.raises(punchboot.MemError):
            s.boot_bpak(img, NULL_UUID)

    assert device.running
    assert device.boot_status is None


@pytest.mark.parametrize(
    ("load_addr", "size"),
    [
        pytest.param(0x40120000, 16 * 1024, id="first-byte"),
        pytest.param(0x60000000 - 16 * 1024, 16 * 1024, id="last-byte"),
    ],
)
def test_image_at_the_ends_of_the_image_region_boots(
    device: Device, tmp_path: pathlib.Path, load_addr: int, size: int
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(size), load_addr=load_addr)

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)

    assert boot_partition(device, SYSTEM_A_UUID) == "A"


@pytest.mark.parametrize(
    "load_addr",
    [
        pytest.param(0x49000001, id="byte-1"),
        pytest.param(0x49000008, id="byte-8"),
        pytest.param(0x4900003F, id="byte-63"),
    ],
)
def test_image_whose_load_address_is_off_a_cache_line_boots(
    device: Device, tmp_path: pathlib.Path, load_addr: int
) -> None:
    """A part that doesn't start on a cache line boots, never refused (D-066, D-008).

    The boot's load loop reads it into a buffer of its own and copies it to its
    address (drivers.md 4.3), from a partition and from the host's stream; the
    block device and the transport see only buffers that own their lines.
    """
    img = make_bpak(tmp_path / "img.bpak", os.urandom(200 * 1024 + 100), load_addr=load_addr)

    # The boot from the partition, as at power on
    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()
    device.start(command_mode=False)
    assert device.wait_exit() == 0
    assert device.boot_status == "A"

    # The host's stream
    device.start()
    assert boot_ram(device, img) == "?"


def test_image_with_empty_kernel_is_rejected(device: Device, tmp_path: pathlib.Path) -> None:
    """A signed image whose kernel part is empty isn't booted (F-292).

    Nothing is hashed for the part, so the image verifies with nothing of it
    at the entry. The handover refuses it.
    """
    img = make_bpak(tmp_path / "img.bpak", b"")

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        # From a partition the result is the boot's: PB_RESULT_ERROR
        with pytest.raises(punchboot.GenericError):
            s.boot_partition(SYSTEM_A_UUID)
        # From the host the last result means "loaded and verified", and the
        # boot fails after it: command mode stays as it was
        s.boot_bpak(img, NULL_UUID)
        assert s.device_get_boardname()

    assert device.running
    assert device.boot_status is None
    # Both were refused by the handover, not on the way to it
    assert device.log.read_text(errors="replace").count("The kernel's part has no entry") == 2


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
        # A digest shorter than the curve (F-058)
        ("sha256", "secp384r1", "pb-development2", "pki/secp384r1-key-pair.pem"),
        ("sha384", "secp521r1", "pb-development3", "pki/secp521r1-key-pair.pem"),
        # A digest longer than the curve
        ("sha512", "prime256v1", "pb-development", "pki/secp256r1-key-pair.pem"),
    ],
    ids=[
        "secp256r1",
        "secp384r1",
        "secp521r1",
        "sha256-secp384r1",
        "sha384-secp521r1",
        "sha512-secp256r1",
    ],
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


# A stop in a boot that command mode started (board.md 5.4, command-mode.md
# 2.3 and 5.5, F-252). The host board's command 'test-boot-stop' arms a stop
# of the next boot in early_boot() or late_boot(): with -PB_ERR_ABORT, a
# hook's stop, or with -PB_ERR_IO, a hook's failure.
EARLY_STOPS = [
    pytest.param("abort", punchboot.GenericError, id="abort"),
    pytest.param("fail", punchboot.IOError, id="fail"),
]


@pytest.mark.parametrize(("how", "error"), EARLY_STOPS)
def test_boot_part_stopped_in_early_boot_stays_in_command_mode(
    device: Device, tmp_path: pathlib.Path, how: str, error: type[punchboot.Error]
) -> None:
    # Before the disconnect: the host gets the error and command mode goes
    # on, in the same session. After a stop (-PB_ERR_ABORT) the device reset.
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.board_run_command("test-boot-stop", f"early:{how}".encode())
        with pytest.raises(error):
            s.boot_partition(SYSTEM_A_UUID)
        assert s.device_get_boardname()
        assert device.running
        assert device.boot_status is None
        # The stop was that boot's: the next one goes on
        s.boot_partition(SYSTEM_A_UUID)

    assert device.wait_exit() == 0
    assert device.boot_status == "A"


@pytest.mark.parametrize(("how", "error"), EARLY_STOPS)
def test_boot_bpak_stopped_in_early_boot_stays_in_command_mode(
    device: Device, tmp_path: pathlib.Path, how: str, error: type[punchboot.Error]
) -> None:
    # The boot is refused before the header is read: the host sends the
    # header after the first OK, and the header's result carries the refusal
    # (command-mode.md 5.5). The session goes on.
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.board_run_command("test-boot-stop", f"early:{how}".encode())
        with pytest.raises(error):
            s.boot_bpak(img, NULL_UUID)
        assert s.device_get_boardname()
        assert device.running
        assert device.boot_status is None
        s.boot_bpak(img, NULL_UUID)

    assert device.wait_exit() == 0
    assert device.boot_status == "?"


@pytest.mark.parametrize("how", ["abort", "fail"])
@pytest.mark.parametrize("source", ["part", "bpak"])
def test_boot_stopped_in_late_boot_resets(
    device: Device, tmp_path: pathlib.Path, source: str, how: str
) -> None:
    # After the disconnect: the host has its last OK, "loaded and verified",
    # and pb resets, whatever the hook returned. After a failure (not
    # -PB_ERR_ABORT) the device went back to reading commands.
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.board_run_command("test-boot-stop", f"late:{how}".encode())
        if source == "part":
            s.boot_partition(SYSTEM_A_UUID)
        else:
            s.boot_bpak(img, NULL_UUID)

    assert device.wait_exit() == 0
    # The hook stopped before it reported a boot
    assert device.boot_status is None


# The boot's preconditions (security.md 4.5, platform.md 2.3, F-248): no
# silent boot. pb-host's --dram-filter-bypassed makes its platform report a
# TrustZone DRAM filter that is bypassed.
def test_boot_is_refused_with_a_bypassed_dram_filter(
    device: Device, tmp_path: pathlib.Path
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()

    # The image boots, with the platform as it is
    assert device.boot() == "A"

    # With the filter bypassed pb stops in command mode instead of booting
    device.extra_args = ["--dram-filter-bypassed"]
    device.start(command_mode=False)
    device.wait_command_mode()
    assert device.boot_status is None

    # A boot the host starts is refused the same way, before anything is
    # read: 'boot part' answers the error, and 'boot bpak' answers it as the
    # header's result (command-mode.md 5.5, D-058). The session goes on
    with device.session() as s:
        with pytest.raises(punchboot.GenericError):
            s.boot_partition(SYSTEM_A_UUID)
        with pytest.raises(punchboot.GenericError):
            s.boot_bpak(img, NULL_UUID)
        assert s.device_get_boardname()
        assert device.running
        assert device.boot_status is None

    # The reason is in the log
    run = device.log.read_text(errors="replace").rpartition("=== start:")[2]
    assert run.count("No boot: the TrustZone DRAM filter is bypassed") == 3


# The serial download stop (board.md 5.1, F-251). pb-host's --boot-source
# says where its platform reports pb was loaded from.
@pytest.mark.parametrize("source", ["usb", "uart"])
def test_serial_download_stops_in_command_mode(
    device: Device, tmp_path: pathlib.Path, source: str
) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()

    # A bootable system, and pb stops: the host that loaded pb wants it
    device.extra_args = ["--boot-source", source]
    device.start(command_mode=False)
    device.wait_command_mode()
    assert device.boot_status is None
    run = device.log.read_text(errors="replace").rpartition("=== start:")[2]
    assert "Loaded by the ROM's serial downloader" in run

    # A boot the host starts is never stopped by it
    with device.session() as s:
        s.boot_partition(SYSTEM_A_UUID)
    assert device.wait_exit() == 0
    assert device.boot_status == "A"


@pytest.mark.parametrize("source", ["storage", "unknown", "debugger"])
def test_other_boot_sources_boot(device: Device, tmp_path: pathlib.Path, source: str) -> None:
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))

    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()

    device.extra_args = ["--boot-source", source]
    assert device.boot() == "A"


def test_serial_download_stop_then_boot_from_ram(device: Device, tmp_path: pathlib.Path) -> None:
    # The factory's flow: pb loaded over USB, an image sent to RAM and run
    img = make_bpak(tmp_path / "img.bpak", os.urandom(16 * 1024))
    device.reset()

    device.extra_args = ["--boot-source", "usb"]
    device.start(command_mode=False)
    device.wait_command_mode()

    with device.session() as s:
        s.boot_bpak(img, NULL_UUID)
    assert device.wait_exit() == 0
    assert device.boot_status == "?"


def test_boot_stop_command_refuses_an_unknown_request(device: Device) -> None:
    with device.session() as s, pytest.raises(punchboot.ArgumentError):
        s.board_run_command("test-boot-stop", b"later")
