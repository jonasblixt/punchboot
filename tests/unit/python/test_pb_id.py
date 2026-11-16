"""pb_id(), the ids of names (bpak ids, keys, logs, commands), against zlib.

helpers.py has its own CRC32 table. pb's and bpak's C code compute the same
CRC32 without its inversions (src/log_source.c, bpak's id.h). F-035 asks
whether zlib.crc32() gives the same ids: not as it is, it inverts at the start
and at the end. With both inversions undone it gives the same id for every
name in use, and for any ASCII name. For text outside Latin-1 it doesn't:
pb_id() takes each character's code point modulo 256, zlib takes bytes.
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


def test_outside_latin1_differs_from_utf8() -> None:
    # The character's code point modulo 256, not its UTF-8 bytes
    name = "k€y"
    assert punchboot.pb_id(name) != zlib.crc32(name.encode(), 0xFFFFFFFF) ^ 0xFFFFFFFF
    assert punchboot.pb_id(name) == zlib_id("k\xacy")
