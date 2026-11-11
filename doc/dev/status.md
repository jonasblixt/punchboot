# Status

_Last updated: 2026-11-11_

## Current focus

3.0.0 on `work-3.0.0` (D-018): one branch until the release. Wire and bpak stay compatible (D-008),
the major bump is for the porting API. Each item's history is in `log/`; the full list before
the trim is at the end of `log/2026-11-09.md`.

- **Board as data (`work-3.0.0-plat`, D-026, D-027):** waits for Jonas's review, then merges into
  `work-3.0.0`. A board is `const struct board` (`pb/board.h`), `src/board.c` sets it up, RoT and
  SLC first. Console split, the SLC order in `slc.c` (F-166), one HABv4 fuse driver (F-165,
  F-167), the SRK hash from the build, revoke slots in `CONFIG_KEYSTORE_SLOTS`, configuration
  fuses as data, pin tables. Runs on jiffy, the MEK, the RT1060 and the 8MP EVK. Untested: fuse writes, the
  8X closed state, imx8mevk and DXL on hardware. `arch-firmware.md` waits for the review.
- **Platform contract (D-030, `platform.md`, `plat-imx.md`):** reviewed, its bugfixes are in. The
  rest follows in commits (order in `log/2026-11-10.md`), each removing its "Not yet" marker. A
  review pass against `board.md`, `boot.md` and the code is written in (2026-11-11): 7 open
  items at its top, to revisit when the board and boot contracts are final. New: F-261, F-262.
- **Board contract (D-031, `board.md`):** written, waits for Jonas's review; its code is F-216,
  F-178, F-249..F-252 (held where it touches the boot until the boot contract is reviewed).
- **Boot flow contract (D-032, `boot.md`):** a draft after four rounds with Jonas and a review
  pass, then a pass against level 1 (D-038: 10 open items left), waits for his review; its code is F-253..F-260 and the
  A/B review's. F-253 (the lost `IN_MEM` source) before
  `work-3.0.0-plat` merges. Then the drivers' contracts (`log/2026-11-11.md`).
- **The documents (D-037):** `doc/` is laid out by level, `doc/README.md` is the map with each
  document's state. `doc/dev/` keeps status, roadmap, decisions, findings (open; the closed ones in
  `findings-closed.md`) and the log.
- **The process and level 1 (D-033 to D-036):** `process.md`, `security.md`, `system.md` and
  `principles.md` are reviewed (Jonas). Documents first, top down; security first, also before
  compatibility. No custom source, `dont-hash` images refused, EOL behind Kconfig (F-263).
- **Tests:** `scripts/pbdev test`, ~20 s: 35 unit suites, `compat.abi`, `doc.lint` (the
  document lint, `process.md` 7) and the pytest integration tests, ASan/UBSan clean, line coverage
  99.2% (`testing.md`).
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (D-012), a firmware compile
  check, Coverity (master only, unverified until merged).
- **Boards:** jiffy (i.MX6UL), imx8mpevk, imx8qxmek and imxrt1060evk run on the bench; imx8dxlevk
  and imx8mevk only build (F-033). Never write fuses on the 8MP EVK, never run SLC or fuse
  commands on the MEK.
- **Test benches:** Linux (`testbench/`, D-022): jiffy, the 8MP EVK and the MEK boot to the login.
  simple target (`testbench/simple/`, D-025): the RT1060 EVK.

**Hard rule (D-008):** never break wire or bpak compatibility; only security comes before it
(D-035). See `compatibility.md`.

## Next steps

0. **Resume here (2026-11-11): Jonas reviews the boot flow contract, `boot.md` (draft, D-032).**
   Start from its "Open for the review" list: ten items left for him, marked **Open (n)** in the
   text, seven closed by level 1 (D-038). The rounds, the review pass and the Firmware Handoff
   and Xen facts are in `log/2026-11-11.md`. After it: the rest of the `board.md`
   review (2.1 is rewritten), then `platform.md`'s
   open items (they follow the two drafts), then the code (F-253 first, before `work-3.0.0-plat`
   merges). The document lint is in (`doc.lint`); the
   conformance suites come with each contract's code.
1. `work-3.0.0-plat`: Jonas's review of the board work (D-026, D-027). On it: the 8M's HAB
   once its SRK_REVOKE word is verified. Clocks stay code for now (Jonas, 2026-11-09). Later:
   the simple target bench on Cortex-A (D-029: arch/board split, 8MP EVK, jiffy, MEK; boot time
   to the simple target).
2. Open from the 3.0.0 work: MMC UHS-I, DDR52 on the EVK (F-113), HS400 tuning (F-116). Boot time:
   MMU/caches on early, the eMMC power-up early. jiffy USB F-134 (before any cache work), F-136.
   CAAM F-127, F-128. The 8MP EVK: F-100, F-102, F-104, F-110, the FEC (F-155), F-156. The MEK:
   F-159. imx8mevk: F-106, F-107, the CAAM RNG untried.
3. Compatibility guards 3 (old PyPI wheels against pb-host), 4's device side, 5 (CODEOWNERS),
   older bpak CLI images; then F-075 (erase), F-076 (dont-hash). `compat-guards` also carries the
   jiffy HAB signing commits, to be split onto their own branch.
4. Security: F-004. JTAG (`scripts/pbdebug`): F-078, F-079, F-081, F-083, F-089, F-090.
5. Check the Coverity job on master. Rebase `nanopi-neo-core2` onto master (WIP in a stash).
   F-010, F-070, F-109, the rest of F-066 and F-067, F-143, F-144, F-147, F-148, F-152.
6. `-Werror` for the host build. Branch `remove-codecov` (D-012). D-023: other boot-path init
   only command mode needs (candidate: the HAB log's registration in `plat_init()`).

## Open questions (for Jonas)

- Project review (2026-11-10, F-181..F-232, `log/2026-11-10.md`): which first? Proposed: F-181 (CAAM,
  check on jiffy), F-182 (eMMC partial blocks), F-183 (stale keystore), then F-184 against F-172.
- A/B review (F-173..F-180): roll back when a verified slot fails to load (F-174)? One boot or
  two for the speculative fallback (F-177)? The fixes for F-173, F-175, F-176, F-178 wait for a go.
- No silent boot (`platform.md` 2.3, F-248): per condition, refuse or boot? The watchdog that
  didn't arm, a failed crypto engine; one rule in every life cycle state; tell the OS in the DT?
- Platform contract: can "type 1 masters are secure at reset" (6UL SRM, CSU_SA, NDA) be written
  in the public docs, or only the register's name (as now)? Put the Arm specs and the TZC-380
  TRM (DDI0431) on the share?
- Which boards are still in use? bebop and pico8ml only have defconfigs (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Mbed TLS: 3.6 LTS or 4.x?
