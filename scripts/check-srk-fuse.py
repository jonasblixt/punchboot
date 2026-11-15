#!/usr/bin/env python3
"""Check that the SRK hash pb fuses is the hash of the SRK table it is signed with (F-213).

    check-srk-fuse.py hab|ahab <SRK table> [<fuse file>]

CONFIG_SRK_FUSE (srktool's *_fuse.bin, which slc_set_configuration() fuses)
and the platform's SRK table (which cst signs pb with) are two settings. A
fuse file of other keys would close the device to its own images, so the
signing step runs this first. Without a fuse file there is nothing to check.

The hashes are srktool's:
- HABv4 (i.MX6UL, i.MX8M): SHA-256 over the SHA-256 of each key record of the
  table, the records after the table's 4 byte header.
- AHAB (i.MX8X): the SHA-512 of the whole table; SHA-256 for a fuse of 32
  bytes.
"""

import hashlib
import pathlib
import struct
import sys

HAB_TABLE_TAG = 0xD7
HAB_KEY_TAG = 0xE1


def hab_hash(table: bytes) -> bytes:
    """Return srktool's HABv4 SRK hash of 'table'."""
    if len(table) < 4 or table[0] != HAB_TABLE_TAG:
        msg = "not a HABv4 SRK table"
        raise ValueError(msg)

    (length,) = struct.unpack(">H", table[1:3])

    if length != len(table):
        msg = f"the header says {length} bytes, the file has {len(table)}"
        raise ValueError(msg)

    digests = b""
    offset = 4

    while offset < len(table):
        (record,) = struct.unpack(">H", table[offset + 1 : offset + 3])

        if table[offset] != HAB_KEY_TAG or record < 4 or offset + record > len(table):
            msg = f"no key record at byte {offset}"
            raise ValueError(msg)

        digests += hashlib.sha256(table[offset : offset + record]).digest()
        offset += record

    return hashlib.sha256(digests).digest()


def ahab_hash(table: bytes, size: int) -> bytes:
    """Return srktool's AHAB SRK hash of 'table', for a fuse of 'size' bytes."""
    if size == hashlib.sha256().digest_size:
        return hashlib.sha256(table).digest()

    return hashlib.sha512(table).digest()


def main() -> int:
    """Compare the fuse file with the table's hash; 1 when they differ."""
    if len(sys.argv) not in (3, 4) or sys.argv[1] not in ("hab", "ahab"):
        sys.stderr.write(__doc__ or "")
        return 2

    scheme, table_path = sys.argv[1], sys.argv[2]
    fuse_path = sys.argv[3] if len(sys.argv) == 4 else ""

    if not fuse_path:
        return 0

    table = pathlib.Path(table_path).read_bytes()
    fuse = pathlib.Path(fuse_path).read_bytes()

    try:
        digest = hab_hash(table) if scheme == "hab" else ahab_hash(table, len(fuse))
    except ValueError as e:
        sys.stderr.write(f"{table_path}: {e}\n")
        return 1

    if digest != fuse:
        sys.stderr.write(
            f"CONFIG_SRK_FUSE ({fuse_path}) is not the hash of the SRK table pb is signed with\n"
            f"({table_path}): a device fused with it would refuse its own images (F-213)\n",
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
