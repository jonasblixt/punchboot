# Status

_Last updated: 2026-10-29_

## Current focus

v2.8.0 (branch `ramlog`): the RAM log (`CONFIG_RAMLOG`, `arch-firmware.md`): early prints before
the UART, `CONFIG_RAMLOG_LOGLEVEL` (debug in RAM, the UART at its own level), `PB_CMD_LOG_READ`
and `punchboot log`. Checked on jiffy over JTAG and USB. Also pbstate reports `version.txt`.
Moving the tests to the host is done (D-004..D-006, `testing.md`). Compatibility guards 1 and 2
are done (`compatibility.md`, Guards).
- **Tests:** 108 pytest (+2 xfail: F-076), 21 unit test programs and
  the `compat.abi` layout check, ~15 s, ASan/UBSan clean. Line coverage 99.2% (was 59%); what's left is in `testing.md`.
  `scripts/pbdev test` runs it all.
- **CI (`build.yml`):** host tests (default and sanitizers), coverage (gcovr HTML artifact, D-012), a
  firmware compile check (jiffy, imx8dxlevk, imx8mevk) and Coverity, all in the dev container. All green on
  `claude-work`. Coverity only runs on master, so it's unverified until merged.
- **i.MX8M DDR (branch `imx8m-dram-cleanup`):** cleaned up behind a golden register trace (D-013),
  uMCTL2/DWC PHY drivers in `src/drivers/ddr` (D-014), `PLAT_IMX8MQ` + `PLAT_IMX8M_COMMON`.
  `PLAT_IMX8MP` + board `imx8mpevk` (2026-10-28): pb runs from OCRAM over `uuu`, the DRAM trains and
  passes a write/read test on the 8MP EVK (`imx8m-ddr.md`, "i.MX8M Plus EVK"). eMMC (HS200) and
  USB command mode work, the DWC3 driver reworked from the RM (F-112). pb boots from eMMC (F-115 fixed).
  Never write fuses on that EVK. Open F-100, F-102, F-104, F-106, F-107, F-110, F-113.
- **MMC/SD (same branch, 2026-10-28):** a test bench of card models from JESD84-B51 and the SD
  spec plus a uSDHC register model (D-017, `testing.md`) found F-116..F-125, all fixed but F-116
  (HS400 tuning). New request-based HAL, one `mmc_device` per card (`mmc_emmc.c`, `mmc_sd.c`),
  SD cards (default/high speed, 4 bit), the imx8mpevk microSD slot on uSDHC2: works on the EVK
  with eMMC HS200. Hardware found F-126 (no TC after busy). HS200 tuning on every boot (2 ms, tap 46
  on the EVK), CMD12 after the data instead of auto CMD12. Next: UHS-I (CMD11, tuning), DDR52
  on the EVK (F-113). jiffy checked (DDR52) before the tuning/CMD12/log changes.
- **Boot time (2026-10-29):** log lines carry `[ms.us]` (no more `ts()`), console at warnings, RAM
  log at debug. The imx8mpevk trains only 4000 MT/s (`struct dram_rates`): plat init 75 ms.
  EXT_CSD read at TRAN_SPEED (jiffy -13 ms, EVK -24 ms). jiffy warm boot: ~127 ms ROM + 38 ms pb to
  `boot_load()`; 10 MiB load+verify 162 ms (EVK 147, `scripts/boot-time-image`, README Metrics).
  The EVK boots pb from eMMC (F-115 fixed): reset to `boot_load()` 126 ms (2D training 51 ms of
  it), the microSD card isn't set up any more (`BOARD_SD_CARD`). Next: CMD1 polling, `plat_mmu_init()` (13 ms), smaller load chunks.
- **CAAM (2026-10-29):** register/descriptor names and fields from the SRMs, JR0 (not JR1) at
  +0x1000; descriptors bit-identical. Checked on jiffy (flashed) and the 8MP EVK: hashing, ECDSA
  P-256 accept/reject. F-127, F-128, F-129 open.

**Hard rule (D-008):** never break wire or bpak compatibility. See `compatibility.md`.

## Next steps

1. Check the Coverity job on master (F-097, fixed in #69). Rebase `nanopi-neo-core2` onto
   master (its WIP is in a stash). F-010 (pbstate `BACKUP_PART`), F-109 (empty LOG bodies).
2. Unit tests for every core module are done (D-007). They found F-068..F-074; fixed F-069
   (positive error codes) and F-073 (UB in `bio_set_hal_flags`). Branch `jiffy-fixes` (on
   `jtag-debugging`) fixes F-057, F-058..F-062, F-072, F-085, F-086, F-049 (no compile warnings
   left), F-088, F-082 (checked on jiffy), `plat_get_boot_source()` (D-009) in
   `board status` (jiffy: eMMC slot or USB), and `board command dump-rom-log` (NXP AN12853).
   Next from the review: F-055
   (erase), F-063, F-070, the rest of F-066 and F-067 (SIGPIPE, orphaned pb-host, `-m32`).
   F-071 (pb adds missing DTB properties, with spare RAM, a performance note and a jiffy test) is
   planned in the roadmap, Phase 1.
3. Compatibility guards: 1 (frozen layout) and 2 (golden bpak images) are done. Next: 3 (old PyPI
   wheels against pb-host), 4 (recorded transcripts), 5 (CODEOWNERS), older bpak CLI images.
   Then the compat fixes F-075 (erase), F-076 (dont-hash, has an xfail test), F-077 (caps 0).
   `compat-guards` also carries the jiffy HAB signing commits, to be split onto their own branch.
4. F-049 is fixed: consider `-Werror` for the host build.
   PR #65 (`high-prio-fixes`, replaces PR #64) is merged (2026-10-23): all of `jiffy-fixes`,
   F-084, F-005, F-063, F-094. Next: F-001, F-004, F-081. F-055/F-075 (erase) get their own PR.
   Branch `remove-codecov` removes Codecov (D-012); Jonas turns off the Codecov app.
5. Phase 1: security fixes F-001, F-004, each with a test (F-002 is intended, D-011).
6. JTAG: `scripts/pbdebug jiffy` (ULINK + OpenOCD + GDB, `build-and-test.md`); `--load` loads pb
   into RAM. Next: F-090, F-089, F-083, F-078/F-079, F-081, F-084.

## Open questions (for Jonas)

- Which boards are still in use? Only jiffy, imx8dxlevk and imx8mevk (placeholder DDR firmware) build (F-051, F-033).
- Keep the virtio drivers for a future QEMU smoke tier? (F-052)
- Mbed TLS: 3.6 LTS or 4.x?
- Compat floor: which oldest released host and firmware must the guards test against? F-075 shows
  v2.0.0 already broke erase for 1.x hosts.
