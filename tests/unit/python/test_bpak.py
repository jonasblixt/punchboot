"""The host's bpak detection and part arithmetic in punchboot.bpak, without a device.

The host writes and verifies a file as bpak when it starts with a valid header
(command-mode.md 5.3), so what has_header() takes decides how a file is
written. The golden images (tests/compat/bpak) were made by released bpak tools.
"""

from __future__ import annotations

import pathlib

import pytest
from punchboot import bpak, helpers

GOLDEN = pathlib.Path(__file__).resolve().parents[2] / "compat" / "bpak"


def header(*parts: tuple[int, int, int]) -> bpak.bpak_header:
    """Return a valid header with parts of (id, size, pad_bytes)."""
    h = bpak.bpak_header(magic=bpak.BPAK_HEADER_MAGIC, hash_kind=1)
    for n, (part_id, size, pad) in enumerate(parts):
        h.parts[n].id = part_id
        h.parts[n].size = size
        h.parts[n].pad_bytes = pad
    return h


def test_header_size() -> None:
    assert bpak.BPAK_HEADER_SIZE == 4096


@pytest.mark.parametrize("img", sorted(GOLDEN.glob("*/*.bpak")), ids=lambda p: p.name)
def test_golden_images_have_a_header(img: pathlib.Path) -> None:
    data = img.read_bytes()

    assert bpak.has_header(data)
    assert helpers.valid_bpak_magic(data)


def test_valid_header() -> None:
    h = header((0xEC103B08, 1000, 24), (0x56F91B86, 512, 0))

    assert bpak.has_header(h.pack())
    assert bpak.has_header(h.pack() + b"payload")


def test_short_data_is_not_bpak() -> None:
    data = header().pack()

    assert not bpak.has_header(data[:-1])
    assert not bpak.has_header(b"")
    # The magic alone is enough for valid_bpak_magic()
    assert helpers.valid_bpak_magic(data[:4])
    assert not helpers.valid_bpak_magic(data[:3])


def test_magic() -> None:
    h = header()
    h.magic ^= 1

    assert not bpak.has_header(h.pack())
    assert not helpers.valid_bpak_magic(h.pack())


def test_hash_kind_is_needed() -> None:
    h = header()
    h.hash_kind = 0

    assert not bpak.has_header(h.pack())


def test_parts_are_512_byte_aligned() -> None:
    assert not bpak.has_header(header((1, 1000, 0)).pack())
    assert bpak.has_header(header((1, 1000, 24)).pack())


def test_metadata_within_its_area() -> None:
    h = header()
    h.meta[0].id = 1
    h.meta[0].offset = bpak.BPAK_METADATA_BYTES - 8
    h.meta[0].size = 8
    assert bpak.has_header(h.pack())

    h.meta[0].size = 9
    assert not bpak.has_header(h.pack())


def test_parts_end_at_the_first_unused() -> None:
    h = header((1, 512, 0), (2, 512, 0))
    h.parts[3].id = 4
    h.parts[3].size = 3  # Not aligned, and not looked at

    assert [p.id for p in bpak.parts(h)] == [1, 2]
    assert bpak.has_header(h.pack())


def test_part_size_and_offset() -> None:
    h = header((1, 1000, 24), (2, 512, 0), (3, 100, 412))
    h.parts[1].flags = bpak.BPAK_FLAG_TRANSPORT
    h.parts[1].transport_size = 300
    p = bpak.parts(h)

    assert [bpak.part_size(x) for x in p] == [1024, 300, 512]
    assert [bpak.part_offset(h, x) for x in p] == [4096, 4096 + 1024, 4096 + 1024 + 300]
