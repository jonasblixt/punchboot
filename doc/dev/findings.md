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
| F-066 | `src/lib/der_helpers.c` | `der_ecsig_to_rs` has no callers and doesn't left-pad short r/s. `der_ec_public_key_data` memcmp with a data-supplied length (0 matches anything, and an OID longer than the reference reads past it); `s -= 2` can underflow. | Reported | partly fixed (`jiffy-fixes`): `der_ecsig_to_rs` now pads short r/s and is used by the CAAM (F-057). `der_ec_public_key_data` is still open |
| | | 2026-11-13 (`ab002f9f`): `der_ec_public_key_data()` takes the key's length and reads nothing past | | |
| | | it; OIDs match on length and bytes. What is left of F-066 is in F-410 (the point's form and length) | | |
| F-078 | jiffy (hardware / boot ROM) | An SRST reset from the JTAG adapter (OpenOCD `reset`, SRSR = 1, POR) makes the boot ROM fall back to the serial downloader (`15a2:007d`) within a second, instead of booting pb from eMMC. A second clean SRST pulse did the same. A watchdog reset does too (SRSR = 0x11, after pb without WDBG was halted over JTAG, and after the `auth token` hang below). Earlier resets when the ULINK was plugged in did reboot into pb, and a power cycle does. Cause unknown. Suspect: pb leaves the eMMC in 8-bit DDR52, and a warm reset doesn't reset the eMMC. Other options: HAB, or the reset itself. `src/board/jiffy/openocd.cfg` has no `reset_config` because of this. | Seen on the board (2026-10-22) | open: a power cycle boots pb. Forcing serial download with the boot mode buttons and resetting works (uuu loads `pb_signed_uuu.imx`), but in the downloader the ROM falls back to after a failed boot, every SDP command with a data phase (write, dcd) hangs. Hardware (jiffy R1 netlist): JTAG SRST is the `RST_N` net (POR_B, LP3470 supervisor, button PB2). The eMMC RST_n is a separate net from pad NAND_WP_B with a 46.4k pull-up, so a POR_B reset doesn't reset the eMMC (see F-079). Likely cause: the same SNVS SV1/Non-Secure state as F-080 (every warm reset after JTAG activity landed in the downloader, the one without JTAG since power-on booted; a direct test is blocked because `dev reset` needs `auth`). SNVS survives a warm reset, so HAB fails on the next boot and the ROM falls back to the downloader, where data writes then hang. A power cycle clears it. Fits every reset seen so far except one SRST at 12:26. |
| F-079 | `src/board/jiffy/board.c` `usdhc_emmc_setup` | Muxes NAND_DQS (ball E6) as the eMMC reset, but on jiffy R1 NAND_DQS isn't connected. The eMMC RST_n is on NAND_WP_B (ball D5, `eMMC_RST_N`, 46.4k pull-up), which pb never configures, so the eMMC never gets a hardware reset. Possibly related to F-078. | Verified (netlist) | open: check NAND_WP_B's USDHC1_RESET_B alt in the RM, and whether the eMMC has RST_n_FUNCTION enabled |
| | | 2026-11-16, the 6UL RM (30.5.101, 30.5.106): NAND_WP_B's ALT1 is USDHC1_RESET_B (reset value ALT5, | | |
| | | GPIO4_IO11); NAND_DQS has no uSDHC function, its ALT1 is CSI_FIELD, which pb selects. Not changed: | | |
| | | with NAND_WP_B on ALT1 the controller drives RST_n, and RST_n_FUNCTION (EXT_CSD 162, written once) | | |
| | | decides whether the eMMC takes it. Read it on jiffy (`mmc extcsd read` in Linux) before muxing the | | |
| | | pad; enabling it is permanent, for Jonas. Today the pull-up holds RST_n high | | |
| F-083 | `punchboot part read` | Fails with `Error: I/O error` on jiffy, both for BOOT1 and the 512 B PB State Primary partition, in an authenticated session. Cause unknown. | Seen on the board (2026-10-22) | open |
| | | The cause (F-190, `command-mode.md` 5.3): no board in the tree sets `READABLE` on a partition, | | |
| | | and a bulk read without it answers an I/O error. The eMMC's BOOT0 and BOOT1 on 2.8.0 the same | | |
| | | (was F-142). Left: which partitions a board lets a host read | | |
| F-104 | `src/drivers/ddr/dwc_ddrphy.c` `dwc_ddrphy_train` | The training results in the message block are never read. U-Boot (all i.MX8M) reads the trained critical delay differences after each 1D run and widens the rank-to-rank gaps (`DRAMTMG2` rd2wr/wr2rd, `RANKCTL`, per P-state) before the DFI init. The imx8mevk has two ranks (`MSTR.active_ranks` 3). TF-A reads these registers back at boot and restores them after retention, so it expects the bootloader to have applied the workaround. | Verified (read, and in the differential trace: U-Boot rewrites `DRAMTMG2`, `FREQ1_DRAMTMG2` and `RANKCTL` before `dfi_init_start`) | open: needs the message block layout and an imx8mevk |
| F-106 | `src/drivers/ddr/umctl2.c` | Low power/refresh values differ from NXP's current sequence: during init `RFSHCTL3` 0x11 and `PWRCTL` 0xa8 (U-Boot: 0x1 and 0xa0, its comment says `en_dfi_dram_clk_disable = 0`; `refresh_mode` 1 is a DDR4 fine granularity setting); at the end `PWRCTL` 0x08 and `RFSHCTL3` 0x10 (U-Boot: clears only `selfref_sw`, 0x80, then `RFSHCTL3` 0 and `selfref_en`, 0x81). Punchboot also writes `DFIPHYMSTR` twice, which the board table already sets. | Verified (differential trace) | open: needs an imx8mevk |
| F-107 | `src/plat/imx8m/ddr.c` `dram_save_timing` | The trained PHY CSRs for TF-A are read after the DFI init and port enable; NXP reads them right after loading the PIE image, before the calibration wait and the DFI init. | Verified (differential trace) | open: needs an imx8mevk |
| F-110 | `src/plat/imx8m/ddr.c` `imx8m_ddr_init` (GPC) | The DDR power-up writes `GPC_PGC_CPU_0_1_MAPPING` (0x303A00EC) and `GPC_PU_PGC_SW_PUP_REQ` bit 5 (0x303A00F8), the i.MX8MQ/8MM layout, and never waits for the request to self-clear. On the i.MX8M Plus 0x303A00EC is the read-only `GPC_LPCR_CM4_AD` and 0x303A00F8 is not in the map; its PU power-up trigger is 0x303A00D8 with `DDRMIX_SW_PUP_REQ` at bit 19 (RM 5.2.10.37). NXP's 8MP DDR tool script writes the same two addresses, and the RM's own example code uses them. Harmless on the 8MP EVK: DDRMIX is powered after reset and the DRAM trains (2026-10-28). | Verified (RM, board) | partly fixed |
| | | 2026-11-16 (`277ec02b`): `imx8m_ddr_soc_power_up()` per SoC. The 8M Plus maps DDRMIX (A53 mapping | | |
| | | bit 21) and requests it (0x303A00D8 bit 19, RM 5.2.10.37, .72), and waits for the request to clear | | |
| | | (1 ms); on the EVK it reads clear right after the write. Three resets, 15 MB, Linux. Left: the | | |
| | | 8MQ's wait, which no 8MQ board has run (it writes what it wrote) | | |
| F-113 | `src/drivers/mmc/imx_usdhc.c`, `src/board/imx8mpevk/board.c` (i.MX8M Plus uSDHC3) | eMMC in DDR52 (8-bit DDR) fails at the first data read after the switch (CMD8, INT status 0x208003: DCE, a data CRC error); HS200 works. 2026-10-28, after the DDR_EN fix (F-123): with the board's fixed DLL override tap 35 the CMD8 still fails the same way; with tap 0 init gets through DDR52 (EXT_CSD reads work), but the first CMD18 (GPT read) fails: the auto CMD12 response in CMD_RSP3 is 0x00d00f00 (CC_ERROR, COM_CRC_ERROR, ILLEGAL_COMMAND, state prg), which doesn't fit a read. HS200 with tap 0 fails the same way (2026-10-29) and works with tap 35: a sampling error, the DLL override is in the read path in SDR too. The right tap depends on board and silicon (tuning). | Verified (board) | open: imx8mpevk uses HS200. Next: what CMD_RSP3 holds after auto CMD12 in DDR, and a DLL setting for DDR instead of the fixed override |
| F-135 | `src/arch/armv7a`, `src/plat/imx6ul/plat.c` `plat_mmu_init` | Nothing sets ACTLR.SMP before the MMU and caches are enabled; as I recall it, the Cortex-A7 MPCore TRM asks for that. What it changes on jiffy is unknown: the 2026-10-29 breadcrumbs show write-back caching of `.no_init` works without it, so it doesn't simply leave Shareable memory uncached (first guess in the USB review, withdrawn). | Reported (TRM from memory, not in `~/work/manuals`) | open: read ACTLR on jiffy (`mrc p15, 0, r0, c1, c0, 1`), get the TRM |
| F-138 | `src/drivers/usb/imx_ci_udc.c` `imx_ci_udc_poll_setup_pkt`, `src/drivers/usb/usbd.c` | Control transfers against 6UL RM 54.4.6.4.2: the setup packet is copied under the tripwire and then acked in ENDPTSETUPSTAT; the RM acks first, so a second SETUP arriving between the two is acked unread. After priming an EP0 data or status stage nothing checks ENDPTSETUPSTAT, so a SETUP that aborts the control transfer leaves `wait_for_completion`/`imx_ci_udc_xfer_zlp` spinning until the watchdog. Unsupported requests are never STALLed (54.4.6.3.2): the host waits out its control timeout (e.g. `lsusb -v` asking for the BOS descriptor) and `usbd_xfer_complete` fails the transfer in progress. `USB_SET_IDLE` primes an OUT status stage a no-data request doesn't have. EP0 IN has ZLT disabled, so a reply that is a multiple of 64 bytes and shorter than wLength would need a ZLP it never gets (no current descriptor is). | Verified (code, RM) | open |
| | | 2026-11-14 (`5d517494`, `8b658bf5`, `47cd8b0f`, `0037c958`): `SET_IDLE` primes no OUT status stage; | |
| | | the ChipIdea's status stage and the stack's wait for endpoint 0's data stage are bounded (500 ms, | |
| | | USB 2.0 9.2.6.4; the data stage hung on a host reset in a GET_DESCRIPTOR on the cdns3 and the | |
| | | ChipIdea, found by the USB package's review); an unsupported request no longer fails connect or the | |
| | | running transfer. Left: the ack order, an aborting SETUP seen only at the bound, STALL, EP0 IN ZLT. | |
| | | Not run on a board | |
| | | 2026-11-16 (`de68d7c0`, `693054ce`, `9303b4cd`): the ack order is the RM's, ENDPTSETUPSTAT cleared | | |
| | | before the copy (a SETUP between the copy and the clear was lost; the test has one); `usbd_hal_ops` | | |
| | | has `ep0_stall`, optional, which the stack calls for what it refuses, and the ChipIdea's is a | | |
| | | protocol stall (RM 42.5.6.3.2). On the RT1060: three unknown requests STALL in 8 ms where libusb | | |
| | | timed out after 3 s; the simple bench 7 of 7. Left: the cdns3's and the DWC3's `ep0_stall`, an | | |
| | | aborting SETUP seen only at the bound, EP0 IN ZLT; jiffy not run (F-455) | | |
| | | 2026-11-16 (`36696ae7`, `c3301080`): the cdns3's `ep0_stall` (SSTALL, then ERDY and REQ_CMPL, as | | |
| | | Linux 6.1), run on the MEK; the DWC3 stalled at its next poll already (seen on the 8MP EVK), its | | |
| | | `ep0_stall` is that code. All three STALL an unknown request in 8 ms. Left: an aborting SETUP seen | | |
| | | only at the bound, EP0 IN ZLT; whether `ep0_stall` is mandatory (`usb.md` 4.4 says so, the stack | | |
| | | registers a driver without it: an out-of-tree controller keeps working), for Jonas | | |
| | | 2026-11-16 (Jonas): `ep0_stall` is mandatory; a driver without it isn't registered (`usb.md` 4.4). | | |
| | | The host fakes always stall. Left: an aborting SETUP seen only at the bound, EP0 IN ZLT | | |
| | | 2026-11-16: EP0 IN ZLT can't happen in the tree: no reply is whole packets and shorter than | | |
| | | `wLength` (the strings are 20 and 74 bytes, the configuration 32, the SPL's product 28). An | | |
| | | out-of-tree class with a 64 byte string would hit it; a fix is each controller's (Linux's | | |
| | | `zero`: a second, empty TRB or dTD in the data stage) and a test build on three boards | | |
| F-153 | Linux 6.18 + TF-A v2.12 on the i.MX8M Plus | Linux powers the audio mix through the GPC itself, which leaves its AIPS5 bridge at the reset defaults (TF-A configures AIPS5 only when its own SiP call powers the domain): the first access (audio mix clock driver, MU2) takes an SError. Not pb; U-Boot doesn't set AIPS5 either. Newer kernels have `drivers/bus/imx-aipstz.c` (MPR0 = 0x77777777 on probe and runtime resume) with a node in `imx8mp.dtsi`. | Verified (EVK) | worked around: the test bench's DTS disables AIPS5 (`59569a10`) |
| F-155 | i.MX8M Plus EVK, `30be0000.ethernet` (FEC) | The FEC's PHY (address 1, reset `gpio4 2`) doesn't answer on MDIO (`MDIO device at address 1 is missing`, `Unable to connect to phy`), with the upstream DTS. The EQOS port probes. No cable was attached. | Seen on the board | open |
| F-166 | `src/plat/*/slc_helpers.c` | The SLC state changes are checked differently per | Verified | partly fixed |
| | | platform: the 6UL locks only from CONFIGURATION, the 8X from any state. No | | |
| | | platform reads the fuses back after writing them. The rules belong in | | |
| | | `src/slc.c`. `work-3.0.0-plat`: they are; still no read-back (the 8X's | | |
| | | life cycle may only change on reset). | | |
| F-167 | `src/plat/imxrt/rot_helpers.c` | The RT1060 reports every key as valid and can't | Verified | partly fixed |
| | | revoke one, although HAB has `SRK_REVOKE` there too. `work-3.0.0-plat`: it | | |
| | | reads `SRK_REVOKE` (`imx_hab_fuses.c`); the EVK board doesn't revoke or lock. | | |
| F-180 | `tools/pbstate/src/pbstate.c` | `pbstate_is_system_verified(NONE)` returns | Verified | open |
| | | `!!enable` (a copy of `is_system_active`). `pbstate_set_system_verified(X)` zeroes the | | |
| | | attempts also when X isn't the enabled system: it ends the other system's trial, and | | |
| | | the next boot rolls back. | | |
| | | 2026-11-13 (`3164cb0c`): `pbstate_set_system_verified(X)` zeroes the attempts only when X is the | | |
| | | enabled system, so confirming the fallback keeps the trial (`boot.md` 3.5). Left: what | | |
| | | `pbstate_is_system_verified(NONE)` returns (0, -EINVAL, any slot verified, or the enabled one), for | | |
| | | Jonas; it still returns `!!enable` | | |
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
| | | 2026-11-13 (`132f13fc`): `boot_jump()` returning after `disconnect()` is done with F-252. Left: | | |
| | | `BIO_FLAG_READABLE` (F-083) | | |
| F-200 | storage, low | `imx_hab_fuses.c:159`: a partly programmed SRK word is refused as already | Reported | open |
| | | fused. `mmc_core.c`: `mmc_wait_state()` and `mmc_bio_xfer()` (`max_chunk_bytes == 0`) can loop forever; | | |
| | | a zero BOOT_MULT gives a 2^32 block bio. `imx_usdhc.c`: ADMA2 addresses truncated to 32 bits. | | |
| | | `plat_get_unique_id()` (6UL, 8M, RT) ignores `imx_ocotp_read()` errors: the device UUID comes from | | |
| | | uninitialised stack. | | |
| | | 2026-11-13: `mmc_bio_xfer()` with `max_chunk_bytes == 0` ends with -PB_ERR_PARAM since F-182, where | | |
| | | it looped | | |
| | | 2026-11-13 (`af45e648`, `507e30fd`, `74d45226`): `plat_get_unique_id()` returns a failed fuse read's | | |
| | | error on the 6UL, 8M and RT and writes nothing; a readable device's bytes are pinned | | |
| | | 2026-11-13 (`71dc3341`, `1ebcd9ac`, `c826b62d`): `mmc_wait_state()` gives up after its timeout, a | | |
| | | boot or RPMB partition of size 0 gets no device, a DMA address above 4 GiB is `-PB_ERR_PARAM`. Left: | | |
| | | the SRK word (the fuse package), and an e.MMC user area with SEC_COUNT 0, which still gets 2^32 | | |
| | | blocks (refusing it could stop a small byte addressed e.MMC that boots today; for Jonas) | | |
| | | 2026-11-13 (`d8d98609`): a partly programmed SRK word is completed when its bits are a subset of the | | |
| | | hash's word (`command-mode.md` 5.2); one with another bit is still refused | | |
| F-205 | USB, low | cdns3: `setup_pkt` shares its cache line with boot.c's callbacks and digest | Reported | open |
| | | (invalidated per SETUP, F-081 class); `trb[128]` and `align_buffer` bounds; four waits without timeout. | | |
| | | dwc3 rounds an OUT TRB up to the max packet: up to 511 bytes past a part in a RAM boot. | | |
| | | `imx_ci_udc.c:193`: `align_length` never cleared (F-131). `imx_salvo_phy.c`: register table from U-Boot | | |
| | | (GPL-2.0+) in a BSD file (F-108 class). | | |
| | | 2026-11-13: `align_length` is done with F-131. Still there, and certain by reading: a cdns3 transfer | | |
| | | of exactly 128 TRBs writes `trb[128]`, and `ep0_xfer_start()` copies into the 512 byte | | |
| | | `align_buffer` unbounded | | |
| | | 2026-11-14 (`e46baba4`): the cdns3's setup packet and rings own their lines. Left: the bulk ring's | | |
| | | and the bounce buffer's bounds, four waits without a timeout | | |
| | | 2026-11-14 (`660ee49d`): the cdns3's bulk ring takes 127 TRBs at most (15.75 MiB), a longer transfer | |
| | | is `-PB_ERR_PARAM`; a reply on endpoint 0 longer than the 512 byte bounce buffer is refused; the | |
| | | core's three waits are bounded (`IN_RST` 100 ms, the status stage 50 ms by USB 2.0 9.2.6.4, `DFLUSH` | |
| | | 1 ms), the glue's is F-281's; a cancel whose flush times out refuses the next bulk transfer until | |
| | | `init`. Bounds from Linux 6.1, no manual. Left: the DWC3's OUT rounding, the salvo PHY's licence. | |
| | | Not run on a board | |
| | | 2026-11-16 (`ce6deb95`): the DWC3's OUT rounding: the rest of the last packet goes to a bounce | | |
| | | buffer in a second, chained TRB (CSP=0, 8MP RM Table 11-1), nothing past `length`; a test has a | | |
| | | host that sends its last packet whole. On the 8MP EVK: odd-sized writes 7 of 7, `boot bpak` of 15 MB | | |
| | | and Linux. Left: the salvo PHY's licence; the cdns3's past `length` unverified | | |
| | | 2026-11-16 (`cae67f28`): bytes past `length` in the bounce TRB are `-PB_ERR_IO`. Unverified on a | | |
| | | board: a short read's count when a short packet ends inside the first TRB with a bounce TRB | | |
| | | queued (the 8M SPL's fastboot, an odd last chunk and a host that sends less) | | |
| | | 2026-11-16, on the MEK (`f470ba88`): the cdns3 writes nothing past `length`: a 700 byte buffer | | |
| | | sent as two whole packets (324 bytes of 0xaa after it) left the buffer's bytes after 700 as they | | |
| | | were (a 1024 byte `STREAM_WRITE_BUFFER` and `part verify`). The extra is dropped without an error, | | |
| | | where the ChipIdea and the DWC3 fail it. Left: the salvo PHY's licence | | |
| F-210 | arch/plat, low | v7m: CACR.FORCEWT never cleared at the handover, VTOR never set to pb's | Reported | open |
| | | table. 6UL: PLL1 raised without writing CACRR.ARM_PODF (F-148 class), SRSR never cleared (stale reset | | |
| | | reason). Exception policy differs per arch, v8 handlers run on the faulting stack and hang by default. | | |
| | | 8X `BOARD_RAM_END` maps non-RAM. `armv7a/arch.c:37`: clean then invalidate of the live stack in C. | | |
| | | `imx_uart.h`: `UFCR_RFDIV_1` is divide by 2. | | |
| | | D-053: pb doesn't clear the reset reason (SRSR), the OS reads the same register; that item | | |
| | | is closed | | |
| | | 2026-11-14 (`0df35aa3`, `2ff24732`, `66d018db`, `978f4fe6`, `b9f341a6`): `UFCR_RFDIV_2` names the | |
| | | divide by 2 (`pb.bin` unchanged); the v7a stack is cleaned and invalidated in one DCCIMVAC by range; | |
| | | v7m sets VTOR to pb's table at the entry (it was the ROM's, 0x00200000: pb's faults went to the | |
| | | ROM's handlers) and clears CACR.FORCEWT at the handover (`pb/arch.h` names it). The 8X's | |
| | | `BOARD_RAM_END`: done by `2599f9d8`, `ffc45fbb`. Reviewed; not run on a board. Left: the exception | |
| | | policy (v8 handlers on the faulting stack, hang by default; v7m hangs; v7a resets), the 6UL's PLL1 | |
| | | and ARM_PODF | |
| | | 2026-11-15 (`b7561ea1`): on the RT1060 with pb at `b3901579` the simple bench passes 7 of 7, the | | |
| | | application now checks CACR.FORCEWT clear at its entry, and VTOR at its table. Left as above: the | | |
| | | exception policy, the 6UL's PLL1 and ARM_PODF (jiffy, F-455) | | |
| | | 2026-11-16 (`5618697d`): the 6UL writes ARM_PODF (divide by 2) on the step clock and waits for | | |
| | | CDHIPR's busy bit; CACRR resets to divide by 1 (RM 18.6.5), jiffy's ROM leaves 1 (read over JTAG). | | |
| | | `test_plat_imx6ul`. Not run on jiffy yet. Not done: speed grade 2's 696 MHz has no VDD_ARM_CAP in | | |
| | | the data sheets on the share (IEC Rev 2.2, CEC: 528 MHz at 1.15 V at most), and pb leaves the | | |
| | | LDOs as the ROM sets them (jiffy: ARM and SOC 1.175 V). Left: the exception policy | | |
| F-214 | `src/board/imxrt1060evk` | `BOARD_IMXRT1060EVK_REV_C` only swaps the FCB; the NOR config | Reported | open |
| | | stays IS25WP064 (ID, 0xD7 erase, 8 MiB), so the probe fails on an EVKC's W25Q128JW. Untried on hardware. | | |
| F-218 | boards, low | `IMX8X_FUSE_DRY_RUN` doesn't gate | Reported | open |
| | | `sc_seco_commit()`/`forward_lifecycle()`. The shared `default_command` changed the 8X boards' answer | | |
| | | from OK to NOT_SUPPORTED (`board.md` 8: fixed for a product in the field). Board data isn't | | |
| | | validated, `CM_AUTH_PASSWORD=y` without a callback: moved to F-249. Dead Kconfig symbols | | |
| | | (`ARMV8_EL3`, `choice BOARD`, `IMXRT_FLASH`). GPT tables can't align partitions. 8X | | |
| | | `early_boot()` ignores SCU results. | | |
| | | 2026-11-13 (`bd8bb259`, `1f0f581f`): with `IMX8X_FUSE_DRY_RUN` the lock and a revocation log and | | |
| | | return OK without a call to the SECO (the MEK's defconfig has it on); `choice BOARD` and | | |
| | | `IMXRT_FLASH` are gone. `ARMV8_EL3` stays, four defconfigs set it. Left: the 8X boards' answer to an | | |
| | | unknown command, GPT tables that can't align partitions, the 8X `early_boot()` ignoring SCU results | | |
| | | 2026-11-14 (`4f5b5dda`): the 8X's `plat_early_init()` checks the SCFW's answers for GPT0's power and | |
| | | rate and resets on a refusal (a refused rate set GPT0 up with the answer's word); the channel's open | |
| | | is checked (it can't fail). The boards' `early_boot()` has nothing left: the boot type is F-251's, | |
| | | the button's status has no result. Left: the 8X boards' answer to an unknown command, GPT tables | |
| | | that can't align partitions | |
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
| | | 2026-11-14 (`88785f79`): with several devices and no `-u`, `slc configure`, `lock`, `eol` and | | |
| | | `revoke-key` stop before they connect and list the devices; every other command still uses the | | |
| | | first. Left: nothing of the row that isn't a choice of Jonas's | | |
| F-228 | `.github/workflows`, `docker/`, `scripts/ci` | Any `v*` tag publishes to PyPI with no | Reported | open |
| | | check against `version.txt`, master or the tests. Actions are pinned by tag, the publish action gets a | | |
| | | long-lived token. The dev image is partly pinned and rebuilt per job. CI builds only `pb.bin` (never the | | |
| | | signed images or `testbench/simple`), warnings don't fail. | | |
| | | 2026-11-13 (`b31112fc`, `54c36b42`): the publish job fails unless the tag is `v` plus `version.txt`; | | |
| | | every action is pinned to the commit its tag names (checked with `git ls-remote`). Left: the long- | | |
| | | lived token, the dev image, CI builds only `pb.bin`, warnings don't fail | | |
| | | 2026-11-16 (`ac00e5bf`, `c877f656`): a warning fails CI's firmware builds (`PB_WERROR=1`; all seven | | |
| | | boards are clean from a clean build, Mbed TLS's `-Wredundant-decls` off for its sources), and the | | |
| | | RT1060's job builds `testbench/simple`. Left: the long-lived token, the dev image, the signed images | | |
| F-230 | `doc/guides/testing.md`, `status.md` | Line coverage is 91.0% (4445/4886), not | Reported (probe) | open |
| | | 99.2%. `imx_caam.c` 38%: `caam_ecda_verify` and the hash path 0%. Never compiled by a host test: | | |
| | | `boot/atf.c`, `boot/simple.c`, `usbd.c`, `pb_dev_cls.c`, `partition/static.c`. No tests: `imx_ocotp.c`, | | |
| | | `imx6ul/hab.c`, the 8X RoT/SLC helpers, the UDCs, `imx_flexspi.c`. | | |
| | | 2026-11-14 (`d4b509a2`..`09a2c478`): measured with CI's gcovr command at `63249d59`: 77.0% of lines, | | |
| | | and 77.4% (7921/10237) with the new tests; 91.8% without NXP's SCFW clients. `hab.c` and the 8X's | | |
| | | fuse, RoT and SLC helpers are at 100%, and `testing.md` has the figure with its date and command, a | | |
| | | row for every test program and the files no test compiles. Left: the DWC3's driver, the virtio | | |
| | | drivers (F-052), what `testing.md` lists as least covered | | |
| | | 2026-11-16 (`7279a795`, `57b1f9f3`, `d0c752c8`): the 6UL's `plat_get_info()`, `tzc380_get_region()`, | | |
| | | `plat_info.c`. 81.3% of lines, 93.4% without the SCFW clients (`testing.md`). Left: the DWC3, the | | |
| | | virtio drivers (F-052), the rest of `testing.md`'s least covered | | |
| F-231 | build, low | Pre-release builds identify as 3.0.0. `blobs/` is shared in the source root | Reported | open |
| | | (pb relinks every make). pbstate is built without sanitizers. `-fno-stack-protector`, no `-Wundef`. | | |
| | | `.gitignore` ignores `*.bin` globally, matching tracked pki files. | | |
| | | 2026-11-13 (`564da2d5`, `b88ebf5a`): `.gitignore` keeps `pki/`'s `.bin` files, pbstate is built with | | |
| | | the sanitizers in a sanitizer run. `-Wundef` isn't added: 785 warnings from 28 lines in 15 files | | |
| | | (the vendored TF-A arch headers, `cm_main.c`, `mbedtls_pb.c`). Left: the version of a pre-release | | |
| | | build, `blobs/`, the stack protector, `-Wundef` | | |
| | | 2026-11-14 (`0447427d`, `26fc9636`, `pkg-b3-wundef` landed): `-Wundef` is on (the six `pb.bin` | | |
| | | byte for byte the same); the blobs are read where they are, nothing is written to `blobs/`, | | |
| | | and the 8M firmware isn't written again, so a second build makes nothing. Left: the version | | |
| | | of a pre-release build, the stack protector | | |
| F-232 | docs | The README (also the PyPI page) describes removed things (PBI, C parts and binary | Reported | open |
| | | wheels, the auth cookie, Bebop/PICO) and lacks the 8MP EVK and RT1060. Missing: a porting guide for | | |
| | | 3.0.0, firmware build and image signing, the A/B update flow and pbstate, the SLC and fusing procedure, | | |
| | | a changelog, SECURITY.md. Nothing on storage, USB or exceptions (the overview that lacked them is | | |
| | | removed, D-062); `roadmap.md` phases 0-1 are done; `testing.md` lacks 12 unit tests; `imx8m-ddr.md` | | |
| | | step 4. | | |
| | | 2026-11-14 (`c86cf19c`..`fdd050a9`): drafts, for Jonas's review: `doc/guides/migration-3.0.md`, | | |
| | | `doc/guides/porting.md`, `CHANGELOG.md` with 3.0.0's notes, and the README without what is gone and | | |
| | | with the boards that exist. Left: the guides' sections for F-247, F-240, F-282 and F-269, the A/B | | |
| | | update flow and pbstate, the SLC and fusing procedure, SECURITY.md; `process.md` 2, `AGENTS.md` and | | |
| | | `roadmap.md` still list the guides as to write | | |
| | | 2026-11-16: `guides/updates.md`, the A/B update flow and pbstate, a draft from `boot.md` 3.5 to | | |
| | | 3.7 and 5.6 and `pbstate`'s options, for Jonas | | |
| F-241 | `plat/imx6ul/plat.c` | `tzasc_init()`: A failure is only logged: nothing tells | Verified (code, RM) | open |
| | | the boot driver that the TZASC is bypassed. Region 1 gets the physical DDR base, while 6UL RM 52.3 says the | | |
| | | TZASC sees the DDR controller's "local addresses": check with OP-TEE that its memory is protected. GPR9 has | | |
| | | no lock bit (RM p. 1277), so the secure OS's CSU policy decides whether the normal world can bypass it; the | | |
| | | TZASC_ENABLE fuse (RM p. 209) makes the ROM enable it. GPR1 TZASC1_BOOT_LOCK unread (TZC-380 TRM, DDI0431, | | |
| | | not on the share) | | |
| | | 2026-11-14 (`cf79ee28`): a `tzasc_init()` that fails is recorded as bypassed and refuses the boot | | |
| | | (F-248). Left, each needing a board or OP-TEE: region 1 gets the physical DDR base, GPR9 has no | | |
| | | lock, TZASC1_BOOT_LOCK is unread | | |
| | | 2026-11-14, D-084: the TZC-380 TRM goes on the share | | |
| | | 2026-11-15, the TZC-380 TRM (DDI0431C) in the Library: TZASC1_BOOT_LOCK drives the TZC-380's | | |
| | | secure_boot_lock input. With it high, lockdown_select is read only, and so are the registers it | | |
| | | selects and the regions lockdown_range names, until the TZASC is reset (TRM 2.2.8, 3.2.3, 3.2.4). | | |
| | | OP-TEE 4.9 programs region 1 at DDR's physical address on the 6UL, as pb does (only the 8M | | |
| | | subtracts the DRAM base), and writes and checks lockdown_range and lockdown_select, but never sets | | |
| | | TZASC1_BOOT_LOCK: its lockdown has no effect. pb must not set it, OP-TEE's checks would then fail. | | |
| | | `cee1b689`: pb reads region 1 back and refuses the boot when the TZASC ignored the write. On | | |
| | | jiffy: System A to the login. Left: whether a warm reset resets the TZASC (F-455), and the CSU | | |
| | | policy that keeps the normal world off the TZASC's registers (TRM 2.1: secure only), the secure OS's | | |
| F-246 | `src/plat/imx6ul/plat.c` | The AIPS-TZ bridges are never programmed: the 6UL | Verified (on jiffy) | open |
| | | RM (14.6) says they "should be programmed before use", reset MPR 0x77000000 and OPACR 0x44444444 | | |
| | | (supervisor only, masters beyond the first two untrusted). U-Boot opens them (`init_aips()`); OP-TEE's code | | |
| | | read doesn't. Seen on jiffy: `devmem 0x021BC050` from Linux's user mode is a bus error ("external abort on | | |
| | | non-linefetch"). Check what else Linux misses (DMA masters), then set them in `plat_init()` (`platform.md` | | |
| | | 4.5) | | |
| | | 2026-11-14, D-084 (Jonas: yes): `plat_init()` writes MPR 0x77777777 and every OPACR 0 on | | |
| | | AIPS-1 and AIPS-2, as `init_aips()`. Built, unit tested, not run on jiffy. A 6ULL's AIPS-3 | | |
| | | isn't set (no board; reserved space on the 6UL) | | |
| | | 2026-11-14 on jiffy from BOOT0 (`0715d30a`): `devmem` of the IOMUXC (0x020E0000, 0x020E4000) | | |
| | | from Linux's user mode reads; the AIPS-TZ's own registers still abort. The OCOTP read hung | | |
| | | (its clock, likely gated). Left: the SDMA's peripheral access, unchecked; a 6ULL | | |
| F-247 | `src/board/*/board.c` | The boards wire the SoC's RoT and SLC functions | Verified (code) | open |
| | | (`board.rot`, the `slc_config` callbacks) and set up the crypto engine and its RNG (`imx_caam_init()`, the | | |
| | | RNG4 instantiation), while the RT's `plat_init()` sets up mbedtls. D-030: `plat_rot`, `plat_slc` and the | | |
| | | crypto engine in `plat_init()` behind a Kconfig option (`platform.md` 2.2, 4.1, 4.2) | | |
| | | 2026-11-14 (`7c67ddee`..`c4cfdf05`): in the code. `struct board` has no `rot` and no `slc`, it has | | |
| | | `slc_fuses` and `slc_fuses_length`; `pb/plat.h` declares `plat_rot` and `plat_slc`, each port's | | |
| | | tables what its boards had (pinned per port); the CAAM and its RNG (the 6UL, the 8M), the CAAM with | | |
| | | its SCFW power requests (the 8X) and Mbed TLS (the RT, the host) are set up last in `plat_init()`, | | |
| | | and nothing else moved on any board. A crypto engine that didn't come up refuses the boot before a | | |
| | | candidate (`PLAT_CRYPTO_ENGINE`, selected by every port), so an A/B trial spends no attempt. Left | | |
| | | open for the guides only: the migration guide's 18.1 and its sections 1 and 13, the porting guide's | | |
| | | 2.3 and 2.4 still describe it as to come. Not run on a board | | |
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
| | | 2026-11-14 (`50c2da13`..`eb743825`): the stop is the core's, `CONFIG_CM_ON_SERIAL_DOWNLOAD` (default | | |
| | | y), before the preconditions and never in a boot command mode started. The 6UL reports a UART | | |
| | | download as UART, the 8X the SCFW's boot type (the MEK and the DXL dropped their own check), the 8M | | |
| | | the ROM log's event 0xd0 as USB (from AN12853, never seen in a board's log), the RT SBMR2's BMOD | | |
| | | 0b01. Left: the RT's blank IVT at the NOR's start, which needs an AHB read of the FlexSPI window in | | |
| | | every boot and a bench first. Not run on a board | | |
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
| F-263 | `src/slc.c`, `src/plat/*`, `src/cm/Kconfig` | EOL: only the host platform implements `set_eol`, and | Verified (code) | open |
| | | nothing lets a product leave it out. `system.md` 4: a Kconfig option, off by default; every port | | |
| | | implements it as the SoC's own end of life state (NXP: field return) or says why it can't | | |
| | | 2026-11-13 (`404803b1`): `CONFIG_SLC_EOL`, off by default, on in the host's defconfig: without it | | |
| | | `slc_set_eol()` and SLC_SET_EOL answer NOT_SUPPORTED and reach nothing, which is what every board | | |
| | | answered already (no port has `set_eol`). Left: each port's own end of life, or why it can't | | |
| | | 2026-11-14, D-084: NXP's field return; a fuse, or the 8X's signed SECO message (PR #51) | | |
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
| | | 2026-11-14: written in part, on the branch `pkg-dma` (nine commits on `e7a3a9b8`, not landed, its | | |
| | | verification not finished: the session's limit): `pb/dma.h` with a D-cache model for the tests, the | | |
| | | cache by range in `pb/arch.h` on every arch, `arch_init()`'s line check, and the CAAM, the uSDHC, | | |
| | | the ChipIdea, the cdns3 and the DWC3 through it. Left: the cores' refusal of a buffer that doesn't | | |
| | | own its lines, the boot's bounce for such a load address, the contracts' markers; then the whole run | | |
| | | and a bench, since every driver it changes runs at every boot | | |
| | | 2026-11-14 (`2761391c`..`45fdb7c6`): in the code. `pb/dma.h` with a D-cache model for the tests, the | | |
| | | cache by range in `pb/arch.h` on every arch, `arch_init()`'s line check, the five DMA drivers | | |
| | | through it; `bio_read()` and the USB transport refuse a buffer that doesn't start on a line | | |
| | | (`-PB_ERR_ALIGN`; three of pb's own buffers were put on a line: the GPT's MBR, the A/B state, the | | |
| | | test fuse box); the boot bounces a part whose load address isn't on a line through a chunk of its | | |
| | | own, so no image is refused. The register writes of the five drivers are the same; their cache | | |
| | | operations changed beyond the ranges in the CAAM, the ChipIdea, the cdns3 and the DWC3, each named | | |
| | | in its commit. Left: `drivers.md` 4.4's assertion "in a debug build", which pb doesn't have (for | | |
| | | Jonas). Not run on a board: every driver it changes runs at every boot | | |
| | | 2026-11-15, on the bench, with every F-282 commit: jiffy (`55689509`: the CAAM, the uSDHC, the | | |
| | | ChipIdea) writes and verifies 15 MB 3 of 3 and boots System A to the login; the MEK (`4a017151`: | | |
| | | the cdns3, the uSDHC, the CAAM) the same, and `boot bpak` (log 2026-11-15); the 8MP EVK | | |
| | | (`e5a4efc4`, the DWC3) boots Linux (log 2026-11-15); the RT1060 (`246bd897`, all but `647584c9`: | | |
| | | the FlexSPI, the ChipIdea) runs `boot bpak`. Left: the "debug build" question, for Jonas | | |
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
| | | 2026-11-14 (`b3f58c2a`): the conformance suite has an erase (exactly the blocks, 0, outside the | |
| | | device, or `-PB_ERR_NOT_SUPPORTED`), `write_done` after a complete write, and a write that fails | |
| | | half way (the error, nothing outside written, usable after), with a fault hook per device. It runs | |
| | | on the RAM disk, the host's disk and flash, the e.MMC (user area and a boot partition), the SD, the | |
| | | uSDHC and the FlexSPI model: every one conforms. Left: the GPT's `512`s (`gpt_pmbr[512]`, three | |
| | | divisions; for Jonas) and a read that fails, in the suite | |
| | | 2026-11-15: the suite has a read that fails (whole blocks, half a block), on the RAM disk, the | | |
| | | e.MMC, the SD, the uSDHC and the FlexSPI; the host's files skip it. Left: the GPT's `512`s (for | | |
| | | Jonas) | | |
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
| | | 2026-11-13 (`b982841b`, `539e946a`, `75debca1`, `279ea82f`, `109ce871`, `cd00f7df`): (3), (4), (11) | | |
| | | and (12) are done: `final` checks its room and the CAAM stores the digest in a buffer of its own | | |
| | | that is copied; a signature that doesn't verify is `-PB_ERR_SIGNATURE`, an engine error | | |
| | | `-PB_ERR_IO`, a key that doesn't parse `-PB_ERR_ASN1` on both backends (every caller turns any of | | |
| | | them into a refusal, nothing changes on the wire); one conformance suite runs against Mbed TLS and | | |
| | | the CAAM's model; a second registration is `-PB_ERR_PARAM`. (8): `imx_caam_stop()` exists and | | |
| | | nothing calls it yet (with F-247). Left: (5) and (6) for Jonas, (8)'s caller, (10): the RT1060 | | |
| | | registers SHA-256 and MD5 while its keystore has P-384 and P-521 keys, so a SHA-384 or SHA-512 | | |
| | | header and those keys' tokens are refused there (`crypto.md` 2: the build has every algorithm its | | |
| | | keys use; the defconfig, for Jonas because of the flash it costs) | | |
| | | 2026-11-14 (`f3b5ef34`): (8)'s caller: `plat_lockdown()` on the 6UL, 8M and 8X ends with | | |
| | | `imx_caam_stop()` (on the 6UL after the OCOTP locks); a failed stop fails the lockdown. Four | | |
| | | register writes no board has run; the commit reverts alone | | |
| | | 2026-11-16, (10)'s cost measured: the RT1060 at `25b44094` with `CONFIG_MBEDTLS_MD_SHA384` and | | |
| | | `_SHA512` on: text 70632 to 74288 bytes (+3.6 KiB), data +640, `pb_flash.bin` 108312 to 112408 bytes | | |
| | | of the 256 KiB BL partition. The curves are in the build already. Not changed: for Jonas | | |
| | | 2026-11-16 (Jonas): (10) is dropped, the RT1060 stays with SHA-256 and MD5: Mbed TLS is to be | | |
| | | replaced, the hashes come with what replaces it | | |
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
| | | 2026-11-13 (`cea925c3`, `9db27585`, `672bb06c`): of (5), every operation is checked at registration | | |
| | | (`stop` too) and `SET_CONFIGURATION` takes 1 or 0; (9) is done. `SET_IDLE`, the debug descriptor and | | |
| | | an unknown string index are as they were: no controller can stall. A host test runs the stack | | |
| | | (`test_usbd`) and the ChipIdea's and the cdns3's drivers | | |
| | | 2026-11-14 (`5d517494`): of (5), `SET_IDLE` no longer returns an error after its answer; it is still | |
| | | answered, not stalled. Command mode's branch that keeps polling after `-PB_ERR_TIMEOUT` | |
| | | (`cm_main.c`) is dead since `21875660`: the next poll is `-PB_ERR_STATE` and the transport restarts | |
| | | 2026-11-14 (`aaaeb9f5`, `5914751c`, `9e26bfe7`): of (8), the socket's `init` listens and returns, | |
| | | `connect` accepts without blocking (`-PB_ERR_AGAIN`), so the ready timeout runs on the host | |
| | | (`test_host_socket`); of (3), the DWC3 refuses a length above 16 MiB - 1 KiB (`-PB_ERR_PARAM`, | |
| | | static asserts on command mode's buffer and the boot's piece), the cdns3 sends one empty packet for | |
| | | a write of 0 bytes and refuses a read of 0; of (1), command mode treats `-PB_ERR_TIMEOUT` while | |
| | | waiting for a command as lost, the dead branch is gone. Left: a restart without a disconnect, the | |
| | | disconnect's ignored result, the DWC3's read of 0 bytes, (2), (4), the ChipIdea's (7) (in work). Not | |
| | | run on a board | |
| | | 2026-11-14 (`647584c9`, `94e64f3f`, `59f5bda7`): of (7), the ChipIdea's `xfer_complete` looks at its | |
| | | endpoint and gives a result once; its queue heads were all cleaned since `b785140a`. Of (3), the | |
| | | ChipIdea's longest transfer is its own, 4 MiB (205 dTDs), a longer one `-PB_ERR_PARAM`; the | |
| | | ChipIdea, the DWC3 and the cdns3 check `CONFIG_CM_BUF_SIZE_KiB` and `CONFIG_BOOT_LOAD_CHUNK_kB` | |
| | | against theirs at build time (the migration guide names the limits). Left: a restart without a | |
| | | disconnect, the disconnect's ignored result, the DWC3's read of 0 bytes, (2), (4) | |
| | | 2026-11-14 (`e18b4785`, `55ac5fe2`, `ee03e73f`, `1c7001b4`): of (1), every restart in command mode | |
| | | is a disconnect, then `init` (`CM_RESTART`), and the socket's disconnect closes the client; a | |
| | | disconnect that fails before a jump is a failed boot (no jump, pb resets), at a restart a failed | |
| | | start (pb resets); of (3), the DWC3 refuses a read of 0 bytes, and command mode never asks for one: | |
| | | a `STREAM_PREPARE_BUFFER` of size 0 (v2.7.1's C API passes any size) reads nothing and answers as | |
| | | before. Left: (2) a short read, (4) speed. Not run on a board | |
| | | 2026-11-15, on the MEK: `boot bpak`, `dev reset` (F-453) and writes of 15 MB run over the cdns3. | | |
| | | Its write of 0 bytes isn't reachable from the MEK's commands: board status always ends in a NUL, | | |
| | | and a board command's empty answer isn't sent | | |
| | | 2026-11-16 (`e880cd49`): D-073's three are stalled: the debug descriptor, `SET_IDLE` and a string | | |
| | | index the class doesn't have (`get_string_descriptor()` returns NULL), on the RT1060, the MEK and | | |
| | | the 8MP EVK. Left: (2) a short read, (4) speed | | |
| | | 2026-11-16 (`0ac39f68`): of (2), the DWC3 fails a transfer that a short packet ends before | | |
| | | `length` (`-PB_ERR_IO`), as the ChipIdea; run on the 8MP EVK. Left: the cdns3's, (4) speed | | |
| | | 2026-11-16 (`2c005bdf`): the cdns3's: its OUT TRBs had no ISP, so after a short packet the | | |
| | | controller went on to the stop gap TRB and the transfer never ended (on the MEK a 100 byte packet | | |
| | | from the host wedged command mode, endpoint 0 too, until a bus reset). Each OUT TRB has ISP now, | | |
| | | EP_STS_ISP ends a bulk OUT transfer, and a count short of `length` (the controller writes back each | | |
| | | TRB's, seen on the MEK) flushes the endpoint and is `-PB_ERR_IO`. Left: (4) speed | | |
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
| | | 2026-11-13 (`49e2ee3f`): refused before the tree is read, grown or hooked, with | | |
| | | `-PB_ERR_NOT_SUPPORTED` and a log line (since F-254 it ended in `-PB_ERR_MEM` unless the regions | | |
| | | cover the flash). A kernel alone in place still boots. No released configuration is refused: in | | |
| | | place is new in 3.0.0 (D-048). Open for the decision: refuse for good, or copy the tree to RAM, and | | |
| | | where | | |
| F-331 | `doc/dev/decisions.md`, `doc/architecture/boot.md` | F-292's refusal has no | Verified (code) | open |
| | | decision entry and no contract text. `compatibility.md` asks for an entry that names what a | | |
| | | security fix rejects and why: an image whose kernel, BL31, BL32 or generic entry part is empty | | |
| | | (or, for the kernel and BL31, has a load address and no part) was jumped into by every release, | | |
| | | and the bpak CLI builds one. `boot.md` 5.1 to 5.4 say where an entry is, not that it lies in | | |
| | | bytes the payload's hash covers. D-074 is the entry; the contract's sentence is still open | | |
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
| | | and nothing boots, by `boot bpak` neither. Its defconfig has `BOOT_POLICY_AB` and `BOOT_LINUX`. | | |
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
| | | 2026-11-14: not done, `board.md` 2.5 doesn't settle it. Proposed by the host test package: an | | |
| | | optional list of extra bootable partitions in the policy object (a `BOOT_POLICY_AB_EXTRA(&cfg, | | |
| | | parts)`), marked by the core after the policy's init, which keeps one policy per board. For Jonas | | |
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
| | | 2026-11-14: `boot.md` 3.2 says a policy "hands out the next candidate of this boot" and not that | | |
| | | each is handed out once: no bound was added. For the contract | | |
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
| F-360 | `doc/architecture/boot.md` 3.5, 3.7, `tools/pbstate` | The contract says who sets a | Verified (code) | open |
| | | rollback bit (3.5, rule 2) and that `boot activate` clears both, but not who else clears one, nor | | |
| | | that both can be set (F-340). Today: `pbstate -s A`/`-s B` (`pbstate_switch_system()`) assigns | | |
| | | `error = 0`, with and without `-c`; `-s none` and a confirmation (`-v`, | | |
| | | `pbstate_set_system_verified()`) keep the bits; libpbstate's `pbstate_clear_error(mask)` clears by | | |
| | | mask and has no command line option; pb clears them only in `boot activate`, a rollback keeps the | | |
| | | other one. Write into 3.5 (or 3.7, for the OS's side) when a bit is cleared and by whom, before the | | |
| | | shared core (F-173) fixes it in code | | |
| F-375 | `src/cm/cm_main.c` `PB_CMD_DEVICE_READ_CAPS`, `command-mode.md` 4.6 | Low. The caps' | Verified (code) | open |
| | | `part_erase_timeout_ms` stays zero: 4.6 says a device fills it, but pb sets no bound on an erase. | | |
| | | Its time is the driver's per block (`bio.md` 3, from the driver's header) times the blocks a host | | |
| | | names. To decide: what the field means (a whole partition, a block, the largest erase pb takes in | | |
| | | one command), then fill it. Released hosts ignore it (F-226) | | |
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
| F-406 | `src/drivers/mmc/`, `tests/`, `ab_state.c` | Low, from F-283. `mmc_init()` that fails | Reported | open |
| | | after it registered its devices leaves them registered and pointing at a slot that is free again | | |
| | | (`drivers.md` 2.1: nothing is registered when init fails). A card of another size at a second | | |
| | | `mmc_init()` keeps the first one's range. `host_disk_init()` called twice leaks a descriptor | | |
| | | (tests). `pb_boot_state_write()` with a handle that isn't one tests a flag on a negative value; the | | |
| | | result is -PB_ERR_PARAM either way | | |
| | | 2026-11-13 (`281e85a2`, `17d0f00d`, `9fe0a282`): every UUID is checked before the first device is | | |
| | | registered; after a failed init the card's devices return `-PB_ERR_STATE` until an init works; a | | |
| | | card of another size or without a partition at a second init is `-PB_ERR_IO`. Left: devices | | |
| | | registered before the bio pool filled up stay (bio has no release), `host_disk_init()` twice, | | |
| | | `pb_boot_state_write()` with a bad handle | | |
| | | 2026-11-14 (`7a5c73b9`): a second `host_disk_init()` closes the first one's file | | |
| | | 2026-11-14 (`7a898c68`): `pb_boot_state_write()` returns a bad handle's error before it looks at | |
| | | flags. Left, for Jonas: devices registered before the bio pool filled stay; `drivers.md` 2.1 says | |
| | | nothing stays registered, `bio.md` has no release for a driver (a porting API change): either 2.1 | |
| | | accepts devices that answer `-PB_ERR_STATE`, as the MMC's do, or `bio.md` gets a release | |
| F-410 | `src/drivers/crypto/caam/imx_caam.c` | From F-181's package and its review. Every buffer | Reported | open |
| | | address is cut to 32 bits (`drivers.md` 4.2: refuse). `imx_caam_init()` empties the output ring and | | |
| | | doesn't reset the job ring (`drivers.md` 2.1; a reset can't be tried from here). The key parser | | |
| | | skips the BIT STRING's two leading bytes unchecked, doesn't check the point's form or length against | | |
| | | the curve, and `caam.ecdsa_key` isn't cleared. A job of a `hash_final()` or a verification that | | |
| | | timed out can still write the caller's buffer later (F-081). The i.MX8M Mini SRM's verify PDB | | |
| | | differs from what the driver and OP-TEE use; the 6UL SRM asks for data on INIT, pb starts a short | | |
| | | hash with none | | |
| | | 2026-11-13 (`e6d029fe`, `6cdc2a6e`, `92ef51e5`): an address that isn't below 4 GiB is refused, the | | |
| | | key buffer is cleared before a key is parsed (a cut key verified after a full one in the test), the | | |
| | | key's point must be uncompressed and of its curve's length (what bpak writes for every key of a | | |
| | | keystore; the three internal keys are pinned). Left, each needing a board: no job ring reset in | | |
| | | init, the 8M Mini SRM's verify PDB, a short hash that starts with INIT and no data. And | | |
| | | `imx_caam_init()` leaves its hash ops registered when `dsa_add_ops()` fails | | |
| | | 2026-11-16 (`cb8bac6d`): a backend whose `dsa_add_ops()` fails takes its hashes back | | |
| | | (`hash_remove_ops()`, the CAAM and Mbed TLS; `test_crypto`); on the MEK and the 8MP EVK with | | |
| | | F-321's build. Left as above: the job ring reset, the 8M Mini SRM's PDB, a short hash's INIT | | |
| F-411 | `src/boot/dtb.c`, `src/lib/fdt` | A device tree part whose header lies about | Verified (run, host) | open |
| | | more than `totalsize` reads outside its part: only the magic, the version and `totalsize` are | | |
| | | checked, and `fdt_open_into()` trusts `off_dt_struct`, `off_dt_strings`, `off_mem_rsvmap` and the | | |
| | | two sizes. Of 200 000 single field lies 45 271 read outside the part (memory the payload's hash | | |
| | | doesn't cover) and one wrote 44 KiB below its destination. Signed input only, F-065's class; the | | |
| | | image handover has it in place. Refusing such a tree refuses an image a release took: a decision | | |
| | | (D-008), for Jonas. Found by the review of F-259 | | |
| F-423 | `error.h`, `bio.md` 4, `cm_main.c`, `command-mode.md` 4.5 | Low, from command mode's | Reported (read) | open |
| | | second package. `include/pb-tools/error.h` declares `pb_error_string()`, which nothing defines. | | |
| | | `bio.md` 4 doesn't say which flags a new partition of an installed table starts with (the code: | | |
| | | the device's at set-up, and none a released partition with its UUID lacked), nor what a failed | | |
| | | install leaves in the pool. Command mode's cached life cycle isn't read again after an install: | | |
| | | on the host board the fuse partition's handle can die with variant 2. `test_part.py` still says | | |
| | | the new table is used after the next boot. 4.5's table has no row for `PB_RESULT_BAD_PAYLOAD` | | |
| | | 2026-11-14 (`2530f937`): `test_part.py`'s comment. `pb_error_string()` stays declared: v2.7.1's host | | |
| | | library defined and used it, and `compatibility.md` freezes the header's values, not its | | |
| | | declarations: for Jonas | | |
| F-427 | `board/jiffy`, `plat.h`, `imx_wdog.c`, `imx_rom_log.c` | Low, from F-248's package. | Reported (read) | open |
| | | jiffy's `init()` logs its RNG's failure and doesn't call `board_report_degraded()` (`board.md` | | |
| | | 2.3). `struct plat_status.crypto_engine` can't tell "failed" from "the platform sets none up" | | |
| | | (`platform.md` 3.8 against 2.3; the hidden symbol `PLAT_CRYPTO_ENGINE` tells the core for now). | | |
| | | `imx_wdog_init()` always returns `PB_OK`, so `wdog_armed` can only be false on the 8X. | | |
| | | `imx_rom_log` has no "was a log found" call. The 8M's serial download event, 0xd0, is from | | |
| | | AN12853 Table 9 and the EVK's one known log already differs from that table in two places | | |
| | | 2026-11-14: the RNG item is stale: since `68e053fc` (F-247) the RNG is `plat_init()`'s and jiffy's | |
| | | `init()` has none. Its failure is still only logged: `plat_status` has no RNG field and | |
| | | `board_report_degraded()` is the board's (a contract question) | |
| F-429 | `src/plat/imx8x/{fusebox,rot_helpers}.c` | The 8X's fuse helpers, found by the tests | Verified (test) | open |
| | | written for them. `imx8x_is_srk_fused()` ignores a failed fuse read and tests an uninitialised | | |
| | | value; the SLC's status and a key's revocation rest on it. `imx8x_fuse_write_srk()` doesn't | | |
| | | compare what is fused: `slc configure` on a device with another or a partly written hash | | |
| | | writes all 16 words again, where the 6UL's driver refuses another hash and completes a partial | | |
| | | one (F-200); what the SCFW does with a word written twice isn't known. | | |
| | | `imx8x_read_key_status()` has no slot check: slots 4 and up read as not revoked, and 32 or more | | |
| | | is an undefined shift. `test_imx8x_helpers` pins what the code does today | | |
| | | 2026-11-14 (`877a7be8`, `583b3d7a`, `023e27ff`): a failed SRK read is an error (the life cycle is | | |
| | | unknown, the gate stays closed, a revocation commits nothing); the SRK write reads all 16 words | | |
| | | first, refuses a word with a bit the hash doesn't have, completes a partial one and never writes a | | |
| | | word that equals its target; a slot of 32 or more is `-PB_ERR_PARAM`. Left, for Jonas: slots 4 to 31 | | |
| | | still read as not revoked (in v2.7.1 a board could map a key there and boot it: an error would | | |
| | | refuse what a release accepted, D-008); what the SCFW does with a partial word written again. Not | | |
| | | run on a board | | |
| F-431 | boards, build, `imx_flexspi.c` | Low, what the wrap-up's packages left or met. The 8X boards' | Reported | open |
| | | example password is in the image and the same on every device (`board.md` 4.4), and | | |
| | | `<board/password.h>` is under `src/board/`, which `board.md` 6 and D-056 don't list among the | | |
| | | headers a board includes (both from F-421). imx8mevk's `IMX8M_IMAGE_SIGN_CERT` defaults to the | | |
| | | CSF certificate, not `IMG1_1`; both 8M boards print `pb.bin` as the final output although they | | |
| | | make images, and relink `pb` on every `make` (the blob rule writes somewhere other than its | | |
| | | target). `imx_flexspi_xfer()` looks at INTR's errors after plain commands only: one during the | | |
| | | last push or pop of a read or a write is lost | | |
| | | 2026-11-14 (`b46e2405`, `928b1dc6`, `3bd9601d`): imx8mevk signs its image data with IMG1_1 (its | | |
| | | signed image changes; both certificates are issued by SRK1), the 8M boards don't relink `pb` on | | |
| | | every `make`, and their final output is the image. Left: the 8X boards' password, | | |
| | | `board/password.h`, the FlexSPI's INTR check after a read or write | | |
| | | 2026-11-14 (`b82afb8a`): the FlexSPI's INTR check after a read or a write. Left: the 8X boards' | |
| | | password, `board/password.h` | |
| F-432 | `src/plat/imx8m/plat_imx8mq.c`, `plat_imx8mp.c` | `plat_get_info()` (`platform.md` 3.11) on the | Verified (code) | open |
| | | 8M, by the manuals only, run on no board. 8MQ: DIGPROG alone, so the B0 and B1 read the | | |
| | | same (U-Boot also reads a ROM word and an OCOTP magic); the core's rate is the ARM PLL's by the | | |
| | | RM's figure, with DIVQ = 2 (OUTPUT_DIV + 1) inferred and the fraction left out. 8MP: the core's | | |
| | | rate is the ARM PLL's, the A53 root's mux (CORE_SEL) isn't read; the RM's DIGPROG reset value | | |
| | | (0x824010) contradicts its own "0x3 = Plus". `mm.h`'s `CCM_ANALOG_DIGPROG` is the 8MQ's | | |
| | | address, wrong on the 8MP (unused). 2026-11-14 on the 8MP EVK: 1.1 (A1, DIGPROG at 0x800 | | |
| | | read), 1000 MHz, 2000 MHz; the CORE_SEL question stays. The 8MQ isn't run (imx8mevk builds) | | |
| | | 2026-11-14: Jonas can borrow an 8MQ | | |
| F-433 | `src/plat/imx6ul/plat.c`, `src/plat/imxrt/plat.c` | `plat_get_info()` on the 6UL and the | Verified (code) | open |
| | | RT. 6UL: the 6ULL's DIGPROG (0x65) is Linux's and U-Boot's, not in the 6UL RM, and the 6ULZ | | |
| | | reads the same; an MMDC on PLL4 is "none set up". RT: the RM gives DIGPROG 0x006C0000 (1.0) | | |
| | | alone, so the revision's split into major and minor is the other i.MX's. 2026-11-14 on the | | |
| | | bench: jiffy 6UL 1.2, 528 MHz, 396 MHz (the ROM's MMDC clock, RM Table 8-43); the RT1060 | | |
| | | 1.0, 600 MHz at speed grade 2. Left: a 6ULL | | |
| | | 2026-11-14: no 6ULL board for now | | |
| F-441 | `src/boot/`, `board/jiffy`, `testbench/` | The delivery through OP-TEE on ARMv7-A | Verified | open |
| | | (`boot.md` 5.7, D-085) doesn't exist: OP-TEE is booted as the kernel's part, with a fixed | | |
| | | `CFG_NS_ENTRY_ADDR`. Left, after the review: the delivery, its host test, jiffy's board, a | | |
| | | test bench add-on with OP-TEE (13.1 of 2026-11-14), and F-241's check from Linux on it | | |
| | | 2026-11-14 (`f6eb8eae`, `ffc9a813`): the delivery (`boot/optee.h`, `CONFIG_BOOT_OPTEE`, `takes()` | | |
| | | in the delivery's ops), jiffy's board data behind the option, `test_optee`; the test bench's | | |
| | | add-ons, `make BOARD=jiffy ADDON=optee`. Left: the bench (OP-TEE 4.9.0 to Linux on jiffy, xtest, | | |
| | | the plain `boot.bpak` directly), then F-241's check from Linux | | |
| | | 2026-11-14, on jiffy (pb from BOOT1): OP-TEE 4.9.0 starts from the part `tee` and enters Linux at | | |
| | | pb's entry, Linux mounts Root B and starts tee-supplicant; the plain `boot.bpak` boots Linux | | |
| | | directly. Two fixes in the add-on: `CFG_NS_ENTRY_ADDR=n` (OP-TEE's i.MX default, 0x80800000, | | |
| | | overrides lr) and OP-TEE 1 MiB lower (jiffy's ramoops is the last MiB). Left: F-444 | | |
| F-442 | `src/plat/*/plat.c`, `src/main.c`, `src/boot/boot.c` | Every platform sets up Mbed TLS | Verified | open |
| | | in its `plat_init()`, and `plat_status.crypto_engine` says whether the build's backends all | | |
| | | came up, Mbed TLS among them. D-086: the core sets the software backend up, the field is | | |
| | | the engine's alone (none, ok, failed), the boot's check the core's (`crypto.md` 2, | | |
| | | `platform.md` 2.2, 2.3, 3.8). Left, after the review: the code and its host tests | | |
| F-443 | `src/boot/optee.c`, OP-TEE | Xen through the OP-TEE delivery | Verified (source) | open |
| | | (Jonas: "op-tee+xen+linux"). pb's side has it: Xen follows the image handover | | |
| | | (`boot.md` 5.2), dom0's kernel and ramdisk are module nodes in the image's DT. OP-TEE | | |
| | | 4.9.0 enters the normal world in SVC, always | | |
| | | (`core/arch/arm/kernel/boot.c:152`: `mon_spsr` is `CPSR_MODE_SVC` and `CPSR_I`); Xen | | |
| | | on AArch32 is entered in Hyp. Needs OP-TEE to return to Non-secure Hyp (the A7 has the virtualization | | |
| | | extensions), then a Xen build for the 6UL and OP-TEE's virtualization for its guests | | |
| F-444 | OP-TEE on jiffy, `plat/imx6ul/plat.c` | An SMC from Linux, after OP-TEE's driver | Seen (board) | open |
| | | probed (`optee_bus_scan`, and the PSCI reset at a reboot), takes a prefetch abort in Monitor mode | | |
| | | at OP-TEE's own monitor vector (0xbdf00c48), handled by Linux's vector: "Bad mode in prefetch | | |
| | | abort handler"; Linux then hangs, the watchdog doesn't reset it (SRST over the ULINK does). The | | |
| | | first SMCs of the probe work. Candidates: F-241's TZASC region 1 at the DRAM's physical base, | | |
| | | where the RM has the controller's local addresses; OP-TEE's own TZC-380 and CSU set up after | | |
| | | pb's; what jiffy's own test bench sets and this add-on doesn't (`CFG_NXP_CAAM`, | | |
| | | `CFG_IMX_SNVS_NS_ACCESS`) | | |
| F-447 | `include/plat/imx8m/fusebox.h` | The i.MX8M's SLC (2026-11-14) rests on public sources | Reported | open |
| | | that don't all agree: the 8MQ's SRK_REVOKE at bank 9 word 3 is its RM's OCOTP register, while its | | |
| | | fuse map calls 0x670 reserved; the 8M Plus's SEC_CONFIG and SRK_HASH are NXP's U-Boot's, its | | |
| | | SRK_REVOKE unknown (the 8MP's keys can't be revoked). Verify on a part that can be spent: a | | |
| | | revocation on an 8MQ (the pico8ml is the only 8MQ on the bench), the 8MP's fuses in its | | |
| | | Security Reference Manual (NDA) | | |
| | | 2026-11-16: the 8MP RM (Rev. 3) doesn't settle it: Table 6-35 calls 0x4C0 to 0x610 (where NXP's | | |
| | | U-Boot has SEC_CONFIG and SRK_HASH) and 0x670 Reserved. Only its Security Reference Manual has | | |
| | | them, which isn't on the share | | |
| F-450 | `testbench/board/pico8ml/`, the pico8ml | Linux on the pico8ml went dark: silent, no reset, | Seen (board) | open |
| | | the console's CP2105 still there. The console's receive works (a login at the prompt). | | |
| | | (1) The BD71837's rails were turned off 30 s after boot as unused: always on since | | |
| | | `cc91b2cf`. (2) After that it still died between 7 s and 3 minutes, also with the NOC | | |
| | | locked at 800 MHz and no rate change; not a DRAM DVFS (the test bench's TF-A has no DRAM | | |
| | | driver, `IMX_DRAM_RETENTION=0`). With `PWRCTL.selfref_en` (`475408cd`) and on another USB | | |
| | | port: 30 minutes, 180 checks of a 200 MiB checksum in RAM. Power (Jonas) or the self | | |
| | | refresh: the run without the bit decides. Also: eth0's PHY doesn't answer at MDIO 1, the | | |
| | | USB host port's DWC3 doesn't start | | |
| F-452 | `testbench/board/pico8ml/`, the pico8ml | `reboot` in Linux hangs the board: "Restarting | Seen (board) | open |
| | | system", then nothing, and no watchdog ends it. TF-A's PSCI reset asserts WDOG_B | | |
| | | (`IMX_WDOG_B_RESET`), which doesn't reset this board. pb's `dev reset` and WDOG1's | | |
| | | software reset from Linux (`devmem 0x30280000 16 0x44`) do reset it, and it boots from | | |
| | | the eMMC again (three times on 2026-11-15). Not so in the session that began in the | | |
| | | serial downloader after the eMMC's boot partition was changed (`mmc bootpart enable`): | | |
| | | both resets ended in the downloader again, the ROM log with "Device init: fail" | | |
| | | (0x8f010b74) for the eMMC, also with BOOT1 enabled again, until a power cycle. Not | | |
| | | looked into: whether the ROM sees a changed boot partition only after a power cycle | | |
| F-455 | jiffy, the TZASC's lock (F-241) | A reset loop, recovered 2026-11-16. A test pb | Seen (board) | open |
| | | (not committed) in BOOT1 set lockdown_range (all 8 regions), lockdown_select's region_register and | | |
| | | GPR1's TZASC1_BOOT_LOCK after pb's region 1 (DDR, both worlds), then wrote region 1 again. After | | |
| | | the `dev reset` that started it, every boot: the CAAM's jobs time out (JRSTAR 0x40000716), the | | |
| | | uSDHC's data DMA fails (CMD8, -7), `cm_setup()` can't make the UUID, so no command mode, and the | | |
| | | watchdog resets it. Its log lines (LOG_INFO) never reached the host, so where DMA stops isn't known; | | |
| | | the region read back as pb wrote it before the lock. The ULINK isn't on USB (last seen 2026-11-14), | | |
| | | so no JTAG reset. To recover: serial download (switches, power cycle), `uuu` with | | |
| | | `pb_signed_uuu.imx` of `cee1b689` (`testbench/output/jiffy/images/punchboot/`), `auth token`, write | | |
| | | it to BOOT1. Then to find out: whether a power cycle alone ends it (the test pb locks again at every | | |
| | | boot, so likely not), why the lock stops DMA when region 1 is unchanged, and whether a warm reset | | |
| | | resets the TZASC | | |
| | | 2026-11-16, jiffy recovered (Jonas: serial download): `5d3dcda1`'s pb over `uuu`, BOOT1 written and | | |
| | | verified, System A to the login from command mode. Then test pbs from RAM only (switches left on serial | | |
| | | download, so every reset lands in the ROM). A warm reset (Linux's `reboot`, `dev reset`) resets the | | |
| | | TZC-380: region 1 attributes 0x1c, lockdown_range and lockdown_select 0. GPR1 is not reset: | | |
| | | TZASC1_BOOT_LOCK (bit 23) stayed set over seven warm resets, so the TZASC came out of each reset | | |
| | | with secure_boot_lock high and lockdown_select ignored its write (TRM 3.2.4). That is the loop: the | | |
| | | test pb's lock outlived its reset. Software clears the bit (GPR1 0x0fc61005 to 0x0f461005), a power | | |
| | | cycle wasn't tried. With the bit high alone, `5d3dcda1`'s pb works: `auth token` (CAAM), 4 MiB written | | |
| | | to Root B and verified (uSDHC DMA), region 1 programmed and read back. lockdown_range enabled | | |
| | | (0x80000007) with the bit high, in either order, resets the SoC within about 1 ms (an asm probe with | | |
| | | UART markers: DRAM loads, stores, cache clean and invalidate, the stack and the GPT all answer up to | | |
| | | the reset; no abort is printed). The first boot's CAAM and CMD8 errors weren't seen again. For Jonas: | | |
| | | should pb clear TZASC1_BOOT_LOCK at boot (a secure OS that sets it would then make the next pb's | | |
| | | boot reset), or refuse the boot when it is set? The probe patch isn't committed | | |
| | | 2026-11-16 (Jonas): pb clears it. `tzasc_init()` on the 6UL clears TZASC1_BOOT_LOCK before the TZASC | | |
| | | is set up, with a log line | | |
| | | 2026-11-16, the bit set while pb runs (JTAG, AHB access port, no halt) breaks the CAAM at once: | | |
| | | `auth token` times out, IRSAR 0, ORSFR 0, JRSTAR 0, JRINTR 0x19, no TZASC fault; set from the reset on, | | |
| | | the CAAM works. A warm reset after it lands in the ROM's serial downloader, whose `uuu` write times out, | | |
| | | until a power cycle (not proven: the CAAM's state outlives it, the closed ROM can't authenticate). The | | |
| | | clear isn't run on a board yet | | |
| | | 2026-11-16, pb's `dev reset` after the bit was set at runtime: the ROM goes to serial download | | |
| | | without trying the eMMC (its log: boot from fuses, closed, FUSE_SEL blown, serial download) and | | |
| | | doesn't answer `uuu`. CAAM JR0 then empty and reset (not wedged), GPR1's bit cleared over JTAG doesn't | | |
| | | help, SBMR1/2 as on a good boot, SRC_GPR10 0. What the ROM keeps across warm resets isn't known | | |
| | | 2026-11-16, the bit set while the ROM's downloader waits (button entry): `uuu` times out and doesn't | | |
| | | recover with the bit cleared. The bit rising after the CAAM is set up breaks it (ROM or pb); set before | | |
| | | the ROM starts (the lockdown probe's reset) everything works. The clear covers only that case, not run | | |
| | | 2026-11-16 (Jonas): the clear is dropped, not committed: it never ran on a board, the TRM suggests | | |
| | | lockdown_select stays read only after a reset with the lock high whatever GPR1 says later, and a lock | | |
| | | set at runtime breaks the CAAM before pb runs again. pb still never sets the bit | | |
| | | 2026-11-16, after Jonas's power cycle: one OpenOCD `init` and AHB-AP reads (no halt) on jiffy, | | |
| | | then `auth token` timed out. SNVS_HPSR 0x8000b300: the SSM in Soft Fail (it read 0x8000bb00, | | |
| | | Non-Secure, in the morning's broken state); HPSVSR 0x80000003: SV0 and SV1 latched. JTAG activity | | |
| | | on the closed part looks like the security violation that blocks the CAAM, and every earlier | | |
| | | "the bit breaks the CAAM" set the bit over JTAG; it would also be F-078's and F-459's warm resets | | |
| | | into the downloader. Not proven: whether auth worked before the reads. To check after a power | | |
| | | cycle: auth, one JTAG read, auth again | | |
| F-458 | the RT1060's boot time | Low. The warm reset to the image's entry was 40.1 ms | Seen (board) | open |
| | | on 2026-11-08 (`testbench/simple/README.md`), 42.3 ms at `b7561ea1` (2026-11-15, the same 5.2 KiB | | |
| | | application, medians of 10). Not tonight's: F-148 adds 0.1 ms, F-457 1.6 ms. Which commit of the | | |
| | | week added the 2.2 ms isn't known; `scripts/boot-time-image` splits pb's steps | | |
| | | 2026-11-16, measured on the EVK without old pbs: 32 KiB more of pb costs the ROM 0.42 ms (13 us a | | |
| | | KiB), so pb's growth since `be98eb8d` (75712 to 101760 bytes) is about 0.34 ms. `boot-time-image` | | |
| | | against the table: the signature 26.6 ms (was 25.9), the A/B state's read 0.6 ms (8 KiB of NOR), | | |
| | | pb's init to `boot_load()` 4.7 ms with F-457's 1.6. In `be98eb8d` pb's timer started after the clocks | | |
| | | and the banner, so the table's 12.7 ms had them; now its init has. The banner is 2.1 ms of console | | |
| | | init on every boot: about 22 characters at 115200 baud, written synchronously (`console.c`, a log | | |
| | | line before and after it from RAM). Left: the signature's 0.7 ms; whether the RT prints the banner | | |
| | | (`CONFIG_PRINT_BOOT_BANNER`, D-023) for Jonas | | |
| | | 2026-11-16, the signature's 0.7 ms: `be98eb8d`'s pb loaded to RAM over the J-Link has 25.89 ms (the | | |
| | | table's), `b7561ea1`'s 26.5 (`boot-time-image -s 4K`, from "Key-store:" to "Authentication | | |
| | | successful": the header's SHA-256 and the ECDSA verify). Bisected by RAM loads (nothing flashed): | | |
| | | the first slow commit is `d00140db` (F-065), which changes nothing in that window. Ruled out: the | | |
| | | Mbed TLS functions' addresses (the same in it and its parent), the read only data's offset (`.ro_data` | | |
| | | moved by 0, 8, 16, 24 bytes: 26.52 each), the D-cache write-through (0.1 ms), the stack's alignment | | |
| | | (32 since F-209, still 26.5). The commits before it vary 25.7 to 26.0. Left: what in the layout costs it | | |
| | | 2026-11-16 (Jonas): the RT1060's defconfig drops the banner (`CONFIG_PRINT_BOOT_BANNER`). From its NOR, | | |
| | | the warm reset to the entry: 41.87 ms (median of 10, was 44.0). `boot-time-image -c` needs the banner and | | |
| | | says so. Left: the signature's 0.7 ms | | |
| | | 2026-11-16 (`ecdd323b`): Mbed TLS's bignum and ECP files at -O2 (pb is -Os): the signature 22.56 | | |
| | | ms (26.58 before), the warm reset to the entry 37.98 ms (41.87), 4.4 KiB more of pb. The 2.2 ms | | |
| | | of the week are more than made up; which commit added them stays unknown | | |
| F-459 | jiffy, after Linux's `reboot` | Linux hangs in the CAAM's job ring interrupt | Seen (board) | open |
| | | 2026-11-16, `5d3dcda1`'s pb from BOOT1 (switches on eMMC): `pbstate -s none` and `reboot` in Linux. | | |
| | | pb's banner (wdog, 0x11), then nothing on the console, no USB, and no watchdog reset in over a | | |
| | | minute. The same reboot to the ROM's serial downloader worked this morning, and pb from BOOT1 after a | | |
| | | `dev reset` reached command mode twice. The ULINK isn't on USB, so no JTAG look. Not known: where pb | | |
| | | stops (before "entering command mode": the platform, the eMMC or pbstate), why the watchdog doesn't | | |
| | | reset it | | |
| | | 2026-11-16, the ULINK back: it isn't pb. Halted over JTAG, the PC is in the kernel (0xc012bbd8, | | |
| | | `__irq_exit_rcu`); of eight samples 300 ms apart, six are `caam_jr_interrupt`, `caam_jr_dequeue`, | | |
| | | `gic_handle_irq` or `__irq_exit_rcu`: an interrupt storm from the CAAM's job ring, before userspace | | |
| | | prints (the kernel is quiet). So pb booted System A: `pbstate -s none` hadn't taken. Linux's | | |
| | | watchdog daemon never starts, the hardware watchdog isn't kicked by pb's setup alone (not known why it | | |
| | | doesn't fire). Not known: whether the CAAM state comes from Linux's run before the warm reset or from | | |
| | | pb (its job ring 0, `imx_caam_stop()` only in `plat_lockdown()`) | | |
| F-460 | `src/drivers/mmc/imx_usdhc.c`, command mode | Low, from F-321. A write's data wait | Verified (code) | open |
| | | is still `USDHC_BUSY_TIMEOUT_us` (5 s) plus 1 us a byte: 9.2 s for command mode's 4 MiB buffer, | | |
| | | against a 5 s watchdog. A card that stops answering in a `part write` resets the board instead of | | |
| | | failing the command. Only command mode writes this much (the A/B state is 512 bytes). A bound needs | | |
| | | the card's write time: R2W_FACTOR times the access time (JESD84-B51 7.3), or a measurement | | |
| | | 2026-11-16: JESD84-B51 gives no maximum for a write: R2W_FACTOR is the typical block program time | | |
| | | as up to 128 times the read access time (7.3.21). So the bound is a measurement, or command mode | | |
| | | writes its buffer in pieces with a kick between (D-073's rule). Measured: jiffy 17.2 MiB/s, the | | |
| | | MEK 15 MB in 1.1 s, the 8MP EVK 15 MB in 0.7 s | | |
| F-462 | `src/drivers/uart/`, `src/plat/*/plat*.c` | Low, from F-102. Waits without a bound | Verified (read) | open |
| | | (`drivers.md` 3): the i.MX UART's TXDC and SRST and the LPUART's TC (the console: may run | | |
| | | before the timer is set up); the ARM PLL's lock on the 6UL, the 8MQ, the 8MP and the RT, and the | | |
| | | RT's CDHIPR handshakes (`platform.md`, the clock set up in `plat_init()`) | | |
| | | 2026-11-16, the RT1060 from RAM over the J-Link: measured, the ARM PLL's lock 54 us, each CDHIPR | | |
| | | handshake 4 to 5 us. A bounded wait through `pb_timeout_*()` (10 ms) came up 4 of 7 loads, the old | | |
| | | loop 9 of 9: the others a HardFault, UNDEFINSTR on a valid `pop` in `plat_get_us_tick()`, called | | |
| | | from the first wait, while the AHB root switches to PLL2. Code fetched during that switch can read | | |
| | | wrong; the tight loop stays in what was fetched. Not landed. A bound there must not call out: a | | |
| | | counted loop, or the switch's wait left as it is | | |
| | | The UARTs: pb proper's console starts after `plat_early_init()`'s timer, but the 8M SPL's before | | |
| | | it (`imx8m_spl_console_init()`, then GPT1's gate in `imx8m_spl_dram_init()`), and the RAM log is | | |
| | | written out then: a putc bounded through `pb_timeout_*()` would read a gated timer. A counted | | |
| | | bound, or a console that starts after the timer | | |
| F-463 | `src/drivers/usb/usbd.c`, `usb.md` 3.2 | Low, a contract question. The stack answers only | Verified (boards) | open |
| | | `usb.md` 3.2's requests and stalls the rest, as written. USB 2.0 9.4 has a configured device answer | | |
| | | more: GET_CONFIGURATION, GET_INTERFACE, GET_STATUS of the interface and of an endpoint, and | | |
| | | CLEAR_FEATURE / SET_FEATURE(ENDPOINT_HALT). On the RT1060, the MEK and the 8MP EVK (pyusb, | | |
| | | 2026-11-16) all of them stall, and `clear_halt()` of either bulk endpoint fails; each board goes on | | |
| | | (dev show after). A host that clears a halt after an error (WinUSB's pipe reset, libusb's | | |
| | | `clear_halt`) gets a stall. CLEAR_FEATURE(HALT) resets the data toggle, which is each controller's: | | |
| | | answering it without that would lose the next packet. The other four the stack could answer alone. | | |
| | | Also seen: a zero length and a 1024 byte packet where a command is read, each board recovers | | |
| F-464 | `src/drivers/mmc/mmc_emmc.c`, the MEK | Boot time. On the MEK pb's init to `boot_load()` is | Seen (board) | open |
| | | 187 ms (`boot-time-image -c`, medians of 5, 2026-11-16), of which 171 ms are the eMMC's CMD1 | | |
| | | loop after pb's CMD0: the card answers busy that long (pb polls every 1 ms). The 8MP EVK's eMMC | | |
| | | takes 6.7 ms for the same. The ROM had the card up already: taking it over in the state the ROM | | |
| | | left it (TRAN, the ROM's RCA and bus mode) instead of CMD0 would save the 171 ms there, at the | | |
| | | cost of an init path that depends on the ROM. For Jonas. The reset to pb's banner is 170.75 ms | | |

## Dependencies / vendored code

| ID | Finding | Status |
|---|---|---|
| F-020 | Mbed TLS pinned to 3.4.0 (EOL). The file list in `src/drivers/crypto/mbedtls/makefile.mk` is tied to that release. `mbedtls_config.h` enables MD5/BASE64/PKCS1_V21, which are unused. Corrected 2026-11-12: MD5 is used, for the device's UUID (`uuid3.c`, `crypto.md` 5); BASE64 and PKCS1_V21 unchecked. | open |
| | 2026-11-13 (`a1c5a80f`): BASE64 and PKCS1_V21 are off; neither was compiled into a build, the | |
| | RT1060's `pb.bin` is the same with and without. Left: the pin to 3.4.0 | |
| | 2026-11-14 (`7e1e5ba9`): the record is `doc/guides/vendored.md`: each component's files, upstream, |
| | version (inferred where no file says, with the evidence), licence and local changes, with the TF-A |
| | export headers, Mbed TLS (fetched by the container, outside the tree) and the single files from |
| | elsewhere. The row's "never synced" was wrong for the SCFW (`ba72c499`); the libc is TF-A v2.0 and a |
| | later `inttypes.h`. libfdt, libuuid, the xlat tables and the libc stay inferred until compared with |
| | upstream's tags. Done |
| | 2026-11-14, what the files and the history say (the coverage package; upstream's dates from memory): | |
| | Kconfiglib 14.1.0 (its `VERSION`, imported `e21a6592`, since reformatted); libfdt of dtc 1.4.1 or a | |
| | snapshot before 1.4.2 (inferred: error codes to `BADNCELLS`, no overlay code; `03a97c91`); libuuid | |
| | of util-linux 2.16 to 2.19, through bpak's copy (`60615325`); TF-A's xlat tables of about v2.3 | |
| | (`ab6b8beb`), and the folder `xlat_tables_v2` holds TF-A's v1 library; libc of TF-A v2.0 | |
| | (`d7eb9189`); NXP's SCFW API 1.31 (`ba72c499`, the NXP release unknown). None was synced since its | |
| | import. Left: the record in the tree | |

## Dead code / cleanup

| ID | Finding | Status |
|---|---|---|
| F-033 | imx8mevk unfinished (`board_init` returns error, no Kconfig). Orphan defconfigs `bebop`, `pico8ml`. imxrt untouched since 2023. | open: ask Jonas |
| | Checked 2026-11-12: imx8mevk is described by `struct board` and builds, what it lacks is F-300. | |
| | The RT1060 EVK runs on the bench. Left: `bebop_defconfig` and `pico8ml_defconfig` have no | |
| | board in `src/board/`; which boards to keep | |
| | 2026-11-15: the pico8ml is a board again (`9ea656f2`) and boots Linux through TF-A in the | |
| | test bench (`f2b29c88`). Left: `bebop_defconfig`, imx8mevk (F-300) | |

## Build / CI / docs

| ID | Finding | Status |
|---|---|---|
| F-052 | `src/drivers/virtio/*` has no user since `plat/qemu` was removed. Verified. | open: keep for a QEMU smoke tier, or remove |
