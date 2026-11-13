# Findings

The gaps between the documents and the code (`process.md` 5), and bugs and tech debt:
from the 2026-10-22 repo scan, the reviews since and the contracts. **Verified** means a human or
an agent read the code and confirmed it. **Reported** means it came from a scan
agent and has not been checked yet; check before acting on it.

Status: `open` / `fixed (<commit>)` / `wontfix (<reason>)`.

This file lists what is open. An entry that is fixed, rejected or closed moves to
`findings-closed.md` with its last status, and keeps its id.

## Security / correctness

| ID | Where | Finding | Check | Status |
|---|---|---|---|---|
| F-004 | `src/board/imx8{qxmek,dxlevk}/board.c:~288` | `strncmp(expected, password, length)` with `length` taken from the host accepts any prefix ("i"). `LOG_DBG` prints a password that is not NUL-terminated. Example boards, but the pattern gets copied. | Verified | open |
| F-065 | `src/boot/*`, `utils_def.h:150` | Hardening, signed input only: `CHECK_OVERLAP` can wrap; no check that load regions are in RAM or that parts don't overlap each other; load-addr meta read as u64 without a size check; `linux.c` doesn't check `fdt_totalsize <= dtb part`; `keystore_id` never compared; no anti-rollback counter. | Reported | open; the `fdt_totalsize` <= part check is done (`39276b9c`) |
| | | Checked 2026-11-12: `CHECK_OVERLAP` is gone, and placement keeps every part inside the board's | | |
| | | image data regions and off the other parts (F-254). Left: the load address is read as a | | |
| | | `uint64_t` without a look at the meta's size, and `keystore_id` is only logged. Anti-rollback | | |
| | | is `roadmap.md`'s | | |
| F-066 | `src/lib/der_helpers.c` | `der_ecsig_to_rs` has no callers and doesn't left-pad short r/s. `der_ec_public_key_data` memcmp with a data-supplied length (0 matches anything, and an OID longer than the reference reads past it); `s -= 2` can underflow. | Reported | partly fixed (`jiffy-fixes`): `der_ecsig_to_rs` now pads short r/s and is used by the CAAM (F-057). `der_ec_public_key_data` is still open |
| | | 2026-11-13 (`ab002f9f`): `der_ec_public_key_data()` takes the key's length and reads nothing past | | |
| | | it; OIDs match on length and bytes. What is left of F-066 is in F-410 (the point's form and length) | | |
| F-076 | `src/boot/image_helpers.c` (payload hash and load) | bpak compat (D-008): punchboot hashes every part, while upstream `verify.c` skips parts with `BPAK_FLAG_EXCLUDE_FROM_HASH` (`bpak add --set-flag dont-hash`). Such images verify with the bpak CLI but fail to boot in punchboot. Supporting the flag only accepts more images, so it is a compatible fix. | Verified (code) | open |
| | | Decided (D-035, `security.md` 4.2): an image with a `dont-hash` part is refused, before the | | |
| | | load, with its own error. Not loaded unhashed as upstream does, and not skipped | | |
| F-078 | jiffy (hardware / boot ROM) | An SRST reset from the JTAG adapter (OpenOCD `reset`, SRSR = 1, POR) makes the boot ROM fall back to the serial downloader (`15a2:007d`) within a second, instead of booting pb from eMMC. A second clean SRST pulse did the same. A watchdog reset does too (SRSR = 0x11, after pb without WDBG was halted over JTAG, and after the `auth token` hang below). Earlier resets when the ULINK was plugged in did reboot into pb, and a power cycle does. Cause unknown. Suspect: pb leaves the eMMC in 8-bit DDR52, and a warm reset doesn't reset the eMMC. Other options: HAB, or the reset itself. `src/board/jiffy/openocd.cfg` has no `reset_config` because of this. | Seen on the board (2026-10-22) | open: a power cycle boots pb. Forcing serial download with the boot mode buttons and resetting works (uuu loads `pb_signed_uuu.imx`), but in the downloader the ROM falls back to after a failed boot, every SDP command with a data phase (write, dcd) hangs. Hardware (jiffy R1 netlist): JTAG SRST is the `RST_N` net (POR_B, LP3470 supervisor, button PB2). The eMMC RST_n is a separate net from pad NAND_WP_B with a 46.4k pull-up, so a POR_B reset doesn't reset the eMMC (see F-079). Likely cause: the same SNVS SV1/Non-Secure state as F-080 (every warm reset after JTAG activity landed in the downloader, the one without JTAG since power-on booted; a direct test is blocked because `dev reset` needs `auth`). SNVS survives a warm reset, so HAB fails on the next boot and the ROM falls back to the downloader, where data writes then hang. A power cycle clears it. Fits every reset seen so far except one SRST at 12:26. |
| F-079 | `src/board/jiffy/board.c` `usdhc_emmc_setup` | Muxes NAND_DQS (ball E6) as the eMMC reset, but on jiffy R1 NAND_DQS isn't connected. The eMMC RST_n is on NAND_WP_B (ball D5, `eMMC_RST_N`, 46.4k pull-up), which pb never configures, so the eMMC never gets a hardware reset. Possibly related to F-078. | Verified (netlist) | open: check NAND_WP_B's USDHC1_RESET_B alt in the RM, and whether the eMMC has RST_n_FUNCTION enabled |
| F-081 | `src/arch/armv7a/cp15.S` `arch_invalidate_cache_range`, callers in `imx_caam.c` | The invalidate rounds the start down to a cache line and uses DCIMVAC (invalidate, no clean), so dirty CPU data sharing the first or last line is lost. `caam_wait_for_job` invalidates `caam.output` (0x8001a20c in the jiffy debug build), whose line also holds the tail of `struct caam` (`init`, `async_hashing`, `hash_align_buf_len`...), `wpb`, another `base`, `mmc_current_part` and `mmc_current_part_config`. Not shown to cause F-080. | Verified (code, symbol map) | open: clean+invalidate (DCCIMVAC) partial lines, or keep DMA buffers in their own cache-line-aligned sections |
| F-083 | `punchboot part read` | Fails with `Error: I/O error` on jiffy, both for BOOT1 and the 512 B PB State Primary partition, in an authenticated session. Cause unknown. | Seen on the board (2026-10-22) | open |
| | | The cause (F-190, `command-mode.md` 5.3): no board in the tree sets `READABLE` on a partition, | | |
| | | and a bulk read without it answers an I/O error. The eMMC's BOOT0 and BOOT1 on 2.8.0 the same | | |
| | | (was F-142). Left: which partitions a board lets a host read | | |
| F-087 | `src/cm/cm_main.c` `error_to_wire` | `-PB_ERR_BAD_PAYLOAD` (payload hash mismatch) has no wire result, so the host reports "Generic error". `include/pb-tools/error.h` has no payload code. | Seen on jiffy (2026-10-22) | open: a new result code is a wire change, see `compatibility.md` (old hosts must handle an unknown code) |
| F-089 | `src/lib/libc/snprintf.c` (vendored) `unsigned_num_print` | `%x` writes the digits and padding without checking the buffer size `n`, so `snprintf` with `%x` can overflow a small buffer (`%d`/`%u`/`%s` are bounded). Unsupported specifiers (`%zu`, `%2u`, `%c`, ...) spin forever instead of failing. | Verified (code) | open: `imx_rom_log.c` works around it (only writes a line with 128 bytes left) |
| F-100 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The timing copy for TF-A in `.umctl2` (NOLOAD, 0x40000000) never writes `fsp_msg`/`fsp_msg_num`, so TF-A gets whatever DRAM held. The copy's size (~12.5 KiB for the EVK) is not checked against the 128 KiB region, and nothing pins `struct dram_timing_info` to TF-A's layout. | Verified (read) | partly fixed (`imx8m-dram-cleanup`): the copy is a 128 KiB `struct dram_timing_save` and too large tables fail with `-PB_ERR_MEM`; `fsp_msg` is never read by upstream TF-A (only `fsp_table` and the tables), so harmless there; the layout pin is open |
| F-102 | `src/drivers/ddr/{umctl2,dwc_ddrphy}.c`, `src/plat/imx8m/ddr_imx8mq.c` | Every poll in the DDR init (STAT, PHY mailbox, CalBusy, DFISTAT, SWSTAT, PLL lock) spins forever. A stuck PHY or bad board table hangs the board without a log. | Verified (read) | open: timeouts need a measurement on hardware |
| F-104 | `src/drivers/ddr/dwc_ddrphy.c` `dwc_ddrphy_train` | The training results in the message block are never read. U-Boot (all i.MX8M) reads the trained critical delay differences after each 1D run and widens the rank-to-rank gaps (`DRAMTMG2` rd2wr/wr2rd, `RANKCTL`, per P-state) before the DFI init. The imx8mevk has two ranks (`MSTR.active_ranks` 3). TF-A reads these registers back at boot and restores them after retention, so it expects the bootloader to have applied the workaround. | Verified (read, and in the differential trace: U-Boot rewrites `DRAMTMG2`, `FREQ1_DRAMTMG2` and `RANKCTL` before `dfi_init_start`) | open: needs the message block layout and an imx8mevk |
| F-106 | `src/drivers/ddr/umctl2.c` | Low power/refresh values differ from NXP's current sequence: during init `RFSHCTL3` 0x11 and `PWRCTL` 0xa8 (U-Boot: 0x1 and 0xa0, its comment says `en_dfi_dram_clk_disable = 0`; `refresh_mode` 1 is a DDR4 fine granularity setting); at the end `PWRCTL` 0x08 and `RFSHCTL3` 0x10 (U-Boot: clears only `selfref_sw`, 0x80, then `RFSHCTL3` 0 and `selfref_en`, 0x81). Punchboot also writes `DFIPHYMSTR` twice, which the board table already sets. | Verified (differential trace) | open: needs an imx8mevk |
| F-107 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The trained PHY CSRs for TF-A are read after the DFI init and port enable; NXP reads them right after loading the PIE image, before the calibration wait and the DFI init. | Verified (differential trace) | open: needs an imx8mevk |
| F-110 | `src/plat/imx8m/ddr.c` `imx8m_ddr_init` (GPC) | The DDR power-up writes `GPC_PGC_CPU_0_1_MAPPING` (0x303A00EC) and `GPC_PU_PGC_SW_PUP_REQ` bit 5 (0x303A00F8), the i.MX8MQ/8MM layout, and never waits for the request to self-clear. On the i.MX8M Plus 0x303A00EC is the read-only `GPC_LPCR_CM4_AD` and 0x303A00F8 is not in the map; its PU power-up trigger is 0x303A00D8 with `DDRMIX_SW_PUP_REQ` at bit 19 (RM 5.2.10.37). NXP's 8MP DDR tool script writes the same two addresses, and the RM's own example code uses them. Harmless on the 8MP EVK: DDRMIX is powered after reset and the DRAM trains (2026-10-28). | Verified (RM, board) | open: per-SoC power-up in `ddr_imx8m{q,p}.c`, with the self-clear wait |
| F-113 | `src/drivers/mmc/imx_usdhc.c`, `src/board/imx8mpevk/board.c` (i.MX8M Plus uSDHC3) | eMMC in DDR52 (8-bit DDR) fails at the first data read after the switch (CMD8, INT status 0x208003: DCE, a data CRC error); HS200 works. 2026-10-28, after the DDR_EN fix (F-123): with the board's fixed DLL override tap 35 the CMD8 still fails the same way; with tap 0 init gets through DDR52 (EXT_CSD reads work), but the first CMD18 (GPT read) fails: the auto CMD12 response in CMD_RSP3 is 0x00d00f00 (CC_ERROR, COM_CRC_ERROR, ILLEGAL_COMMAND, state prg), which doesn't fit a read. HS200 with tap 0 fails the same way (2026-10-29) and works with tap 35: a sampling error, the DLL override is in the read path in SDR too. The right tap depends on board and silicon (tuning). | Verified (board) | open: imx8mpevk uses HS200. Next: what CMD_RSP3 holds after auto CMD12 in DDR, and a DLL setting for DDR instead of the fixed override |
| F-116 | `src/drivers/mmc/mmc_core.c` `mmc_setup` (HS400) | HS400 without Enhanced Strobe selects HS_TIMING 3 without tuning in HS200 first (JESD84-B51 6.6.2.3 steps 5-6). With `MMC_BUS_WIDTH_8BIT_DDR_STROBE` the card is switched to BUS_WIDTH 0x86, then `imx_usdhc_set_bus_width()` rejects the width. No board uses HS400. | Verified (bench, `test_mmc_emmc` `test_hs400`) | open |
| F-131 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_start` | For a buffer that isn't 4 KiB aligned, page 0 of the dTD is `align_buffer` at offset 0 with only `align_length` bytes valid, but the controller fills page 0 up to its 4 KiB boundary before page 1, so an unaligned transfer longer than `align_length` sends or receives the wrong bytes. pb's large buffers are aligned, the small ones fit in one page, so nothing hits it today. | Verified (code) | open: point page 0 at `align_buffer + 4096 - align_length` |
| F-134 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | For an OUT transfer into a buffer that isn't 4 KiB aligned, the data is copied from `align_buffer` into the buffer and then the buffer is invalidated (DCIMVAC), which architecturally discards the copy while the lines are dirty. Every command read takes this path (`cmd` at 0x81826200 in the jiffy build), yet commands work on jiffy, and the D-cache does hold dirty lines there (the F-130 breadcrumbs in `.no_init` were lost on reset without a clean, log 2026-10-29). Not understood: perhaps the Cortex-A7 performs DCIMVAC as clean and invalidate (unchecked; that would also hide F-081 on jiffy). | Verified (code, symbol map); why it works is open | open: invalidate before the copy, not after; check DCIMVAC on the board (dirty a line, invalidate, read back) |
| F-135 | `src/arch/armv7a`, `src/plat/imx6ul/plat.c` `plat_mmu_init` | Nothing sets ACTLR.SMP before the MMU and caches are enabled; as I recall it, the Cortex-A7 MPCore TRM asks for that. What it changes on jiffy is unknown: the 2026-10-29 breadcrumbs show write-back caching of `.no_init` works without it, so it doesn't simply leave Shareable memory uncached (first guess in the USB review, withdrawn). | Reported (TRM from memory, not in `~/work/manuals`) | open: read ACTLR on jiffy (`mrc p15, 0, r0, c1, c0, 1`), get the TRM |
| F-136 | `src/drivers/usb/imx_ci_udc.c` (prime, flush, bus reset) | Against 6UL RM 54.4.6: the prime loop waits for ENDPTPRIME to clear but never checks that ENDPTSTAT got set (PRIME 0 and STAT 0 is a failed prime, 54.4.6.4.2.2; the dTD then stays Active for ever, F-130's symptom). `imx_ci_udc_flush_ep` doesn't check ENDPTSTAT and repeat a refused flush (54.4.6.6.5). The EP0 flush in `poll_setup_pkt` doesn't wait before EP0 is primed again. The bus reset handler waits `while (FLUSH == 0x00FF00FF)`, which ends when the first bit clears, and flushes without waiting for ENDPTPRIME to clear (54.4.6.2.1). None of the loops has a timeout. | Verified (code, RM) | open |
| F-137 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | Only the Active bit of the last dTD is checked. Halted, Data Buffer Error and Transaction Error are ignored (an RX overflow halts the endpoint and returns `PB_OK`), and the bytes left in Total Bytes are not reported (54.4.6.6.4). A short packet in an earlier dTD of a multi-dTD read retires that dTD and leaves the last one Active. pb's exact-size protocol avoids the last today. | Verified (code, RM 54.4.6.6.4) | open |
| F-138 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_poll_setup_pkt`, `src/drivers/usb/usbd.c` | Control transfers against 6UL RM 54.4.6.4.2: the setup packet is copied under the tripwire and then acked in ENDPTSETUPSTAT; the RM acks first, so a second SETUP arriving between the two is acked unread. After priming an EP0 data or status stage nothing checks ENDPTSETUPSTAT, so a SETUP that aborts the control transfer leaves `wait_for_completion`/`imx_ci_udc_xfer_zlp` spinning until the watchdog. Unsupported requests are never STALLed (54.4.6.3.2): the host waits out its control timeout (e.g. `lsusb -v` asking for the BOS descriptor) and `usbd_xfer_complete` fails the transfer in progress. `USB_SET_IDLE` primes an OUT status stage a no-data request doesn't have. EP0 IN has ZLT disabled, so a reply that is a multiple of 64 bytes and shorter than wLength would need a ZLP it never gets (no current descriptor is). | Verified (code, RM) | open |
| F-139 | `src/drivers/usb/imx_usb2_phy.c`, `src/board/jiffy/board.c` `cm_board_init` | 6UL clocks and PHY: `imx_usb2_phy_init` writes PLL_SIC at USBPHY+0xa0, which the 6UL USBPHY doesn't have (its map ends at VERSION, 0x80; the PLL is CCM_ANALOG_PLL_USB1). jiffy only sets EN_USB_CLKS in PLL_USB1 (reset 0x00012000: BYPASS set, POWER clear) and relies on the ROM having powered, unbypassed and locked PLL3. The charger detector is on after reset (USB_ANALOG_USB1_CHRG_DETECT EN_B = CHK_CHRG_B = 0) and RM Fig. 55-3 turns it off before D+ is pulled up; pb doesn't. Works on the board. | Verified (code, RM 18.7.2, 55.3, 55.2.7) | open |
| F-140 | `src/drivers/usb/imx_ci_udc.c` | USBMODE.SDIS is set; the RM: in high speed every OUT packet is then answered with NYET, and it "substantially limits" throughput. Worth measuring for flashing speed. The init also writes ENDPOINTLISTADDR before USBMODE.CM, the RM (54.4.6.1) sets the mode first. `CMD_ITC(x)` cast to `uint16_t` after the shift, so ITC was always 0 (immediate; fine for a polling driver). | Verified (code, RM 54.6.33, 54.4.6.1) | open (ITC: `USBCMD_ITC(0)` since `5bd1d20e`, same value) |
| F-148 | `src/plat/imxrt/plat.c` `plat_init` | The clock roots depend on how the chip was reset: pb sets the ARM PLL and the periph clock muxes but not all dividers, so it inherits them. On the RT1060 EVK after a power-on boot (loaded over JTAG after the ROM) it reports AHB 150, IPG 75, PER 37 MHz; booted from the NOR after a software reset, AHB 600, IPG 200, PER 100 MHz. 200 MHz is above the IPG root's 150 MHz maximum (RT1060 data sheet). Set AHB_PODF, IPG_PODF and PERCLK_PODF explicitly. | Verified (board) | open |
| F-149 | `src/board/imxrt1060evk/imximage.cfg` | The DCD writes IOMUXC_GPR17 (0x400ac044 = 0x55555555) and sets GPR16 bit 2 to make all of the FlexRAM OCRAM. HAB rejects the write: `punchboot log hab` on the RT1060 EVK has a failure, invalid address (0x22) in a CSF/DCD command, for that write. So the FlexRAM keeps its fused split (256 KiB OCRAM); pb fits, since its buffers end at 0x202bf000. On an open part the ROM goes on; on a closed one the boot fails. Drop the DCD or set GPR17 from pb (the ROM's DCD address list doesn't allow IOMUXC_GPR). | Verified (board, HAB log) | open |
| F-153 | Linux 6.18 + TF-A v2.12 on the i.MX8M Plus | Linux powers the audio mix through the GPC itself, which leaves its AIPS5 bridge at the reset defaults (TF-A configures AIPS5 only when its own SiP call powers the domain): the first access (audio mix clock driver, MU2) takes an SError. Not pb; U-Boot doesn't set AIPS5 either. Newer kernels have `drivers/bus/imx-aipstz.c` (MPR0 = 0x77777777 on probe and runtime resume) with a node in `imx8mp.dtsi`. | Verified (EVK) | worked around: the test bench's DTS disables AIPS5 (`59569a10`) |
| F-155 | i.MX8M Plus EVK, `30be0000.ethernet` (FEC) | The FEC's PHY (address 1, reset `gpio4 2`) doesn't answer on MDIO (`MDIO device at address 1 is missing`, `Unable to connect to phy`), with the upstream DTS. The EQOS port probes. No cable was attached. | Seen on the board | open |
| F-156 | Linux 6.18 on the i.MX8M Plus EVK, `drivers/regulator/pca9450-regulator.c` | The PCA9450 driver registers a restart handler at priority 130, above PSCI's 129, that sends the PMIC SW_RST 0x14 (a cold reset). Every `reboot` and every panic restart (`panic=5`) power cycles the DRAM and loses ramoops, whatever `reboot=` says; pb's `log linux` then shows an older record. Only a watchdog reset (WDOG_B, warm with `nxp,wdog_b-warm-reset` in the test bench DTS) keeps it: a hang, which panics Linux on the watchdog pretimeout 2 s before the reset. Not pb. jiffy has no PMIC handler and keeps it on every path. | Verified (EVK, pb fill/check and Linux pstore) | open: a test bench kernel patch that lowers the handler below PSCI, or a product DTS/board that doesn't need it |
| F-158 | `src/plat/{imx6ul,imx8m,imx8x}/plat.c` `plat_mmu_init` | `rw_nox` maps `[_zero_region_start, _no_init_end)` as one RW Normal region, which assumes `ram` and `buffers` are next to each other. On the 8MP that's 0x944000..0x40885000, about 1 GiB: unpopulated address space as Normal memory (speculative accesses allowed), the AIPS/GIC/DDRC windows (they still get Device attributes only because the innermost region wins) and a partial overlap with the static OCRAM entry, which `mmap_add_region` would reject with `ENABLE_ASSERTIONS`. On jiffy and the 8X the gap is DRAM. | Verified (code, 8MP map) | open: map `ram` and `buffers` as two regions |
| F-166 | `src/plat/*/slc_helpers.c` | The SLC state changes are checked differently per | Verified | partly fixed |
| | | platform: the 6UL locks only from CONFIGURATION, the 8X from any state. No | | |
| | | platform reads the fuses back after writing them. The rules belong in | | |
| | | `src/slc.c`. `work-3.0.0-plat`: they are; still no read-back (the 8X's | | |
| | | life cycle may only change on reset). | | |
| F-167 | `src/plat/imxrt/rot_helpers.c` | The RT1060 reports every key as valid and can't | Verified | partly fixed |
| | | revoke one, although HAB has `SRK_REVOKE` there too. `work-3.0.0-plat`: it | | |
| | | reads `SRK_REVOKE` (`imx_hab_fuses.c`); the EVK board doesn't revoke or lock. | | |
| F-171 | `testbench/simple/src/main.c` | The boot time test reported one boot of 3262 s (it fails | Plausible | open |
| | | over 1 s) and passed when run again. The time is the clock at entry minus the | | |
| | | reset time kept in the RT1060's persistent words. A series that didn't end (no | | |
| | | magic cleared) leaves them set: the next series' first boot takes the old reset | | |
| | | time. 3262 s is about the time since the previous boot time run. Not verified. | | |
| F-172 | `src/drivers/usb/cdns3_udc_core.c` (the MEK) | Writes reset the MEK now and then: a | Verified | open |
| | | small `part write` after a large one, or the first write right after the board | | |
| | | comes up ("Generic error", then the board resets, a watchdog reset into the ROM | | |
| | | loader in serial download mode). Seen with and without the EP0 fix (F-159), from | | |
| | | RAM (uuu) and from eMMC. Waiting ~5 s after it enumerates and a `dev show` before | | |
| | | the write helped. The cdns3 bulk path's TODOs (TRB count, burst, TRBERR) are the | | |
| | | suspects. A reset with garbage in the enabled boot partition leaves the MEK dead | | |
| | | until SW2 is set to serial download. | | |
| F-180 | `tools/pbstate/src/pbstate.c` | `pbstate_is_system_verified(NONE)` returns | Verified | open |
| | | `!!enable` (a copy of `is_system_active`). `pbstate_set_system_verified(X)` zeroes the | | |
| | | attempts also when X isn't the enabled system: it ends the other system's trial, and | | |
| | | the next boot rolls back. | | |
| | | 2026-11-13 (`3164cb0c`): `pbstate_set_system_verified(X)` zeroes the attempts only when X is the | | |
| | | enabled system, so confirming the fallback keeps the trial (`boot.md` 3.5). Left: what | | |
| | | `pbstate_is_system_verified(NONE)` returns (0, -EINVAL, any slot verified, or the enabled one), for | | |
| | | Jonas; it still returns `!!enable` | | |
| F-184 | `src/drivers/usb/cdns3_udc_core.c:263` | The clean covers `desc_count` TRBs, not the | Verified | open |
| | | terminating one at `trb[desc_count]`: for some counts its flags word is in a cache line that isn't | | |
| | | cleaned, and a stale TRB from a larger transfer stays in memory. The effect on the controller is | | |
| | | unverified; it fits F-172 (a small write after a large one). Clean `desc_count + 1`. | | |
| F-190 | `cm_main.c` | Low. IDENTIFIER_READ sends 23 bytes of `buffer[0]` unauthenticated (`rz` | Reported | open |
| | | not cleared), the partition table 57 of every 128. `boot_jump()` can return after `disconnect()`. | | |
| | | `bytes_to_verify -= 4096` underflows for bpak with size < 4096. A failed data read in | | |
| | | `cmd_stream_prep_buffer` still answers OK. `block_dev` starts at 0, a valid bio. No build-time bounds | | |
| | | (buffer size, 16 keys in `cmd_slc_read`, board name). No real board sets `BIO_FLAG_READABLE`: the cause | | |
| | | of F-083 and F-142. | | |
| | | `block_dev` at 0 is fixed with F-185: each session starts without a stream (`-PB_ERR_STATE`). | | |
| | | 2026-11-13 (`50bc306f`, `6f7003d1`, `e845881a`, `5e19d5e1`, `30ca9656`): the identifier and each | | |
| | | table entry carry only themselves and zeros, bpak's `part verify` refuses a size below a header, | | |
| | | SLC_READ lists 16 keys at most, the buffers' bounds are static asserts; the board's name and a | | |
| | | failed read in `cmd_stream_prep_buffer` were handled already. Left: `boot_jump()` returning after | | |
| | | `disconnect()` (F-252's second half) and `BIO_FLAG_READABLE` (F-083) | | |
| F-194 | `gpt.c:296`, `src/bio.c` | Installing a table from command mode doesn't update | Reported (probe) | open |
| | | the bio pool: until a reset the old partitions are listed and written at their old LBAs. Not documented | | |
| | | either. D-070, `bio.md` 4: in effect at once. An unchanged partition (UUID and blocks) keeps its | | |
| | | handle and flags, a changed one's handle is dead and its slot unused until the reset, a new one | | |
| | | gets a new slot; the stream ends and the policy's init runs again; no board hook runs again. For | | |
| | | the code: `bio_get_part_by_uu()` and `cmd_part_tbl_read()` stop at the first slot that isn't | | |
| | | valid; the pool needs room for both tables' partitions | | |
| F-196 | `src/drivers/fuse/imx_ocotp.c:73` | The fuse timing is hard-coded for a 66 MHz ipg_clk | Reported | open |
| | | (STROBE_PROG 0x299). The RT1060 uses the driver at 75-200 MHz (F-148): a 3-9 us program strobe instead | | |
| | | of 10 us on an irreversible write. Checked against both RMs, not on hardware. Compute TIMING from the | | |
| | | ipg rate. | | |
| F-197 | `imx_ocotp.c:69,91-104,121` | Against the 6UL RM: ERROR is cleared by a plain CTRL | Reported | open |
| | | write, not CTRL_CLR (35.5.1), so after one failed access everything is `-PB_ERR_IO` until reset; no 2 us | | |
| | | postamble after a write (35.3.1.4); the index is masked with 0x7F, not checked; a read doesn't check | | |
| | | ERROR (a locked word reads 0xBADABADA with OK). | | |
| F-198 | `src/drivers/memc/imx_flexspi.c` | The erase starts with the largest block without | Reported | open |
| | | checking the address is aligned to it, rounds a short tail up to a full block, ignores write-enable and | | |
| | | busy-wait results. Latent with the EVK's layout. IDATSZ is 16 bits: a read of 64 KiB or more is | | |
| | | truncated and waited for forever. No loop has a timeout or a watchdog kick. | | |
| F-199 | `src/drivers/mmc/imx_usdhc.c:338` | A command timeout (CTOE) returns | Reported (probe) | open |
| | | `-PB_ERR_IO`; `mmc_sd.c:281` detects SD 1.x cards by `-PB_ERR_TIMEOUT` on CMD8, so a 1.x card fails init | | |
| | | on uSDHC. `test_sdsc_v1` runs only on the HAL model. | | |
| F-200 | storage, low | `imx_hab_fuses.c:159`: a partly programmed SRK word is refused as already | Reported | open |
| | | fused. `mmc_core.c`: `mmc_wait_state()` and `mmc_bio_xfer()` (`max_chunk_bytes == 0`) can loop forever; | | |
| | | a zero BOOT_MULT gives a 2^32 block bio. `imx_usdhc.c`: ADMA2 addresses truncated to 32 bits. | | |
| | | `plat_get_unique_id()` (6UL, 8M, RT) ignores `imx_ocotp_read()` errors: the device UUID comes from | | |
| | | uninitialised stack. | | |
| | | 2026-11-13: `mmc_bio_xfer()` with `max_chunk_bytes == 0` ends with -PB_ERR_PARAM since F-182, where | | |
| | | it looped | | |
| F-201 | `src/drivers/usb/usbd.c:28` | `enumerated` is never cleared: on the 8MP EVK a replug or | Reported | open |
| | | bus reset ends in a read on an unconfigured EP2, `cm_run` exits and the board resets. Clear it in | | |
| | | `usbd_init()` and on bus reset. | | |
| F-203 | `usbd.c:340-349` | An EP0 request during a bulk transfer cancels it and restarts it from | Reported | open |
| | | offset 0: a `punchboot list` or `lsusb -v` during a `part write` corrupts the chunk and swallows the | | |
| | | next command. | | |
| F-204 | `cdns3_udc_core.c:507` | `cdns3_udc_core_stop()` is a stub: after `disconnect()` the MEK | Reported | open |
| | | stays enumerated with EP0 armed at pb's `setup_pkt`, a SETUP after the handover writes 8 bytes into the | | |
| | | OS's memory. | | |
| F-205 | USB, low | cdns3: `setup_pkt` shares its cache line with boot.c's callbacks and digest | Reported | open |
| | | (invalidated per SETUP, F-081 class); `trb[128]` and `align_buffer` bounds; four waits without timeout. | | |
| | | dwc3 rounds an OUT TRB up to the max packet: up to 511 bytes past a part in a RAM boot. | | |
| | | `imx_ci_udc.c:193`: `align_length` never cleared (F-131). `imx_salvo_phy.c`: register table from U-Boot | | |
| | | (GPL-2.0+) in a BSD file (F-108 class). | | |
| F-206 | `src/drivers/wdog/imx_wdog.c`, `plat/imx6ul/plat.c:385` | `CONFIG_ENABLE_WATCHDOG=n`: | Reported | open |
| | | `base` stays 0, kicks and `imx_wdog_reset_now()` write at address 0 and `plat_reset()` spins (every | | |
| | | defconfig has it on). WT is `delay_s * 2` unmasked: 5.5 s for 5, 128 s wraps. `reset_now` also clears | | |
| | | WDA. | | |
| F-207 | `src/arch/*/entry*` | The D-cache is enabled without invalidating it first (v7a, v7m, | Reported | open |
| | | v8a). The 6UL and RT ROMs use the caches and only disable them (RMs 8.4.4, 9.5.4); a stale valid line | | |
| | | would serve old data. Plausible, jiffy works. Invalidate by set/way before enabling. | | |
| F-209 | `src/link.lds:60-67` | The stack is directly above `.keystore_key`; on the RT1060 (no | Reported | open |
| | | MPU region, 4 KiB stack, software ECDSA) an overflow rewrites a root-of-trust key silently. | | |
| | | `.vector_handlers` is an orphan section placed after `_code_end`, outside the overlap check (F-157). | | |
| F-210 | arch/plat, low | v7m: CACR.FORCEWT never cleared at the handover, VTOR never set to pb's | Reported | open |
| | | table. 6UL: PLL1 raised without writing CACRR.ARM_PODF (F-148 class), SRSR never cleared (stale reset | | |
| | | reason). Exception policy differs per arch, v8 handlers run on the faulting stack and hang by default. | | |
| | | 8X `BOARD_RAM_END` maps non-RAM. `armv7a/arch.c:37`: clean then invalidate of the live stack in C. | | |
| | | `imx_uart.h`: `UFCR_RFDIV_1` is divide by 2. | | |
| | | D-053: pb doesn't clear the reset reason (SRSR), the OS reads the same register; that item | | |
| | | is closed | | |
| F-212 | `src/board/imx8dxlevk` | The DXL board offers `slc lock`, but its build can't sign pb | Reported | open |
| | | (`imx8xl_sign_image`: not supported): a locked DXL rejects the only image there is. Drop | | |
| | | `set_configuration_locked` until signing exists. | | |
| F-213 | `configs/*`, board `makefile.mk` | The SRK hash that is fused (`CONFIG_SRK_FUSE`) and | Reported | open |
| | | the SRK table pb is signed with are independent settings: a product that changes one fuses a hash that | | |
| | | closes the device to its own images. D-027 and `srk_fuse.h` claim they match. Derive or check at build | | |
| | | time. | | |
| F-214 | `src/board/imxrt1060evk` | `BOARD_IMXRT1060EVK_REV_C` only swaps the FCB; the NOR config | Reported | open |
| | | stays IS25WP064 (ID, 0xD7 erase, 8 MiB), so the probe fails on an EVKC's W25Q128JW. Untried on hardware. | | |
| F-217 | `src/board/host/board.c`, `test_fuse_bio.c` | The host board's fuses are on a GPT | Reported | open |
| | | partition and load in `late_init`: with a bad GPT a locked host device reads as open, the opposite of | | |
| | | real boards, so the D-026 order can't be tested end to end. | | |
| F-218 | boards, low | `IMX8X_FUSE_DRY_RUN` doesn't gate | Reported | open |
| | | `sc_seco_commit()`/`forward_lifecycle()`. The shared `default_command` changed the 8X boards' answer | | |
| | | from OK to NOT_SUPPORTED (`board.md` 8: fixed for a product in the field). Board data isn't | | |
| | | validated, `CM_AUTH_PASSWORD=y` without a callback: moved to F-249. Dead Kconfig symbols | | |
| | | (`ARMV8_EL3`, `choice BOARD`, `IMXRT_FLASH`). GPT tables can't align partitions. 8X | | |
| | | `early_boot()` ignores SCU results. | | |
| F-220 | security core, low | `caam_hash_input()` refills the alignment buffer while its async | Reported | open |
| | | job may read it (wrong digest, fails closed). `rot.c:117,134` return error codes through unsigned types: | | |
| | | `slc read` loops ~2^32 times after a failed `rot_init()`. Parts are 512-aligned, a 4 KiB block device | | |
| | | fails mid-load on a second part. `mbedtls_pb.c:183` skips `mbedtls_pk_free()`. `slc_set_configuration()` | | |
| | | goes on when the state read is NOT_SUPPORTED. D-068 (Jonas): no read at a byte offset in `bio` for | | |
| | | now, so the 4 KiB case stays a limit (`bio.md` 3.1, `roadmap.md`) | | |
| | | 2026-11-13 (`997f6a8a`, `58d39d9d`): the CAAM's block buffer isn't refilled under its job, and the | | |
| | | key's context is freed when it doesn't parse (nothing leaked with Mbed TLS 3.4.0). Left: `rot.c`'s | | |
| | | error codes through unsigned types, `slc_set_configuration()` on NOT_SUPPORTED, the 4 KiB limit | | |
| | | (D-068) | | |
| F-226 | `tools/punchboot/transport.py:39` | One fixed 10 s USB timeout; the device's caps | Reported | open |
| | | timeouts are ignored. After a timeout the session stays usable and reads the late result as the next | | |
| | | command's. Several error paths leave transfers unread the same way. | | |
| | | 2026-11-13 (`f43615bb`): a failed transfer ends the host's session, and the USB transport takes the | | |
| | | caps' `operation_timeout_ms` once the caps are read. Left: `part_erase_timeout_ms` (F-375) | | |
| F-227 | host tools, low | `session.py:556` masks the verify size to 32 bits. With several | Reported | open |
| | | devices and no `-u` the first is used, also for `slc` commands. Three different is-this-bpak checks. The | | |
| | | CI's only-grows guards compare with the merge-base, which is HEAD on a push to master. | | |
| | | 2026-11-13 (`cbc47194`, `997a705e`, `229c0c71`): a verify above 4 GiB is refused before anything is | | |
| | | sent, `part verify` checks for a bpak as `part write` does, the CI guard compares a push with its | | |
| | | previous head. Left: several devices and no `-u` (the first is used, also for `slc`), for Jonas | | |
| F-228 | `.github/workflows`, `docker/`, `scripts/ci` | Any `v*` tag publishes to PyPI with no | Reported | open |
| | | check against `version.txt`, master or the tests. Actions are pinned by tag, the publish action gets a | | |
| | | long-lived token. The dev image is partly pinned and rebuilt per job. CI builds only `pb.bin` (never the | | |
| | | signed images or `testbench/simple`), warnings don't fail. | | |
| F-230 | `doc/guides/testing.md`, `status.md` | Line coverage is 91.0% (4445/4886), not | Reported (probe) | open |
| | | 99.2%. `imx_caam.c` 38%: `caam_ecda_verify` and the hash path 0%. Never compiled by a host test: | | |
| | | `boot/atf.c`, `boot/simple.c`, `usbd.c`, `pb_dev_cls.c`, `partition/static.c`. No tests: `imx_ocotp.c`, | | |
| | | `imx6ul/hab.c`, the 8X RoT/SLC helpers, the UDCs, `imx_flexspi.c`. | | |
| F-231 | build, low | Pre-release builds identify as 3.0.0. `blobs/` is shared in the source root | Reported | open |
| | | (pb relinks every make). pbstate is built without sanitizers. `-fno-stack-protector`, no `-Wundef`. | | |
| | | `.gitignore` ignores `*.bin` globally, matching tracked pki files. | | |
| F-232 | docs | The README (also the PyPI page) describes removed things (PBI, C parts and binary | Reported | open |
| | | wheels, the auth cookie, Bebop/PICO) and lacks the 8MP EVK and RT1060. Missing: a porting guide for | | |
| | | 3.0.0, firmware build and image signing, the A/B update flow and pbstate, the SLC and fusing procedure, | | |
| | | a changelog, SECURITY.md. Nothing on storage, USB or exceptions (the overview that lacked them is | | |
| | | removed, D-062); `roadmap.md` phases 0-1 are done; `testing.md` lacks 12 unit tests; `imx8m-ddr.md` | | |
| | | step 4. | | |
| F-236 | `plat_boot_reason` | `src/plat/*/plat.c`: No common meaning: the 6UL's own enum, | Verified (code) | open |
| | | the 8X's SCFW value, the RT's raw WRSR bitmask (several bits set: "?"), the 8M not implemented, the host 0. | | |
| | | `platform.md` 3: `plat_get_boot_reason()` with a kind, the raw value and a name | | |
| F-238 | `plat/{imx8m,imxrt}/plat.c` | The tick and `log_time_start()` are set up in | Verified (code) | open |
| | | `plat_init()`, after `board.console_init()`: before it there is no `pb_delay_*()` and the log time is 0. | | |
| | | The 6UL and the 8X do it in `plat_early_init()` | | |
| F-239 | link scripts | `PB_ENTRY` repeated as a constant in | Verified (code) | open |
| | | `src/plat/{imx6ul,imx8x}/link.lds` and `src/board/imxrt1060evk/link.lds` (0x80001000, 0x80020000, | | |
| | | 0x20249000); the RT's `MEMORY` is in its board. `buffers` fixed in the platforms, `BOARD_RAM_*` only `-D` | | |
| | | flags, `PB_RAM_LENGTH` set by hand although it follows from the ROM's load window and `PB_ENTRY`. Nothing | | |
| | | checks at link time that pb is where the ROM may load it, `buffers` in working memory, or that they don't | | |
| | | overlap. `platform.md` 5.1: Kconfig values, `BOARD_DRAM_*`, the `pb` region, `ASSERT`s | | |
| | | 2026-11-13, from F-259: the Firmware Handoff region is checked at link time against `ram` and | | |
| | | `buffers` only; that it lies in working memory needs the board's DRAM here too | | |
| F-240 | `src/board/*/makefile.mk` | The SoC's packaging and signing are in the boards: | Verified (code) | open |
| | | jiffy holds the 6UL CSF recipe (IVT at `PB_ENTRY - 0xC00`, uuu's 0x00910000), the MEK and DXL repeat the 8X | | |
| | | recipe. `FINAL_OUTPUT` is set with `$(eval)` in recipes. Signing inputs are Kconfig on the 6UL, make | | |
| | | variables on the 8X. `platform.md` 6.2 | | |
| F-241 | `plat/imx6ul/plat.c` | `tzasc_init()`: A failure is only logged: nothing tells | Verified (code, RM) | open |
| | | the boot driver that the TZASC is bypassed. Region 1 gets the physical DDR base, while 6UL RM 52.3 says the | | |
| | | TZASC sees the DDR controller's "local addresses": check with OP-TEE that its memory is protected. GPR9 has | | |
| | | no lock bit (RM p. 1277), so the secure OS's CSU policy decides whether the normal world can bypass it; the | | |
| | | TZASC_ENABLE fuse (RM p. 209) makes the ROM enable it. GPR1 TZASC1_BOOT_LOCK unread (TZC-380 TRM, DDI0431, | | |
| | | not on the share) | | |
| F-242 | `src/arch` | `src/arch/armv8a/Kconfig`, `src/arch/{armv7a,armv8a}/asm_macros.S`: | Reported | open |
| | | Leftovers: the Kconfig help mentions sunxi FEL (no sunxi platform), a TF-A stack macro calls | | |
| | | `plat_my_core_pos`, which no platform defines (the macro seems unused) | | |
| F-320 | `doc/interfaces/platform.md` 3.3, `src/boot/boot.c` | Stretches without a kick on the boot | Verified (code) | open |
| | | path that aren't in 3.3's list of what `CONFIG_WATCHDOG_TIMEOUT` must be longer than. Since F-243 the | | |
| | | kicks are: after each chunk, and before the jump. (1) 3.3 doesn't say whether a chunk's kick comes before | | |
| | | or after it. It is after, so the first stretch is arming to the first chunk plus that chunk: two entries | | |
| | | of the list, added. (2) Between two candidates (F-256): the failed one's last hash, the policy's report | | |
| | | and next (A/B writes its state), the next header's read and signature, its first chunk. A candidate that | | |
| | | fails before its first chunk (the header, the key, the signature, placement) has no kick at all, and a | | |
| | | chain policy adds one such stretch per candidate. (3) After the last chunk: the final hash, the payload's | | |
| | | check, prepare, `late_boot()`, `plat_lockdown()`; 3.3 names them, its list doesn't. Measured, when the | | |
| | | hardware answers (the README, `testbench/README.md`, `testbench/simple/README.md`): the header's read and | | |
| | | ECDSA P-256 7 ms on jiffy and the 8MP EVK (CAAM), 25.9 ms on the RT1060 (Mbed TLS, software), the | | |
| | | signature alone 1.3 ms on the MEK; the last chunk's SHA-256 9 and 8 ms; pb's init 27 ms on jiffy, 91 ms on | | |
| | | the 8MP EVK. Not measured: the policy's state writes, prepare with a device tree. All far below 5 s; what | | |
| | | isn't is a driver's wait that runs to its bound (F-321). To decide: the list names these stretches, or the | | |
| | | loop kicks once per candidate. `boot_image_copy_and_hash()` (no caller, kept by D-020) copies and hashes a | | |
| | | whole part at once: no chunk to kick for | | |
| F-321 | `src/drivers/mmc/imx_usdhc.c`, the defconfigs | One chunk's read may wait longer | Verified (code) | open |
| | | than the watchdog. The uSDHC's data wait is `USDHC_BUSY_TIMEOUT_us` (5 s) plus 1 us per byte, and a chunk | | |
| | | is one request: 6.0 s for 1 MiB (jiffy, imx8mpevk), 9.2 s for 4 MiB (imx8qxmek, imx8mevk, imx8dxlevk), | | |
| | | against a `CONFIG_WATCHDOG_TIMEOUT` of 5 s (the DXL: 10 s). A card that stops answering in a read ends in | | |
| | | a watchdog reset, not in `-PB_ERR_TIMEOUT` and the next candidate (`boot.md` 2), and for an A/B trial each | | |
| | | reset spends an attempt. `drivers.md` 3 (D-073): the driver's header gives the longest time per unit, and | | |
| | | the core asks in pieces that fit the watchdog with room to spare; `platform.md` 3.3: the board states one | | |
| | | chunk on its slowest medium. No header and no board states either, and `CONFIG_BOOT_LOAD_CHUNK_kB` was | | |
| | | chosen for speed. When the card answers, by the measured rates: a chunk takes 13 ms on jiffy (77 MiB/s), | | |
| | | 12 ms on the 8MP EVK (87 MiB/s), 23 ms on the MEK (173 MiB/s). Read in the code, not run on a board | | |
| F-322 | `src/plat/imx8x/plat.c` | Low. The 8X's `plat_wdog_kick()` is a call to the SCU | Reported | open |
| | | (`sc_timer_ping_wdog()`, a message each way), since F-243 once per 4 MiB chunk and before the jump: a | | |
| | | handful in a boot of the MEK's 15 MB image. `platform.md` 3.3 has the kick cheap; the i.MX WDOG's is two | | |
| | | register writes, the SCU's isn't measured. To measure on the MEK (`scripts/boot-time-image`). With | | |
| | | `plat-imx.md` 7: the 8X's kick before arming is unchecked | | |
| F-246 | `src/plat/imx6ul/plat.c` | The AIPS-TZ bridges are never programmed: the 6UL | Verified (on jiffy) | open |
| | | RM (14.6) says they "should be programmed before use", reset MPR 0x77000000 and OPACR 0x44444444 | | |
| | | (supervisor only, masters beyond the first two untrusted). U-Boot opens them (`init_aips()`); OP-TEE's code | | |
| | | read doesn't. Seen on jiffy: `devmem 0x021BC050` from Linux's user mode is a bus error ("external abort on | | |
| | | non-linefetch"). Check what else Linux misses (DMA masters), then set them in `plat_init()` (`platform.md` | | |
| | | 4.5) | | |
| F-247 | `src/board/*/board.c` | The boards wire the SoC's RoT and SLC functions | Verified (code) | open |
| | | (`board.rot`, the `slc_config` callbacks) and set up the crypto engine and its RNG (`imx_caam_init()`, the | | |
| | | RNG4 instantiation), while the RT's `plat_init()` sets up mbedtls. D-030: `plat_rot`, `plat_slc` and the | | |
| | | crypto engine in `plat_init()` behind a Kconfig option (`platform.md` 2.2, 4.1, 4.2) | | |
| F-248 | `src/boot/boot.c`, `src/plat/*/plat.c` | A failed security precondition only logs | Verified (code) | open |
| | | and the boot goes on: a bypassed 6UL TZASC (`tzasc_init()`), an 8X watchdog that didn't arm. Nothing | | |
| | | records them for the boot or command mode. `platform.md` 2.3: `plat_get_status()`, checked by `boot()`, no | | |
| | | silent boot; the policy is `security.md` 4.5 (D-036). What pb booted without goes into the | | |
| | | handover (`system.md` 3, `boot.md` 5.6: `pb,status` and a `status` field; the encoding is open) | | |
| | | D-050: a second entry, `board-degraded` (bit 1), set by a board with `board_report_degraded()` | | |
| | | (`board.md` 2.3) | | |
| F-251 | `src/board/{imxrt1060evk,imx8qxmek,imx8dxlevk}` | The serial download | Verified (code) | open |
| | | check is in the boards: the 8X reads the SCU's boot type in `early_boot`. The RT had two in a | | |
| | | `late_boot` that was never called, removed with F-264 and to come back in its | | |
| | | `plat_get_boot_source()`: SBMR2's BMOD not zero (an EVK set to internal boot reads 0b10 there, so | | |
| | | BMOD alone doesn't say serial download), and a blank IVT (0 or 0xffffffff) at the NOR's start, for | | |
| | | a board with fused boot that was loaded over serial. `plat_get_boot_source()` is `-PB_ERR_NOT_SUPPORTED` on the | | |
| | | RT, 8X and 8M; the 6UL | | |
| | | reports USB only when the ROM's USB was addressed, a UART download as UNKNOWN. `board.md` 5.1: | | |
| | | `CONFIG_CM_ON_SERIAL_DOWNLOAD` (default y) in `boot()` before `early_boot()`, the platforms report | | |
| | | it, the boards drop theirs. jiffy and the 8MP EVK then stop in command mode after a `uuu` load | | |
| | | instead of booting from storage | | |
| F-252 | `src/cm/cm_main.c` | A stop in a boot command mode started: `-PB_ERR_ABORT` | Verified (code) | open |
| | | is command mode's "leave and reset" (`device reset`), so a hook's stop in `boot part` or | | |
| | | `boot bpak` resets the board (the 8X's button, which stops these boots on purpose). After the | | |
| | | transport's disconnect, a failed `late_boot` or `plat_lockdown()` other than ABORT returns to the | | |
| | | loop, which reads from the disconnected transport (unchecked). `board.md` 5.4: before the | | |
| | | disconnect the host gets the error and command mode goes on, after it pb resets | | |
| F-390 | `src/plat/imx8m/link.lds`, `src/plat/imx8x/link.lds` | Not everything pb links is in | Verified (code) | open |
| | | `pb` or `buffers`, which `platform.md` 5.3 rests the check of a board's image data regions on | | |
| | | (F-254): the i.MX8M's DDR controller state (`.umctl2`, 128 KiB at the start of the DRAM, 0x40000000) | | |
| | | and the 8X's reset stub (0x80000000, BL31's place once it ran) have regions of their own. A board | | |
| | | of the 8M that lists the DRAM from its start passes the check, and an image is loaded over the | | |
| | | controller's state. The boards in the tree start above it (0x41060000). Link it inside `buffers`, | | |
| | | or give the check the platform's regions. With F-239 | | |
| F-391 | `src/plat/{imx6ul,imx8x}/link.lds`, `src/board/*/board.c` | Low. The lengths the link | Verified (code) | open |
| | | scripts give `pb` and `buffers` reach into memory the boards' images use: jiffy's `buffers` are 16 MiB | | |
| | | to 0x82021000 and its kernel is at 0x82000000; the 8X's `pb` is 16 MiB to 0x81020000 and the MEK's | | |
| | | kernel is at 0x80200000. So the boards' image data regions (F-254) start inside those lengths, above | | |
| | | what is linked today (8 MiB, 200 KiB), and a pb that grows past a region's start is found by | | |
| | | `board_setup()` on the board, not by the linker. The regions also repeat the link scripts' numbers | | |
| | | (D-055 names the cost). One source for both, and lengths that end where the board's first region | | |
| | | starts. With F-239 | | |
| F-392 | `src/board/{jiffy,imx8qxmek,imx8dxlevk,imx8mpevk,imx8mevk}/board.c` | The boards' image | Verified (code) | open |
| | | data regions (F-254) leave out DRAM that nothing of pb's is in and that every release loaded into: | | |
| | | jiffy 0x80038000 to 0x80101000 and 0x81847000 to 0x82000000; the MEK and the DXL 0x80053000 to | | |
| | | 0x80080000 and 0x85826000 to 0x86000000; the 8M boards 0x40020000 to 0x40060000 and 0x40885000 to | | |
| | | 0x41060000 (what is linked on 2026-11-12). It is the room pb and its buffers grow in, and D-055 | | |
| | | names the cost; the test benches' images are outside it. An image in the field with a part there | | |
| | | is refused now (D-008). For Jonas: keep the room, or start each region at the end of a link | | |
| | | length that is made to fit (F-391), so that the linker holds the limit. Found by the review of F-254 | | |
| F-259 | `src/boot/simple.c` | The generic handover passes r0 = the system's name, | Verified (code) | open |
| | | r1 = the boot partition's UUID, and can't grow. `boot.md` 5.4: Arm's Firmware Handoff, a transfer | | |
| | | list in a board region with the image's DT or pb's structure; the tag requested in the | | |
| | | specification (Jonas); the simple target test bench follows. `BOOT_FH`, the region | | |
| | | `BOOT_FH_REGION_BASE` and `_SIZE`; the checksum against the reference library | | |
| | | 2026-11-13: written, reviewed and verified (74 of 74, the sanitizers, six boards, the simple | | |
| | | target's build) on the branch `land-p10`, nine commits on `a793102b`; not on `work-3.0.0-mega` yet, | | |
| | | because it changes `configs/imx8mpevk_defconfig`, which has an uncommitted change of Jonas's. Not | | |
| | | run on the RT1060. The tag of pb's structure is a placeholder from the non-standard range (0xfffb00) | | |
| | | 2026-11-13: landed (`6e40e034..b1186de2`): 76 of 76 at `b1186de2`, also with the sanitizers, and the | | |
| | | six boards build; on the RT1060 EVK the simple target's suite passes 7 of 7 with the application's | | |
| | | `fh-*` checks. Left: the tag (the marker in `boot.md` 5.4) | | |
| F-261 | `plat/{imx6ul,imx8m}/plat.c` | The TrustZone filter's open region is the SoC's | Verified (code) | open |
| | | whole DRAM window, not the board's DRAM: 2 GiB on the 6UL (`tzasc_init()`), 4 or 8 GiB on the 8M | | |
| | | (`tzasc_open_dram()`). With less DRAM than the window, the copies of DRAM inside it are open to | | |
| | | the normal world until a secure OS replaces the region. `platform.md` 4.3, item 3: the board's DRAM | | |
| | | (`BOARD_DRAM_*`, F-239) | | |
| F-263 | `src/slc.c`, `src/plat/*`, `src/cm/Kconfig` | EOL: only the host platform implements `set_eol`, and | Verified (code) | open |
| | | nothing lets a product leave it out. `system.md` 4: a Kconfig option, off by default; every port | | |
| | | implements it as the SoC's own end of life state (NXP: field return) or says why it can't | | |
| F-325 | `doc/interfaces/board.md` 5.3, `include/boot/image_helpers.h` | What 5.3 doesn't say | Verified (code) | open |
| | | about `boot_image_get_bpak_header()`, and what the code does since F-266. (1) Its header: | | |
| | | `boot/image_helpers.h`, with the format's other functions, so a board that asks for the header | | |
| | | also gets the core's verification functions (`boot_image_auth_header()`, | | |
| | | `boot_image_load_and_hash()`); 6 names only `<bpak/bpak.h>`. A header of the format that is for | | |
| | | boards comes with the format's interface (`boot.md` 4.4). (2) After a candidate failed in | | |
| | | prepare, and after a boot that `late_boot()` or the lockdown stopped: NULL, read from "valid | | |
| | | from prepare to the jump", so command mode and the next boot's `early_boot()` get none. (3) It | | |
| | | is there from the payload's hash on, which in a `boot bpak` is while command mode sends its | | |
| | | last answer, before prepare; nothing of the board's runs there. (4) The pointer isn't kept: | | |
| | | the next candidate's header is read into the same memory, so a pointer a DT hook kept from a | | |
| | | candidate that failed later in prepare points at unverified bytes while the next one is | | |
| | | loaded. For the contract: say (1) to (4) in 5.3 | | |
| | | From the review of F-266: the header handed out has its signature cleared (`signature` zero, `signature_sz` 0): those are the bytes that were signed, and the text should say so | | |
| F-326 | `include/boot/boot.h`, `include/pb/board.h`, `src/board/imxrt1060evk/board.c` | bpak | Verified (code) | open |
| | | is still named where nothing of it is used, since F-266: `boot/boot.h` includes | | |
| | | `<bpak/bpak.h>`, so every board that includes it for `boot_get_flags()` gets the format's | | |
| | | header (D-047: a board that doesn't ask for the header doesn't include it); `pb/board.h` | | |
| | | forward declares `struct bpak_header`; the RT's board includes `<bpak/bpak.h>` and | | |
| | | `<boot/image_helpers.h>`. Taking the include out of `boot.h` needs a look at who relied on it | | |
| F-269 | `include/pb/board.h`, `src/plat/*/plat.c`, `src/plat/*/makefile.mk` | A board | Verified (code) | open |
| | | can't map hardware the platform didn't: the tables are built by each platform's | | |
| | | `plat_mmu_init()` from its own `imx_mmap[]`, and `MAX_MMAP_REGIONS` and `MAX_XLAT_TABLES` are | | |
| | | `-D` flags of the platform's (32 each). D-054, `platform.md` 5.2, `board.md` 2.8: `mmap` in | | |
| | | `struct board`, a list of `PB_MAP_DEVICE`, `PB_MAP_MEMORY_RO` and `PB_MAP_MEMORY_RW` regions | | |
| | | (`include/pb/mmu.h`), mapped by `pb_mmu_init()`, which refuses a region that overlaps pb, the | | |
| | | board's DRAM or a region of the platform's; the two numbers become Kconfig values. With F-158 | | |
| F-270 | `src/board/{imx8qxmek,imx8dxlevk}/board.c`, `include/plat/imx8x/imx8x.h` | The | Verified (code) | open |
| | | two 8X boards include the arch's headers (`arch/arch_helpers.h`, `arch/armv8a/timer.h`) and the system | | |
| | | controller's API by its own path (`sci/svc/{misc,pad,pm,seco}/api.h`, found through the | | |
| | | platform's `-Iinclude/plat/imx8x/`). `imx8x.h` includes only `sci/ipc.h` and the pad service. | | |
| | | D-056, `platform.md` 7: the API stays with the platform; `imx8x.h` includes the services boards | | |
| | | use (pm, misc, seco, pad), the boards include `imx8x.h` only, and the arch comes through | | |
| | | `include/pb/arch.h`. Out-of-tree 8X boards change their includes, not their calls | | |
| F-280 | `src/boot/image_helpers.c` `boot_image_load_and_hash()`, the USB drivers | `boot | Reported | open |
| | | bpak`: the device reads a part `CONFIG_BOOT_LOAD_CHUNK_kB` at a time (1 MiB on the imx8mpevk | | |
| | | and jiffy, 4 KiB on the RT1060 with a 32 KiB buffer, 4 MiB elsewhere), hosts send it in | | |
| | | transfers of the caps' `chunk_transfer_max_bytes` (the buffer's size, 4 MiB by default). | | |
| | | Where the two differ, one host transfer fills several reads. `command-mode.md` 4.4 and 6 | | |
| | | (D-060) ask the transport for that. Read in the code, not run: try `boot bpak` on the | | |
| | | imx8mpevk and jiffy. The requirement is `usb.md` 2.3 | | |
| | | 2026-11-12, on the bench, pb `64d5bba0`: `boot bpak` loads, verifies and starts Linux on the 8MP EVK | | |
| | | (DWC3, 15 MB) and on jiffy (ChipIdea, 5.6 MB), pieces of 1 MiB against transfers of 4 MiB. Left: the | | |
| | | cdns3 (the MEK's two sizes are equal) and the RT1060 (4 KiB pieces) | | |
| F-281 | `src/drivers/{mmc,virtio,usb}` | Waits without a bound that no finding | Reported (read) | open |
| | | names: `mmc_rw()`'s wait for the card's state after a transfer and the same loop in | | |
| | | `mmc_set_ext_csd()`; virtio block's completion; the cdns3 glue's wait for power-on-ready. With | | |
| | | F-102, F-136, F-198, F-200 and F-205 they are the list. `drivers.md` 3: every wait has a bound | | |
| | | with its source and returns `-PB_ERR_TIMEOUT` | | |
| F-282 | `src/arch/*/include/arch/arch.h`, the DMA drivers | The DMA rule has | Verified (code) | open |
| | | no place in the code. Each driver calls `arch_clean_cache_range()` and | | |
| | | `arch_invalidate_cache_range()` itself, through the arch's own header (`<arch/arch.h>`; | | |
| | | `include/pb/arch.h` doesn't declare them), with its own alignment (64, 256, 4096, none) and | | |
| | | order. `arch_clean_invalidate_cache_range()` is declared for ARMv7-A and defined only for | | |
| | | ARMv8-A; ARMv7-M has none. A comment in `cache-ops.S` cites a note in `include/pb/arch.h` that | | |
| | | isn't there. Devices write into memory that shares its lines: the CAAM's digest on the user's | | |
| | | stack, the A/B state's blocks, an image part that starts inside a line. `drivers.md` 4: | | |
| | | `include/pb/dma.h` (`PB_DMA_ALIGN`, a macro for a buffer that owns its lines, `dma_out()`, | | |
| | | `dma_in_begin()`, `dma_in_end()`); the class's core refuses a caller's buffer that doesn't | | |
| | | own its lines, and the boot's load loop bounces an image part whose load address doesn't | | |
| | | (D-066), with a host test that loads an image with such an address. The root of F-081, | | |
| | | F-134, F-184, F-205 and F-220 | | |
| F-283 | `src/bio.c`, its users | `bio` against `bio.md`. A length that | Verified (code) | open |
| | | isn't whole blocks goes to the driver as it is (F-182). A length of 0 reaches the driver, and | | |
| | | the MMC's returns `-PB_ERR` (`mmc_bio_xfer()` starts with `rc = -1`). No block buffer, no | | |
| | | largest block size, and a buffer isn't checked for its | | |
| | | lines (F-282). `bio_allocate_parent()` doesn't check the range against the parent (F-193). | | |
| | | Nothing releases a table's partitions (F-194) or keeps a driver's second `init` from | | |
| | | registering its devices again. 512 is written out in `gpt.c` (four places), and | | |
| | | `ab_state.c` writes the state's 512 bytes as a block of any size. `bio.h` says `-PB_ERR_IO` | | |
| | | for a bad parent and `-PB_TIMEOUT`; the code returns `-PB_ERR_PARAM`. No conformance suite: | | |
| | | the RAM disk and the host's disk write exact byte counts, the drivers don't (`bio.md` 8) | | |
| | | D-073, `bio.md` 2.6: nothing refuses a second device with a UUID that is taken; the block | | |
| | | getters return `int`, so 2^31 blocks or more read as an error (verified in `bio.c`) | | |
| | | 2026-11-13 (`8feab2e9`, `5fae6bda`, `e3d8f37b`, `d9f0e19a`, `4d25aeca`, `f799b8e2`, `2bc5a469`): a | | |
| | | block above `CONFIG_BIO_MAX_BLOCK_SIZE` and a second device with a taken UUID are refused, a length | | |
| | | of 0 doesn't reach the driver, a second `mmc_init()` registers nothing again, the block getters are | | |
| | | 64 bits, `bio.h` names the codes, the A/B state's erase counts the device's blocks. Left: the GPT's | | |
| | | four `512` (on a 4 KiB device the entries would sit at byte 512 of LBA 1: a real 4Kn GPT, a refusal, | | |
| | | or that layout; for Jonas), and the rest of the conformance suite (erase, `write_done`, a write that | | |
| | | fails half way, the FlexSPI) | | |
| | | 2026-11-13 (`a95356ad`): one of the GPT's 512s went with the old repair path; `gpt_pmbr[512]` and | | |
| | | three divisions (`gpt_init_tbl()` twice, `gpt_read_backup()`) are left | | |
| F-284 | `src/drivers/memc/imx_flexspi.c` | A write copies `size / 4 + 1` | Reported (read) | open |
| | | words into the FIFO in its last round: 1 to 4 bytes past the caller's buffer (whether they | | |
| | | reach the flash is unverified, the data size is the exact length). A read whose length isn't | | |
| | | a multiple of 4 is refused with `-PB_ERR_INVALID_ARGUMENT`, after the command was triggered. | | |
| | | The buffer is read and written as `uint32_t *` whatever its alignment. The busy wait's | | |
| | | result is ignored after a read and after each erase. No host test (F-230). With F-198. | | |
| | | `bio.md` 3. The write's tail is verified in the code, the rest is as reported | | |
| F-285 | `src/drivers/mmc/` | What the MMC drivers leave behind isn't written | Reported (read) | open |
| | | down. PARTITION_ACCESS stays on the last partition accessed; the bus stays in the board's | | |
| | | mode (8-bit DDR52 on jiffy, HS200 on the 8M and 8X boards); BOOT_BUS_CONDITIONS is rewritten | | |
| | | at init unless `MMC_CFG_FLAG_KEEP_BOOT_BUS_CONDITIONS`; a write to a boot partition moves | | |
| | | BOOT_PARTITION_ENABLE off it first. `mmc_power_off()` has no caller. `bio.md` 5: in the | | |
| | | header, with what brings the card back for the ROM after a warm reset (F-078, F-079) | | |
| F-286 | `src/crypto.c`, `imx_caam.c`, `mbedtls_pb.c`, the users | Crypto against | Reported (read) | open |
| | | `crypto.md`. (1) `hash_init()` keeps the backend selected when its `init` fails, and returns | | |
| | | `-PB_ERR_NOT_SUPPORTED` where `crypto.h` says `-PB_ERR_PARAM`. (2) The CAAM's `init` neither | | |
| | | drains its ring nor resets `async_hashing` (F-181); a synchronous update after an | | |
| | | asynchronous one doesn't wait for the job, and an update that only fills the block buffer | | |
| | | doesn't either (F-220). (3) The CAAM's `final` ignores `length`: it stores the whole digest | | |
| | | into a smaller buffer, by DMA into the user's memory, and invalidates it (F-081). (4) A | | |
| | | signature that doesn't verify is `-PB_ERR` on the CAAM, the same as an engine error; Mbed | | |
| | | TLS returns `-PB_ERR_SIGNATURE` also for a key that doesn't parse. (5) `dsa_verify()`'s `alg` | | |
| | | only selects the backend: the key's own curve decides. `md_alg` is unused by the CAAM, | | |
| | | `md_length` unchecked by both; `auth_token` passes 64 whatever the hash. (6) The CAAM's | | |
| | | signature parser ignores the outer SEQUENCE's length. (7) No error path of any user closes | | |
| | | its hash (`image_helpers.c`, `cm_main.c`, `uuid3.c`). (8) No `stop`: the job ring's registers | | |
| | | name pb's buffers at the jump. (9) F-020 says MD5 is unused; the device's UUID is an MD5 | | |
| | | (`uuid3.c`), and `HASH_MD5_BROKEN` has no caller in the tree. (10) The RT1060's build has | | |
| | | SHA-256 and MD5 only, so a P-384 or P-521 key verifies nothing there. (11) No conformance | | |
| | | suite: no test reaches the CAAM's hash or verify (F-230). (12) Two backends that register | | |
| | | the same algorithm: the first is used, silently (D-071: refused). (13) `dsa_verify()` doesn't | | |
| | | look at an open hash (D-071: refused, in the core). (1), (3), (4) and (9) are | | |
| | | verified in the code, the rest is as reported. D-073: `md_length` is the digest size of | | |
| | | `md_alg`, checked in the core; `auth_token` is fixed in the same commit | | |
| | | 2026-11-13 (`d15aa7bf`, `a5154faf`, `b62c5527`): (1), (2) and (13) are done; (7) is superseded by | | |
| | | D-071 (a user that gives a hash up calls nothing) and (9) by `crypto.md` 5. Left: (3) `final`'s | | |
| | | length and where the engine writes, (4) the verdict's codes, (5) `alg` against the key and | | |
| | | `md_length` with the token's call (`struct dsa_ops.verify` doesn't get `alg`: a porting API change | | |
| | | or a parser in the core, for Jonas), (6) the signature's outer length (how strict: (b) and (c) | | |
| | | refuse what a release took on the CAAM, for Jonas), (8) `stop`, (10) the RT1060's hashes, (11) the | | |
| | | suite for both backends, (12) a second registration's code | | |
| F-287 | `src/drivers/usb/`, `src/cm/cm_main.c`, `src/plat/host/socket.c` | The | Reported (read) | open |
| | | transport and USB against `usb.md`. (1) Lost isn't reported: the ChipIdea and the cdns3 | | |
| | | handle a bus reset inside the driver and return PB_OK, the DWC3 fails only a bulk transfer | | |
| | | that was running; no driver looks at an unplug; command mode keeps polling after | | |
| | | `-PB_ERR_TIMEOUT` and restarts with `init` alone, without a disconnect, whose result it | | |
| | | ignores elsewhere (F-185, F-201). (2) No driver gives the bytes that arrived, and none | | |
| | | treats a short packet before the length as an error (F-137). (3) Lengths: the DWC3 masks a | | |
| | | transfer to 24 bits; the cdns3 builds no TRB for 0 bytes and still arms the endpoint | | |
| | | (unverified: `board status` with an empty text on the MEK); the ChipIdea's limit follows | | |
| | | `CONFIG_CM_BUF_SIZE_KiB`. (4) Speed: only the DWC3 asks for high speed, none acts on the | | |
| | | result, the descriptors say 512 bytes whatever the bus. (5) `usbd_init_hal_ops()` doesn't | | |
| | | check `xfer_cancel`, `poll_setup_pkt` and `ep0_xfer_zlp`, which are called unchecked. | | |
| | | `SET_CONFIGURATION`'s value isn't looked at; `SET_IDLE` is answered and then returns an | | |
| | | error. (6) Withdrawn in the final read: what the stack sends is out only and needs no | | |
| | | alignment (`drivers.md` 4.3). (7) The ChipIdea cleans only endpoint 0's queue heads, | | |
| | | `xfer_complete` ignores its | | |
| | | endpoint and returns PB_OK again when called again; the cdns3 overwrites `EP_IEN` per | | |
| | | endpoint (effect unverified). (8) The socket's `init` blocks in `accept()`. (9) Stale: | | |
| | | `test_host_usb.py`'s docstring says the cdns3 can't send its serial number (fixed, F-159); | | |
| | | F-139 names `cm_board_init`, the function is `board_cm_init`. No USB code is compiled by a | | |
| | | host test (F-230). D-073: the debug descriptor, `SET_IDLE` and a string index the class | | |
| | | doesn't have are stalled, where today they are answered (`usb.md` 3.2) | | |
| F-310 | `doc/interfaces/bio.md`, `include/pb/bio.h` | `bio.md` describes five | Verified (code) | open |
| | | functions of `bio.h` and doesn't name them: `bio_set_ios()` and `bio_set_ios_erase()` ("then | | |
| | | the callbacks", 2.1), `bio_get_hal_flags()` (2.5 names the setter), | | |
| | | `bio_set_write_done_cb()` ("a device without the callback", 3.4) and | | |
| | | `bio_set_install_partition_cb()` ("the callback is the partition table's", 3.5). Found by | | |
| | | the lint (F-288), which has them as its exceptions (`UNNAMED` in `check_docs.py`). To name | | |
| | | them is a change to a reviewed contract; the list is emptied with it | | |
| F-311 | `tests/doc/check_docs.py` | The lint excuses an entry of `platform.md`, | Verified (code) | open |
| | | `board.md` or `boot.md` that isn't in its header by any marker in the entry's text or in | | |
| | | its section's intro (`process.md` 7), and the entries have markers for what the code does | | |
| | | otherwise: 11 of the platform's 14 entries, 8 of the board's 13 and the boot's 6 would | | |
| | | pass if their names left the header. The drivers' contracts are checked with a narrow | | |
| | | rule (F-288): only a marker that the entry starts with. The eight entries of the platform | | |
| | | and the board that aren't in the code start with theirs. The boot's six are marked by | | |
| | | section 3's intro, which the narrow rule doesn't read: one rule for all needs that form | | |
| | | decided | | |
| F-291 | `src/boot/image.c`, `dtb.c` | The image handover can't boot an image with a | Verified (code) | open |
| | | DT part in place: `boot_dtb_grow()` opens the DT into itself at its load address (`dtb.c`, | | |
| | | `fdt_open_into()`) and `/chosen` is patched there, which in place is the flash. On a NOR the | | |
| | | writes are lost or fault, on the host's flash (mapped read only) pb-host would crash. Found with | | |
| | | F-253's host tests, whose images have a kernel part only. `boot.md` 4.1 and 5.2 don't say | | |
| | | what the image handover does with a DT in place; Firmware Handoff (5.4, F-259) copies the DT | | |
| | | into its region and has no such problem. Until decided: refuse it before the DT is touched | | |
| F-331 | `doc/dev/decisions.md`, `doc/architecture/boot.md` | F-292's refusal has no | Verified (code) | open |
| | | decision entry and no contract text. `compatibility.md` asks for an entry that names what a | | |
| | | security fix rejects and why: an image whose kernel, BL31, BL32 or generic entry part is empty | | |
| | | (or, for the kernel and BL31, has a load address and no part) was jumped into by every release, | | |
| | | and the bpak CLI builds one. `boot.md` 5.1 to 5.4 say where an entry is, not that it lies in | | |
| | | bytes the payload's hash covers. D-074 is the entry; the contract's sentence is still open | | |
| F-294 | `src/plat/imx8m/plat.c`, `makefile.mk`, `link.lds` | `bl_params`' memory on | Verified (code) | open |
| | | the i.MX8M isn't what `platform.md` 5.3 has: it is `plat_bl_params`, an object in `pb`'s `.data` (where | | |
| | | `atf.c` had it), with `PLAT_BL_PARAMS_SIZE` from the makefile, not a SoC region outside `pb` that the | | |
| | | header names with `PLAT_BL_PARAMS_BASE` and that the link script keeps `pb` and `buffers` off. On the | | |
| | | 8MP every byte of the OCRAM that BL31 takes them from (below 0x960000) is in `pb`'s own region, so a | | |
| | | region of its own means cutting `pb`'s: not done without a board. New with F-258: `link.lds` fails the | | |
| | | build when the object ends above the limit. The i.MX8M Quad's limit isn't known (no board of it has | | |
| | | TF-A). With F-239 | | |
| F-295 | `src/board/imx8dxlevk/board.c` | The DXL EVK moved to the TF-A delivery | Reported | open |
| | | (F-258, `boot.md` 5.3). The board has only been built: no DXL is at hand, and this tree builds no image | | |
| | | for it (the test bench has jiffy, imx8mpevk and imx8qxmek). Before, pb started the part `atf` directly | | |
| | | with x0 to x3 = 0 and named no BL33; the image needed `atf`, `dt` and `ramdisk`. Two things changed for | | |
| | | an image in the field. It needs the part `kernel` now, as BL33's entry: an image whose BL33 has another | | |
| | | name, or is inside `atf`, booted and no longer does (D-008). And whatever is in `atf`, the product's own | | |
| | | BL31 or another payload, is started with x0 = the `bl_params` list where it got 0: a BL31 that doesn't | | |
| | | read x0 starts BL33 at its own address as before, one that reads it gets Linux at EL2 with the device | | |
| | | tree in x0 (`set_dtb_boot_arg` is new for the board), and a payload that takes x0 for something else | | |
| | | gets a pointer it didn't get. To check with the board and with the images of its products: that they | | |
| | | boot, and which BL31 it is. If they don't, the board keeps the image handover with `atf` as its entry, | | |
| | | without a delivery, which is what it had | | |
| F-300 | `src/board/imx8mevk/board.c` | The board has a partition table and neither | Verified (code) | open |
| | | a boot source policy nor a handover: `board_setup()` fails on the handover, nothing is bootable | | |
| | | and nothing boots, by `boot bpak` neither. Its defconfig has `BOOT_AB_DRIVER` and `BOOT_LINUX`. | | |
| | | With F-033: finish the board (a policy, `BOOT_POLICY_AB()` or `BOOT_POLICY_FIXED()`, and a | | |
| | | handover) or drop it. `board.md` 2.5 and 3: both are mandatory. Since F-249 they are checked: | | |
| | | `board_setup()` fails on the policy's check, one step earlier, with `-PB_ERR_PARAM` as before | | |
| | | and with the RoT, the SLC, `init()` and the table set up as before. Its command mode is what | | |
| | | it was: the table can be installed and written, nothing boots. The log names both. It is the | | |
| | | one board that fails a check of `board.md` 3, and why the check isn't at the start (F-335) | | |
| F-301 | `src/board/host/board.c` | The host board's boot source policy is an object | Verified (code) | open |
| | | its board file builds by hand: A/B's functions, with an init that also sets `BIO_FLAG_BOOTABLE` | | |
| | | on the flash's two partitions, which only `boot part` boots. `board.md` 2.5: a policy marks its | | |
| | | candidates, a board doesn't; and one policy per board. To end it: a policy made by a macro | | |
| | | that can carry candidates it never hands out, or a second policy per board. Since F-256 the | | |
| | | marks are in the policy's init, which `board_setup()` skips when the table fails: on a blank | | |
| | | disk the flash's partitions aren't bootable until a table is installed and pb restarts | | |
| F-335 | `src/board.c` | `boot_policy` and `boot_handover` are checked when their step | Verified (code) | open |
| | | comes, after the partition table, not at the start of `board_setup()` with `rot` and `slc` | | |
| | | (`board.md` 2 and 3, F-249). A step before them that fails hides the check: a board without a | | |
| | | policy gets the table's error on a blank disk, and the missing policy shows only once a table | | |
| | | is installed. At the start, a failed check would skip the RoT, the SLC, `init()` and the table | | |
| | | (`board.md` 2.1). imx8mevk fails it (F-300): its life cycle would be unknown, so its command | | |
| | | mode closed, in a build without a method (F-271), and without storage. Today it can install | | |
| | | and write its table. Move the check to the start when F-300 is done, with F-216 | | |
| F-323 | `src/boot/boot.c` `boot_from_policy()` | Low. The core's loop over a policy's | Reported (review) | open |
| | | candidates has no bound of its own: it ends when `next` says there is none. The in-tree | | |
| | | policies end (fixed, chain, A/B). A board's own policy whose `next` never ends would loop, and | | |
| | | since F-243 the loads in it kick the watchdog, where it reset before. A bound in the core, or | | |
| | | a sentence in `boot.md` 3.2 that a policy hands out each candidate once per boot | | |
| F-341 | `tests/`, `configs/host_defconfig` | Speculative mode (`boot.md` 3.6) has unit | Verified (code) | open |
| | | tests only. pb-host can't have `CONFIG_BOOT_AB_SPECULATIVE`: the host has no watchdog, which | | |
| | | the symbol depends on, and `test_ab_state_speculative` builds `ab_state.c` with the symbol | | |
| | | outside Kconfig. Not tested together: the core's loop with it (both slots fail: command | | |
| | | mode), the real `pbstate` confirming a slot, the handover's mode (F-260). A second pb-host | | |
| | | configuration with a watchdog on the host, and cases in `test_ab_state.py` | | |
| F-342 | `doc/architecture/boot.md` 3.6 | Speculative mode's text is silent where | Verified (code) | open |
| | | F-257 had to choose. (1) What the alternation writes besides `enable`: nothing, but | | |
| | | `remaining_boot_attempts = 0`, which changes only a counter left behind with nothing enabled; | | |
| | | the error bits stay, so the OS learns the mode from the handover, not from a rollback bit. | | |
| | | (2) Nothing enabled while a slot is confirmed (`pbstate -s none` keeps `verified`): A first | | |
| | | all the same, also when only B is confirmed. (3) A boot's second candidate that is confirmed | | |
| | | is 3.5's fallback, mode fallback, also after a speculative first one. (4) "From this boot" | | |
| | | also after a rollback's target failed: the trial that ran out is tried. (5) `boot status` | | |
| | | is the enabled slot (3.4), which isn't the slot that runs when the written slot failed and | | |
| | | the other one booted: the OS confirms by `pb,active-system`. Review, and write what stands | | |
| | | into 3.6. From the review of F-257, three points of design: (a) with nothing enabled and | | |
| | | only B confirmed, A, which isn't confirmed, is booted first, and the mode reads speculative | | |
| | | although a confirmed slot exists (3.6's "A first", literally). (b) When the same boot's | | |
| | | second candidate booted, the state names the other slot as enabled: an OS that confirms | | |
| | | "the active slot" as `pbstate` reports it confirms the wrong one, the one that failed, and | | |
| | | the mode never ends. It needs `pb,active-system` or `pb,boot-partition` from the handover | | |
| | | (F-260). (c) "From this boot" writes nothing, so with a confirmed A that is corrupt and a B | | |
| | | that verifies and hangs, the device resets into B forever, where without the symbol it | | |
| | | reaches command mode: rule 3's guarantee for a hang covers only the two cases that write. | | |
| | | And rule 7 ("no format change ... their alternation is `enable`") is met with one more | | |
| | | field written, `remaining_boot_attempts = 0`, which rule 4 needs | | |
| F-345 | `doc/architecture/boot.md` 3.4 | The policy's contract names no | Verified (code) | open |
| | | function for the booted system's name. 5.6 has it from "the policy" and `board.md` 2.7 says | | |
| | | `resolve_part_name` goes because the name is the policy's, but section 3 lists `init`, `next`, | | |
| | | `report`, `mode`, `activate` and `status` only. The code (F-260) has `boot_policy_system_name(uuid)` | | |
| | | and an optional `system_name` in `struct boot_policy_ops`: A/B answers "A", "B" or "?" for any UUID | | |
| | | (`boot_ab_part_uu_to_name()`), the fixed and the chain policy have none, and the handover then | | |
| | | writes no `pb,active-system` and an empty `system_name`. To write into 3.4, with whether a policy | | |
| | | without systems may name its partitions | | |
| F-346 | `src/rot.c` | `rot_no_of_keys()` returns its error, | Verified (code) | open |
| | | `-PB_ERR_NOT_SUPPORTED` as a `size_t` when the RoT isn't set up, and `rot_key_idx_to_id()` an error | | |
| | | as a `key_id_t`: a caller can't tell them from a count or an id. A loop over the count runs to | | |
| | | `SIZE_MAX` (`dtb.c` had one; unreachable, no image verifies without a RoT). `boot.c` | | |
| | | (`collect_info()`, F-260) takes a count above 255, the most a keystore holds, for the error and | | |
| | | stops the boot. An `int` with the count or the id as an out argument, or 0 keys without a RoT | | |
| F-348 | `doc/architecture/boot.md` 5.5, 5.6 | pb's properties in a device tree: the | Verified (code) | open |
| | | text is silent on two things the device tree code decides. (1) Does pb own the `pb,*` names, and | | |
| | | remove a property it has no value for (an image's placeholder, what a hook wrote)? 5.5 says pb's | | |
| | | values win, 5.6 says "absent". The code leaves a released property it has no value for (`pb,slc` | | |
| | | without an SLC, `pb,active-system` without a name), as every release did (D-008: an out-of-tree hook | | |
| | | that writes `pb,active-system` itself), and removes only the new ones (`pb,boot-partition` without a | | |
| | | partition, a new property that didn't fit). On a board without an SLC a next stage can so read a | | |
| | | `pb,slc` that isn't pb's. (2) A new property that doesn't fit the tree is left out with a warning | | |
| | | and the image boots (F-260): 5.6 has a next stage take an absent entry for an older pb, and nothing | | |
| | | tells it "no room" but pb's log. To decide and write into 5.5: the namespace, and what a next stage | | |
| | | is told when a property didn't fit | | |
| F-349 | `src/boot/boot.c` `collect_info()` | Boot time: pb's | Verified (code) | open |
| | | information is read on every board's boot path now, before the first candidate, and its cost isn't | | |
| | | measured: `system.md` 5.3 asks for a recorded time. The boards with a device tree read the same in | | |
| | | prepare before. New for the RT1060 EVK and the host, whose handover writes none of it but the name | | |
| | | and the UUID: the device identity (2 OCOTP reads and an MD5), the life cycle (1 to 9 OCOTP reads) | | |
| | | and the status of the 3 keys (3 reads), in a boot of 40.1 ms from a warm reset to the image's entry | | |
| | | (`testbench/simple/README.md`). Measure it on the RT (`pytest tests -k boot_time`) and record it | | |
| | | there | | |
| F-360 | `doc/architecture/boot.md` 3.5, 3.7, `tools/pbstate` | The contract says who sets a | Verified (code) | open |
| | | rollback bit (3.5, rule 2) and that `boot activate` clears both, but not who else clears one, nor | | |
| | | that both can be set (F-340). Today: `pbstate -s A`/`-s B` (`pbstate_switch_system()`) assigns | | |
| | | `error = 0`, with and without `-c`; `-s none` and a confirmation (`-v`, | | |
| | | `pbstate_set_system_verified()`) keep the bits; libpbstate's `pbstate_clear_error(mask)` clears by | | |
| | | mask and has no command line option; pb clears them only in `boot activate`, a rollback keeps the | | |
| | | other one. Write into 3.5 (or 3.7, for the OS's side) when a bit is cleared and by whom, before the | | |
| | | shared core (F-173) fixes it in code | | |
| F-363 | `src/drivers/usb/` | Since F-185 command mode ends the session on every lost | Verified (code) | open |
| | | the transport reports, but on USB a replug or a bus reset during a session isn't reported | | |
| | | (`usb.md` 2.4, 4.3), so on jiffy, the RT1060 and the MEK the next host on a locked device is still | | |
| | | authenticated and has the stream. What would have to report it as an error of the running transfer, | | |
| | | or keep it for the next `xfer_start`/`xfer_complete`: the ChipIdea's `imx_ci_udc_xfer_complete()` | | |
| | | (`imx_ci_udc_irq_process()` flushes on `USBSTS_URI` and returns PB_OK); the cdns3's | | |
| | | `cdns3_udc_core_xfer_complete()` (`cdns_process_irq()` re-arms endpoint 0 on `ISTS_CFGRESI`, | | |
| | | `ISTS_U2RESI` isn't looked at); the DWC3's `dwc3_udc_xfer_complete()` reports a reset only for an | | |
| | | endpoint with a transfer running, and `dwc3_udc_xfer_start()` clears it, so one between two transfers | | |
| | | of a command is lost; `DEVT_DISCONNECT` is logged only; the stack's `usbd_xfer_complete()` answers a | | |
| | | SETUP during a session (a new `SET_ADDRESS`, `SET_CONFIGURATION`) and restarts the bulk transfer | | |
| | | without an error. With F-201 (`enumerated`) and F-287 (1) | | |
| F-375 | `src/cm/cm_main.c` `PB_CMD_DEVICE_READ_CAPS`, `command-mode.md` 4.6 | Low. The caps' | Verified (code) | open |
| | | `part_erase_timeout_ms` stays zero: 4.6 says a device fills it, but pb sets no bound on an erase. | | |
| | | Its time is the driver's per block (`bio.md` 3, from the driver's header) times the blocks a host | | |
| | | names. To decide: what the field means (a whole partition, a block, the largest erase pb takes in | | |
| | | one command), then fill it. Released hosts ignore it (F-226) | | |
| F-394 | `src/board/host/board.c` | Low. The host board sets up its disk before Mbed TLS, | Verified (code) | open |
| | | the real boards their crypto engine first: a failed `init()` on the host leaves no crypto, so the | | |
| | | RAM boot after a failed `init()` (F-216, `board.md` 2.1) has no end-to-end test | | |
| F-395 | `src/board/{imx8mevk,imxrt1060evk}/board.c` | Low. imx8mevk's `board_command()` appends | Reported | open |
| | | a NUL at `response[*response_size]` after `snprintf()`, the pattern of F-250 in a board command | | |
| | | (latent with a 4 MiB buffer; `board.md` 4.4 has no clamp rule for a command's response). The RT's | | |
| | | `board_command()` has an unused variable `rc` (line 350), the one warning of its build | | |
| F-396 | `src/link.lds`, the armv7 links | Low. jiffy's and the RT1060's ELF have | Verified (readelf) | open |
| | | everything in one RWE LOAD segment, as the aarch64 boards had (F-090); arm-none-eabi's ld doesn't | | |
| | | warn. Only the ELF's headers | | |
| F-397 | `mmc_emmc.c:362`, `dwc3_udc.c:359,651` | Low. At `CONFIG_LOGLEVEL` 0 or 1 | Verified (build) | open |
| | | without the RAM log, variables that only a log line uses are unused (`start`, `speed`, `caplength`): | | |
| | | warnings in the imx8mpevk build. Seen with F-109 | | |
| F-398 | `Makefile` | Nothing depends on the flags: a changed `cflags` or `ldflags` | Verified (build) | open |
| | | (`CONFIG_OPTIMIZE` on the command line, an edited `makefile.mk`) neither compiles nor links again in | | |
| | | an existing build directory, shown with F-090's flag. `srk_fuse.o` doesn't depend on `scripts/gen- | | |
| | | srk-fuse.py` (read). The value files of F-183 are the way | | |
| F-399 | `src/cm/Kconfig`, `cm_main.c` | Low. `CONFIG_CM_TRANSPORT_READY_TIMEOUT` has no range, | Reported | open |
| | | and its `* 1000000L` overflows a 32-bit long above 2147 s | | |
| F-400 | `scripts/pbdev`, `scripts/ci/host-test.sh` | Low. Two `scripts/pbdev test` runs at | Verified (run) | open |
| | | once in one tree fail: both install the package through setuptools' `build/` directory and the wheel | | |
| | | builds collide ("Failed building wheel for punchboot"). The default and the sanitizer run must go | | |
| | | one after the other | | |
| F-401 | `tools/punchboot/session.py` | Low. `part_erase` masks `start_lba` to 32 bits, so a | Reported | open |
| | | partition past block 2^32 wraps (pb 3.0.0 refuses the range, older devices unchecked). After a | | |
| | | host's timeout on USB a device before 3.0.0 can still hold the late result, which the next session | | |
| | | reads as its first answer (read, not tried). `pyproject.toml` says Python 3.10 and up; only 3.13 is | | |
| | | tested | | |
| F-402 | `src/plat/*/Kconfig`, `platform.md` 3.10 | `platform.md` 3.10 has one option per | Verified (code) | open |
| | | port of the SoC that can be a device; each platform's choice has only the port a board in the tree | | |
| | | uses. Without an option: the 6UL's USB OTG2, the 8MQ's and the 8MP's USB2, the 8QXP's USB OTG1, the | | |
| | | 8DXL's USB OTG1, the RT1060's USB OTG2. Their set up would be SoC code nobody can run. For Jonas: | | |
| | | write it untested, or does the rule mean the ports that have code. From F-267 | | |
| F-403 | `doc/architecture/boot.md` 3, `ab_state.c` | What the A/B policy does where the | Verified (code) | open |
| | | contract is silent, from the fixes of F-173 to F-393, for Jonas. (1) A resync of two valid copies | | |
| | | that can't be written: init returns PB_OK with the primary's state, while a repair of a corrupt or | | |
| | | unreadable backup that can't be written fails init; both go on, or both fail? (2) Both enable bits | | |
| | | set: next() boots A, `boot status` answers none, pbstate says both. (3) The verified slot handed out | | |
| | | in place of a trial fails in the same boot: without speculative mode there is no candidate. (4) | | |
| | | After a write that failed, the decision stays in memory: a later write in the same power cycle | | |
| | | persists it, and an uncounted trial is then counted. (5) No entry in pb's status for a state that | | |
| | | couldn't be written (D-043 expects one). (6) After a failed init: -PB_ERR_STATE for activate, next() | | |
| | | and a register write, a register reads 0 with PB_OK, the system's name is still answered; should 3.1 | | |
| | | ask every policy to be safe after a failed init? (7) `boot activate` after an unreadable state | | |
| | | writes the defaults and the activation: the board registers on the disk are lost. (8) One good copy | | |
| | | beside an unreadable one is used and repaired | | |
| F-404 | `src/boot/ab_state.c` | A repair of the backup alone (corrupt, unreadable, or the resync | Reported | open |
| | | of F-176) goes through the commit that writes both copies: power lost while the good primary is | | |
| | | rewritten leaves a torn primary beside a stale backup. Write only the backup. Read, not run | | |
| F-405 | `include/boot/policy.h`, `boot.c`, `ab_state.h`, `tools/pbstate` | Low. Comments that no | Reported | open |
| | | longer hold: `policy.h` and `boot.c` say next() can return the error of a state write that failed | | |
| | | (A/B no longer does), `ab_state.h` lists errors `boot_ab_state_set_boot_partition()` doesn't return. | | |
| | | pbstate returns a primary's read error without trying the backup, where pb uses a good backup; it | | |
| | | reads the backup only when the primary is corrupt | | |
| F-406 | `src/drivers/mmc/`, `tests/`, `ab_state.c` | Low, from F-283. `mmc_init()` that fails | Reported | open |
| | | after it registered its devices leaves them registered and pointing at a slot that is free again | | |
| | | (`drivers.md` 2.1: nothing is registered when init fails). A card of another size at a second | | |
| | | `mmc_init()` keeps the first one's range. `host_disk_init()` called twice leaks a descriptor | | |
| | | (tests). `pb_boot_state_write()` with a handle that isn't one tests a flag on a negative value; the | | |
| | | result is -PB_ERR_PARAM either way | | |
| F-407 | `src/drivers/usb/imx_ci_udc.c:38`, `CMakeLists.txt` | Low. A build without | Verified (build) | open |
| | | `CONFIG_CM` doesn't compile while `DRIVER_IMX_CI_UDC` is on, its default: the driver uses | | |
| | | `CONFIG_CM_BUF_SIZE_KiB` (the 6UL, the RT, the DXL). On the host `pb_core` is a static library, so a | | |
| | | task in a file nothing else references isn't linked into pb-host (read). From F-268 | | |
| F-408 | `tools/pbstate`, `testbench/simple/src/pbstate.c` | Low, from F-180 and F-221. pbstate | Reported | open |
| | | accepts trailing characters after a number (`-c 3x`, `-w 0 1zz`); `print_configuration()` ignores | | |
| | | the getters' errors and prints both systems as disabled on a CRC failure before it fails; `-v` with | | |
| | | anything but a or b exits 1, not EINVAL; `-c` without `-s` is ignored. The simple target's copy | | |
| | | still zeroes the attempts whichever slot it confirms, where pbstate now keeps the trial when the | | |
| | | other slot is confirmed, and so does the helper `pbstate_confirms()` in `test_ab_state.c` | | |
| F-410 | `src/drivers/crypto/caam/imx_caam.c` | From F-181's package and its review. Every buffer | Reported | open |
| | | address is cut to 32 bits (`drivers.md` 4.2: refuse). `imx_caam_init()` empties the output ring and | | |
| | | doesn't reset the job ring (`drivers.md` 2.1; a reset can't be tried from here). The key parser | | |
| | | skips the BIT STRING's two leading bytes unchecked, doesn't check the point's form or length against | | |
| | | the curve, and `caam.ecdsa_key` isn't cleared. A job of a `hash_final()` or a verification that | | |
| | | timed out can still write the caller's buffer later (F-081). The i.MX8M Mini SRM's verify PDB | | |
| | | differs from what the driver and OP-TEE use; the 6UL SRM asks for data on INIT, pb starts a short | | |
| | | hash with none | | |
| F-411 | `src/boot/dtb.c`, `src/lib/fdt` | A device tree part whose header lies about | Verified (run, host) | open |
| | | more than `totalsize` reads outside its part: only the magic, the version and `totalsize` are | | |
| | | checked, and `fdt_open_into()` trusts `off_dt_struct`, `off_dt_strings`, `off_mem_rsvmap` and the | | |
| | | two sizes. Of 200 000 single field lies 45 271 read outside the part (memory the payload's hash | | |
| | | doesn't cover) and one wrote 44 KiB below its destination. Signed input only, F-065's class; the | | |
| | | image handover has it in place. Refusing such a tree refuses an image a release took: a decision | | |
| | | (D-008), for Jonas. Found by the review of F-259 | | |
| F-412 | `plat/imx8x/plat.c`, `src/log.c`, `board/imx8mpevk` | From F-235's package. The | Verified (code) | open |
| | | 8X's `plat_boot_reason_str()` has no names for the SCFW's reasons 13 and 14 (`V2X_DEBUG`, | | |
| | | `BOOT_STAGE`, by the SCFW API guide). On the 8M and the RT a log line before `plat_init()` reads the | | |
| | | GPT's counter through a base of 0, because `gpt_base` isn't set yet; F-238 ends it. The 8MP EVK's | | |
| | | `board_console_init()` writes the IOMUXC before `plat_init()` opens its clock gates; it works on the | | |
| | | bench (read only). `command-mode.md` 2.1 still names F-235 in its text | | |
| F-413 | `board/{imx8mevk,host}`, `cm_main.c`, `board.md` 4.4 | From F-095's package. | Reported (read) | open |
| | | imx8mevk's `board_command()` writes `response[(*response_size)++] = 0` after `snprintf()`, past its | | |
| | | room when the text is longer, and several boards return `snprintf()`'s full length as the size (the | | |
| | | core cuts it now). The host board's test command writes `arg[size] = 0`, a byte past a 1024-byte | | |
| | | request (inside the buffer). `rot_no_of_keys()` returns an error cast to `size_t` without a RoT | | |
| | | (`boot.c` handles it, `cmd_slc_read()` is bounded now). A transport whose read can never be started | | |
| | | after a connect is restarted every round for ever with the watchdog kicked, as after a failed | | |
| | | `complete()`. `board.md` 4.4 says `rsp` holds `*rsp_size` bytes "(the command buffer)"; since F-095 | | |
| | | it is the smaller of the host's size and a buffer | | |
| F-414 | `doc/guides/testing.md`, `bio.md` | From F-191's package. The coverage note in | Reported (read) | open |
| | | `testing.md` names `gpt.c` lines 298 and 383 to 389 as unreachable; that code is gone. The tables in | | |
| | | `bio.md` 2.4 and section 8 are wider than the aligner makes them (they were before the package) | | |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-020 | Mbed TLS pinned to 3.4.0 (EOL). The file list in `src/drivers/crypto/mbedtls/makefile.mk` is tied to that release. `mbedtls_config.h` enables MD5/BASE64/PKCS1_V21, which are unused. Corrected 2026-11-12: MD5 is used, for the device's UUID (`uuid3.c`, `crypto.md` 5); BASE64 and PKCS1_V21 unchecked. | open |
| F-022 | Vendored code has no version record: Kconfiglib, libfdt, libuuid, TF-A xlat_tables_v2 + libc, NXP SCFW (littlefs was removed). | open |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-030 | libc files never compiled: `memcpy.c`, `strncmp.c`, `strlcpy.c`, `strnlen.c`, `strrchr.c`. Also `xlat_tables_v2/aarch32/nonlpae_tables.c`. | open |
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. Corrected 2026-11-12 (`command-mode.md` 5.3, 5.8): only `PART_RESIZE` is retired. `PART_TBL_INSTALL` is in use (`board.md` 2.4), and `AUTH_SET_PASSWORD` was never implemented and is to be (`roadmap.md`). | open: removing them breaks old hosts (D-008); at most answer `NOT_SUPPORTED`, never reuse the slots |
| | Left after the correction: `PART_RESIZE` still has its case in `cm_main.c`, and the two string | |
| | functions in `src/wire.c` return "" | |
| F-033 | imx8mevk unfinished (`board_init` returns error, no Kconfig). Orphan defconfigs `bebop`, `pico8ml`. imxrt untouched since 2023. | open: ask Jonas |
| | Checked 2026-11-12: imx8mevk is described by `struct board` and builds, what it lacks is F-300. | |
| | The RT1060 EVK runs on the bench. Left: `bebop_defconfig` and `pico8ml_defconfig` have no | |
| | board in `src/board/`; which boards to keep | |
| F-035 | The same data is defined in several places: exception names (C ×1, Python ×3, before D-021), partition flags, SLC enum, and the crc32 id table (`helpers.py`, could be `zlib.crc32`). | partly fixed (D-021): exception names are defined once (`errors.py`), and `wire.py` mirrors the wire values checked against `abi.txt`. Left: `pbtest.py`'s `PB_CMD_*`, the crc32 table |
| F-145 | De-duplication left from the 2026-10-29 slimming review (D-020): board boilerplate (default GPT table and `partitions.h` byte-identical in 5 boards, the same `board_init` sequence and 3 dev keys in all, cm `.tops` in 6), ~900 lines; the write/read/check/print block 42 times in `tools/punchboot/api_*.c` and the validate/call/exception pattern 28 times in `python_wrapper.c`, ~550; `plat_mmu_init` and `plat_get_unique_id` near-identical in imx6ul/imx8m/imx8x; the bpak hash-kind switch 4 times in `image_helpers.c`; one-provider crypto ops tables; `test_mmc_core.c`'s own fake card vs the card models. Porting API: `arch_init()` empty everywhere, single-member `console_ops`, `board_init(struct imx*_platform *)` (imx8m struct empty), cm `command`/`status` stubs in 4 boards. | open; the host tools part is gone with the C library (D-021) |
| | Checked 2026-11-12: gone with D-026 are the boards' `board_init` sequence and signature and | |
| | their cm `.tops`; a shared default layout was tried and moved back (Jonas, D-026). One ops | |
| | table per crypto backend is the contract's (D-071). The MMU set up is F-158 and F-269. Left: | |
| | `plat_get_unique_id` per port, the hash-kind switch (4 times), `arch_init()` (empty in every | |
| | arch; D-065 gives it the line size's check), `console_ops` | |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-041 | Wheels are never tested (no cibuildwheel `test-command`, before D-021). There are no Python unit tests. | partly fixed (D-021): `punchboot-tools.yml` installs and runs the wheel on Linux, Windows and macOS. No Python unit tests yet |
| F-043 | No `ruff-format` hook despite the format config. mypy only runs through pre-commit.ci. There is no `_punchboot.pyi`. | open (`_punchboot.pyi` is moot since D-021) |
| F-044 | Sphinx docs are mostly stubs, with version 1.0.0 in `conf.py`, broken header paths, and `sphinx<7`. The README test instructions are wrong (no `BOARD`). `doc/guides/release-checklist.txt` is stale. | partly fixed: Sphinx/RTD removed (D-020); README and the checklist still open |
| F-052 | `src/drivers/virtio/*` has no user since `plat/qemu` was removed. Verified. | open: keep for a QEMU smoke tier, or remove |
| F-067 | Host test infra: `pbtest.py` `stop()` ignores pb-host's exit code, so sanitizer/LeakSanitizer failures at teardown are lost. The socket file appears at `bind()`, before `listen()` (startup race). No client timeouts, no pytest-timeout/ctest `TIMEOUT`. pb-host `write()` can die from SIGPIPE. Orphaned pb-host if pytest is killed. `fakes/ramdisk.c` computes `lba * 512` in 32 bits. 64-bit host hides 32-bit overflows (F-054); consider a `-m32` job. Verified (read). | partly fixed (claude-work): teardown fails on non-zero exit and sanitizer reports; ramdisk overflow fixed; the socket path appears only after `listen()`; ctest `TIMEOUT` (60 s per unit test, 600 s for the integration suite). Client timeouts in the C library, SIGPIPE, orphans and `-m32` open |
| F-347 | The host board has no device tree part, so no integration test reads what a handover wrote: `/chosen` and pb's information in it are reached by the unit tests only (`test_image.c`, `test_tfa_image.c`), and `late_boot` reports the partition's name alone. F-260's `test_boot_mode` reads the boot's mode from a debug line of `boot.c` in pb-host's log (`Device.boot_mode`, `pbtest.py`), which a change of the log's text breaks. A device tree part in the host's images and a dump of it (or of `struct boot_info`) next to `--boot-status` would let the tests read the handover itself | open |
