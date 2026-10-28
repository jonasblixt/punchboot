# i.MX8M LPDDR4 init

The DRAM init in `src/plat/imx8m/umctl2_lp4.c`, used by `imx8mevk` (i.MX8MQ EVK, LPDDR4,
3200 and 667 MT/s). There is no imx8mevk in the lab, so the code is checked by a golden register
trace on the host (D-013).

## Files

| File | What |
|---|---|
| `include/plat/imx8m/umctl2.h` | Synopsys uMCTL2 controller (DDRC): register map, FREQ1..3 shadows, fields |
| `include/plat/imx8m/ddrphy.h` | Synopsys DWC LPDDR4 PHY: CSR numbering, PMU mailbox/reset CSRs, message codes |
| `include/plat/imx8m/lpddr4.h` | JEDEC MR3/MR11 encodings |
| `include/plat/imx8m/dram.h` | `struct dram_timing_info` (TF-A ABI), `dram_timing`, `umctl2_init()` |
| `include/plat/imx8m/mm.h` | SRC DDRC resets, GPC power up, `DDRC_DDR_SS_GPR0` |
| `src/plat/imx8m/umctl2_lp4.c` | The init sequence |
| `src/board/imx8mevk/lpddr4_timing.c` | Board tables: controller, PHY, message blocks, PIE, trained CSR list, `fsp_msg` |
| `tests/unit/test_imx8m_ddr.c` | Device model + golden trace (`tests/unit/data/imx8m_ddr_init.trace`) |

The training firmware (`lpddr4_pmu_train_{1d,2d}_{imem,dmem}.bin`) comes from NXP under its own
EULA and is not in the repository. The firmware build needs it (F-051), any placeholder files will
do for a compile check (see "Testing").

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
AMD's material; nothing in `ddrphy.h` can. The ZynqMP DDR_PHY chapter of UG1087 describes the
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

## Init sequence

`umctl2_init()`, following NXP's reference code:

1. Hold the controller and PHY in reset (SRC DDRC_RCR), power up the DDR domain (GPC), start the
   DRAM PLL for the first training run's data rate.
2. Release the APB reset, write the board's controller table (`ddrc_cfg`), release the core and
   PHY resets. Put the SDRAM in self refresh by software (`PWRCTL.selfref_sw`) and wait for
   `STAT`. Set `DFIMISC.dfi_frequency` (quasi-dynamic, through `SWCTL`).
3. Write the board's PHY table (`ddrphy_cfg`).
4. For each entry in `fsp_msg` (3200 1D, 667 1D, 3200 2D on the EVK): change the PLL if the data
   rate changes, give the APB access (`MicroContMuxSel`), load IMEM and DMEM and read them back,
   write the message block, start the PMU (`MicroReset`), follow the mailbox until "training done"
   (0x07) or "failed" (0xff), stall the PMU.
5. Load the PIE image, wait for `CalBusy`, start the DFI init (`DFIMISC.dfi_init_start`), wait for
   `DFISTAT.dfi_init_complete`, set `dfi_init_complete_en`, leave self refresh, enable PHY master
   requests, wait for normal operating mode, enable auto refresh and AXI port 0.
6. Read back the trained PHY CSRs and copy all tables to `.umctl2` (0x40000000) for TF-A.

PHY CSR numbers are `[23:20]` P-state, `[19:12]` block, `[11:0]` register (`ddrphy.h`), so the
trained CSR list in the board file reads as "MASTER0/DBYTEn register r, for P-states 0..2".

## Testing

`test_imx8m_ddr` builds `umctl2_lp4.c` and the EVK timing for the host, with
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

Compile check of the firmware with placeholder blobs (inside `scripts/pbdev shell`):

```sh
mkdir -p build-imx8mevk/fakeblobs
for f in 1d_imem 1d_dmem 2d_imem 2d_dmem; do
    head -c 1024 /dev/zero > build-imx8mevk/fakeblobs/lpddr4_pmu_train_$f.bin
done
FB=build-imx8mevk/fakeblobs
make BOARD=src/board/imx8mevk KCONFIG_CONFIG=configs/imx8mevk_defconfig \
     CROSS_COMPILE=aarch64-linux-gnu- \
     IMX8M_UMCTL_TRAIN_1D_I=$FB/lpddr4_pmu_train_1d_imem.bin \
     IMX8M_UMCTL_TRAIN_1D_D=$FB/lpddr4_pmu_train_1d_dmem.bin \
     IMX8M_UMCTL_TRAIN_2D_I=$FB/lpddr4_pmu_train_2d_imem.bin \
     IMX8M_UMCTL_TRAIN_2D_D=$FB/lpddr4_pmu_train_2d_dmem.bin \
     build-imx8mevk/pb.bin
```

## Not verified

- Fields marked `[i.MX8M]` in `umctl2.h` (STAT, DFIMISC dfi_frequency/dfi_init_start/
  ctl_idle_en, DFISTAT, DFIPHYMSTR, PWRCTL.dis_cam_drain_selfref, PCCFG.go2critical_en) and the
  SRC/GPC bit names come from NXP's reference code and the i.MX8M RM, not from the ZynqMP FSBL.
- The STAT value waited for in step 2 (selfref_state 2, selfref_type 2, operating_mode 3) is NXP's;
  the meaning of each value is not documented here.
- The PHY block and CSR names follow NXP's reference code and DDR tool output. The message block
  (DMEM 0x54000..) fields are not named (see suggestions).

## Suggestions

Done on `imx8m-dram-cleanup` (2026-10-28): headers split by IP, named registers and fields, FREQ
shadows derived instead of listed, training driven by the board's `fsp_msg` table, one firmware
loader, PLL settings as a table, board timing as a `.c` file with static tables, errors propagated
(F-098), golden trace test.

Next, roughly in order of value:

1. **Fix the board controller table (F-101).** FREQ1 derate/refresh values are overwritten and the
   FREQ0 ones are never set. Regenerate the table with NXP's DDR tool, or check against NXP's
   imx8mq-evk timing, then update the golden trace.
2. **Timeouts on every poll (F-102).** STAT, the mailbox, CalBusy, DFISTAT, SWSTAT and the PLL lock
   spin forever. A hang here is a dead board with no log. Use `plat_get_us_tick()`; the training
   runs take a while each, so the timeouts need a measurement on hardware.
3. **TF-A handover (F-100).** `fsp_msg`/`fsp_msg_num` in the copy are never written (`.umctl2` is
   NOLOAD, so they are whatever DRAM holds), and the copy's size is not checked against the
   128 KiB region. Pin the struct layout with `_Static_assert`s on the aarch64 offsets.
4. **Name the message block.** The fsp tables are raw DMEM offsets (`0x54003` is the data rate,
   `0x54005` PHY RON/RTT, ...). Named offsets or a struct per firmware version would make the
   board file reviewable. Needs the layout from NXP's DDR tool.
5. **Compile imx8mevk in CI with placeholder blobs (F-103).** Nothing builds it today; this branch
   showed it compiles.
6. **Make the PHY code SoC independent** when another i.MX8M variant (8MM/8MN/8MP, same controller
   and PHY) is added: move the uMCTL2/PHY parts to `src/drivers/ddr/`, keep SoC resets/PLL in the
   platform, as U-Boot does in `drivers/ddr/imx/imx8m`.
7. **Small cleanups once there is hardware:** the duplicate `DFIPHYMSTR` write; the normal-mode wait
   that masks STAT bits [1:0] while LPDDR4's operating_mode is [2:0]; `PCTRL_0`, `SWCTL` and
   `DBG1` in the board table that the code writes again; streaming messages logged with their
   string index, so they can be decoded with NXP's string file.
