# Firmware Handoff: a brief

For the review of `boot.md` 5.4: its items 1 and 2 (closed since, D-039) and Open (6). A guide: it explains and decides
nothing. From the specification's text, version 2.0, read 2026-11-11 (sources at the end). Where
it says "the specification doesn't say", that was checked, not assumed.

## What it is

A specification from Arm, Linaro and the U-Boot community for how one boot stage passes
information to the next: a **transfer list** (TL) in memory, and a **register convention** that
tells the receiver where it is. It grew out of U-Boot's bloblist. TF-A, OP-TEE, Hafnium and
U-Boot use it. Version 2.0 is the current one; 0.9 and 1.0 are withdrawn (shipped products
computed the header checksum differently from 1.0's text).

## The transfer list

A header of 0x18 bytes, then entries, each 8-byte aligned. Everything is little-endian.

| Header field | Size | Value                                                                         |
| ------------ | ---- | ----------------------------------------------------------------------------- |
| `signature`  | 4    | 0x4a0fb10b                                                                    |
| `checksum`   | 1    | A byte sum modulo 256 over the used part; 0 when checksums are off. See below |
| `version`    | 1    | 2                                                                             |
| `hdr_size`   | 1    | 0x18                                                                          |
| `alignment`  | 1    | The largest alignment an entry needs, as a power of two; 3 for a new list     |
| `used_size`  | 4    | Header, entries and padding; a multiple of 8                                  |
| `total_size` | 4    | The whole area, with the spare room after `used_size`                         |
| `flags`      | 4    | Bit 0: the list uses checksums                                                |
| reserved     | 4    | 0                                                                             |

An entry is a 3-byte `tag_id`, a 1-byte `hdr_size` (8), a 4-byte `data_size`, then the data. The
next entry starts at the next 8-byte boundary.

- **The checksum's value isn't settled by the text.** The header table sets it so that all used
  bytes sum to 0 modulo 256. The procedures for creating and checking a list in the same
  document store the sum of the other bytes instead. 1.0 was withdrawn over a checksum that
  products computed differently, and U-Boot's list discussed a correction in late 2025. Follow
  the reference library, and test against a real receiver, before trusting either reading.
- The list's base is non-zero and 8-byte aligned.
- A receiver checks the signature and the version; the sizes and the checksum are optional
  checks. A version above its own may be read, not changed or moved; unknown tags are skipped.
- A receiver may move the list, add entries, change them and remove them. So nothing in the
  list may point into the list. Pointers to memory outside it are allowed and discouraged.
- An entry type should appear once.

## Tags

| Range                | Use                                                                        |
| -------------------- | -------------------------------------------------------------------------- |
| 0 to 0x7fffff        | Standard. Allocated in the specification before use                        |
| 0x800000 to 0xffefff | Reserved                                                                   |
| 0xfff000 to 0xffffff | Not standard, no allocation: "local experimentation or closed-source       |
|                      | components" under one integrator's control. Strongly discouraged otherwise |

Allocated today: 0 void (padding), 1 FDT, 2 and 3 HOB, 4 ACPI, 5 TPM event log, 6 TPM CRB base,
7 DT overlay, and 0x100 to 0x109 for Trusted Firmware (OP-TEE's pageable part, SPMC and FF-A
manifests, entry point info for AArch64 (0x102) and AArch32 (0x108), memory layouts, Mbed TLS
heap, GPT error info).

**Getting a tag:** a pull request to the specification's repository that adds the entry's data
layout to the text. Every maintainer has to acknowledge it (six: four at Arm, one at Linaro,
Simon Glass). Sign-off (DCO), CC-BY-SA-4.0. No ranges are reserved per project; projects are
encouraged to cluster their ids at a 16 or 256 boundary.

**What a layout must obey:** it never changes in a way old readers can't handle (otherwise a new
tag), it carries no version number of its own, and it has no pointers into the list.

## The registers

|                                                | AArch64                      | AArch32                    |
| ---------------------------------------------- | ---------------------------- | -------------------------- |
| The devicetree, or 0                           | x0                           | r2                         |
| The signature and the convention's version (1) | x1: 0x4a0fb10b in bits 31:0, | r1: 0x0fb10b in bits 23:0, |
|                                                | 1 in bits 39:32, the rest 0  | 1 in bits 31:24            |
| Reserved, 0                                    | x2                           | r0                         |
| The list's base                                | x3                           | r3                         |

The devicetree register and the list are tied together: when the list has an FDT entry (tag 1)
the register must point to the FDT in that entry, and the register is 0 when the list has none.
A DT handed over with this convention is therefore always the one inside the list.

## What the specification doesn't say

- **Zephyr.** Not in the specification, and Zephyr has no support for it: its issue and pull
  request searches for "firmware handoff" and "transfer list" find nothing on the subject
  (2026-11-11). A Zephyr application booted by pb reads the list itself.
- **Cortex-M.** The receiver is "in the AArch32 or AArch64 execution state". M-profile, RISC-V
  and others aren't mentioned. Using AArch32's registers on ARMv7-M is pb's own extension.
- **Changing the FDT in place.** No rule for the FDT entry beyond the general ones, and nothing
  on what happens to the register's pointer when a receiver moves the list.
- **A maximum size**, beyond the 32-bit fields.
- **Who allocates numbers**, beyond the pull request and the maintainers' acknowledgement.

## What it means for `boot.md`

- **Item 1, the list in a board region, the DT copied into it** (decided, D-039). The copy isn't a choice once
  the list is in a board region: the register may only point to a DT inside the list. What is
  open is where the list lives. The alternative is a list the image brings, with its DT and
  spare room, which pb verifies as a part and appends to. That saves the copy and makes the
  list's place and size the image's, not the board's.
- **Item 2, replacing `simple`** (decided, D-039). On Cortex-A it is the standard as written. On Cortex-M,
  where `simple` is used today (the RT1060), it is pb's extension of the standard, with the
  standard's list. The receiver's side is small: check r1, read the list at r3. There is a
  reference library (TF-Shared's transfer-list-library), and U-Boot's bloblist is another.
- **Open (6), pb's structure.** The draft's layout meets the rules: little-endian, no pointers,
  no version field, `data_size` tells a reader how many fields there are, so appending is the
  compatible change the specification asks for. The fields and the 16-key limit are confirmed
  (D-040), `status` is bit flags (D-041). To settle before the request: which id to ask for.
- **The tag request** (closed as Open (15): not released before the tag exists). It needs six
  acknowledgements, on the maintainers' time, and they may ask why a bootloader's own
  information needs a standard tag. Two fallbacks if it isn't there for 3.0.0: ship the DT
  variant only, which needs no tag, or hold the generic handover back. The non-standard range
  doesn't fit: pb is open source and used by more than one integrator.
- **The DT overlay entry (tag 7)** is not a way around patching `/chosen` by itself: the receiver
  has to apply it. Whether the kernels pb boots do is unchecked.

## Sources

- The specification, 2.0: `https://firmwarehandoff.github.io/firmware_handoff/main/`
  (introduction, transfer list, register conventions).
- Its repository, with `contributing.rst` and `maintainers.rst`:
  `https://github.com/FirmwareHandoff/firmware_handoff`.
- The reference library: `https://github.com/TF-Shared/transfer-list-library`.
