# Status

_Last updated: 2026-11-09_

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
- **Tests:** `scripts/pbdev test`, ~20 s: 35 suites (unit tests, `compat.abi`, the pytest
  integration tests), ASan/UBSan clean, line coverage 99.2% (`testing.md`).
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (D-012), a firmware compile
  check, Coverity (master only, unverified until merged).
- **Boards:** jiffy (i.MX6UL), imx8mpevk, imx8qxmek and imxrt1060evk run on the bench; imx8dxlevk
  and imx8mevk only build (F-033). Never write fuses on the 8MP EVK, never run SLC or fuse
  commands on the MEK.
- **Test benches:** Linux (`testbench/`, D-022): jiffy, the 8MP EVK and the MEK boot to the login.
  MCU (`testbench/mcu/`, D-025): the RT1060 EVK.
- **Done on `work-3.0.0`:** bpak 1.0.1 (D-024), i.MX8M DDR drivers (D-013, D-014), the MMC/SD
  HAL and its test bench (D-017), CAAM from the SRMs with the RNG4, the jiffy USB fixes, pure
  Python host tools (D-021), logs by name (`punchboot log`), boot time measurements, slimming
  (D-019, D-020), the RoT/SLC fixes (F-001, F-161..F-164, F-168).

**Hard rule (D-008):** never break wire or bpak compatibility. See `compatibility.md`.

## Next steps

1. `work-3.0.0-plat`: Jonas's review. On it: clocks (tables or not, for Jonas); the 8M's HAB
   once its SRK_REVOKE word is verified.
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

- Which boards are still in use? bebop and pico8ml only have defconfigs (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Mbed TLS: 3.6 LTS or 4.x?
- Compat floor: which oldest released host and firmware must the guards test against? F-075 shows
  v2.0.0 already broke erase for 1.x hosts.
