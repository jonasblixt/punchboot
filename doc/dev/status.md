# Status

_Last updated: 2026-11-17 (D-093: an installed table is in effect after the reset, a changed
one locks its partitions' writes; D-094: the SRK hash is the board's, in `slc_fuses`)_

## Current focus

3.0.0 on `work-3.0.0-mega` (D-018), one branch until the release. Wire and bpak stay
compatible (D-008), the major bump is for the porting API. History is in `log/`.

- **Documents:** levels 0 to 3 reviewed or in force; `doc/README.md` is the map,
  `decisions.md` has an index (D-075). Left in the code: each contract's "Not yet in the code".
- **Tests:** `scripts/pbdev test`, 119 suites and checks, ASan/UBSan clean; builds and tests
  one at a time with `PB_JOBS=6` (load under 8, Jonas). CI: host tests, coverage, all boards,
  Coverity (master only).
- **Boards (2026-11-16, night):** all in command mode, no system enabled.
  - jiffy: BOOT1 (enabled) `661d0e14`'s tree, BOOT0 the OP-TEE add-on's pb; Root B the add-on's
    root. Serial download is a button held at power-up. In Linux `pbstate-pb`. No JTAG on jiffy.
  - pico8ml (closed, HAB test keys, token outside the repository): `6208fbb3`'s pb in BOOT0 and
    BOOT1, BOOT1 enabled. Never `reboot` it (F-452).
  - RT1060: its NOR `f3493d4f`'s pb, the simple bench 7 of 7. The MEK and the 8MP EVK: BOOT0
    (enabled) `5f79b257`'s tree, BOOT1 `2c9dee80`'s, Root B test data; the 8MP's System A the
    F-156 kernel.
  - Never write fuses on the 8MP EVK (`CONFIG_SLC_NO_FUSE_WRITES`, D-087); never run SLC or
    fuse commands on the MEK. imx8dxlevk and imx8mevk only build (F-300).
- **Test benches:** Linux (`testbench/`): jiffy, the 8MP EVK, the MEK and the pico8ml boot to
  the login. Simple target (`testbench/simple/`): the RT1060.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it.

## Next steps

0. **Resume here: `log/2026-11-16.md`'s last three sections** (the night), then the evening's.
   No JTAG on jiffy (F-455). Nothing pushed since `58c9cf3e`'s rebase.
   - OP-TEE on jiffy: xtest's regression suite passes (F-444, F-465 fixed in the test bench);
     `boot bpak` the add-on's `boot.bpak` with `--pretend-part` System B (log, the evening).
   - pico8ml: F-450 (intermittent, power first: a bench supply, lockup detectors), F-452 (its
     resets), then the bring-up's step 5 and the boot time list (`testbench/README.md`).
     TF-A's 8MQ patch could go upstream.
   - Not yet on a board (log 2026-11-14): the 8MP's USB2 and the 8DXL's OTG1 (F-402),
     `-fno-pie` (F-454) on the pico8ml.
   - D-084's rest: EOL with data (F-263, PR #51). F-447 (the 8M's SLC sources), F-432 (the
     pico8ml, when it is back), F-433.
1. **The 3.0.0 gate list** (`roadmap.md`, D-092): groups 1 and 2 are done but Jonas's: F-447,
   F-228 (the workflow switch after pypi.org), F-259, the review of `guides/slc.md` and
   `updates.md`. Then group 3's contract sentences (his).
2. `roadmap.md`'s first section: the Firmware Handoff tag (Jonas, upstream), the bench run, the
   review of the guides and the notes.
3. The rest of `findings.md`: by board (8MP EVK F-104 to F-107, F-113, F-155), boot time.

## Open questions (for Jonas)

- D-094 (log 2026-11-17): `struct slc_ops` keeps the optional `set_configuration`, unused now.
- D-093's choices (log 2026-11-17): the lock is per device, its partitions only, and a locked
  write answers `ERROR` (`-PB_ERR_STATE`); "changed" is the partitions' UUIDs and blocks.

- From 2026-11-16: F-462 left open (Jonas): the RT's two core clock waits stay bare loops, a
  counted one faults now and then.
- Review: the guides and `CHANGELOG.md` (F-232), D-074, D-070, D-066, D-056, the logs' "For
  Jonas".
- Contract sentences: `platform.md` 3.10's port rule (D-084), `usb.md` 4.2's stop (F-453),
  `platform.md` 3.3's kicks (F-320); `boot.md` F-331, F-323, F-345, F-360, F-342; `board.md`
  F-325, F-335; `command-mode.md` F-375; `bio.md` F-310; F-311.
- F-447: the 8M's Security Reference Manual into the share (Jonas). F-228: PyPI's trusted
  publishing set up on pypi.org (Jonas).
- `Co-Authored-By` trailers in 376 pushed commits (left for now, Jonas).
