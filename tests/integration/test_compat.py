"""Backward compatibility of the wire protocol and the bpak format (D-008).

See doc/dev/compatibility.md. The golden images in tests/compat/bpak were made
once by a released bpak CLI (tests/compat/bpak/generate.sh) and must keep
booting. The Python mirrors of wire values and the bpak id function are checked
against tests/compat/abi.txt, which tests/compat/check_abi.py keeps in sync with
the C headers.
"""

from __future__ import annotations

import hashlib
import re
from typing import TYPE_CHECKING

import punchboot
import pytest
from punchboot import bpak, wire

from . import pbtest
from .pbtest import NULL_UUID, REPO_ROOT, SYSTEM_A_UUID

if TYPE_CHECKING:
    import pathlib

    from _pytest.mark.structures import ParameterSet

    from .pbtest import Device

COMPAT = REPO_ROOT / "tests" / "compat"
GOLDEN = COMPAT / "bpak"
IMAGES = sorted(GOLDEN.glob("v*/*.bpak"))

# Golden images that a known bug keeps from booting (D-007)
KNOWN_BUGS = {
    "dont-hash.bpak": "F-076: punchboot hashes parts that have BPAK_FLAG_EXCLUDE_FROM_HASH",
}

# ids that are stored in images and keystores, or hardcoded in boards
NAMED_IDS = {
    "kernel": 0xEC103B08,
    "pb-development": 0xA90F9680,
    "pb-development2": 0x25C6DD36,
    "pb-development3": 0x52C1EDA0,
    "optee-arch": 0x99E5E80A,
    "pb": 0x8C837982,
}


def abi() -> dict[str, int]:
    """Return the frozen values in abi.txt, by name."""
    text = (COMPAT / "abi.txt").read_text()
    pairs = re.findall(r"^(?:value|id) (\S+) (\S+)$", text, re.MULTILINE)
    return {name: int(value, 0) for name, value in pairs}


def golden(img: pathlib.Path) -> ParameterSet:
    """Return a pytest param for a golden image, xfail if a known bug breaks it."""
    marks = []
    if img.name in KNOWN_BUGS:
        marks.append(pytest.mark.xfail(strict=True, reason=KNOWN_BUGS[img.name]))
    return pytest.param(img, id=f"{img.parent.name}/{img.name}", marks=marks)


def test_golden_images_are_unchanged() -> None:
    sums = {}
    for line in (GOLDEN / "SHA256SUMS").read_text().splitlines():
        digest, name = line.split()
        sums[name] = digest

    assert sorted(sums) == sorted(str(img.relative_to(GOLDEN)) for img in IMAGES)
    for name, digest in sums.items():
        assert hashlib.sha256((GOLDEN / name).read_bytes()).hexdigest() == digest, name


@pytest.mark.parametrize("img", [golden(img) for img in IMAGES])
def test_golden_image_boots_from_ram(device: Device, img: pathlib.Path) -> None:
    with device.session() as s:
        s.boot_bpak(img, NULL_UUID)
    assert device.wait_exit() == 0

    assert device.boot_status == "?"


@pytest.mark.parametrize("img", [golden(img) for img in IMAGES])
def test_golden_image_boots_from_partition(device: Device, img: pathlib.Path) -> None:
    with device.session() as s:
        s.part_write(img, SYSTEM_A_UUID)
        s.boot_set_boot_part(SYSTEM_A_UUID)
    device.reset()

    assert device.boot() == "A"


def test_bpak_ids() -> None:
    values = abi()
    ids = {name: value for name, value in values.items() if name.startswith("BPAK_ID_")}

    assert ids
    for name, value in ids.items():
        string = name.removeprefix("BPAK_ID_").lower().replace("_", "-")
        assert punchboot.pb_id(string) == value, name
    for name, value in NAMED_IDS.items():
        assert punchboot.pb_id(name) == value, name


def test_python_mirrors_match_the_wire() -> None:
    values = abi()

    for slc in punchboot.SLC:
        assert slc.value == values[f"PB_{slc.name}"], slc
    flags = punchboot.PartitionFlags
    assert flags.FLAG_BOOTABLE.value == values["BIO_FLAG_BOOTABLE"]
    assert flags.FLAG_OTP.value == values["PB_WIRE_PART_FLAG_OTP"]
    assert flags.FLAG_WRITABLE.value == values["BIO_FLAG_WRITABLE"]
    assert flags.FLAG_ERASE_BEFORE_WRITE.value == values["BIO_FLAG_ERASE_BEFORE_WRITE"]
    assert flags.FLAG_READABLE.value == values["BIO_FLAG_READABLE"]

    # The raw wire constants that the tests use
    for name in dir(pbtest):
        if name.startswith(("PB_CMD_", "PB_WIRE_MAGIC")):
            assert getattr(pbtest, name) == values[name], name


def abi_layout() -> tuple[dict[str, int], dict[str, tuple[int, int]]]:
    """Return the frozen struct sizes, and the field offsets and sizes, in abi.txt."""
    text = (COMPAT / "abi.txt").read_text()
    sizes = {n: int(s) for n, s in re.findall(r"^struct (\S+) (\d+)$", text, re.MULTILINE)}
    found = re.findall(r"^field (\S+) (\d+) (\d+)$", text, re.MULTILINE)
    fields = {n: (int(o), int(s)) for n, o, s in found}
    return sizes, fields


def test_host_wire_and_bpak_match_the_abi() -> None:
    # The host's wire protocol (punchboot/wire.py) and the bpak header it
    # parses (punchboot/bpak.py) mirror the C headers
    sizes, fields = abi_layout()
    structs = {s.__name__: s for s in (*wire.STRUCTS, *bpak.STRUCTS)}

    # Every frozen wire struct is mirrored, so a new one must be added too
    assert {n for n in sizes if n.startswith("pb_")} <= set(structs)
    for name, struct in structs.items():
        assert struct.sizeof() == sizes[name], name
        for field, (offset, size) in fields.items():
            struct_name, _, field_name = field.partition(".")
            if struct_name == name:
                descriptor = getattr(struct, field_name)
                assert (descriptor.offset, descriptor.size) == (offset, size), field

    values = abi()
    enums = {
        wire.Command: "PB_CMD_",
        wire.Result: "PB_RESULT_",
        wire.AuthMethod: "PB_AUTH_",
        wire.Slc: "PB_SLC_",
    }
    for enum, prefix in enums.items():
        for member in enum:
            assert member.value == values[member.name], member
        # Every frozen value is mirrored, so a new one must be added too
        frozen = {n for n in values if n.startswith(prefix)} - {"PB_RESULT_RESPONSE_MAX_SIZE"}
        assert frozen <= set(enum.__members__), enum
    for name in dir(wire):
        if name.startswith(("PB_WIRE_", "PB_COMMAND_", "PB_RESULT_RESPONSE")):
            assert getattr(wire, name) == values[name], name
    for name in dir(bpak):
        if name.startswith(("BPAK_MAX_", "BPAK_HEADER_MAGIC", "BPAK_PART_ALIGN", "BPAK_FLAG_",
                            "BPAK_METADATA_BYTES", "BPAK_SIGNATURE_MAX_BYTES")):
            assert getattr(bpak, name) == values[name], name
