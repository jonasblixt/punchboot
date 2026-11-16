# Status

_Last updated: 2026-11-16, afternoon (unattended: F-321, F-102, F-461 fixed, F-287's cdns3, F-410,
F-210's ARM_PODF; jiffy in SNVS Soft Fail after JTAG, waits for a power cycle)_

## Current focus

3.0.0 on `work-3.0.0-mega` (D-018), one branch until the release. Wire and bpak stay
compatible (D-008), the major bump is for the porting API. History is in `log/`.

- **Documents:** levels 0 to 3 reviewed or in force; `doc/README.md` is the map,
  `decisions.md` has an index (D-075). Left in the code: each contract's "Not yet in the code".
- **Tests:** `scripts/pbdev test`, 119 suites and checks, ASan/UBSan clean; builds and tests
  one at a time with `PB_JOBS=6` (load under 8, Jonas). CI: host tests, coverage, all boards,
  Coverity (master only).
- **Boards (2026-11-16, afternoon):**
  - jiffy: SNVS Soft Fail after JTAG reads (F-455), the CAAM blocked, `auth` times out: needs a
    power cycle. BOOT0 (enabled) the morning's test pb, BOOT1 `5d3dcda1`'s. Serial download is a
    button held at power-up. In Linux `pbstate-pb`. No JTAG on jiffy.
  - pico8ml (closed, HAB test keys, token outside the repository): `6208fbb3`'s pb in BOOT0 and
    BOOT1, BOOT1 enabled. Never `reboot` it (F-452).
  - RT1060: its NOR `59e4ed41`'s pb (no DCD, the curve code at -O2: 37.98 ms to the entry), the
    simple bench 7 of 7. The MEK: BOOT0
    (enabled) `f470ba88`'s tree, BOOT1 a log-only probe. The 8MP EVK: BOOT0 (enabled)
    `277ec02b`'s two-stage pb, BOOT1 a log-only probe, System A the F-156 kernel; its console is
    another session's logger (inbox file).
  - Never write fuses on the 8MP EVK (`CONFIG_SLC_NO_FUSE_WRITES`, D-087); never run SLC or
    fuse commands on the MEK. imx8dxlevk and imx8mevk only build (F-300).
- **Test benches:** Linux (`testbench/`): jiffy, the 8MP EVK, the MEK and the pico8ml boot to
  the login. Simple target (`testbench/simple/`): the RT1060.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it.

## Next steps

0. **Resume here: `log/2026-11-16.md`'s last section** (the afternoon), then
   `log/2026-11-15.md`'s last section. jiffy: power cycle, then auth and today's pb (F-210).
   No JTAG on jiffy (F-455). Nothing pushed since `58c9cf3e`'s rebase.
   - pico8ml: F-450 (intermittent, power first: a bench supply, lockup detectors), F-452 (its
     resets), then the bring-up's step 5 and the boot time list (`testbench/README.md`).
     TF-A's 8MQ patch could go upstream.
   - Not yet on a board (log 2026-11-14): the RT1060's entry (F-210, F-207), the 8M's lockdown
     (D-084), the 8MP's USB2 and the 8DXL's OTG1 (F-402), `-fno-pie` (F-454) on the pico8ml.
   - D-084's rest: EOL with data (F-263, PR #51), F-241. F-444 (OP-TEE's SMC on jiffy), F-447
     (the 8M's SLC sources), F-432 (the pico8ml, when it is back), F-433.
1. `roadmap.md`'s first section: the Firmware Handoff tag (Jonas, upstream), the bench run, the
   review of the guides and the notes.
2. The rest of `findings.md`: by board (8MP EVK F-104 to F-107, F-113, F-155), boot time.

## Open questions (for Jonas)

- From 2026-11-16: F-463 (answer USB 2.0's standard requests, or `usb.md` 3.2 as is), F-460
  (command mode's writes in pieces with a kick, or a measured bound), F-462 (the RT's clock waits,
  the UARTs: a counted bound?), F-464 (take the eMMC over from the ROM: 171 ms on the MEK).
- D-086 (Mbed TLS by the core) and its Open (1); then F-442's code.
- Review: the guides and `CHANGELOG.md` (F-232), D-074, D-070, D-066, D-056, the logs' "For
  Jonas".
- Contract sentences: `platform.md` 3.10's port rule (D-084), `usb.md` 4.2's stop (F-453),
  `platform.md` 3.3's kicks (F-320); `boot.md` F-291, F-331, F-323, F-345, F-360, F-342, F-348;
  `board.md` F-325, F-335, F-392; `command-mode.md` F-375; `bio.md` F-310; F-311.
- Choices left in findings: F-283 and F-200 (the GPT's 512s, SEC_COUNT 0), F-406, F-411, F-180,
  F-227's default device.
- Which findings gate the 3.0.0 tag; bebop's defconfig (F-033); virtio for QEMU (F-052).
- `Co-Authored-By` trailers in 376 pushed commits (left for now, Jonas).
