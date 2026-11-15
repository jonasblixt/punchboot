# Status

_Last updated: 2026-11-15 (D-089; F-453 fixed; F-402, F-320, F-322, F-213, F-280, F-081 closed)_

## Current focus

3.0.0 on `work-3.0.0-mega` (D-018), one branch until the release. Wire and bpak stay
compatible (D-008), the major bump is for the porting API. History is in `log/`.

- **Documents:** levels 0 to 3 reviewed or in force; `doc/README.md` is the map,
  `decisions.md` has an index (D-075). Left in the code: each contract's "Not yet in the code".
- **Tests:** `scripts/pbdev test`, 119 suites and checks, ASan/UBSan clean; builds and tests
  one at a time with `PB_JOBS=6` (load under 8, Jonas). CI: host tests, coverage, all boards,
  Coverity (master only).
- **Boards (2026-11-15):**
  - jiffy: **stuck in a reset loop (F-455)**, a TZASC test pb in BOOT1; needs serial download
    and `uuu` (Jonas). BOOT0 an older pb; Root A the test bench's root image, Root B test data.
  - pico8ml (closed, HAB test keys, token outside the repository): `6208fbb3`'s pb, with the
    SPL's fastboot (D-089), in BOOT0 and BOOT1; boot switches on eMMC, BOOT1 enabled. Linux from
    System A. Never `reboot` it (F-452).
  - RT1060 from its NOR: `b3901579`, the simple bench 7 of 7 (log 2026-11-15). The MEK: `ad44621b`'s pb in BOOT1
    (enabled), BOOT0 keeps its pb of 2026-11-14; both its USB ports are wired. The 8MP EVK:
    `b3901579`'s two-stage pb in BOOT1 (enabled, switches on eMMC), the single-stage pb of
    2026-11-14 in BOOT0.
  - Never write fuses on the 8MP EVK (`CONFIG_SLC_NO_FUSE_WRITES`, D-087); never run SLC or
    fuse commands on the MEK. imx8dxlevk and imx8mevk only build (F-300).
- **Test benches:** Linux (`testbench/`): jiffy, the 8MP EVK, the MEK and the pico8ml boot to
  the login. Simple target (`testbench/simple/`): the RT1060.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it.

## Next steps

0. **Resume here: `log/2026-11-15.md`**, its last section. Nothing pushed since
   `58c9cf3e`'s rebase.
   - The SPL's fastboot (D-089) runs on the pico8ml, as `1209:0001` (Jonas). Left: a udev
     rule for that id on the bench host.
   - pico8ml: F-450 (intermittent, power first: a bench supply, lockup detectors), F-452 (its
     resets), then the bring-up's step 5 and the boot time list (`testbench/README.md`).
     TF-A's 8MQ patch could go upstream.
   - The 8M Plus: its SPL boots pb proper on the EVK from the eMMC and over USB (`26fd5a1d`),
     Linux after it (log 2026-11-15). Not run: the signed image (the EVK is open).
   - Not yet on a board (log 2026-11-14): the RT1060's entry (F-210, F-207), the 8M's lockdown
     (D-084), the 8MP's USB2 and the 8DXL's OTG1 (F-402), `-fno-pie` (F-454) on the pico8ml.
   - D-084's rest: EOL with data (F-263, PR #51), F-241. F-444 (OP-TEE's SMC on jiffy), F-447
     (the 8M's SLC sources), F-432 (the pico8ml, when it is back), F-433.
1. `roadmap.md`'s first section: the Firmware Handoff tag (Jonas, upstream), the bench run, the
   review of the guides and the notes.
2. The rest of `findings.md`: by board (8MP EVK F-100 to F-110, F-155, F-156; MEK F-172),
   JTAG (F-078 to F-090), boot time.

## Open questions (for Jonas)

- D-086 (Mbed TLS by the core) and its Open (1); then F-442's code.
- Review: the guides and `CHANGELOG.md` (F-232), D-074, D-070, D-066, D-056, the logs' "For
  Jonas".
- Contract sentences: `platform.md` 3.10's port rule (D-084), `usb.md` 4.2's stop (F-453),
  `platform.md` 3.3's kicks (F-320); `boot.md` F-291, F-331, F-323, F-345, F-360, F-342, F-348;
  `board.md` F-325, F-335, F-392; `command-mode.md` F-375; `bio.md` F-310; F-311.
- Choices left in findings: F-140 (SDIS off: 15% faster writes on jiffy), F-283 and F-200 (the
  GPT's 512s, SEC_COUNT 0), F-406, F-411, F-180, F-227's default device.
- Which findings gate the 3.0.0 tag; bebop's defconfig (F-033); virtio for QEMU (F-052).
