# Status

_Last updated: 2026-10-22_

## Current focus

Moving the tests to the host is done (D-004..D-006, `testing.md`).
- **Tests:** all 43 bash tests are ported (63 pytest plus 7 CMocka cases, ~9 s, ASan/UBSan clean,
  58% line coverage). `scripts/pbdev test` runs it all.
- **CI (`build.yml`):** host tests (default and sanitizers), coverage, a firmware compile check
  (jiffy, imx8dxlevk) and Coverity, all in the dev container. It hasn't run on GitHub yet.
- **QEMU removed:** the bash/QEMU suite, `src/board/test`, `src/plat/qemu`,
  `include/plat/qemu` and `configs/test_defconfig` are gone.

## Next steps

1. Push, so the new `build.yml` runs on GitHub. The coverage upload and Coverity can only be
   verified there.
2. Unit tests for the low-coverage modules: `ab_state`, `image_helpers`, `bpak.c`, `der_helpers`,
   `wire`, `linux.c` (DTB patching).
3. Fix F-049 (format strings). Consider `-Werror` for the host build.
4. Phase 1: security fixes F-001, F-002, F-004, F-005, each with a test.

## Open questions (for Jonas)

- Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Is open command mode in SLC EOL intended? (F-002)
- Mbed TLS: 3.6 LTS or 4.x?
