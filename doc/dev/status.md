# Status

_Last updated: 2026-11-12_

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
- **Boards:** jiffy (i.MX6UL), imx8qxmek and imxrt1060evk run `2d9437b9` from their boot
  media (flashed 2026-11-13, the log has what they showed). imx8mpevk runs `a793102b` from
  RAM only (F-409). imx8dxlevk and imx8mevk only build (F-033, F-300). Never write fuses on
  the 8MP EVK, never run SLC or fuse commands on the MEK. Untested on any board: fuse
  writes, the 8X closed state.
- **Test benches:** Linux (`testbench/`, D-022): jiffy, the 8MP EVK and the MEK boot to the
  login. simple target (`testbench/simple/`, D-025): the RT1060 EVK.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it
(D-035). See `compatibility.md`.

## Next steps

0. **Resume here (2026-11-13): `log/2026-11-13.md`.** Landed in the night and the day, a
   package of findings per agent: `board_setup()`, the build, the host tools, the A/B state,
   the block device core (F-182), command mode's USB and the tasks (F-267, F-268), pbstate,
   crypto (F-181). First, in this order:
   - **F-409: the 8MP EVK doesn't boot pb `a793102b` from eMMC**; it runs from RAM over
     serial download. Find it before any other board is flashed: the MEK and jiffy run
     `2d9437b9`, the RT1060 too.
   - **F-259 is on the branch `land-p10`**, reviewed and verified, not landed: it changes
     `configs/imx8mpevk_defconfig`, which has an uncommitted change of Jonas's. Then the
     RT1060 has to show it.
   - Three packages were stopped half way, in worktrees under `.claude/worktrees/`: the GPT
     (F-191 and its neighbours), the platform's lifecycle (F-235, F-236), command mode's
     small ones (F-190, F-095, F-252). Not started: the small driver and build ones.
   - The log's "For Jonas" lists: about forty questions the code met.
1. What 3.0.0 waits for besides the code is `roadmap.md`'s first section: the Firmware
   Handoff tag (Jonas, upstream), the bench run, the two guides, the release notes.
2. Jonas's review of the board work's code (D-026, D-027; its shape is D-046's since F-265).
   On it: the 8M's HAB once its SRK_REVOKE word is verified. Clocks stay code for now (Jonas,
   2026-11-09).
3. Open from the 3.0.0 work: MMC UHS-I, DDR52 on the EVK (F-113), HS400 tuning (F-116). Boot
   time: MMU/caches on early, the eMMC power-up early. jiffy USB F-134 (before any cache
   work), F-136. CAAM F-127. The 8MP EVK: F-100, F-102, F-104, F-110, the FEC (F-155), F-156.
   The MEK: F-172, F-204. imx8mevk: F-106, F-107, the CAAM RNG untried.
4. Security: F-004. JTAG (`scripts/pbdebug`): F-078, F-079, F-081, F-083, F-089, F-090.
5. Check the Coverity job on master. Rebase `nanopi-neo-core2` onto master (WIP in a stash).
   F-010, F-109, the rest of F-066 and F-067, F-143, F-147, F-148. `-Werror` for the host
   build. D-023: other boot-path init only command mode needs (candidate: the HAB log's
   registration in `plat_init()`).

## Open questions (for Jonas)

- The order: F-181 (High: after an abandoned hash the CAAM reads the old job's result as
  "verified"; a model, not run on hardware), F-182 (eMMC partial blocks) and F-183 (a stale
  keystore) aren't in the order above. Before F-259, or after? Then F-184 against F-172.
- A/B review: the fixes for F-173, F-175, F-176 and F-178 wait for a go.
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
