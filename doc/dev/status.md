# Status

_Last updated: 2026-10-22_

## Current focus

Moving the tests to the host is done (D-004..D-006, `testing.md`).
- **Tests:** 80 pytest (+1 xfail, F-072) and 180 CMocka cases in 16 unit test programs, ~11 s,
  ASan/UBSan clean. Line coverage 99.2% (was 59%); what's left is in `testing.md`.
  `scripts/pbdev test` runs it all.
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (Codecov via OIDC), a
  firmware compile check (jiffy, imx8dxlevk) and Coverity, all in the dev container. All green on
  `claude-work`. Coverity only runs on master, so it's unverified until merged.
- **QEMU removed:** the bash/QEMU suite, `src/board/test`, `src/plat/qemu`,
  `include/plat/qemu` and `configs/test_defconfig` are gone.

## Next steps

1. PR #60 (`claude-work` → master) is open. Check the Coverity job after the merge.
2. Unit tests for every core module are done (D-007). They found F-068..F-074; fixed F-069
   (positive error codes) and F-073 (UB in `bio_set_hal_flags`). Next from the review: F-057 (CAAM DER
   overflow), F-055 (erase), F-063, the boot-path bugs F-058..F-062, F-070, F-072, the rest of F-067.
   F-071 (pb adds missing DTB properties, with spare RAM, a performance note and a jiffy test) is
   planned in the roadmap, Phase 1.
3. Fix F-049 (format strings). Consider `-Werror` for the host build.
4. Phase 1: security fixes F-001, F-002, F-004, F-005, each with a test.

## Open questions (for Jonas)

- Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Is open command mode in SLC EOL intended? (F-002)
- Mbed TLS: 3.6 LTS or 4.x?
