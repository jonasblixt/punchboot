"""A flattened device tree, as much of one as the tests need.

The host's test images carry a device tree part ('dt'), and pb-host writes
the tree its handover prepared to a file (pb-host --dt). build() makes the
part, properties() reads a node of what pb-host wrote. The format is the
Devicetree Specification's, chapter 5: a big endian header, the memory
reservation block, the structure block and the strings block.
"""

from __future__ import annotations

import struct
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterator

FDT_MAGIC = 0xD00DFEED
FDT_VERSION = 17
FDT_LAST_COMP_VERSION = 16
FDT_HEADER_SIZE = 40
FDT_BEGIN_NODE = 1
FDT_END_NODE = 2
FDT_PROP = 3
FDT_NOP = 4
FDT_END = 9


def _align4(data: bytes) -> bytes:
    return data + b"\0" * (-len(data) % 4)


def build(nodes: dict[str, dict[str, bytes]]) -> bytes:
    """Return a tree of the root ("/") and its children, each with its properties.

    'nodes' maps a node's path, "/" or "/name", to its properties.
    """
    strings = bytearray()
    offsets: dict[str, int] = {}

    def prop(name: str, value: bytes) -> bytes:
        if name not in offsets:
            offsets[name] = len(strings)
            strings.extend(name.encode() + b"\0")
        head = struct.pack(">III", FDT_PROP, len(value), offsets[name])
        return head + _align4(value)

    def node(name: str, props: dict[str, bytes]) -> bytes:
        body = struct.pack(">I", FDT_BEGIN_NODE) + _align4(name.encode() + b"\0")
        return body + b"".join(prop(k, v) for k, v in props.items())

    structure = node("", nodes.get("/", {}))
    for path, props in nodes.items():
        if path != "/":
            structure += node(path.lstrip("/"), props) + struct.pack(">I", FDT_END_NODE)
    structure += struct.pack(">II", FDT_END_NODE, FDT_END)

    rsvmap = bytes(16)  # The terminating entry only
    off_rsvmap = FDT_HEADER_SIZE
    off_struct = off_rsvmap + len(rsvmap)
    off_strings = off_struct + len(structure)
    total = off_strings + len(strings)
    header = struct.pack(
        ">10I",
        FDT_MAGIC,
        total,
        off_struct,
        off_strings,
        off_rsvmap,
        FDT_VERSION,
        FDT_LAST_COMP_VERSION,
        0,  # boot_cpuid_phys
        len(strings),
        len(structure),
    )
    return header + rsvmap + structure + bytes(strings)


def _walk(blob: bytes) -> Iterator[tuple[str, str | None, bytes]]:
    """Yield each node's path with None, and each property's node path, name and value."""
    magic, total, off_struct, off_strings = struct.unpack_from(">4I", blob)
    if magic != FDT_MAGIC or total > len(blob):
        msg = "Not a flattened device tree"
        raise ValueError(msg)

    def text(offset: int) -> str:
        return blob[offset : blob.index(b"\0", offset)].decode()

    names: list[str] = []
    pos = off_struct
    while True:
        (token,) = struct.unpack_from(">I", blob, pos)
        pos += 4
        if token == FDT_BEGIN_NODE:
            name = text(pos)
            pos += len(_align4(name.encode() + b"\0"))
            names.append(name)
            yield "/" + "/".join(names[1:]), None, b""
        elif token == FDT_END_NODE:
            names.pop()
        elif token == FDT_PROP:
            length, name_offset = struct.unpack_from(">II", blob, pos)
            value = blob[pos + 8 : pos + 8 + length]
            pos += 8 + len(_align4(value))
            yield "/" + "/".join(names[1:]), text(off_strings + name_offset), value
        elif token == FDT_END:
            return
        elif token != FDT_NOP:
            msg = f"Bad token {token:#x} at {pos - 4}"
            raise ValueError(msg)


def properties(blob: bytes, path: str) -> dict[str, bytes] | None:
    """Return the properties of the node at 'path' ("/chosen"), None when there is none."""
    found: dict[str, bytes] | None = None
    for node, name, value in _walk(blob):
        if node != path:
            continue
        if found is None:
            found = {}
        if name is not None:
            found[name] = value
    return found


def u32(value: bytes) -> int:
    """Return a property's one cell."""
    (number,) = struct.unpack(">I", value)
    return int(number)


def u32_list(value: bytes) -> list[int]:
    """Return a property's cells."""
    return list(struct.unpack(f">{len(value) // 4}I", value))


def string(value: bytes) -> str:
    """Return a property's string, which ends with a NUL."""
    if not value.endswith(b"\0"):
        msg = f"Not a string: {value!r}"
        raise ValueError(msg)
    return value[:-1].decode()
