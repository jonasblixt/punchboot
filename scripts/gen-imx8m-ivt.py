#!/usr/bin/env python3
#
# Punch BOOT
#
# Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
#
# SPDX-License-Identifier: BSD-3-Clause
#
"""pb proper's part of the i.MX8M Plus image (D-088, plat-imx.md 6).

HABv4's IVT and boot data (imx-mkimage's imx_header_v2_t, 0x40 bytes),
pb.bin, padding and room for a CSF, so that the part is a multiple of
4 KiB, the largest page the ROM's API reads in. The IVT is at
entry - 0x40 and loaded from there; its CSF pointer is the room's start.
"""

import argparse
import struct
import sys
from pathlib import Path

HEADER_SIZE = 0x40
PAGE = 0x1000
IVT_HEADER = 0x412000D1  # Tag 0xd1, length 0x20 (big endian), version 0x41


def main() -> int:
    """Write the part."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("entry", type=lambda v: int(v, 0), help="pb proper's entry")
    parser.add_argument("csf_room", type=lambda v: int(v, 0), help="bytes for the CSF")
    parser.add_argument("image", type=Path, help="pb.bin")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    image = args.image.read_bytes()
    self_addr = args.entry - HEADER_SIZE
    body = HEADER_SIZE + len(image)
    total = -(-(body + args.csf_room) // PAGE) * PAGE
    csf = self_addr + total - args.csf_room

    ivt = struct.pack(
        "<8I",
        IVT_HEADER,
        args.entry,
        0,
        0,  # No DCD
        self_addr + 0x20,  # The boot data, after the IVT
        self_addr,
        csf,
        0,
    )
    boot_data = struct.pack("<3I", self_addr, total, 0)
    header = (ivt + boot_data).ljust(HEADER_SIZE, b"\0")
    part = (header + image).ljust(total, b"\0")

    if len(part) != total or csf < self_addr + body:
        sys.stderr.write("gen-imx8m-ivt.py: the part doesn't add up\n")
        return 1

    args.output.write_bytes(part)
    return 0


if __name__ == "__main__":
    sys.exit(main())
