# Status

_Last updated: 2026-10-22_

## Current focus

**The host-native test setup is complete in function** (D-004, D-005; `testing.md`).
- All 43 bash/QEMU tests are ported: 63 pytest plus 7 CMocka cases.
- The full run takes about 9 s, versus about 15 min for QEMU.
- It's ASan/UBSan clean, with 58% line coverage (QEMU suite: 54%).
- `scripts/pbdev host-test` runs it all.

The bash/QEMU suite still exists and is what `build.yml` runs. `build.yml` is deliberately
unchanged for now (Jonas).

## Next steps

1. Switch CI to the host tests (`scripts/pbdev host-test` in the container), then delete the
   bash/QEMU suite, `src/board/test`, the semihosting gcov hack and `tests/makefile.mk`. Waiting
   on Jonas's go-ahead for `build.yml`.
2. Unit tests for the low-coverage modules: `ab_state`, `image_helpers`, `bpak.c`, `der_helpers`,
   `wire`, `linux.c` (DTB patching).
3. Fix F-049 (format strings), which the host build makes visible. Consider `-Werror` for the host build.
4. Then Phase 1: security fixes F-001..F-005, each with a test.

## Open questions (for Jonas)

- OK to switch `build.yml` to the host tests now?
- Is open command mode in SLC EOL intended? (F-002)
- Which boards are still in use? (F-033)
- Mbed TLS: 3.6 LTS or 4.x?
