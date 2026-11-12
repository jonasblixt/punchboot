# Findings

Bugs and tech debt from the 2026-10-22 repo scan. **Verified** means a human or
an agent read the code and confirmed it. **Reported** means it came from a scan
agent and has not been checked yet; check before acting on it.

Status: `open` / `fixed (<commit>)` / `wontfix (<reason>)`.

This file lists what is open. An entry that is fixed, rejected or closed moves to
`findings-closed.md` with its last status, and keeps its id.

## Security / correctness

| ID | Where | Finding | Check | Status |
|---|---|---|---|---|
| F-004 | `src/board/imx8{qxmek,dxlevk}/board.c:~288` | `strncmp(expected, password, length)` with `length` taken from the host accepts any prefix ("i"). `LOG_DBG` prints a password that is not NUL-terminated. Example boards, but the pattern gets copied. | Verified | open |
| F-006 | `src/drivers/partition/gpt.c:~411` | Loop over `p->first_lba` has no 128-entry limit. Primary and backup tables are never compared. | Reported | partly fixed (claude-work): entry loop bounded by `no_of_parts` (F-056); tables still not compared |
| F-007 | `src/boot/ab_state.c` | If a state partition is missing, it still does I/O with a | Verified | open |
| | | negative handle. The B→A rollback branch is not symmetric with A→B. `validate()` | | |
| | | zeroes `crc` as a side effect. `init` sets `cfg` before validating, so a failed init | | |
| | | leaves it half set up. NULL-`cfg` part split out as F-012. Checked 2026-11-09: `bio` | | |
| | | rejects the negative handle (`-PB_ERR_PARAM`) and init then fails on the commit; the | | |
| | | B→A branch ends in the same state as A→B. Read errors: F-173. | | |
| F-008 | `src/drivers/crypto/mbedtls/mbedtls_pb.c` | Return values of the `mbedtls_sha*` calls are ignored. | Reported | open |
| F-010 | `tools/pbstate/CMakeLists.txt` | `BACKUP_PART` is set from `PB_STATE_PRIMARY_PART` (copy-paste). The target is listed twice in `install`. | Reported | open |
| F-064 | `src/drivers/partition/gpt.c:152` | Header `last_lba` (last usable) is `last - 32`, which is the first backup-entry block; should be `last - 33`. pb's own allocation is unaffected; other tools could overlap the backup array. | Verified (code) | open |
| F-065 | `src/boot/*`, `utils_def.h:150` | Hardening, signed input only: `CHECK_OVERLAP` can wrap; no check that load regions are in RAM or that parts don't overlap each other; load-addr meta read as u64 without a size check; `linux.c` doesn't check `fdt_totalsize <= dtb part`; `keystore_id` never compared; no anti-rollback counter. | Reported | open; the `fdt_totalsize` <= part check is done (`39276b9c`) |
| F-066 | `src/lib/der_helpers.c` | `der_ecsig_to_rs` has no callers and doesn't left-pad short r/s. `der_ec_public_key_data` memcmp with a data-supplied length (0 matches anything, and an OID longer than the reference reads past it); `s -= 2` can underflow. | Reported | partly fixed (`jiffy-fixes`): `der_ecsig_to_rs` now pads short r/s and is used by the CAAM (F-057). `der_ec_public_key_data` is still open |
| F-074 | `src/cm/cm_main.c` stream read/write | After a failed STREAM_INITIALIZE, `block_dev` is an error code, and `bio_get_flags()` of it has the flag bits set, so the readable/writable checks pass. `stream_check()` then fails on the block size, so it's harmless. | Verified (test) | open |
| F-075 | `cm_main.c` `cmd_part_erase`, `struct pb_command_erase_part` | Wire compat (D-008): `start_lba`/`block_count` were taken from `rz` in 2309738 (v2.0.0). A pre-2.0 host sends zeros, so a current device erases 0 blocks and answers OK: a silent no-op instead of a full erase. Fix: `block_count == 0` means the whole partition. Coordinate with F-055. | Verified (code) | open |
| F-076 | `src/boot/image_helpers.c` (payload hash and load) | bpak compat (D-008): punchboot hashes every part, while upstream `verify.c` skips parts with `BPAK_FLAG_EXCLUDE_FROM_HASH` (`bpak add --set-flag dont-hash`). Such images verify with the bpak CLI but fail to boot in punchboot. Supporting the flag only accepts more images, so it is a compatible fix. | Verified (code) | open |
| | | Decided (D-035, `security.md` 4.2): an image with a `dont-hash` part is refused, before the | | |
| | | load, with its own error. Not loaded unhashed as upstream does, and not skipped | | |
| F-078 | jiffy (hardware / boot ROM) | An SRST reset from the JTAG adapter (OpenOCD `reset`, SRSR = 1, POR) makes the boot ROM fall back to the serial downloader (`15a2:007d`) within a second, instead of booting pb from eMMC. A second clean SRST pulse did the same. A watchdog reset does too (SRSR = 0x11, after pb without WDBG was halted over JTAG, and after the `auth token` hang below). Earlier resets when the ULINK was plugged in did reboot into pb, and a power cycle does. Cause unknown. Suspect: pb leaves the eMMC in 8-bit DDR52, and a warm reset doesn't reset the eMMC. Other options: HAB, or the reset itself. `src/board/jiffy/openocd.cfg` has no `reset_config` because of this. | Seen on the board (2026-10-22) | open: a power cycle boots pb. Forcing serial download with the boot mode buttons and resetting works (uuu loads `pb_signed_uuu.imx`), but in the downloader the ROM falls back to after a failed boot, every SDP command with a data phase (write, dcd) hangs. Hardware (jiffy R1 netlist): JTAG SRST is the `RST_N` net (POR_B, LP3470 supervisor, button PB2). The eMMC RST_n is a separate net from pad NAND_WP_B with a 46.4k pull-up, so a POR_B reset doesn't reset the eMMC (see F-079) | Likely cause: the same SNVS SV1/Non-Secure state as F-080 (every warm reset after JTAG activity landed in the downloader, the one without JTAG since power-on booted; a direct test is blocked because `dev reset` needs `auth`). SNVS survives a warm reset, so HAB fails on the next boot and the ROM falls back to the downloader, where data writes then hang. A power cycle clears it. Fits every reset seen so far except one SRST at 12:26.
| F-079 | `src/board/jiffy/board.c` `usdhc_emmc_setup` | Muxes NAND_DQS (ball E6) as the eMMC reset, but on jiffy R1 NAND_DQS isn't connected. The eMMC RST_n is on NAND_WP_B (ball D5, `eMMC_RST_N`, 46.4k pull-up), which pb never configures, so the eMMC never gets a hardware reset. Possibly related to F-078. | Verified (netlist) | open: check NAND_WP_B's USDHC1_RESET_B alt in the RM, and whether the eMMC has RST_n_FUNCTION enabled |
| F-081 | `src/arch/armv7a/cp15.S` `arch_invalidate_cache_range`, callers in `imx_caam.c` | The invalidate rounds the start down to a cache line and uses DCIMVAC (invalidate, no clean), so dirty CPU data sharing the first or last line is lost. `caam_wait_for_job` invalidates `caam.output` (0x8001a20c in the jiffy debug build), whose line also holds the tail of `struct caam` (`init`, `async_hashing`, `hash_align_buf_len`...), `wpb`, another `base`, `mmc_current_part` and `mmc_current_part_config`. Not shown to cause F-080. | Verified (code, symbol map) | open: clean+invalidate (DCCIMVAC) partial lines, or keep DMA buffers in their own cache-line-aligned sections |
| F-083 | `punchboot part read` | Fails with `Error: I/O error` on jiffy, both for BOOT1 and the 512 B PB State Primary partition, in an authenticated session. Cause unknown. | Seen on the board (2026-10-22) | open |
| F-087 | `src/cm/cm_main.c` `error_to_wire` | `-PB_ERR_BAD_PAYLOAD` (payload hash mismatch) has no wire result, so the host reports "Generic error". `include/pb-tools/error.h` has no payload code. | Seen on jiffy (2026-10-22) | open: a new result code is a wire change, see `compatibility.md` (old hosts must handle an unknown code) |
| F-089 | `src/lib/libc/snprintf.c` (vendored) `unsigned_num_print` | `%x` writes the digits and padding without checking the buffer size `n`, so `snprintf` with `%x` can overflow a small buffer (`%d`/`%u`/`%s` are bounded). Unsupported specifiers (`%zu`, `%2u`, `%c`, ...) spin forever instead of failing. | Verified (code) | open: `imx_rom_log.c` works around it (only writes a line with 128 bytes left) |
| F-090 | `src/plat/imx8x` link, imx8dxlevk | `.text` grew past 64 KiB (0x11000 after `plat_get_boot_source`), so `.data` no longer starts on a 64 KiB boundary (ld's aarch64 max page size), and ld puts `.text` through `.bss` in one RWX LOAD segment: `warning: build-imx8dxlevk/pb has a LOAD segment with RWX permissions`. imx8mpevk (2026-10-28) too: `.text` starts at 0x920000 and ends inside the same 64 KiB page as `.data`. Only the ELF headers; `pb.bin` and the MMU setup (linker symbols) are the same. | Verified (readelf) | open: `-z max-page-size=0x1000` for the aarch64 link, or align `.data` in the linker script |
| F-095 | `src/cm/cm_main.c` `cmd_board`, `tools/punchboot/api_board.c` | The device ignores `response_buffer_size` and lets the board fill the whole command buffer. When the response is larger than the host's buffer, the host returns an error without reading the response, which stays on the link and is read as the next results. 2.x hosts also don't check the first result (the check is commented out), so they send the request transfer even after an error. | Verified (code) | open: pass `min(response_buffer_size, CM_BUF_SIZE)` to the board, but check first whether any released host sends 0 there (D-008) |
| F-100 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The timing copy for TF-A in `.umctl2` (NOLOAD, 0x40000000) never writes `fsp_msg`/`fsp_msg_num`, so TF-A gets whatever DRAM held. The copy's size (~12.5 KiB for the EVK) is not checked against the 128 KiB region, and nothing pins `struct dram_timing_info` to TF-A's layout. | Verified (read) | partly fixed (`imx8m-dram-cleanup`): the copy is a 128 KiB `struct dram_timing_save` and too large tables fail with `-PB_ERR_MEM`; `fsp_msg` is never read by upstream TF-A (only `fsp_table` and the tables), so harmless there; the layout pin is open |
| F-102 | `src/drivers/ddr/{umctl2,dwc_ddrphy}.c`, `src/plat/imx8m/ddr_imx8mq.c` | Every poll in the DDR init (STAT, PHY mailbox, CalBusy, DFISTAT, SWSTAT, PLL lock) spins forever. A stuck PHY or bad board table hangs the board without a log. | Verified (read) | open: timeouts need a measurement on hardware |
| F-104 | `src/drivers/ddr/dwc_ddrphy.c` `dwc_ddrphy_train` | The training results in the message block are never read. U-Boot (all i.MX8M) reads the trained critical delay differences after each 1D run and widens the rank-to-rank gaps (`DRAMTMG2` rd2wr/wr2rd, `RANKCTL`, per P-state) before the DFI init. The imx8mevk has two ranks (`MSTR.active_ranks` 3). TF-A reads these registers back at boot and restores them after retention, so it expects the bootloader to have applied the workaround. | Verified (read, and in the differential trace: U-Boot rewrites `DRAMTMG2`, `FREQ1_DRAMTMG2` and `RANKCTL` before `dfi_init_start`) | open: needs the message block layout and an imx8mevk |
| F-106 | `src/drivers/ddr/umctl2.c` | Low power/refresh values differ from NXP's current sequence: during init `RFSHCTL3` 0x11 and `PWRCTL` 0xa8 (U-Boot: 0x1 and 0xa0, its comment says `en_dfi_dram_clk_disable = 0`; `refresh_mode` 1 is a DDR4 fine granularity setting); at the end `PWRCTL` 0x08 and `RFSHCTL3` 0x10 (U-Boot: clears only `selfref_sw`, 0x80, then `RFSHCTL3` 0 and `selfref_en`, 0x81). Punchboot also writes `DFIPHYMSTR` twice, which the board table already sets. | Verified (differential trace) | open: needs an imx8mevk |
| F-107 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The trained PHY CSRs for TF-A are read after the DFI init and port enable; NXP reads them right after loading the PIE image, before the calibration wait and the DFI init. | Verified (differential trace) | open: needs an imx8mevk |
| F-110 | `src/plat/imx8m/ddr.c` `imx8m_ddr_init` (GPC) | The DDR power-up writes `GPC_PGC_CPU_0_1_MAPPING` (0x303A00EC) and `GPC_PU_PGC_SW_PUP_REQ` bit 5 (0x303A00F8), the i.MX8MQ/8MM layout, and never waits for the request to self-clear. On the i.MX8M Plus 0x303A00EC is the read-only `GPC_LPCR_CM4_AD` and 0x303A00F8 is not in the map; its PU power-up trigger is 0x303A00D8 with `DDRMIX_SW_PUP_REQ` at bit 19 (RM 5.2.10.37). NXP's 8MP DDR tool script writes the same two addresses, and the RM's own example code uses them. Harmless on the 8MP EVK: DDRMIX is powered after reset and the DRAM trains (2026-10-28). | Verified (RM, board) | open: per-SoC power-up in `ddr_imx8m{q,p}.c`, with the self-clear wait |
| F-113 | `src/drivers/mmc/imx_usdhc.c`, `src/board/imx8mpevk/board.c` (i.MX8M Plus uSDHC3) | eMMC in DDR52 (8-bit DDR) fails at the first data read after the switch (CMD8, INT status 0x208003: DCE, a data CRC error); HS200 works. 2026-10-28, after the DDR_EN fix (F-123): with the board's fixed DLL override tap 35 the CMD8 still fails the same way; with tap 0 init gets through DDR52 (EXT_CSD reads work), but the first CMD18 (GPT read) fails: the auto CMD12 response in CMD_RSP3 is 0x00d00f00 (CC_ERROR, COM_CRC_ERROR, ILLEGAL_COMMAND, state prg), which doesn't fit a read. HS200 with tap 0 fails the same way (2026-10-29) and works with tap 35: a sampling error, the DLL override is in the read path in SDR too. The right tap depends on board and silicon (tuning). | Verified (board) | open: imx8mpevk uses HS200. Next: what CMD_RSP3 holds after auto CMD12 in DDR, and a DLL setting for DDR instead of the fixed override |
| F-116 | `src/drivers/mmc/mmc_core.c` `mmc_setup` (HS400) | HS400 without Enhanced Strobe selects HS_TIMING 3 without tuning in HS200 first (JESD84-B51 6.6.2.3 steps 5-6). With `MMC_BUS_WIDTH_8BIT_DDR_STROBE` the card is switched to BUS_WIDTH 0x86, then `imx_usdhc_set_bus_width()` rejects the width. No board uses HS400. | Verified (bench, `test_mmc_emmc` `test_hs400`) | open |
| F-127 | `src/drivers/crypto/caam/imx_caam.c` `caam_wait_for_job` | The job status is read from JRSTAR after the output ring entry is removed, not from the entry's status word, and a mismatching descriptor address is logged as "Job failed". JRSTAR only holds the status of the last job that completed, so this works only while one job is in flight (true today). | Verified (SRM, code) | open (low) |
| F-128 | `src/cm/cm_main.c` `cmd_boot_bpak`, `src/board/imx8mpevk` | The imx8mpevk has no boot driver, so `boot_load()` returns `-PB_ERR_PARAM` before reading the image. `cmd_boot_bpak` has already answered OK, so the host keeps sending the image, pb parses it as commands (`Invalid command: 0`) and the board resets into the serial downloader. The host sees `Error: Transfer`. | Seen on the board (2026-10-29) | partly fixed: the EVK has a boot driver (2026-10-29), so `boot_load()` gets as far as the image. Open: how the protocol recovers when `boot_load()` fails before the image has been read. D-058 (Jonas), `command-mode.md` 5.5: after its first OK the device always reads the header's transfer, and the header's result carries the refusal |
| F-131 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_start` | For a buffer that isn't 4 KiB aligned, page 0 of the dTD is `align_buffer` at offset 0 with only `align_length` bytes valid, but the controller fills page 0 up to its 4 KiB boundary before page 1, so an unaligned transfer longer than `align_length` sends or receives the wrong bytes. pb's large buffers are aligned, the small ones fit in one page, so nothing hits it today. | Verified (code) | open: point page 0 at `align_buffer + 4096 - align_length` |
| F-134 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | For an OUT transfer into a buffer that isn't 4 KiB aligned, the data is copied from `align_buffer` into the buffer and then the buffer is invalidated (DCIMVAC), which architecturally discards the copy while the lines are dirty. Every command read takes this path (`cmd` at 0x81826200 in the jiffy build), yet commands work on jiffy, and the D-cache does hold dirty lines there (the F-130 breadcrumbs in `.no_init` were lost on reset without a clean, log 2026-10-29). Not understood: perhaps the Cortex-A7 performs DCIMVAC as clean and invalidate (unchecked; that would also hide F-081 on jiffy). | Verified (code, symbol map); why it works is open | open: invalidate before the copy, not after; check DCIMVAC on the board (dirty a line, invalidate, read back) |
| F-135 | `src/arch/armv7a`, `src/plat/imx6ul/plat.c` `plat_mmu_init` | Nothing sets ACTLR.SMP before the MMU and caches are enabled; as I recall it, the Cortex-A7 MPCore TRM asks for that. What it changes on jiffy is unknown: the 2026-10-29 breadcrumbs show write-back caching of `.no_init` works without it, so it doesn't simply leave Shareable memory uncached (first guess in the USB review, withdrawn). | Reported (TRM from memory, not in `~/work/manuals`) | open: read ACTLR on jiffy (`mrc p15, 0, r0, c1, c0, 1`), get the TRM |
| F-136 | `src/drivers/usb/imx_ci_udc.c` (prime, flush, bus reset) | Against 6UL RM 54.4.6: the prime loop waits for ENDPTPRIME to clear but never checks that ENDPTSTAT got set (PRIME 0 and STAT 0 is a failed prime, 54.4.6.4.2.2; the dTD then stays Active for ever, F-130's symptom). `imx_ci_udc_flush_ep` doesn't check ENDPTSTAT and repeat a refused flush (54.4.6.6.5). The EP0 flush in `poll_setup_pkt` doesn't wait before EP0 is primed again. The bus reset handler waits `while (FLUSH == 0x00FF00FF)`, which ends when the first bit clears, and flushes without waiting for ENDPTPRIME to clear (54.4.6.2.1). None of the loops has a timeout. | Verified (code, RM) | open |
| F-137 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_xfer_complete` | Only the Active bit of the last dTD is checked. Halted, Data Buffer Error and Transaction Error are ignored (an RX overflow halts the endpoint and returns `PB_OK`), and the bytes left in Total Bytes are not reported (54.4.6.6.4). A short packet in an earlier dTD of a multi-dTD read retires that dTD and leaves the last one Active. pb's exact-size protocol avoids the last today. | Verified (code, RM 54.4.6.6.4) | open |
| F-138 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_poll_setup_pkt`, `src/drivers/usb/usbd.c` | Control transfers against 6UL RM 54.4.6.4.2: the setup packet is copied under the tripwire and then acked in ENDPTSETUPSTAT; the RM acks first, so a second SETUP arriving between the two is acked unread. After priming an EP0 data or status stage nothing checks ENDPTSETUPSTAT, so a SETUP that aborts the control transfer leaves `wait_for_completion`/`imx_ci_udc_xfer_zlp` spinning until the watchdog. Unsupported requests are never STALLed (54.4.6.3.2): the host waits out its control timeout (e.g. `lsusb -v` asking for the BOS descriptor) and `usbd_xfer_complete` fails the transfer in progress. `USB_SET_IDLE` primes an OUT status stage a no-data request doesn't have. EP0 IN has ZLT disabled, so a reply that is a multiple of 64 bytes and shorter than wLength would need a ZLP it never gets (no current descriptor is). | Verified (code, RM) | open |
| F-139 | `src/drivers/usb/imx_usb2_phy.c`, `src/board/jiffy/board.c` `cm_board_init` | 6UL clocks and PHY: `imx_usb2_phy_init` writes PLL_SIC at USBPHY+0xa0, which the 6UL USBPHY doesn't have (its map ends at VERSION, 0x80; the PLL is CCM_ANALOG_PLL_USB1). jiffy only sets EN_USB_CLKS in PLL_USB1 (reset 0x00012000: BYPASS set, POWER clear) and relies on the ROM having powered, unbypassed and locked PLL3. The charger detector is on after reset (USB_ANALOG_USB1_CHRG_DETECT EN_B = CHK_CHRG_B = 0) and RM Fig. 55-3 turns it off before D+ is pulled up; pb doesn't. Works on the board. | Verified (code, RM 18.7.2, 55.3, 55.2.7) | open |
| F-140 | `src/drivers/usb/imx_ci_udc.c` | USBMODE.SDIS is set; the RM: in high speed every OUT packet is then answered with NYET, and it "substantially limits" throughput. Worth measuring for flashing speed. The init also writes ENDPOINTLISTADDR before USBMODE.CM, the RM (54.4.6.1) sets the mode first. `CMD_ITC(x)` cast to `uint16_t` after the shift, so ITC was always 0 (immediate; fine for a polling driver). | Verified (code, RM 54.6.33, 54.4.6.1) | open (ITC: `USBCMD_ITC(0)` since `5bd1d20e`, same value) |
| F-142 | `src/cm`, `punchboot part read` | `part read` of eMMC BOOT0 or BOOT1 on jiffy fails with `Error: I/O error` (2.8.0, 2026-10-29). Not looked into. | Seen on the board | open |
| F-143 | `src/drivers/mmc/mmc_core.c:168` | `#ifdef MMC_CORE_DEBUG_IOS` lacks the `CONFIG_` prefix, so the Kconfig option `MMC_CORE_DEBUG_IOS` never has an effect. | Verified (code) | open |
| F-144 | `src/plat/imx8x/plat.c` `imx8x_wdog_init` | With `CONFIG_ENABLE_WATCHDOG` off the function has no `return` (UB; the caller gets garbage). | Verified (code) | open |
| F-148 | `src/plat/imxrt/plat.c` `plat_init` | The clock roots depend on how the chip was reset: pb sets the ARM PLL and the periph clock muxes but not all dividers, so it inherits them. On the RT1060 EVK after a power-on boot (loaded over JTAG after the ROM) it reports AHB 150, IPG 75, PER 37 MHz; booted from the NOR after a software reset, AHB 600, IPG 200, PER 100 MHz. 200 MHz is above the IPG root's 150 MHz maximum (RT1060 data sheet). Set AHB_PODF, IPG_PODF and PERCLK_PODF explicitly. | Verified (board) | open |
| F-149 | `src/board/imxrt1060evk/imximage.cfg` | The DCD writes IOMUXC_GPR17 (0x400ac044 = 0x55555555) and sets GPR16 bit 2 to make all of the FlexRAM OCRAM. HAB rejects the write: `punchboot log hab` on the RT1060 EVK has a failure, invalid address (0x22) in a CSF/DCD command, for that write. So the FlexRAM keeps its fused split (256 KiB OCRAM); pb fits, since its buffers end at 0x202bf000. On an open part the ROM goes on; on a closed one the boot fails. Drop the DCD or set GPR17 from pb (the ROM's DCD address list doesn't allow IOMUXC_GPR). | Verified (board, HAB log) | open |
| F-152 | `src/drivers/mmc/mmc_sd.c:303` | `dev->rca << RCA_SHIFT_OFFSET`: the 16-bit RCA is promoted to `int` and shifted by 16, which overflows for an RCA from 0x8000 (UBSan in `test_mmc_sd` and `test_imx_usdhc` with `PB_SANITIZE=ON`, RCA 0xAAAA). The sanitizer CI run should fail on it. | Verified (UBSan) | open |
| F-153 | Linux 6.18 + TF-A v2.12 on the i.MX8M Plus | Linux powers the audio mix through the GPC itself, which leaves its AIPS5 bridge at the reset defaults (TF-A configures AIPS5 only when its own SiP call powers the domain): the first access (audio mix clock driver, MU2) takes an SError. Not pb; U-Boot doesn't set AIPS5 either. Newer kernels have `drivers/bus/imx-aipstz.c` (MPR0 = 0x77777777 on probe and runtime resume) with a node in `imx8mp.dtsi`. | Verified (EVK) | worked around: the test bench's DTS disables AIPS5 (`59569a10`) |
| F-155 | i.MX8M Plus EVK, `30be0000.ethernet` (FEC) | The FEC's PHY (address 1, reset `gpio4 2`) doesn't answer on MDIO (`MDIO device at address 1 is missing`, `Unable to connect to phy`), with the upstream DTS. The EQOS port probes. No cable was attached. | Seen on the board | open |
| F-156 | Linux 6.18 on the i.MX8M Plus EVK, `drivers/regulator/pca9450-regulator.c` | The PCA9450 driver registers a restart handler at priority 130, above PSCI's 129, that sends the PMIC SW_RST 0x14 (a cold reset). Every `reboot` and every panic restart (`panic=5`) power cycles the DRAM and loses ramoops, whatever `reboot=` says; pb's `log linux` then shows an older record. Only a watchdog reset (WDOG_B, warm with `nxp,wdog_b-warm-reset` in the test bench DTS) keeps it: a hang, which panics Linux on the watchdog pretimeout 2 s before the reset. Not pb. jiffy has no PMIC handler and keeps it on every path. | Verified (EVK, pb fill/check and Linux pstore) | open: a test bench kernel patch that lowers the handler below PSCI, or a product DTS/board that doesn't need it |
| F-157 | `src/boot/image_helpers.c` `boot_image_verify_parts`, `src/boot/dtb.c` `overlaps_pb` | The check that a part doesn't load over pb lists code, data, stack, bss and `.no_init`, but not `.ro_data` (keystore included; `image_helpers.c` imports `_ro_data_region_*` and never uses them), `.ramlog` or the i.MX8M's `.umctl`. A signed image with a part on any of them passes. Two hand-kept lists of the same regions. | Verified (code, jiffy/8MP maps) | open: one list, or two linker ranges for all of `ram` and all of `buffers` |
| F-158 | `src/plat/{imx6ul,imx8m,imx8x}/plat.c` `plat_mmu_init` | `rw_nox` maps `[_zero_region_start, _no_init_end)` as one RW Normal region, which assumes `ram` and `buffers` are next to each other. On the 8MP that's 0x944000..0x40885000, about 1 GiB: unpopulated address space as Normal memory (speculative accesses allowed), the AIPS/GIC/DDRC windows (they still get Device attributes only because the innermost region wins) and a partial overlap with the static OCRAM entry, which `mmap_add_region` would reject with `ENABLE_ASSERTIONS`. On jiffy and the 8X the gap is DRAM. | Verified (code, 8MP map) | open: map `ram` and `buffers` as two regions |
| F-160 | `src/cm/cm_main.c` `cmd_bpak_read` | Returns `bpak_valid_header()`'s bpak error code to `cm_run()`, which logs it as a pb error (`Command error -7` for `-BPAK_NOT_SUPPORTED`). The host gets the right `NOT_FOUND`, only the log is wrong. | Seen on the RT1060 EVK (2026-11-08) | open |
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
| F-173 | `src/boot/ab_state.c` | `init` ignores `bio_read()`'s result: a copy that can't | Verified | open |
| | | be read counts as corrupt. When both reads fail and writes work, it writes the defaults | | |
| | | (nothing enabled) over a good state and returns `PB_OK`: the next boot has no active | | |
| | | partition and stays in command mode. Shown on the ramdisk (`fail_reads_after`). Install | | |
| | | the defaults only when both copies were read and are bad, else return the read error. | | |
| F-175 | `src/boot/ab_state.c` | `boot activate` with an unknown UUID clears `enable`, | Verified | open |
| | | `verified` and `error` in RAM, then returns `-PB_ERR_PART_NOT_BOOTABLE`. For the rest of | | |
| | | command mode `boot status` says none and booting the active slot fails, and the next | | |
| | | commit writes it (on the host board a board register write: nothing to boot after the | | |
| | | reset). `test_set_boot_partition` asserts the RAM change. Return before the change. | | |
| F-176 | `src/boot/ab_state.c` | `init` keeps two valid copies that differ (power lost | Verified | open |
| | | between the two writes, by pb or `pbstate`). A verified system never commits, so the | | |
| | | stale backup can stay, and it comes back when the primary goes bad (shown: B verified | | |
| | | in the primary, the old trial state in the backup, a bad primary rolls back to A). | | |
| | | Commit when they differ: the primary is always written first. | | |
| F-177 | `src/boot/ab_state.c` | Speculative rollback sets the fallback's attempts to 1 | Verified | open |
| | | and boots it at once without using one: it gets two boots (`pbstate switch B 3` gives | | |
| | | three). In the ping-pong, `error =` drops the other rollback bit. No board uses the | | |
| | | speculative mode; `test_speculative_rollback_from_a` asserts B, B, A. Not intended: | | |
| | | `boot.md` 3.6 (D-032), F-257. | | |
| F-178 | `src/board.c` | When `boot_ab_state_init()` fails (a repair write, a state partition | Verified | open |
| | | missing from the table), `board_setup()` returns before `late_init` and the boot | | |
| | | driver: in command mode `boot activate` is `-PB_ERR_NOT_SUPPORTED` and `boot partition` | | |
| | | `-PB_ERR_PARAM`, the commands that would repair it. The GPT failure path above sets up | | |
| | | the driver for that reason. | | |
| F-179 | `src/boot/ab_state.c` | Minor. With both `enable` bits set `get()` boots A, | Verified | open |
| | | `boot_ab_state_get_boot_partition()` reports none and `pbstate` both active (no writer | | |
| | | sets both today; `validate()` could reject bits outside A and B). Normal mode with no | | |
| | | verified fallback logs "Rollback to B system", then fails with `-PB_ERR`. A rollback to | | |
| | | a verified slot fails when its commit fails (read-only eMMC); booting it would be safe, | | |
| | | the next boot picks it again. `boot activate` leaves `remaining_boot_attempts` as it | | |
| | | was. `pb_boot_state_defaults(&boot_state_backup)` does nothing: commit writes only | | |
| | | `boot_state`. `boot.md` 3.2 (D-040): a rollback or an alternation that can't be written is | | |
| | | handed out; a trial whose attempt can't be written only when no verified slot is left | | |
| F-180 | `tools/pbstate/src/pbstate.c` | `pbstate_is_system_verified(NONE)` returns | Verified | open |
| | | `!!enable` (a copy of `is_system_active`). `pbstate_set_system_verified(X)` zeroes the | | |
| | | attempts also when X isn't the enabled system: it ends the other system's trial, and | | |
| | | the next boot rolls back. | | |
| F-181 | `imx_caam.c`, `cm_main.c`, `image_helpers.c` | High. An async hash job that is | Reported (probe) | open |
| | | abandoned (a read error in `boot_image_load_and_hash()`, a failed `part verify`: no `hash_final`) stays | | |
| | | in the CAAM's one-slot output ring. Every later `caam_wait_for_job()` returns the previous job's | | |
| | | completion, so `caam_ecda_verify()` reports verified from the hash job's status. In a model of the ring, | | |
| | | `auth token` then accepted any well-formed signature on a locked device. Not run on hardware. Same root | | |
| | | as F-127. Drain the ring in `caam_hash_init()` and before a verify, take the status from the ring entry, | | |
| | | finish the hash on every error path. | | |
| F-182 | `src/drivers/mmc/mmc_core.c:163`, `cm_main.c` | High. `mmc_rw()` uses `blocks = length / | Verified | open |
| | | 512` while bio's range check rounds up: a `part write` whose size isn't a block multiple loses its tail | | |
| | | and returns OK; under 512 bytes CMD24 goes out with no data and the card stays in rcv. `part verify` of | | |
| | | such a size hashes stale buffer bytes. The host disk and the ramdisk write exact byte counts, so no test | | |
| | | sees it. In cm: zero-pad the tail and round up (hosts send raw sizes, D-008). | | |
| F-183 | `Makefile:124-134` | High. `keystore.o` has no prerequisites and `srk_fuse.o` depends | Verified | open |
| | | only on the file: a changed `CONFIG_KEYSTORE`/`CONFIG_SRK_FUSE` isn't regenerated in an existing build | | |
| | | directory (shown with an out-of-tree jiffy build). pb keeps the test keys and the test SRK hash, which | | |
| | | `slc set-configuration` would fuse. `PB_VERSION` has the same gap. Depend on the input file and the | | |
| | | config. | | |
| F-184 | `src/drivers/usb/cdns3_udc_core.c:263` | The clean covers `desc_count` TRBs, not the | Verified | open |
| | | terminating one at `trb[desc_count]`: for some counts its flags word is in a cache line that isn't | | |
| | | cleaned, and a stale TRB from a larger transfer stays in memory. The effect on the controller is | | |
| | | unverified; it fits F-172 (a small write after a large one). Clean `desc_count + 1`. | | |
| F-185 | `src/cm/cm_main.c`, `usbd.c` | `authenticated` survives a USB replug, a bus reset and | Reported | open |
| | | `restart_command_mode`: the next host on a powered, locked device is authenticated. No idle timeout. | | |
| | | Clear it on bus reset and transport restart. `command-mode.md` 3.1: the session ends with the | | |
| | | transport, and the stream is the session's too (`block_dev`, `stream_written`, `stream_failed`) | | |
| F-186 | `cm_main.c` `cmd_part_verify`, `cmd_bpak_read` | Neither checks | Reported | open |
| | | `BIO_FLAG_READABLE`/`VISIBLE`: `part verify` with size 1, 2, ... and hash guesses reads any partition | | |
| | | byte by byte, hidden ones too. It also retargets the stream's `block_dev`. | | |
| F-187 | `cm_main.c:881` | The auth gate sends one result for `board command`, but hosts send the | Reported | open |
| | | request anyway (F-095): the device parses it as commands and answers INVALID_COMMAND twice; with an | | |
| | | empty request the host hangs. Drain the request and send the second result. | | |
| F-188 | `cm_main.c` `cmd_part_erase` | No `BIO_FLAG_WRITABLE` check. F-055 listed it and is | Reported | open |
| | | marked fixed, but `6a1a0e23` fixed only the range. | | |
| F-189 | `cm_main.c`, `usbd.c` | The watchdog is kicked only between commands: a `part verify` or | Reported | open |
| | | `boot part` of a few hundred MiB against a 5 s watchdog resets the board (timing not measured). | | |
| | | `cm_read`/`cm_write` spin on AGAIN without a kick or timeout. | | |
| F-190 | `cm_main.c` | Low. IDENTIFIER_READ sends 23 bytes of `buffer[0]` unauthenticated (`rz` | Reported | open |
| | | not cleared), the partition table 57 of every 128. `boot_jump()` can return after `disconnect()`. | | |
| | | `bytes_to_verify -= 4096` underflows for bpak with size < 4096. A failed data read in | | |
| | | `cmd_stream_prep_buffer` still answers OK. `block_dev` starts at 0, a valid bio. No build-time bounds | | |
| | | (buffer size, 16 keys in `cmd_slc_read`, board name). No real board sets `BIO_FLAG_READABLE`: the cause | | |
| | | of F-083 and F-142. | | |
| F-191 | `src/drivers/partition/gpt.c:353,412` | A valid GPT with fewer than 128 entries: | Reported (probe) | open |
| | | the backup array is read at `last-32` and the repair writes 33 blocks. Every boot rewrites the PMBR and | | |
| | | primary, fails on the backup and registers no partitions. | | |
| F-192 | `gpt.c:433` | The entry loop stops at the first entry with `first_lba == 0`: | Reported (probe) | open |
| | | with entry 0 deleted (`sgdisk -d 1`) the later partitions aren't registered and init returns OK. Skip | | |
| | | empty entries. | | |
| F-193 | `gpt.c:263,446` | LBAs from the disk aren't validated: an entry at LBA 1 becomes | Reported (probe) | open |
| | | a writable partition over the GPT, a 64-bit `last_lba` is truncated, a primary with MyLBA 5000 makes the | | |
| | | repair write there. | | |
| F-194 | `gpt.c:296`, `src/bio.c` | Installing a table from command mode doesn't update | Reported (probe) | open |
| | | the bio pool: until a reset the old partitions are listed and written at their old LBAs. Not documented | | |
| | | either. D-070, `bio.md` 4: in effect at once. An unchanged partition (UUID and blocks) keeps its | | |
| | | handle and flags, a changed one's handle is dead and its slot unused until the reset, a new one | | |
| | | gets a new slot; the stream ends and the policy's init runs again; no board hook runs again. For | | |
| | | the code: `bio_get_part_by_uu()` and `cmd_part_tbl_read()` stop at the first slot that isn't | | |
| | | valid; the pool needs room for both tables' partitions | | |
| F-195 | `gpt.c:385,416` | Low. Repairing one copy rewrites PMBR, primary and backup: a power | Reported | open |
| | | loss while the good copy is rewritten leaves no table. Write only the bad copy. | | |
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
| F-201 | `src/drivers/usb/usbd.c:28` | `enumerated` is never cleared: on the 8MP EVK a replug or | Reported | open |
| | | bus reset ends in a read on an unconfigured EP2, `cm_run` exits and the board resets. Clear it in | | |
| | | `usbd_init()` and on bus reset. | | |
| F-202 | `src/plat/imx8m/plat.c:225`, `main.c:42` | After a failed DDR init command mode runs | Reported | open |
| | | with `cmd`, `result`, `buffer` and dwc3's TRBs in untrained DRAM (`.no_init`, 0x40062000-). Reset on a | | |
| | | DDR failure, or keep them in OCRAM. | | |
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
| F-211 | `src/delay.c:33-47` | Low. Timeouts compare with an absolute end tick: if the 32-bit us | Verified | open |
| | | counter wraps between `ts_end` and the next poll the timeout takes another 71 minutes. Callers ignore | | |
| | | `pb_timeout_init_us()`'s result. Use `(now - start) >= timeout`. | | |
| F-212 | `src/board/imx8dxlevk` | The DXL board offers `slc lock`, but its build can't sign pb | Reported | open |
| | | (`imx8xl_sign_image`: not supported): a locked DXL rejects the only image there is. Drop | | |
| | | `set_configuration_locked` until signing exists. | | |
| F-213 | `configs/*`, board `makefile.mk` | The SRK hash that is fused (`CONFIG_SRK_FUSE`) and | Reported | open |
| | | the SRK table pb is signed with are independent settings: a product that changes one fuses a hash that | | |
| | | closes the device to its own images. D-027 and `srk_fuse.h` claim they match. Derive or check at build | | |
| | | time. | | |
| F-214 | `src/board/imxrt1060evk` | `BOARD_IMXRT1060EVK_REV_C` only swaps the FCB; the NOR config | Reported | open |
| | | stays IS25WP064 (ID, 0xD7 erase, 8 MiB), so the probe fails on an EVKC's W25Q128JW. Untried on hardware. | | |
| F-216 | `src/board.c:122-158` | F-178 is one of three: a failing `init()` or `late_init()` also | Reported | open |
| | | returns before `setup_boot_driver()`, so a RAM boot of a rescue image isn't possible when the eMMC | | |
| | | fails. | | |
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
| F-221 | `tools/pbstate/src/main.c` | `-s` with anything but a/b switches to NONE and | Reported (probe) | open |
| | | exits 0. `counter` is `uint8_t`: `-c 256` is 0, which means verified. `-w 0 zzz` writes 0. `strtol` | | |
| | | fails from 0x80000000 on 32-bit. Read-only calls end in `sync()`. | | |
| F-222 | `tools/pbstate/CMakeLists.txt`, `src/lib/crc.c` | `libpbstate.so` exports a | Reported (probe) | open |
| | | global `crc32` that isn't zlib's: with zlib loaded first every state fails the CRC check. Hide or rename | | |
| | | it. | | |
| F-223 | `tools/punchboot/session.py:242` | `_stream_finalize()` never checks the result. | Reported (probe) | open |
| | | FINALIZE enables the written eMMC boot partition (F-082): if that fails, `part write` exits 0 and the | | |
| | | board boots the old one. | | |
| F-224 | `tools/punchboot/__main__.py:567` | `boot enable` without its argument sends the | Reported (probe) | open |
| | | null UUID: the device disables boot, exit 0. Make it required. | | |
| F-225 | `__main__.py`, `session.py`, `partition.py` | The CLI replaces every exception's | Reported (probe) | open |
| | | message with a fixed string (no udev rule reads "Generic error"). Unknown partition flag bits or SLC | | |
| | | values from a newer device raise `ValueError`, which the CLI doesn't catch. Tracebacks for a missing | | |
| | | file, a binary board response, a non-semver version. | | |
| F-226 | `tools/punchboot/transport.py:39` | One fixed 10 s USB timeout; the device's caps | Reported | open |
| | | timeouts are ignored. After a timeout the session stays usable and reads the late result as the next | | |
| | | command's. Several error paths leave transfers unread the same way. | | |
| F-227 | host tools, low | `session.py:556` masks the verify size to 32 bits. With several | Reported | open |
| | | devices and no `-u` the first is used, also for `slc` commands. Three different is-this-bpak checks. The | | |
| | | CI's only-grows guards compare with the merge-base, which is HEAD on a push to master. | | |
| F-228 | `.github/workflows`, `docker/`, `scripts/ci` | Any `v*` tag publishes to PyPI with no | Reported | open |
| | | check against `version.txt`, master or the tests. Actions are pinned by tag, the publish action gets a | | |
| | | long-lived token. The dev image is partly pinned and rebuilt per job. CI builds only `pb.bin` (never the | | |
| | | signed images or `testbench/simple`), warnings don't fail. | | |
| F-229 | `Makefile:31` | The result of `genconfig.py` is ignored: a failure builds with | Reported (probe) | open |
| | | the old `.config`, a mistyped defconfig name builds the Kconfig defaults, both with exit 0. | | |
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
| F-235 | `src/main.c` | After a failed `plat_early_init()` or `plat_init()` command mode | Verified (code) | open |
| | | runs without what it needs: no console or tick, no MMU, no fuses, on the 8M the buffers in untrained DRAM | | |
| | | (F-202). `platform.md` 2: reset. The RT's `plat_init()` also returns `mbedtls_pb_init()`'s error | | |
| F-236 | `plat_boot_reason` | `src/plat/*/plat.c`: No common meaning: the 6UL's own enum, | Verified (code) | open |
| | | the 8X's SCFW value, the RT's raw WRSR bitmask (several bits set: "?"), the 8M not implemented, the host 0. | | |
| | | `platform.md` 3: `plat_get_boot_reason()` with a kind, the raw value and a name | | |
| F-237 | `include/pb/plat.h` | Also `src/device_uuid.c`: `plat_get_us_tick()` is | Verified (code) | open |
| | | `unsigned int` in the header, `uint32_t` in three ports. `plat_reset()` isn't `noreturn`. | | |
| | | `platform_ns_uuid` is only picked up with `extern` in `device_uuid.c`, and nothing says it must never | | |
| | | change (`platform.md` 3.6) | | |
| F-238 | `plat/{imx8m,imxrt}/plat.c` | The tick and `log_time_start()` are set up in | Verified (code) | open |
| | | `plat_init()`, after `board.console_init()`: before it there is no `pb_delay_*()` and the log time is 0. | | |
| | | The 6UL and the 8X do it in `plat_early_init()` | | |
| F-239 | link scripts | `PB_ENTRY` repeated as a constant in | Verified (code) | open |
| | | `src/plat/{imx6ul,imx8x}/link.lds` and `src/board/imxrt1060evk/link.lds` (0x80001000, 0x80020000, | | |
| | | 0x20249000); the RT's `MEMORY` is in its board. `buffers` fixed in the platforms, `BOARD_RAM_*` only `-D` | | |
| | | flags, `PB_RAM_LENGTH` set by hand although it follows from the ROM's load window and `PB_ENTRY`. Nothing | | |
| | | checks at link time that pb is where the ROM may load it, `buffers` in working memory, or that they don't | | |
| | | overlap. `platform.md` 5.1: Kconfig values, `BOARD_DRAM_*`, the `pb` region, `ASSERT`s | | |
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
| F-243 | `image_helpers.c` | `load_and_hash()`: The boot path never kicks the watchdog, | Verified (code) | open |
| | | only command mode does: loading and verifying must fit in `CONFIG_WATCHDOG_TIMEOUT` (5 s) from arming. | | |
| | | `platform.md` 3: kick once per chunk and before the jump, never in `pb_delay`/`pb_timeout` | | |
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
| F-249 | `src/board.c` | The board's data isn't checked: `cm_board_init()` dereferences | Verified (code) | open |
| | | a NULL `cm_transport`; a name over 16 characters overruns `board_id` (`memcpy` with `strlen`); no | | |
| | | boot driver, or several (the last wins); `.slc = NULL`. `CONFIG_CM_AUTH_PASSWORD=y` without | | |
| | | `password_auth` in jiffy's, the RT's and the host's defconfigs; imx8mevk has `CONFIG_BOOT_LINUX` | | |
| | | without `boot_linux`. `board.md` 3: checked in `board_setup()` and `cm_board_init()`. D-046: | | |
| | | "one boot driver" becomes a policy and a handover that are set; a module that isn't compiled | | |
| | | in is a link error, not a check. D-049: `cm_transport` leaves the board, and its check with it | | |
| F-250 | `src/board.c`, `src/board/*/board.c` | `default_status()` and the boards' | Verified (code) | open |
| | | `status()` set `*length` from `snprintf()`, then append the NUL at `buf[*length]`: a truncated text | | |
| | | writes past the buffer. Latent, `CM_BUF_SIZE` is 4 MiB. `board.md` 4.3: the board writes at most | | |
| | | `*length - 1` bytes, the core clamps and appends the NUL | | |
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
| F-254 | `src/boot/image_helpers.c` | `boot_image_verify_parts()` checks the parts | Verified (code) | open |
| | | against code, data, stack, bss and `.no_init`, not `.ro_data` (the keystore's keys; `ro_data_start` | | |
| | | is imported and unused) nor `.ramlog`. `CHECK_OVERLAP` has no wrap-around check (`dtb.c` has its own | | |
| | | `overlaps()`, which does), parts aren't checked against each other nor against the board's DRAM. A | | |
| | | signed image is trusted, but unverified payload bytes land there before the payload hash is checked | | |
| | | (`principles.md` 2). An allow list instead of a deny list. D-055, `boot.md` 4.3, `board.md` | | |
| | | 2.9: `image_regions` in `struct board`; a part lies inside one region and over no other part; | | |
| | | that no region overlaps `pb`, `buffers`, the Firmware Handoff region or `bl_params`' region is | | |
| | | checked once, with the board's data. The platform header names the SoC memory a board may list | | |
| F-257 | `src/boot/ab_state.c` | Speculative mode differs from `boot.md` 3.6: the | Verified (code) | open |
| | | fallback gets one trial attempt (two boots, F-177) and a rollback bit is dropped. Per `boot.md`: | | |
| | | only with no viable candidate, alternate per boot, no attempts, ends when the OS confirms, the | | |
| | | handover says so, `depends on ENABLE_WATCHDOG`. D-040: it also starts with nothing enabled (A | | |
| | | first), and `BOOT_AB_SPECULATIVE` is its only switch: the board's `rollback_mode` field goes | | |
| F-259 | `src/boot/simple.c` | The generic handover passes r0 = the system's name, | Verified (code) | open |
| | | r1 = the boot partition's UUID, and can't grow. `boot.md` 5.4: Arm's Firmware Handoff, a transfer | | |
| | | list in a board region with the image's DT or pb's structure; the tag requested in the | | |
| | | specification (Jonas); the simple target test bench follows. `BOOT_FH`, the region | | |
| | | `BOOT_FH_REGION_BASE` and `_SIZE`; the checksum against the reference library | | |
| F-260 | `src/boot/dtb.c` | `boot_dtb_set_chosen()` reads the RoT, the SLC and the device | Verified (code) | open |
| | | id itself, and runs the board's hook after `pb,device-uuid` and before pb's other properties. | | |
| | | `boot.md` 5.5, 5.6: pb's information collected once, the hook first, `pb,version`, | | |
| | | `pb,boot-partition` and `pb,boot-mode` added (and in the README's table). From F-256: the | | |
| | | boot's mode is the core's: NORMAL for a named partition or the host's stream, the policy's | | |
| | | after its report; `boot_policy_mode()` alone is stale after a failed fallback | | |
| F-261 | `plat/{imx6ul,imx8m}/plat.c` | The TrustZone filter's open region is the SoC's | Verified (code) | open |
| | | whole DRAM window, not the board's DRAM: 2 GiB on the 6UL (`tzasc_init()`), 4 or 8 GiB on the 8M | | |
| | | (`tzasc_open_dram()`). With less DRAM than the window, the copies of DRAM inside it are open to | | |
| | | the normal world until a secure OS replaces the region. `platform.md` 4.3, item 3: the board's DRAM | | |
| | | (`BOARD_DRAM_*`, F-239) | | |
| F-262 | `plat/imx8x/plat.c` | `plat_reset()` before `plat_early_init()` returns: the IPC | Verified (code) | open |
| | | handle is still 0 and `sc_ipc_write()` returns on it. `platform.md` 3.1: valid from the first | | |
| | | instruction after `arch_init()`. The other ports before early init (the WDOG's clock): unchecked | | |
| F-263 | `src/slc.c`, `src/plat/*`, `src/cm/Kconfig` | EOL: only the host platform implements `set_eol`, and | Verified (code) | open |
| | | nothing lets a product leave it out. `system.md` 4: a Kconfig option, off by default; every port | | |
| | | implements it as the SoC's own end of life state (NXP: field return) or says why it can't | | |
| F-266 | `include/pb/board.h`, `src/boot/boot.c`, the boards | `late_boot()` gets | Verified (code) | open |
| | | `struct bpak_header *header`, not `const`, which ties the hook to one image format. D-047, | | |
| | | `board.md` 5.3: the argument goes; `boot_image_get_bpak_header()` returns the verified | | |
| | | header as `const`, NULL before the payload's hash is checked. The RT's and the host's hooks | | |
| | | ignore the argument today | | |
| F-267 | `src/board/*/board.c`, `src/board.c`, `src/cm/`, `include/pb/cm.h` | Command | Verified (code) | open |
| | | mode's USB is set up by each board's `cm_init` (clocks or power domains, the PHY, the | | |
| | | controller: SoC work on every board), and the transport is `cm_transport` in `struct board`, | | |
| | | a `struct cm_transport_ops` copied into `cm_config`. D-049, `board.md` 4.1 and 4.2, `platform.md` | | |
| | | 3.10: `plat_cm_init()` at the start of `cm_run()`, the port a Kconfig choice of the platform's; | | |
| | | command mode calls the transport's functions directly, linked to the USB class driver or to the | | |
| | | host platform's socket (today in `src/board/host/board.c`). `cm_init` and `cm_transport` leave | | |
| | | `struct board` | | |
| F-268 | `src/cm/cm_main.c`, `include/pb/cm.h`, `src/board.c`, the link scripts | Command | Verified (code) | open |
| | | mode's `process` hook (`struct cm_config`, called while it waits for a command) can't be set by a | | |
| | | board since D-026: `cm_board_init()` doesn't copy it and `struct board` has no member. Out-of-tree | | |
| | | boards use it (Jonas). D-051, `board.md` 4.6: `DEFINE_TASK(name, run)` puts a task into a linker | | |
| | | section that every link script keeps; `main()` calls the tasks in turn while pb is stopped, also | | |
| | | without `CONFIG_CM`, in no order, until one returns an error; command mode becomes one of them | | |
| | | (`cm_main_task`: `cm_main.c`'s blocking loop turned into a step); `process` goes. With F-239 | | |
| | | (the link scripts) and F-267 | | |
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
| F-271 | `src/cm/cm_main.c`, `src/cm/Kconfig`, `configs/imx8m{,p}evk_defconfig` | A build | Verified (code) | open |
| | | with `CONFIG_CM_AUTH` and no method builds: once locked, its command mode answers NOT_SUPPORTED | | |
| | | to `AUTHENTICATE` for good. The imx8mevk's and the imx8mpevk's defconfigs select none. D-059 | | |
| | | (Jonas), `command-mode.md` 3.2: a build with authentication selects at least one method, or it | | |
| | | doesn't build; the two defconfigs get the token. `CONFIG_CM_AUTH` itself stays the product's | | |
| | | switch: off means no gate, an open command mode also on a locked device, by choice. The | | |
| | | option's help text says so | | |
| F-272 | `src/cm/cm_main.c` | Only `cmd_part_tbl_read()` looks at `BIO_FLAG_VISIBLE`. | Verified (code) | open |
| | | Stream init, erase, verify, bpak read, `boot part` and activate find a hidden partition by its | | |
| | | UUID (`bio_get_part_by_uu()`). With F-186 (`part verify` reads any partition byte by byte). | | |
| | | `command-mode.md` 5.3, D-058 (Jonas): a hidden partition is internal to pb and answers | | |
| | | NOT_FOUND to every command; `part verify` needs no `READABLE`. No board in the tree hides a | | |
| | | partition (children inherit `VISIBLE` from their device) | | |
| F-273 | `src/cm/cm_main.c` `stream_check()` | The stream's byte offset is divided by | Verified (code) | open |
| | | the block size and the remainder dropped: an offset that isn't a block multiple reads or writes | | |
| | | from the block's start and answers OK. The Python host sends multiples (0, the chunk size, the | | |
| | | partition's size less 4 KiB). `command-mode.md` 5.4: INVALID_ARGUMENT | | |
| F-274 | `src/cm/cm_main.c`, `tools/punchboot/session.py` | Exchanges that get out of | Verified (code) | open |
| | | step. (1) A refused `STREAM_PREPARE_BUFFER` (the gate, the size, the id) sends one result, | | |
| | | but hosts send the data whatever the first result says: it is read as commands. As F-187 for | | |
| | | `board command`. (2) `BOARD_STATUS_READ` whose `status()` fails sends the error result, the | | |
| | | data and a second result; the Python host raises at the first and leaves two transfers on the | | |
| | | link. Check what the 2.x C host did (the transcripts) before changing either side. (3) Most | | |
| | | `cm_write()` results are ignored, and a failed `cm_read()` inside a command returns to the loop | | |
| | | on the same transport. `command-mode.md` 4.4: a failed transfer ends the session | | |
| F-275 | `src/cm/cm_main.c` `pb_command_parse()` | The frame is checked before the gate: | Verified (code) | open |
| | | a locked device answers INVALID_COMMAND before authentication to a command its build doesn't | | |
| | | have and to a frame with a wrong magic, which tells a host which commands the build knows. | | |
| | | D-058 (Jonas), `command-mode.md` 3.2: NOT_AUTHENTICATED for everything but the identifier and | | |
| | | the authentication, from 3.0.0. A change of the wire's meaning, for `security.md` 4.7 | | |
| F-276 | `src/cm/cm_main.c` `cmd_auth()` | A failed authentication is answered at once: | Verified (code) | open |
| | | a host can try passwords as fast as the transport carries them. D-058 (Jonas), | | |
| | | `command-mode.md` 3.3: the result of a failed attempt is sent 1 s after the data arrived, with | | |
| | | every method, the watchdog kicked meanwhile; no counter, no lockout | | |
| F-277 | `src/cm/cm_main.c` `cm_read()`, `cm_write()`, `PB_CMD_DEVICE_READ_CAPS` | A | Verified (code) | open |
| | | transfer inside a command has no timeout: a host that stops (`AUTHENTICATE`, then no data) | | |
| | | leaves pb waiting without a kick (F-189), for ever without a watchdog. The caps' | | |
| | | `operation_timeout_ms` and `part_erase_timeout_ms` are never set. D-058 (Jonas), | | |
| | | `command-mode.md` 4.6: `CONFIG_CM_TRANSFER_TIMEOUT` (10 s), kicking while waiting, then the | | |
| | | session ends and the transport restarts; the caps carry pb's timeouts (F-226 for the host) | | |
| F-278 | `src/cm/cm_main.c` `cmd_auth()`, `auth_read_data()` | Low. An `AUTHENTICATE` with | Verified (code) | open |
| | | a size of 0 or above 1024 ends the authentication of a session that had it (`authenticated = | | |
| | | false` after `auth_read_data()` refused), one with a method that isn't in the build leaves it. | | |
| | | `command-mode.md` 3.3 (D-060): a refusal before the data changes nothing and isn't delayed; | | |
| | | with F-276 | | |
| F-279 | `src/cm/cm_main.c` `cmd_bpak_read()`, `cmd_part_verify()` | Low. The header's | Verified (code) | open |
| | | block is the partition's block count less 4 KiB of blocks, unsigned: on a partition below | | |
| | | 4 KiB it wraps, `bio_read()` refuses the range and the host gets INVALID_ARGUMENT. | | |
| | | `command-mode.md` 5.3 (D-060): NOT_FOUND for `PART_BPAK_READ`. `boot.c` has the same | | |
| | | arithmetic for a bootable partition | | |
| F-280 | `src/boot/image_helpers.c` `boot_image_load_and_hash()`, the USB drivers | `boot | Reported | open |
| | | bpak`: the device reads a part `CONFIG_BOOT_LOAD_CHUNK_kB` at a time (1 MiB on the imx8mpevk | | |
| | | and jiffy, 4 KiB on the RT1060 with a 32 KiB buffer, 4 MiB elsewhere), hosts send it in | | |
| | | transfers of the caps' `chunk_transfer_max_bytes` (the buffer's size, 4 MiB by default). | | |
| | | Where the two differ, one host transfer fills several reads. `command-mode.md` 4.4 and 6 | | |
| | | (D-060) ask the transport for that. Read in the code, not run: try `boot bpak` on the | | |
| | | imx8mpevk and jiffy. The requirement is `usb.md` 2.3 | | |
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
| | | host test (F-230) | | |
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
ed; add a timeout to `caam_wait_for_job`; consider one INIT_FINAL job when nothing was submitted |
| F-291 | `src/boot/image.c`, `dtb.c` | The image handover can't boot an image with a | Verified (code) | open |
| | | DT part in place: `boot_dtb_grow()` opens the DT into itself at its load address (`dtb.c`, | | |
| | | `fdt_open_into()`) and `/chosen` is patched there, which in place is the flash. On a NOR the | | |
| | | writes are lost or fault, on the host's flash (mapped read only) pb-host would crash. Found with | | |
| | | F-253's host tests, whose images have a kernel part only. `boot.md` 4.1 and 5.2 don't say | | |
| | | what the image handover does with a DT in place; Firmware Handoff (5.4, F-259) copies the DT | | |
| | | into its region and has no such problem. Until decided: refuse it before the DT is touched | | |
| F-330 | `src/boot/dtb.c` | An empty device tree part that the board names is read | Verified (test) | open |
| | | at its load address, bytes no hash covers: `boot_dtb_grow()` checks the FDT header there (the | | |
| | | vendored libfdt's `fdt_check_header()`: the magic and the version only) and compares its | | |
| | | `totalsize` with the part's size, 0. A blob there that says `totalsize` 0 passes, is grown by the | | |
| | | headroom, gets pb's `/chosen` and is handed to the next stage, and to BL32: a device tree nothing | | |
| | | authenticated (`security.md` 4.2). Anything else there is refused (`-PB_ERR`). As F-292 it needs a | | |
| | | signed header, and what an earlier, refused load left at the address. Found with F-292, in a | | |
| | | throwaway case of `test_image.c`. Refuse a device tree part that is smaller than an FDT header, and | | |
| | | a `totalsize` below it, before the part is read; Firmware Handoff's device tree (F-259) the same | | |
| F-331 | `doc/dev/decisions.md`, `doc/architecture/boot.md` | F-292's refusal has no | Verified (code) | open |
| | | decision entry and no contract text. `compatibility.md` asks for an entry that names what a | | |
| | | security fix rejects and why: an image whose kernel, BL31, BL32 or generic entry part is empty | | |
| | | (or, for the kernel and BL31, has a load address and no part) was jumped into by every release, | | |
| | | and the bpak CLI builds one. `boot.md` 5.1 to 5.4 say where an entry is, not that it lies in | | |
| | | bytes the payload's hash covers | | |
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
| | | handover) or drop it. `board.md` 2.5 and 3: both are mandatory, checked by F-249 | | |
| F-301 | `src/board/host/board.c` | The host board's boot source policy is an object | Verified (code) | open |
| | | its board file builds by hand: A/B's functions, with an init that also sets `BIO_FLAG_BOOTABLE` | | |
| | | on the flash's two partitions, which only `boot part` boots. `board.md` 2.5: a policy marks its | | |
| | | candidates, a board doesn't; and one policy per board. To end it: a policy made by a macro | | |
| | | that can carry candidates it never hands out, or a second policy per board. Since F-256 the | | |
| | | marks are in the policy's init, which `board_setup()` skips when the table fails: on a blank | | |
| | | disk the flash's partitions aren't bootable until a table is installed and pb restarts | | |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-020 | Mbed TLS pinned to 3.4.0 (EOL). The file list in `src/drivers/crypto/mbedtls/makefile.mk` is tied to that release. `mbedtls_config.h` enables MD5/BASE64/PKCS1_V21, which are unused. Corrected 2026-11-12: MD5 is used, for the device's UUID (`uuid3.c`, `crypto.md` 5); BASE64 and PKCS1_V21 unchecked. | open |
| F-022 | Vendored code has no version record: Kconfiglib, libfdt, libuuid, TF-A xlat_tables_v2 + libc, NXP SCFW (littlefs was removed). | open |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-030 | libc files never compiled: `memcpy.c`, `strncmp.c`, `strlcpy.c`, `strnlen.c`, `strrchr.c`. Also `xlat_tables_v2/aarch32/nonlpae_tables.c`. | open |
| F-031 | Stale `-I src/libc/include/...` in the armv7a/armv8a makefiles. The gcovr excludes still name `bearssl` and `src/fdt`. | open |
| F-032 | Deprecated commands still handled: `PART_TBL_INSTALL`, `PART_RESIZE`, `AUTH_SET_PASSWORD`. `pb_wire_command_string`/`pb_wire_slc_string` are stubs. Corrected 2026-11-12 (`command-mode.md` 5.3, 5.8): only `PART_RESIZE` is retired. `PART_TBL_INSTALL` is in use (`board.md` 2.4), and `AUTH_SET_PASSWORD` was never implemented and is to be (`roadmap.md`). | open: removing them breaks old hosts (D-008); at most answer `NOT_SUPPORTED`, never reuse the slots |
| F-033 | imx8mevk unfinished (`board_init` returns error, no Kconfig). Orphan defconfigs `bebop`, `pico8ml`. imxrt untouched since 2023. | open: ask Jonas |
| F-035 | The same data is defined in several places: exception names (C ×1, Python ×3, before D-021), partition flags, SLC enum, and the crc32 id table (`helpers.py`, could be `zlib.crc32`). | partly fixed (D-021): exception names are defined once (`errors.py`), and `wire.py` mirrors the wire values checked against `abi.txt`. Left: `pbtest.py`'s `PB_CMD_*`, the crc32 table |
| F-145 | De-duplication left from the 2026-10-29 slimming review (D-020): board boilerplate (default GPT table and `partitions.h` byte-identical in 5 boards, the same `board_init` sequence and 3 dev keys in all, cm `.tops` in 6), ~900 lines; the write/read/check/print block 42 times in `tools/punchboot/api_*.c` and the validate/call/exception pattern 28 times in `python_wrapper.c`, ~550; `plat_mmu_init` and `plat_get_unique_id` near-identical in imx6ul/imx8m/imx8x; the bpak hash-kind switch 4 times in `image_helpers.c`; one-provider crypto ops tables; `test_mmc_core.c`'s own fake card vs the card models. Porting API: `arch_init()` empty everywhere, single-member `console_ops`, `board_init(struct imx*_platform *)` (imx8m struct empty), cm `command`/`status` stubs in 4 boards. | open; the host tools part is gone with the C library (D-021) |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-041 | Wheels are never tested (no cibuildwheel `test-command`, before D-021). There are no Python unit tests. | partly fixed (D-021): `punchboot-tools.yml` installs and runs the wheel on Linux, Windows and macOS. No Python unit tests yet |
| F-043 | No `ruff-format` hook despite the format config. mypy only runs through pre-commit.ci. There is no `_punchboot.pyi`. | open (`_punchboot.pyi` is moot since D-021) |
| F-044 | Sphinx docs are mostly stubs, with version 1.0.0 in `conf.py`, broken header paths, and `sphinx<7`. The README test instructions are wrong (no `BOARD`). `doc/guides/release-checklist.txt` is stale. | partly fixed: Sphinx/RTD removed (D-020); README and the checklist still open |
| F-051 | Most boards don't build: imx8qxmek uses an old SCI API (`sci_seco_api.h`, `sc_pm_*`, `sc_pad_set`); imx8dxlevk needs a `keystore.bpak` that isn't in the repo (it builds with the internal keystore); imx8mevk needs proprietary DDR blobs; imxrt1060evk had an implicit `ts_print` (gone with the timestamps, 2026-10-29: it builds, with four unused-code warnings in `board.c`, and runs on the EVK). Only the QEMU board was ever built in CI. | Verified | open: CI builds jiffy + imx8dxlevk; ask Jonas which boards to keep (F-033) |
| F-052 | `src/drivers/virtio/*` has no user since `plat/qemu` was removed. | Verified | open: keep for a QEMU smoke tier, or remove |
| F-067 | Host test infra: `pbtest.py` `stop()` ignores pb-host's exit code, so sanitizer/LeakSanitizer failures at teardown are lost. The socket file appears at `bind()`, before `listen()` (startup race). No client timeouts, no pytest-timeout/ctest `TIMEOUT`. pb-host `write()` can die from SIGPIPE. Orphaned pb-host if pytest is killed. `fakes/ramdisk.c` computes `lba * 512` in 32 bits. 64-bit host hides 32-bit overflows (F-054); consider a `-m32` job. | Verified (read) | partly fixed (claude-work): teardown fails on non-zero exit and sanitizer reports; ramdisk overflow fixed; the socket path appears only after `listen()`; ctest `TIMEOUT` (60 s per unit test, 600 s for the integration suite). Client timeouts in the C library, SIGPIPE, orphans and `-m32` open |
| F-109 | `include/pb/log_def.h`: a disabled LOG macro expands to nothing, so `else LOG_DBG(...);` is an empty body and warns (`-Wempty-body`) at `CONFIG_LOGLEVEL=1` (seen in `src/self_test.c`, since removed). Expanding to `do { } while (0)` fixes it. Verified (build). | open |
| F-147 | `Makefile` `menuconfig oldconfig:` runs `scripts/$@.py`, but `scripts/oldconfig.py` doesn't exist (there is `olddefconfig.py`, unreachable); `.PHONY` names `olddefconfig` and `keystore`, which have no rules. `README.md:208` puts `createtoken.sh` in "the tools folder" (it's `scripts/`) and passes `-sha256` (the script adds the `-`). `build.yml:75` gives `upload-artifact` a `fail_ci_if_error` left from Codecov. | open |
