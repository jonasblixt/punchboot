# Status

_Last updated: 2026-11-15 (the i.MX8M's SPL, D-088; F-450)_

## Current focus

3.0.0 on `work-3.0.0-mega` (D-018), one branch until the release. Wire and bpak stay
compatible (D-008), the major bump is for the porting API. History is in `log/`.

- **Documents:** levels 0 to 3 reviewed or in force; `doc/README.md` is the map,
  `decisions.md` has an index (D-075). Left in the code: each contract's "Not yet in the code".
- **Tests:** `scripts/pbdev test`, 119 suites and checks, ASan/UBSan clean; builds and tests
  one at a time with `PB_JOBS=6` (load under 8, Jonas). CI: host tests, coverage, all boards,
  Coverity (master only).
- **Boards (2026-11-15):**
  - jiffy: `55689509` from BOOT1 (BOOT0 an older pb); Root A has the test bench's root image,
    Root B random test data. The OP-TEE add-on's pb is no longer on it.
  - pico8ml (closed, HAB test keys, token outside the repository): the SPL and pb proper
    (D-088) from BOOT1, BOOT0 an older SPL build; boot switches on eMMC. Linux from System A.
  - RT1060 from its NOR, the MEK from BOOT0: `246bd897` (log 2026-11-14). The 8MP EVK offline.
  - Never write fuses on the 8MP EVK (`CONFIG_SLC_NO_FUSE_WRITES`, D-087); never run SLC or
    fuse commands on the MEK. imx8dxlevk and imx8mevk only build (F-300).
- **Test benches:** Linux (`testbench/`): jiffy, the 8MP EVK, the MEK and the pico8ml boot to
  the login. Simple target (`testbench/simple/`): the RT1060.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it.

## Next steps

0. **Resume here: `log/2026-11-15.md`**, its last three sections. Nothing pushed since
   `58c9cf3e`'s rebase.
   - pico8ml: F-450 without `selfref_en` (power or self refresh), F-451 (USB recovery with the
     SPL: Jonas), the 8M Plus's SPL on the EVK, then the bring-up's step 5 and the boot time list
     (`testbench/README.md`). TF-A's 8MQ patch could go upstream.
   - The bench of what landed without a board (log 2026-11-14): USB on all four, the MMU maps,
     the RT1060's entry (F-210, F-207), the DMA rule (F-282), the 8MP EVK through TF-A, the
     8M's lockdown (D-084).
   - D-084's rest: F-402, F-320, EOL with data (F-263, PR #51), F-322, F-241, F-294.
   - F-444 (OP-TEE's SMC on jiffy), F-447 (the 8M's SLC sources), F-432, F-433.
1. `roadmap.md`'s first section: the Firmware Handoff tag (Jonas, upstream), the bench run, the
   review of the guides and the notes.
2. The rest of `findings.md`: by board (8MP EVK F-100 to F-110, F-155, F-156; MEK F-172),
   JTAG (F-078 to F-090), boot time.

## Open questions (for Jonas)

- D-086 (Mbed TLS by the core) and its Open (1); then F-442's code.
- Review: the guides and `CHANGELOG.md` (F-232), D-074, D-070, D-066, D-056, the logs' "For
  Jonas".
- Contract sentences: `boot.md` F-291, F-331, F-323, F-345, F-360, F-342, F-348; `board.md`
  F-325, F-335, F-392; `platform.md` F-320; `command-mode.md` F-375; `bio.md` F-310; F-311.
- Choices left in findings: F-140 (SDIS off: 15% faster writes on jiffy), F-283 and F-200 (the
  GPT's 512s, SEC_COUNT 0), F-406, F-411, F-180, F-227's default device.
- Which findings gate the 3.0.0 tag; bebop's defconfig (F-033); virtio for QEMU (F-052).
- The Arm specs and the TZC-380 TRM on the share (D-084).
