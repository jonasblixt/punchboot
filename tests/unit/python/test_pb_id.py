"""pb_id(), the ids of names (bpak ids, keys, logs, commands), against zlib.

pb's and bpak's C code compute a CRC32 without its inversions over the
name's bytes (src/log_source.c, bpak's id.h). zlib.crc32() inverts at the
start and at the end; with both inversions undone it gives the same id for
every name in use, and for any ASCII name. A name that isn't ASCII is its
UTF-8 bytes, as the device's C string and the bpak CLI's argument have it
(F-430): pb_id() took each character's code point modulo 256.
"""

from __future__ import annotations

import pathlib
import re
import zlib

import punchboot
import pytest

REPO = pathlib.Path(__file__).resolve().parents[3]


def zlib_id(name: str) -> int:
    """CRC32 without the inversions, with zlib."""
    return zlib.crc32(name.encode("latin-1"), 0xFFFFFFFF) ^ 0xFFFFFFFF


def names_in_use() -> set[str]:
    """Return the names the tree gives an id: bpak ids, logs, commands, the abi's."""
    names = {"pb", "list", "kernel", "dt", "ramdisk", "atf", "tee", "fdt", "pb-load-addr"}
    patterns = (
        r'(?:bpak_id|pb_id)\("([^"]+)"\)',
        r'DEFINE_COMMAND\(\s*"([^"]+)"',
        r'\.name = "([a-z0-9-]+)"',
    )
    for path in [*REPO.glob("src/**/*.[ch]"), *REPO.glob("include/**/*.h")]:
        text = path.read_text(errors="replace")
        for pattern in patterns:
            names.update(re.findall(pattern, text))
    abi = (REPO / "tests" / "compat" / "abi.txt").read_text()
    for name in re.findall(r"^id BPAK_ID_(\w+) ", abi, re.MULTILINE):
        names.add(name.lower().replace("_", "-"))
    return names


def test_known_ids() -> None:
    # Values the device has: abi.txt's and the test board's command
    assert punchboot.pb_id("pb") == 0x8C837982
    assert punchboot.pb_id("list") == 0x658C2704
    assert punchboot.pb_id("test-command") == 0xC72B6E9E
    assert punchboot.pb_id("kernel") == 0xEC103B08
    assert punchboot.pb_id("pb-load-addr") == 0xD1E64A4B
    assert punchboot.pb_id("") == 0


def test_names_in_use_have_the_same_id_with_zlib() -> None:
    names = names_in_use()

    assert len(names) > 20
    for name in sorted(names):
        assert punchboot.pb_id(name) == zlib_id(name), name


def test_zlib_crc32_as_it_is_differs() -> None:
    for name in ("pb", "list", "kernel"):
        assert zlib.crc32(name.encode()) != punchboot.pb_id(name), name


@pytest.mark.parametrize("length", [1, 2, 7, 64])
def test_every_ascii_name(length: int) -> None:
    for first in range(128):
        name = "".join(chr((first + 31 * n) % 128) for n in range(length))
        assert punchboot.pb_id(name) == zlib_id(name), repr(name)


def device_id(data: bytes) -> int:
    """Compute the device's pb_id() of a C string's bytes, bit by bit."""
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


@pytest.mark.parametrize("name", ["k€y", "nyckel-å", "é"])
def test_not_ascii_is_its_utf8_bytes(name: str) -> None:
    # F-430: the device's C string, and the bpak CLI's argument, is the
    # name's UTF-8 bytes. pb_id() took each code point modulo 256 ("k€y"
    # had the id of the bytes 6b ac 79)
    assert punchboot.pb_id(name) == device_id(name.encode())
    assert punchboot.pb_id(name) != device_id(bytes(ord(ch) & 0xFF for ch in name))


def test_device_id_is_pb_id_for_ascii() -> None:
    for name in ("pb", "list", "kernel", "test-command", "pb-load-addr"):
        assert device_id(name.encode()) == punchboot.pb_id(name), name
