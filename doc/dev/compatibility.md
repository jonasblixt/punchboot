# Compatibility: wire protocol and bpak format

**Hard rule (D-008): never break backward compatibility of the wire protocol or of the bpak
image format.** Devices in the field are not updated in step with the host tools, factory lines
pin old tool versions, and signed images outlive both. This file lists what is frozen, what may
change and how, and the guards that check it.

Read it before touching: `include/pb-tools/{wire,error}.h`, `src/wire.c`, `src/cm/`,
`tools/punchboot/` (C API and Python), `include/bpak/`, `src/lib/bpak.c`,
`src/boot/image_helpers.c`, keystore codegen, or anything that writes or reads images.

## Wire protocol: what is frozen

- `PB_WIRE_MAGIC` (`PBL0`). Changing it makes every old peer reject every frame. There is no
  other version field, so a "v2 protocol" can only be added *next to* the old one, never instead.
- The 512-byte `pb_command`/`pb_result` frames, little-endian, packed.
- Every value of `pb_commands`, `pb_results`, `pb_slc`, `pb_auth_method` and the
  `PB_WIRE_PART_FLAG_*` bits. Only append, before the sentinel. Never remove, renumber or reuse
  a slot. `result_code` is an `int8_t`, so `pb_results` must stay below 128.
- Size, offset and type of every field in every request/response struct.
- The sequence for each command: which follow-up transfers happen, in which order, with which
  sizes, and that every exchange ends with a `pb_result`.
- **Semantics**: the same bytes must mean the same thing. A layout-compatible change that changes
  what a zero or an old value does is still a break (see F-075).
- The auth payloads: the token format (signature over what, which encoding) and the password format.
- A raw `struct bpak_header` crosses the wire, so the bpak rules below apply to it too.

## Wire protocol: how to extend it

- **New field**: take it from an `rz[]` array. Zero must mean the old behaviour, because old hosts
  and old devices send zeros there. The device ignores `rz` content, so a new host must not rely
  on an old device honouring a new field: gate it on a capability.
- **New command**: append to `pb_commands`. Old devices answer `-PB_RESULT_INVALID_COMMAND`; the
  host must treat that as "not supported" and fall back or give a clear error.
- **Capabilities**: `PB_CMD_DEVICE_READ_CAPS` is the only negotiation. New capabilities go in its
  `rz[18]`. The host must handle zero (an old device) for every caps field, including today's.
- **Retiring a command** (F-032): the handler may answer `-PB_RESULT_NOT_SUPPORTED`, but the enum
  slot stays forever and is never reused. Prefer keeping the handler.
- The device side must keep accepting what released hosts send, and the host side must keep
  understanding what released devices send. Both directions count.

## bpak format: what is frozen

- `struct bpak_header`: 4096 bytes, every field offset, `BPAK_HEADER_MAGIC`, the `BPAK_MAX_*`
  sizes, `BPAK_PART_ALIGN`.
- The values of `bpak_hash_kind`, `bpak_signature_kind`, `bpak_key_kind` and the `BPAK_FLAG_*` bits.
- The ID function (`bpak_id`, CRC32 of the name): part IDs, meta IDs, `key_id` and `keystore_id`
  are stored in images and keystores, and boards hardcode the hex values.
- Meta encodings punchboot reads: `pb-load-addr` (u64 per part ref), `optee-arch`.
- **What is signed**: the hash (`hash_kind`) of the whole header with `signature` and
  `signature_sz` zeroed (`bpak_copyz_signature` + `image_helpers.c`), and the signature encoding
  the bpak CLI produces for each kind.
- **What the payload hash covers**: part data in header order, each part `bpak_part_size()`
  bytes (`transport_size` for transport parts).
- punchboot's on-disk layout: the payload from LBA 0 of the partition, the header in the last
  4 KiB (`pb_api_partition_write`, `boot.c`, `cm_main.c`). File layout: header first.
- The keystore layout (`key.h`/`keystore.h`) and the `bpak generate keystore` output that codegen
  consumes.

Rule for the firmware: every image that a released bpak CLI produced and a released punchboot
accepted must still be accepted. The one exception is rejecting images that were always invalid
(a security fix); that needs a decision entry naming what is now rejected and why.

Extensions (new hash/sign kinds, new flags, new meta) are allowed if old images keep verifying
the same way. New images that old firmware can't read are fine only if they are opt-in.

## Guards today

- `tests/integration/pbtest.py` hardcodes some command numbers and talks raw wire (`Wire`), so
  renumbering those commands fails the stream tests. That is incidental, not a full table.
- `test_wire.c` checks the helpers, not the layout or the values.
- Every bpak image in the tests is made at test time by the pinned bpak CLI (`make_bpak`). That
  checks today's CLI against today's firmware, never an old image.
- Nothing asserts a struct size, a field offset or an enum value.

So today a break is only caught by review. The proposed guards are in the roadmap (Phase 1,
"Compatibility guards").

## Checklist for a change in this area

1. Does any frozen item above change? Then stop: it needs another design.
2. Old host + new device, and new host + old device: what does each side send, and what does
   the other do with it? Write the answer in the PR.
3. If a zero or an absent field reaches new code, is the behaviour the old one?
4. Update the frozen manifests in the same PR (once they exist) and say why in the commit.
