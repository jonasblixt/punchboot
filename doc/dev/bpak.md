# bpak in punchboot

Upstream: `../bpak` (github.com/jonasblixt/bpak), currently v0.9.3. The file format is
frozen for now (D-001).

## What is vendored

- `src/lib/bpak.c` and `include/bpak/{bpak.h,id.h,keystore.h,key.h}` are a subset of upstream `lib/`, around **v0.9.0/0.9.1**.
- Local changes compared with upstream:
  - `PACK()` from `pb-tools/compat.h` replaces `__attribute__((packed))`.
  - No `version.h`, no build_config.
  - No calloc/free hooks.
  - Reformatted by punchboot's clang-format.
- The same `bpak.c` is also compiled into the Python wheel (`setup.py`).

## What punchboot actually uses

- Header parsing: `bpak_valid_header`, `bpak_foreach_part`, `bpak_part_offset`, `bpak_part_size`, `bpak_get_part`.
- Metadata: `bpak_get_meta*`, only for `pb-load-addr` (u64 per part). `src/boot/atf.c` also uses `optee-arch` (`0x99e5e80a`).
- Board files hardcode part IDs (`atf`, `dt`, `ramdisk`, `kernel`) as hex constants.
- **Signature and payload verification are punchboot's own** (`src/boot/image_helpers.c`). They do not go through bpak's verify code.
- Not used: merkle, transport/delta, the bpak crypto and pkg APIs.

## Coupling points

1. `sizeof(struct bpak_header) == 4096` is baked into the LBA math (`boot.c`, `cm_main.c`, `image_helpers.c`). The header sits at the end of the partition.
2. The raw header is sent over the wire protocol.
3. Keystore codegen depends on the output of `bpak generate keystore --decorate` and on the `key.h`/`keystore.h` layouts.
4. CI builds the bpak CLI v0.9.3. The embedded copy is ~0.9.0, and there is no sync procedure (F-021).
