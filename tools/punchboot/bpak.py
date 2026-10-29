"""The parts of the bpak header the host needs, a mirror of include/bpak/bpak.h.

The bpak format is frozen (D-008): test_compat.py checks the layout here
against tests/compat/abi.txt. Signatures and hashes are the device's job.
"""

# The struct classes are named and laid out like the C structs they mirror
# ruff: noqa: D101, N801

from __future__ import annotations

from .wire import Struct, u8, u16, u32, u64

BPAK_HEADER_MAGIC = 0x42504132
BPAK_MAX_PARTS = 32
BPAK_MAX_META = 32
BPAK_METADATA_BYTES = 1920
BPAK_SIGNATURE_MAX_BYTES = 512
BPAK_PART_ALIGN = 512
BPAK_FLAG_TRANSPORT = 1 << 1


class bpak_meta_header(Struct):
    _fields_ = (
        ("id", u32),
        ("size", u16),
        ("offset", u16),
        ("part_id_ref", u32),
        ("pad", u8 * 4),
    )


class bpak_part_header(Struct):
    _fields_ = (
        ("id", u32),
        ("size", u64),
        ("offset", u64),
        ("transport_size", u64),
        ("pad_bytes", u16),
        ("flags", u8),
        ("pad", u8),
    )


class bpak_header(Struct):
    _fields_ = (
        ("magic", u32),
        ("pad0", u8 * 4),
        ("meta", bpak_meta_header * BPAK_MAX_META),
        ("parts", bpak_part_header * BPAK_MAX_PARTS),
        ("metadata", u8 * BPAK_METADATA_BYTES),
        ("hash_kind", u8),
        ("signature_kind", u8),
        ("alignment", u16),
        ("payload_hash", u8 * 64),
        ("key_id", u32),
        ("keystore_id", u32),
        ("pad1", u8 * 42),
        ("signature", u8 * BPAK_SIGNATURE_MAX_BYTES),
        ("signature_sz", u16),
    )


BPAK_HEADER_SIZE = bpak_header.sizeof()

STRUCTS: tuple[type[Struct], ...] = (bpak_meta_header, bpak_part_header, bpak_header)


def parts(header: bpak_header) -> list[bpak_part_header]:
    """Return the parts in use, up to the first unused slot (bpak_foreach_part)."""
    used = []
    for p in header.parts:
        if not p.id:
            break
        used.append(p)
    return used


def valid_header(header: bpak_header) -> bool:
    """Check the header like bpak_valid_header() does."""
    if header.magic != BPAK_HEADER_MAGIC:
        return False
    if any((p.size + p.pad_bytes) % BPAK_PART_ALIGN for p in parts(header)):
        return False
    for m in header.meta:
        if not m.id:
            break
        if m.size + m.offset > BPAK_METADATA_BYTES:
            return False
    return bool(header.hash_kind)


def part_size(part: bpak_part_header) -> int:
    """Return the size of a part in the file (bpak_part_size)."""
    if part.flags & BPAK_FLAG_TRANSPORT:
        return int(part.transport_size)
    return int(part.size + part.pad_bytes)


def part_offset(header: bpak_header, part: bpak_part_header) -> int:
    """Return the offset of a part in the file (bpak_part_offset)."""
    offset = BPAK_HEADER_SIZE
    for p in parts(header):
        if p.id == part.id:
            break
        offset += part_size(p)
    return offset
