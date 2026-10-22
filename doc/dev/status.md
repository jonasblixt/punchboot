# Status

_Last updated: 2026-10-22_

## Current focus

Moving the tests to the host is done (D-004..D-006, `testing.md`), in PR #61. Branch
`compat-guards`: compatibility guards 1 and 2 are done (`compatibility.md`, Guards).
- **Tests:** 91 pytest (+3 xfail: F-072, F-076 twice), 180 CMocka cases in 16 unit test programs and
  the `compat.abi` layout check, ~12 s, ASan/UBSan clean. Line coverage 99.2% (was 59%); what's left is in `testing.md`.
  `scripts/pbdev test` runs it all.
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (Codecov via OIDC), a
  firmware compile check (jiffy, imx8dxlevk) and Coverity, all in the dev container. All green on
  `claude-work`. Coverity only runs on master, so it's unverified until merged.
- **QEMU removed:** the bash/QEMU suite, `src/board/test`, `src/plat/qemu`,
  `include/plat/qemu` and `configs/test_defconfig` are gone.

**Hard rule (D-008):** never break wire or bpak compatibility. See `compatibility.md`.

## Next steps

1. PR #61 is merged (2026-10-22): check its Coverity job. PR #62 (`compat-guards` → master) is open.
   `jtag-debugging` (item 6) is based on it.
2. Unit tests for every core module are done (D-007). They found F-068..F-074; fixed F-069
   (positive error codes) and F-073 (UB in `bio_set_hal_flags`). Next from the review: F-057 (CAAM DER
   overflow), F-055 (erase), F-063, the boot-path bugs F-058..F-062, F-070, F-072, the rest of F-067
   (SIGPIPE, orphaned pb-host, `-m32`).
   F-071 (pb adds missing DTB properties, with spare RAM, a performance note and a jiffy test) is
   planned in the roadmap, Phase 1.
3. Compatibility guards: 1 (frozen layout) and 2 (golden bpak images) are done. Next: 3 (old PyPI
   wheels against pb-host), 4 (recorded transcripts), 5 (CODEOWNERS), older bpak CLI images.
   Then the compat fixes F-075 (erase), F-076 (dont-hash, has an xfail test), F-077 (caps 0).
   `compat-guards` also carries the jiffy HAB signing commits, to be split onto their own branch.
4. Fix F-049 (format strings). Consider `-Werror` for the host build.
5. Phase 1: security fixes F-001, F-002, F-004, F-005, each with a test.
6. JTAG: `scripts/pbdebug jiffy` (ULINK + OpenOCD + GDB, `build-and-test.md`) works on the board.
   F-080 fixed (CAAM timeout). On the closed board, JTAG blocks the CAAM until a power cycle.
   Next: `pbdebug --load`, F-082 (BOOT0/BOOT1 flip), F-083, F-078/F-079, F-081, F-084.

## Open questions (for Jonas)

- Which boards are still in use? Only jiffy and imx8dxlevk build (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Is open command mode in SLC EOL intended? (F-002)
- Mbed TLS: 3.6 LTS or 4.x?
- Compat floor: which oldest released host and firmware must the guards test against? F-075 shows
  v2.0.0 already broke erase for 1.x hosts.
