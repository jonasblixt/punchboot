# i.MX8M DRAM init

The DRAM init of the i.MX8M platforms: `imx8m_ddr_init()` in `src/plat/imx8m/ddr.c`, on top of
the uMCTL2 and DWC PHY drivers in `src/drivers/ddr/` (D-014). Used by `imx8mevk` (i.MX8MQ EVK,
LPDDR4, 3200, 400 and 100 MT/s, the low two in PHY bypass mode) and `imx8mpevk` (i.MX8M Plus EVK,
6 GiB LPDDR4 MT53E1536M32D4DT, 4000, 400 and 100 MT/s). There is no imx8mevk in the lab, so the
8MQ code is checked by a golden register trace on the host (D-013). The 8MP EVK is on the bench:
its DRAM trains and passes a write/read test (2026-10-28, see "i.MX8M Plus EVK").

## Files

| File | What |
|---|---|
| `include/drivers/ddr/umctl2.h`, `src/drivers/ddr/umctl2.c` | Synopsys uMCTL2 controller: offsets from the base, FREQ1..3 shadows, fields, controller-only init steps (`DRIVERS_DDR_UMCTL2`) |
| `include/drivers/ddr/dwc_ddrphy.h`, `src/drivers/ddr/dwc_ddrphy.c` | Synopsys DWC DDR4/LPDDR4 PHY: CSR numbering, PMU mailbox/reset CSRs, message codes; firmware load, training run, calibration, CSR readback (`DRIVERS_DDR_DWC_PHY`) |
| `include/drivers/ddr/dram_cfg.h` | Table types (`dram_cfg_param`, `dram_fsp_msg`) |
| `include/plat/imx8m/ddrc.h` | Absolute `DDRC_*(n)` addresses on the i.MX8M, for the board tables |
| `include/plat/imx8m/dram.h` | PHY base/stride, `struct dram_timing_info` (TF-A ABI), `dram_timing`, `imx8m_ddr_init()` |
| `include/plat/imx8m/mm.h` | SRC DDRC resets, GPC power up, `DDRC_DDR_SS_GPR0` |
| `src/plat/imx8m/ddr.c` | The init sequence, common to the i.MX8M SoCs, and the TF-A copy |
| `src/plat/imx8m/ddr_soc.h`, `ddr_imx8mq.c`, `ddr_imx8mp.c` | Per-SoC hooks: reset hold, DRAM clock (8MQ: SSCG PLL at 3200 MT/s; 8MP: PLL14xx at 4000 MT/s and the DRAM1 clock gate; both: PHY bypass from DRAM_ALT_CLK_ROOT at 400 and 100 MT/s) |
| `include/plat/imx8m/clock_imx8mp.h` | 8MP PLL14xx registers (the 8MQ's FRAC/SSCG PLLs are in `clock.h`) |
| `src/board/imx8mevk/lpddr4_timing.c`, `src/board/imx8mpevk/lpddr4_timing.c` | Board tables from NXP's DDR tool: controller, PHY, message blocks, PIE, trained CSR list, `fsp_msg` |
| `tests/unit/test_imx8m_ddr.c`, `test_imx8mp_ddr.c` | Device model + golden trace per SoC (`tests/unit/data/imx8m{,p}_ddr_init.trace`) |

The training firmware (`lpddr4_pmu_train_{1d,2d}_{imem,dmem}.bin`) comes from NXP under its own
EULA and is not in the repository. The firmware build needs it (F-051); placeholder files do for a compile check (see "Testing").

## The IP, and how it compares to AMD/Xilinx

The i.MX8M DDR subsystem is two Synopsys DesignWare IPs, and AMD/Xilinx licensed the same
controller for the Zynq UltraScale+ MPSoC/RFSoC PS, but not the same PHY.

| | i.MX8M (8MQ, also 8MM/8MN/8MP) | Zynq UltraScale+ MPSoC/RFSoC (PS DDR) |
|---|---|---|
| Controller | DWC uMCTL2 at 0x3d400000 | DWC uMCTL2 "DDRC" at 0xFD070000 (`xlnx,zynqmp-ddrc-2.40a`) |
| Controller register map | MSTR 0x000 ... SWCTL 0x320, PCCFG 0x400, PCTRL_n 0x490 + 0xb0·n, FREQ1..3 shadows at +0x1000·(n+1) | Same offsets (PCTRL_1 at 0x540), DFITMG0_SHADOW at 0x2190 |
| PHY | DWC DDR4/LPDDR4 multiPHY with firmware training: PMU microcontroller, IMEM/DMEM, message block, mailbox, PIE | DWC DDR multiPHY with a PUB (PHY Utility Block) at 0xFD080000: hardware training started through PIR, PGCR/PGSR/DXnGCR/ZQnPR registers |
| Training | NXP firmware blobs, 1D and 2D, per frequency set point | PUB state machine, no firmware |
| Versal | n/a | Versal uses AMD's own DDRMC (NoC) and XPHY, neither is Synopsys |

So: **same controller, different PHY generation.** Everything in `umctl2.h` can be checked against
AMD's material; nothing in `dwc_ddrphy.h` can. The ZynqMP DDR_PHY chapter of UG1087 describes the
PUB PHY and does not apply here.

How this was checked (2026-10-28):

- AMD's ZynqMP FSBL, `lib/sw_apps/zynqmp_fsbl/src/xfsbl_ddr_init.c` in Xilinx/embeddedsw
  (`SPDX-License-Identifier: MIT`), programs the DDRC at every offset in `umctl2.h` that it uses,
  with the same field positions. The fields in `umctl2.h` that it programs were compared one by
  one; the others are marked `[i.MX8M]`.
- The same file trains the PHY through `PIR` (`DDR_PHY_PIR_OFFSET`) and `PGCR1.PUBMODE`, the PUB
  interface. The i.MX8M PHY has no PUB; it is driven by the PMU mailbox at CSR 0xd0004..0xd0034.
- Linux `drivers/edac/synopsys_edac.c` serves `xlnx,zynqmp-ddrc-2.40a` and `snps,ddrc-3.80a`
  (the generic uMCTL2) with the same ECC register offsets (ECCCFG0 0x70, ECCSTAT 0x78).
- UG1087 (Zynq UltraScale+ Devices Register Reference) lists DDR_PHY registers such as PGSR1
  (0xFD080034), PTR2 and DXnBDLR, all PUB registers.

## Other SoCs (checked 2026-10-28, against U-Boot master)

| SoC | Controller | PHY | Shares code with this driver? |
|---|---|---|---|
| i.MX8M Plus (also 8MM/8MN) | uMCTL2 at 0x3d400000, same map (NXP's 8MP EVK table writes MSTR 0xa3080020 at 0x3d400000) | Same firmware-trained DWC PHY at 0x3c000000, same mailbox; firmware `*_202006`, 4000/400/100 MT/s | Yes. Differences: SRC reset sequence, DRAM PLL (PLL14xx instead of SSCG), PHY bypass mode for data rates <= 625 MT/s (DRAM clock from the CCM instead of the PLL), inline ECC, DDR4 option |
| Allwinner H5 (also H3/A64/R40/V3s) | One register window with Synopsys-named controller *and* PHY registers at Allwinner offsets (PIR 0x00, PWRCTL 0x04, DRAMTMG 0x58, SCHED 0x1c0, DXn at 0x300); U-Boot `dram_sunxi_dw.c` | PUB-style, hardware training through PIR/PGSR, like the ZynqMP's; DDR3/LPDDR3/DDR2 | No. Different register map, different PHY generation, no firmware |
| Allwinner H6/H616 | uMCTL2 at the standard offsets (MSTR 0x000, DRAMTMG 0x100, SWCTL 0x320) | ZynqMP-like PUB PHY, different bit fields (U-Boot `dram_sun50i_h6.h`) | Controller only |

U-Boot's DDR code (`drivers/ddr/imx`, `arch/arm/mach-sunxi/dram_*`) and NXP's timing files in it
are GPL-2.0+: reference only, no copying into punchboot (BSD-3-Clause). See "Licences and
provenance".

## What can be taken from AMD's documentation

- **Register offsets and field layouts** of the controller are facts, and the ZynqMP FSBL that
  states them is MIT licensed. They were used to check `umctl2.h`; no code was copied.
- **UG1085/UG1087 text** is AMD copyrighted documentation without a licence to reproduce it. Use it
  as a reference (UG1085's DDR controller chapter explains ports, QoS, ECC, low power and
  frequency change well), but don't copy descriptions into the source.
- **The FSBL's PHY code** (PUB training, `XFsbl_DdrcPhyTraining`) does not apply to this PHY.
- What would help with the PHY is Synopsys' PHY training firmware application note (message
  block layout per DRAM type). NXP ships its content in the DDR tool, not under a licence that
  allows copying it here.

## Licences and provenance

Punchboot is BSD-3-Clause. U-Boot, where NXP's i.MX8M DDR init and board timing files are
published, is GPL-2.0+, so nothing may be copied from it. The DDR code was first added in 2019
(`c84cb90e`) following NXP's U-Boot code; its comments, a few typos and most of the EVK timing
values matched NXP's GPL-2.0+ files, under a BSD-3-Clause header without NXP's notice. Where each
part stands now (2026-10-28):

| Part | Basis | Status |
|---|---|---|
| Reset release, power up, `DDR_SS_GPR0`, order of the controller and DFI init steps (`ddr.c`, `umctl2.c`, `ddr_imx8mq.c`) | NXP's BSD-3-Clause TF-A DRAM code (`plat/imx/imx8m/ddr/dram_retention.c`, `dram.c`), which re-runs these steps after retention. NXP's copyright is in those files | OK. Some values differ from TF-A's (DFIMISC, PWRCTL, RFSHCTL3, the 8MQ `SRC_DDRC2_RCR` hold value), noted in the files |
| Register map and fields (`umctl2.h`, `ddrc.h`) | Facts, checked against AMD's MIT licensed ZynqMP FSBL; no code copied | OK |
| PHY CSRs, mailbox protocol, message codes (`dwc_ddrphy.h`) | Facts about the PHY and its firmware, written up in punchboot's words | OK |
| Firmware load and training code (`dwc_ddrphy.c`) | Clean-room implementation (2026-10-28) from `doc/dev/spec-dwc-ddrphy.md`, by an implementer that read only the spec and the public headers; record in the commit message (`ee093be7`) | OK. The implementer was a language model: the process was clean room, but its training data may include U-Boot |
| DRAM clock settings (`ddr_imx8mq.c`) | NXP's DDR tool v4.2.0 output for the EVK (register settings: PLL, DRAM_APB/DRAM_ALT clock roots, DRAM_SEL bypass) | OK |
| `struct dram_timing_info` layout (`dram.h`) | Interface to TF-A (BSD-3-Clause); comments in punchboot's words | OK |
| 8MP clocks and resets (`ddr_imx8mp.c`, `clock_imx8mp.h`) | NXP's DDR tool v4.2.0 stress test script for the 8MP EVK (reset values, DRAM_APB clock, PLL setting 0xfa031), the 8MP RM (PLL14xx fields, clock root sources, CCGR5 DRAM1) | OK |
| 8MP EVK timing (`src/board/imx8mpevk/lpddr4_timing.c`) and its golden trace | NXP's DDR Tool v4.2.0 under BSD-3-Clause, Copyright 2026 NXP, FW2020.06; NXP's notice kept, all 1758 register/value pairs checked identical to the tool's output by script | OK |
| EVK timing (`src/board/imx8mevk/lpddr4_timing.c`) and its golden trace | Generated by NXP's DDR Tool v4.2.0 under BSD-3-Clause, Copyright 2026 NXP (`c129bbf9`); NXP's notice kept, data as generated. Replaced the GPL-derived table (F-108) | OK |
| Training firmware | NXP's binaries, not in the repository | Distributed images that contain it follow NXP's firmware licence |

**Updating a board timing.** Only NXP's DDR tool (the i.MX8M DDR tool with its register
programming aid) can produce the tables for a board: the controller values follow from the DRAM part
and the board, but the PHY configuration, message blocks and the PIE image come from Synopsys' PHY
init code inside the tool. The tool's C output ("Init Code") is BSD-3-Clause, Copyright NXP (v4.2.0;
check the header of each new output). To use it:

1. Copy the generated C code as the board's `lpddr4_timing.c`, keeping NXP's header (add a
   punchboot line and our copyright for the changes). Replace its U-Boot includes with
   `<pb/pb.h>` and `<plat/imx8m/dram.h>` and make the tables `static`; leave the data as generated.
   It uses the same struct and table layout.
2. The tool's stress test script (the part before "Init Code") shows the clock setup per frequency
   set point: PLL or PHY bypass, and the clock roots. `imx8m_ddr_soc_set_clock()` must support each
   data rate in `fsp_msg`.
3. Regenerate the golden trace (`PB_UPDATE_GOLDEN=1`), check its diff, and say in the commit that
   the timing changed.

## Init sequence

`imx8m_ddr_init()`, following NXP's reference code (SoC hooks in parentheses):

1. Hold the controller and PHY in reset (`imx8m_ddr_soc_reset()`), power up the DDR domain (GPC),
   set the DRAM clock for the first training run's data rate (`imx8m_ddr_soc_set_clock()`).
2. Release the APB reset, write the board's controller table (`ddrc_cfg`), release the core and
   PHY resets. Put the SDRAM in self refresh by software (`PWRCTL.selfref_sw`) and wait for
   `STAT`. Set `DDR_SS_GPR0` to LPDDR4 mode if `MSTR.lpddr4`. Set `DFIMISC.dfi_frequency`
   (quasi-dynamic, through `SWCTL`).
3. Write the board's PHY table (`ddrphy_cfg`).
4. For each entry in `fsp_msg` (3200 1D, 667 1D, 3200 2D on the EVK): change the clock if the
   data rate changes, give the APB access (`MicroContMuxSel`), load IMEM and DMEM and read them back,
   write the message block, start the PMU (`MicroReset`), follow the mailbox until "training done"
   (0x07) or "failed" (0xff), stall the PMU.
5. Load the PIE image, wait for `CalBusy`, start the DFI init (`DFIMISC.dfi_init_start`), wait for
   `DFISTAT.dfi_init_complete`, set `dfi_init_complete_en`, leave self refresh, enable PHY master
   requests, wait for normal operating mode, enable auto refresh and AXI port 0.
6. Read back the trained PHY CSRs and copy all tables to `.umctl2` (0x40000000) for TF-A.

PHY CSR numbers are `[23:20]` P-state, `[19:12]` block, `[11:0]` register, so the
trained CSR list in the board file reads as "MASTER0/DBYTEn register r, for P-states 0..2" (`dwc_ddrphy.h`).

## Testing

`test_imx8m_ddr` builds `ddr.c`, `ddr_imx8mq.c`, both drivers and the EVK timing for the host, with
`tests/unit/fakes/mmio/pb/mmio.h` in front of the real one. Every register access goes to a model
of the controller (STAT, SWSTAT, DFISTAT), the PHY (CalBusy, mailbox with a scripted training run)
and the PLL (lock). Every write, and the timing copy for TF-A, is compared to
`tests/unit/data/imx8m_ddr_init.trace`. Failure tests cover a failed training in each run, a bad
firmware readback and an unsupported data rate.

- A cosmetic change must keep the trace identical.
- A change that is meant to change what reaches the hardware regenerates the trace in the same
  commit (`PB_UPDATE_GOLDEN=1 build-host/tests/unit/test_imx8m_ddr`) and says why.
- The model only answers polls the way the hardware should; it can't say whether the sequence is
  right. That needs an imx8mevk.

Compile check of the firmware: `scripts/pbdev firmware imx8mevk` (also in CI) builds it with zero
filled placeholder training firmware. With `IMX8M_FW_DIR` set to a directory with NXP's four
`lpddr4_pmu_train_*.bin` files it uses those instead.

## Comparing with U-Boot

NXP's current i.MX8M DDR init is in U-Boot (`drivers/ddr/imx/imx8m/ddr_init.c`,
`drivers/ddr/imx/phy/*.c`, GPL-2.0+). Running it on the host against the same device model and
board timing as `imx8m_ddr_init()`, and diffing the two register traces, shows exactly where
punchboot departs from it. Because of the licence the harness is never committed or distributed: it
lives in `build-uboot-diff/`, which `/build-*` in `.gitignore` covers (the dev container only
mounts the repository). Never `git add -f` it, and don't ship archives of the working tree. It is
quick to recreate:

- Copy the four U-Boot files and `arch/arm/include/asm/arch-imx8m/ddr.h`, plus a host version of
  `dram_pll_init()`/`dram_disable_bypass()` from `clock_imx8mq.c` (or `clock_imx8mm.c` for 8MM/8MP).
- A force-included compat header maps `readl`/`writel`/`setbits_le32`... to `fake_mmio_read/write`,
  stubs `binman_sym*`, `debug`, `is_imx8m*()`, logs `clock_set_target_val()`, and points `_end`
  (the firmware location) at a buffer. Empty stub headers for the rest of U-Boot's includes.
- `struct dram_timing_info` has the same layout in both, so `src/board/imx8mevk/lpddr4_timing.c`
  (compiled with punchboot's headers) feeds U-Boot's `ddr_init(&dram_timing)` directly.
- Both runners link the same model as `test_imx8m_ddr` and the same firmware contents (32 KiB IMEM,
  16 KiB DMEM per image, U-Boot's fixed lengths); runs of 16 bit writes (firmware loads) are
  collapsed to one line with a checksum before diffing.

Result for the imx8mevk, with the old (pre tool) timing (2026-10-28): reset sequence, controller/PHY tables, message blocks, PIE,
`DDR_SS_GPR0` and the TF-A copy are identical. Differences: F-104 (rank-to-rank gaps), F-105 (CCM
DRAM clock roots), F-106 (PWRCTL/RFSHCTL3 values, `selfref_en`, duplicate `DFIPHYMSTR`), F-107 (when
the trained CSRs are saved). Harmless: U-Boot reprograms the PLL before every run (punchboot only
when the rate changes), skips reloading an identical firmware image, waits 100 us instead of 1 ms
in the PLL sequence, and doesn't wait for STAT after entering self refresh. The harness also found
the overrun of the TF-A copy (F-100). For the 8MP, the same harness with NXP's 8MP EVK timing gives
a reference trace before `ddr_imx8mp.c` exists.

On the i.MX8M the PHY's APB address is `4 * csr` (U-Boot `ddrphy_addr_remap()`), which is what
`csr_stride` models. The i.MX9 version of that function is a real remap; a PHY on an i.MX9 would
need a remap hook in `struct dwc_ddrphy` instead of the stride.

## Not verified

- Fields marked `[i.MX8M]` in `umctl2.h` (STAT, DFIMISC dfi_frequency/dfi_init_start/
  ctl_idle_en, DFISTAT, DFIPHYMSTR, PWRCTL.dis_cam_drain_selfref, PCCFG.go2critical_en) and the
  SRC/GPC bit names come from NXP's reference code and the i.MX8M RM, not from the ZynqMP FSBL.
- The STAT value waited for in step 2 (selfref_state 2, selfref_type 2, operating_mode 3) is NXP's;
  the meaning of each value is not documented here.
- The PHY block and CSR names follow NXP's reference code and DDR tool output. The message block
  (DMEM 0x54000..) fields are not named (see suggestions).

## Adding a SoC

**Another i.MX8M (8MM, 8MN).** Same controller and PHY (see the table above). The i.MX8M Plus went
this way (2026-10-28); what it needed beyond the list: the DRAM1 clock gate (CCGR5) is off after
reset and the ROM leaves it off, so `imx8m_ddr_soc_reset()` opens it (without it the first DDRC
write faults and the SoC resets); and the GPC writes in `ddr.c` are the 8MQ/8MM layout (F-110).

1. `src/plat/Kconfig`: `config PLAT_IMX8MP` (in `choice PLAT`) with `select PLAT_IMX8M_COMMON`.
   That builds `src/plat/imx8m` and selects both DDR drivers.
2. `src/plat/imx8m/ddr_imx8mp.c`, built for `CONFIG_PLAT_IMX8MP`, implements `ddr_soc.h`:
   - `imx8m_ddr_soc_reset()`: the 8MM/8MN/8MP sequence (`DDRC_RCR` 0x8F00001F, then 0x8F00000F).
   - `imx8m_ddr_soc_set_clock()`: the PLL14xx DRAM PLL above 625 MT/s, PHY bypass (DRAM clock from
     the CCM) at or below it. Bypass also needs the PHY told about it in the board's PHY/message
     block tables, as NXP's tool generates them.
3. The firmware blobs: `ddr.c` uses the `lpddr4_pmu_train_*` symbol names, which come from the
   file names. NXP's 8MP files are `*_202006.bin`; either copy them to the fixed names in the
   board makefile, or move the `struct dwc_ddrphy_fw` definitions to the board.
4. Check where the TF-A on the 8MP expects the saved timing (punchboot: 0x40000000; U-Boot's
   default on the 8MM: OCRAM_S at 0x180000).
5. A board timing file from NXP's DDR tool, run for the board (see "Licences and provenance"),
   and a golden trace of its own (copy `test_imx8m_ddr`, with `ddr_imx8mp.c`). **Not** U-Boot's
   `board/nxp/imx8mp_evk/lpddr4_timing.c`: it is GPL-2.0+. Using it in the local U-Boot harness as a
   reference is fine; committing it, or a trace generated from it, is not.
6. Inline ECC and DDR4 are new code paths in `ddr.c`/the drivers.

**A SoC with only the controller (ZynqMP, Allwinner H6/H616).** Select only `DRIVERS_DDR_UMCTL2`
and write the PHY part in the platform. The `umctl2_*` steps assume a DFI PHY that does its own
init after `dfi_init_start`; check them against the SoC's reference sequence.

**Allwinner H5.** Shares nothing; its DRAM code belongs in its platform.

## i.MX8M Plus EVK

Bring-up from RAM, nothing flashed: boot switches on serial download, the EVK's USB-C download
port on a USB-A port of the host (it enumerates as `1fc9:0146`, `uuu`: `MX865 SDPS`), console on
UART2, the FT4232H's third port (`/dev/serial/by-id/usb-FTDI_Quad_RS232-HS-if02-port0`), 115200.

```sh
mkdir -p build-imx8mpevk/fw-202006    # inside the repo, the container only mounts that
cp <firmware-imx>/firmware/ddr/synopsys/lpddr4_pmu_train_*_202006.bin build-imx8mpevk/fw-202006/
IMX8M_FW_DIR=build-imx8mpevk/fw-202006 scripts/pbdev firmware imx8mpevk imx8mp_image
uuu SDPS: boot -f build-imx8mpevk/pb.imx
```

`imx8mp_image` wraps `pb.bin` with `mkimage -T imx8mimage` (ROM_VERSION v2, loader at 0x920000,
the start of the OCRAM free area after the ROM's 0x900000-0x917fff, RM Figure 6-2). pb runs from
OCRAM (384 KiB up to 0x97ffff); `.no_init` and the MMU tables are in DRAM from 0x40060000.

First boot (2026-10-28): all four training runs pass (4000 1D, 400 and 100 1D in bypass, 4000 2D
after the PLL is reprogrammed), 16 MiB write/read at 0x41000000, 0x80000000 and 0xbf000000 without
errors. The ROM leaves SYSTEM_PLL1/2 running and the DRAM PLL at its reset value.

eMMC (2026-10-28): uSDHC3 on the NAND pads (ALT2; the uSDHC3 input daisy chains default to the
ENET pads and are set to the NAND ones), 200 MHz root, HS200 8-bit (DDR52 fails, F-113). 32 GB,
4 MiB boot partitions, no GPT on the EVK's eMMC. `BOOT_BUS_CONDITIONS` was set to 0x12.

USB command mode (2026-10-28): USB1 (DWC3 at 0x38100000, glue at 0x381f0000, the 8MQ PHY code
fits: same PHY_CTRL0..2 bits, ATERESET is reserved), CCGR77/79/92, HSIO BLK_CTRL USB clock enable.
The DWC3 driver was reworked from the RM (F-112). Works: enumeration at high speed, `dev show`,
`part list`, 4 MiB writes to BOOT1 at about 12 MB/s with device-side verify (CAAM, era 9), `dev
reset`. The boot partitions are not readable by design, so a large read needs a GPT partition.

Not done yet: HSIOMIX/USB PHY power-up for an eMMC boot (F-115), the ARM PLL, TF-A handover on
the 8MP (step 4 of "Adding a SoC").

## Suggestions

Done on `imx8m-dram-cleanup` (2026-10-28): headers split by IP, named registers and fields, FREQ
shadows derived instead of listed, training driven by the board's `fsp_msg` table, one firmware
loader, PLL settings as a table, board timing as a `.c` file with static tables, errors propagated
(F-098), golden trace test, uMCTL2 and DWC PHY drivers in `src/drivers/ddr` with per-SoC hooks
(D-014), clean-room PHY driver, EVK timing from NXP's DDR tool with PHY bypass clocking (F-101,
F-105, F-108).

Next, roughly in order of value:

1. **Align with NXP's current sequence (F-104, F-106, F-107)**, one finding per commit with an updated
   golden trace, once an imx8mevk (or an 8MP board, where the same code runs) can confirm it.

2. **Timeouts on every poll (F-102).** STAT, the mailbox, CalBusy, DFISTAT, SWSTAT and the PLL lock
   spin forever. A hang here is a dead board with no log. Use `plat_get_us_tick()`; the training
   runs take a while each, so the timeouts need a measurement on hardware.
3. **TF-A handover (F-100).** `fsp_msg`/`fsp_msg_num` in the copy are never written (`.umctl2` is
   NOLOAD, so they are whatever DRAM holds). Pin the struct layout with `_Static_assert`s on the
   aarch64 offsets. (The size is checked now.)
4. **Name the message block.** The fsp tables are raw DMEM offsets (`0x54003` is the data rate,
   `0x54005` PHY RON/RTT, ...). Named offsets or a struct per firmware version would make the
   board file reviewable. Needs the layout from NXP's DDR tool.
5. **Rank-to-rank gap from training (F-104).** U-Boot reads the critical delay differences from
   each 1D run's message block and widens `DRAMTMG2` rd2wr/wr2rd and `RANKCTL` gaps for every
   P-state before the DFI init ("rank-to-rank workaround"), on all i.MX8M. Punchboot skips it; the
   EVK has two ranks.
6. **Small cleanups once there is hardware:** the duplicate `DFIPHYMSTR` write; the normal-mode wait
   that masks STAT bits [1:0] while LPDDR4's operating_mode is [2:0]; `PCTRL_0`, `SWCTL` and
   `DBG1` in the board table that the code writes again; streaming messages logged with their
   string index, so they can be decoded with NXP's string file.
