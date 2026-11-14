# Status

_Last updated: 2026-11-14 (D-083, the board's keys are its `makefile.mk`'s)_

## Current focus

3.0.0 on `work-3.0.0-mega` (D-018): one branch until the release; `work-3.0.0` and
`work-3.0.0-plat` are no longer worked on. Wire and bpak stay compatible (D-008), the major
bump is for the porting API. Each item's history is in `log/`.

- **The documents:** every document of levels 0 to 3 is reviewed or in force; `doc/README.md`
  is the map, `decisions.md` has an index (D-075).
- **The code, a commit per finding (D-057):** in so far, each in `log/2026-11-12.md`: in
  place, `struct board`'s objects, the policy interface, the handover and delivery model,
  pb's information, the boot path's watchdog, speculative mode, `late_boot`, the board's
  data checked, command mode's list and placement's allow list. Left: each contract's "Not
  yet in the code" table.
- **Tests:** `scripts/pbdev test`, ~35 s: 47 unit suites, `compat.abi`, `doc.lint` (the
  document lint, `process.md` 7) and the pytest integration tests, ASan/UBSan clean. Line
  coverage is 91.0% by F-230's count; the 99.2% in `testing.md` is from 2026-10-22.
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (D-012), a compile check
  of all six boards, Coverity (master only, unverified until merged).
- **Boards (2026-11-13, evening, log "The bench of all four boards"):** the RT1060 runs
  `bc640ba1` from its NOR; jiffy (BOOT0), the 8MP EVK and the MEK (BOOT1) run `bc640ba1`
  with the F-424 fix (`b6a3df47`). Since 2026-11-14 all four run `246bd897` (log, "The
  tip on all four"): jiffy, the 8MP EVK and the MEK from BOOT0 (BOOT1 keeps an older pb),
  the RT from its BL. All four in command mode, nothing enabled. The MEK's BOOT0 has
  `69e185dc` with the F-434 fix (`af6e7060`) (log, "F-434").
  imx8dxlevk and imx8mevk only build (F-033, F-300). Never write fuses on the 8MP EVK,
  never run SLC or fuse commands on the MEK. Untested on any board: fuse writes, the 8X
  closed state.
- **Test benches:** Linux (`testbench/`, D-022): jiffy, the 8MP EVK and the MEK boot to the
  login. simple target (`testbench/simple/`, D-025): the RT1060 EVK.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it
(D-035). See `compatibility.md`.

## Next steps

0. **Resume here (2026-11-14): `log/2026-11-14.md`, "The wrap-up" and "The parked
   branches".** The unattended round and its parked branches are in, verified at `1b212d0e`.
   Nothing of it ran on a board. First:
   - The bench, in this order: the DMA rule (F-282) and USB on all four, the preconditions
     (the TrustZone filter), F-247 and the CAAM's stop, the 8MP EVK through TF-A. All four
     run `246bd897` and reach command mode with the preconditions met; boot time measured
     (log), jiffy and the 8MP EVK boot Linux. F-434 fixed (`af6e7060`); the MEK
     to init 1069 ms (log). D-078 (`3e7399c6`, the command line in the handover's
     configuration): the 8MP EVK runs it from BOOT0, normal and verbose command lines seen.
     D-079 (`5c55b9ff`): the board's memory left Kconfig for its `makefile.mk`, the six
     `pb.bin` unchanged. D-080: the Kconfig menus by subject, drivers by what the SoC has.
     D-081: `BOOT_AB_DRIVER` gone. D-082: the RAM log always at debug, no sync mode.
     D-083: the keys are the board's `VERIFY_KEY<n>`, no bpak keystore; `pki/` tidied;
     `gen-keystore.py` writes pb's key list `pb_keys` (`pb/keys.h`), read with `cryptography`.
   - Held for Jonas: `bec794b7` on `pkg-plat` (the watchdog half a second shorter, F-206).
   - Jonas's review: the guides and `CHANGELOG.md` (F-232), D-074, the logs' "For Jonas".
   - `plat_get_info()` and the core's `info` (D-077): run on all four boards. Left: F-432,
     F-433.
1. What 3.0.0 waits for besides the code is `roadmap.md`'s first section: the Firmware
   Handoff tag (Jonas, upstream), the bench run, the review of the guides and the notes.
2. Jonas's review of the board work's code (D-026, D-027, D-046). Clocks stay code (2026-11-09).
3. The rest is `findings.md`: by board (the 8MP EVK F-100 to F-110, F-155, F-156; the MEK
   F-172, F-204; imx8mevk F-106, F-107), JTAG (F-078 to F-090), boot time, Coverity on master.

## Open questions (for Jonas)

- Sentences for reviewed contracts, each with what the code does meanwhile. `boot.md`: F-291
  (a DT part in place: refuse, or copy it to RAM, and where), F-331 (5.1, with D-074), F-323
  (3.2), F-345 (3.4), F-360 (3.5), F-342 (3.6), F-348 (5.5). `board.md`: F-325 (5.3), F-335,
  F-392 (the image regions leave out the room pb grows in, which old releases loaded into).
  `platform.md`: F-320 (3.3). `command-mode.md`: F-375 (4.6). `bio.md`: F-310. The lint:
  F-311.
- Recorded as my reading, to confirm: D-074 (an entry in an empty part is refused, against
  D-008), D-070 (which form of "A"), D-066 (which drivers have a `stop`), D-056 (D-054's
  form).
- Which open findings gate the 3.0.0 tag (`roadmap.md`).
- Platform contract: can "type 1 masters are secure at reset" (6UL SRM, CSU_SA, NDA) be
  written in the public docs, or only the register's name (as now)? Put the Arm specs and
  the TZC-380 TRM (DDI0431) on the share?
- Which boards are still in use? bebop and pico8ml only have defconfigs (F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Mbed TLS: 3.6 LTS or 4.x?
