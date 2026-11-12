# Status

_Last updated: 2026-11-12_

## Current focus

3.0.0 on `work-3.0.0` (D-018): one branch until the release. Wire and bpak stay compatible (D-008),
the major bump is for the porting API. Each item's history is in `log/`; the full list before
the trim is at the end of `log/2026-11-09.md`.

- **Board as data (`work-3.0.0-plat`, D-026, D-027):** waits for Jonas's review, then merges into
  `work-3.0.0`. A board is `const struct board` (`pb/board.h`), `src/board.c` sets it up, RoT and
  SLC first. Console split, the SLC order in `slc.c` (F-166), one HABv4 fuse driver (F-165,
  F-167), the SRK hash from the build, revoke slots in `CONFIG_KEYSTORE_SLOTS`, configuration
  fuses as data, pin tables. Runs on jiffy, the MEK, the RT1060 and the 8MP EVK. Untested: fuse writes, the
  8X closed state, imx8mevk and DXL on hardware.
- **Platform contract (D-030, D-053 to D-056, `platform.md`, `plat-imx.md`):** reviewed (Jonas,
  2026-11-12), no open items. From the review: a board maps hardware the platform didn't with
  `mmap` in `struct board` (D-054, F-269); images load only into the board's image data regions,
  `image_regions` (D-055, F-254; changes `boot.md` 4.3); a board gets a system controller's
  API through the platform header, not by its own path (D-056, F-270). The first two are new entries in
  `board.md` (2.8, 2.9). Its bugfixes are in; the rest follows in commits.
- **Board contract (D-031, D-045 to D-052, `board.md`):** reviewed (Jonas, 2026-11-12), no open
  items. One member per concept in `struct board`, a variant is an object (D-046); in place is a
  flag in the static table (D-048); command mode's USB is the platform's (D-049); tasks while
  pb is stopped (D-051). Its code: F-216, F-178, F-249..F-252, F-264..F-268.
- **Boot flow contract (D-032, D-038 to D-041, D-043, D-044, `boot.md`):** reviewed (Jonas), no
  open items. Its code is F-253..F-260 and the A/B review's. F-253 (the in place source) before
  `work-3.0.0-plat` merges.
- **Command mode contract (D-057 to D-059, `command-mode.md`):** reviewed (Jonas, 2026-11-12),
  no open items. The session, the gate, each command's exchange and what a failure leaves.
  `CONFIG_CM_AUTH` stays the product's switch, with at least one method when it is on. Its
  code: F-271 to F-280, F-185 to F-190. A second pass (D-060): the three places that mean bpak
  and how another image format joins (5.9), `INVALID_COMMAND` against `NOT_SUPPORTED` (4.5).
  A build may leave bpak out, once there is a second format (Jonas, D-061; `system.md` 3).
  Then the drivers' contracts (`log/2026-11-11.md`).
- **The documents (D-037):** `doc/` is laid out by level, `doc/README.md` is the map with each
  document's state. The agent instructions are `AGENTS.md`, harness-neutral (D-042). `doc/dev/`
  keeps status, roadmap, decisions, findings (open; the closed ones in `findings-closed.md`) and
  the log. The two overviews of the code (firmware, host tools) are removed (D-062).
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

0. **Resume here (2026-11-12): the documents before the code (Jonas, D-057).** The three
   contracts and `command-mode.md` are reviewed and committed. Next the drivers' contracts
   (block devices, the crypto backends, the USB device controllers, DMA and caches).
   Then the code, a commit per finding: F-253 first (in place, before `work-3.0.0-plat`
   merges), then F-265 (`struct board`'s objects) with F-256, F-258, F-259; F-267 and F-268
   (command mode's USB, the tasks). The conformance suites come with each contract's code.
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
- Platform contract: can "type 1 masters are secure at reset" (6UL SRM, CSU_SA, NDA) be written
  in the public docs, or only the register's name (as now)? Put the Arm specs and the TZC-380
  TRM (DDI0431) on the share?
- F-264: wire the RT's `late_boot` (the charger check), or remove it with
  `board_get_in_mem_image()`?
- Which boards are still in use? bebop and pico8ml only have defconfigs (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Mbed TLS: 3.6 LTS or 4.x?
