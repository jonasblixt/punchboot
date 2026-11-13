# The i.MX ports

How the NXP i.MX ports meet the platform contract (`platform.md`, D-030): the SoC facts, where
they come from, what TF-A, OP-TEE and U-Boot do on the same SoCs, and where each port stands.
Sections follow `platform.md`'s.

Sources are cited by document and section. The SRMs are under NDA: they are named, never quoted.
U-Boot (GPL-2.0+) and OP-TEE (BSD-2-Clause) are cited for what they do, not copied. TF-A is the
tree pb builds against (`build-tfa/src`, v2.12).

## 1. Ports and variants

| Port   | SoCs (Kconfig)                                                             | Arch    | Boards              |
| ------ | -------------------------------------------------------------------------- | ------- | ------------------- |
| imx6ul | i.MX6UL (`PLAT_IMX6UL`)                                                    | ARMv7-A | jiffy               |
| imx8m  | i.MX8MQ, i.MX8M Plus (`PLAT_IMX8MQ`, `PLAT_IMX8MP`, both select            | ARMv8-A | imx8mevk, imx8mpevk |
|        | `PLAT_IMX8M_COMMON`; per-variant code in `plat_imx8mq.c`, `plat_imx8mp.c`) |         |                     |
| imx8x  | i.MX8QXP, i.MX8DXL (`PLAT_IMX8X`, `PLAT_IMX8XL`, both select               | ARMv8-A | imx8qxmek,          |
|        | `PLAT_IMX8X_COMMON`)                                                       |         | imx8dxlevk          |
| imxrt  | i.MX RT10xx (`PLAT_IMXRT`)                                                 | ARMv7-M | imxrt1060evk        |

All under `SOC_FAMILY_IMX`. Shared IP drivers with the SoC's facts as data: `imx_hab_fuses`
(HABv4 fuses), `imx_rom_log` (the ROM's event log, NXP AN12853), `imx_hab_log` (the ROM's HAB API),
`imx_ocotp`, `imx_wdog`, `imx_gpt`, `imx_iomuxc` (pins), `imx_caam`, `tzc380`.

## 2. Lifecycle

|                  | imx6ul                        | imx8m                   | imx8x                   | imxrt      |
| ---------------- | ----------------------------- | ----------------------- | ----------------------- | ---------- |
| Who sets up DRAM | The ROM, from the board's DCD | `plat_init()` trains it | The SCFW (container)    | None used  |
| pb runs from     | DRAM (jiffy: 0x80001000)      | OCRAM                   | DRAM, a reset stub at   | OCRAM      |
|                  |                               |                         | 0x80000000              |            |
| Watchdog         | WDOG1 (`imx_wdog`)            | WDOG1                   | The SCFW (`sc_timer_*`) | WDOG1      |
| Reset reason     | SRC_SRSR                      | SRC_SRSR                | `sc_pm_reset_reason()`  | WDOG1 WRSR |
| ROM state saved  | ROM log, the serial           | ROM log                 | -                       | -          |
|                  | downloader's USB address      |                         |                         |            |
| HAB log          | yes                           | yes                     | - (AHAB)                | yes        |

- **Watchdog power-down counters.** Each WDOG's power-down counter is enabled out of every reset
  and drives WDOG_B low after 16 s unless WMCR.PDE is cleared (6UL RM 57.5.3); whether that resets
  the board depends on WDOG_B reaching a pad. U-Boot clears WDOG1-3 on the 6UL
  (`imx_wdog_disable_powerdown()`, both stages) and the 8M (`imx_set_wdog_powerdown()`, SPL). The
  6UL's and the 8M's `plat_early_init()` clear WDOG1-3, the RT's WDOG1 and 2, with their clocks on
  (F-245). Out of the ROM PDE is set on all three on jiffy, on WDOG2 and 3 on the 8MP EVK, on WDOG1
  and 2 on the RT1060 EVK. The RT's WDOG3 is an RTWDOG, enabled out of reset (CS resets to 0x2980,
  RT RM 58.5.1.2); the ROM leaves it disabled (CS 0x2520 on the EVK).
- **The ROM's event log** (AN12853; the log `rom`). The pointer to it is at 0x1E0 on the 6UL
  (64 words, event definition version 0: IDs in bits 23:0) and at 0x9E0 on the 8M (128 words,
  version 2: IDs in bits 31:24), where the log is in the ROM's part of the OCRAM, below pb
  (Tables 6, 11, 13). `plat_early_init()` saves it before anything overwrites it. The 8M's ticks
  have no documented unit, and the 8MP EVK's ROM logs a tick with event 0x82, which Table 9
  doesn't list. Not saved: the 8X's (four buffers).
- **The HAB log** (the log `hab`) is read through the ROM's HAB API, `report_status` and
  `report_event` in its vector table (RVT): at 0x100 on the 6UL, 0x00200300 on the RT1060 (RM
  9.12.1), 0x900 on the 8M Plus and 0x880 on the 8MQ. The 8M's have 64-bit pointers and are
  U-Boot's addresses (the RMs don't have them); the 8MP's is checked on the EVK.
  `imx_hab_log_init()` checks the RVT's header (tag 0xdd) before anything is called, and the ROM
  is called at the first read, so its data must be mapped: the 8M maps the whole ROM (252 KiB
  on the 8MP, 128 KiB on the 8MQ).
- **Warm resets.** The 6UL's `plat_early_init()` clears SRC_SCR's warm reset enable, as U-Boot's
  `init_src()` does. The others: unchecked.
- **The 8M's DRAM.** Trained in `plat_init()` (`imx8m-ddr.md`); a failure returns the error, and
  `main()` resets: command mode's buffers are in DRAM (`.no_init`, the `buffers` region).

## 3. Services

- **Reset**: WDOG1's software reset on the 6UL, the 8M and the RT (`imx_wdog_reset_now()`), a
  board reset through the SCFW on the 8X (`sc_pm_reset()`). Before `plat_early_init()`
  (`platform.md` 3.1):
  - The WDOG's driver keeps no state, so what `plat_reset()` needs is WDOG1's clock, and that
    is on when the ROM hands over. The 6UL's and the RT's ROM gate a list of clocks that
    WDOG1's isn't in (6UL RM 8.4.3, Table 8-5; RT RM 9.5.3, Table 9-6: not CCGR3's CG8). The
    8M's ROM uses WDOG1 and opens its gate (8MP RM 6.1.4.3, Table 6-6; 8MQ RM 6.1.4.3,
    Table 6-7), and WDOG_CLK_ROOT is on out of reset (CCM_TARGET_ROOT114 resets to
    0x10000000, 8MP and 8MQ RM 5.1.7.10). By the manuals: not tried on a board, and not true
    for a pb that a debugger loaded after gating clocks.
  - The 8X's `plat_reset()` opens the SCFW's channel itself when `plat_early_init()` hasn't
    yet. When the SCFW refuses the reset, `sc_pm_reset()` returns (SCFW API 13.4.3.19: an
    invalid type, or no access to `SC_R_SYSTEM`) and `plat_reset()` waits for the watchdog.
- **Unique id**: OCOTP words (6UL, 8M, RT; the 8M's are words 1 and 2, F-169), `sc_misc_unique_id()`
  on the 8X. 8 bytes each.
- **Boot source**: only the 6UL, from the ROM log (AN12853 Table 7) and the eMMC's boot partition
  configuration.
- **Tick**: GPT1 on the 6UL, 8M and RT, GPT0 through the SCFW on the 8X. The 6UL's and the RT's
  run from the 24 MHz crystal (F-084, F-238).
- **Command mode's USB** (`plat_cm_init()`, `platform.md` 3.10). The port is a Kconfig choice,
  `<P>_CM_USB`, in the port's `Kconfig`; an option selects the USB stack, pb's class and the
  drivers its set up calls. What `plat_cm_init()` does for each, in this order, as the boards'
  `cm_init` hooks did before it:

| Option               | Port     | Clocks and power                        | PHY              | Controller   |
| -------------------- | -------- | --------------------------------------- | ---------------- | ------------ |
| `IMX6UL_CM_USB_OTG1` | USB OTG1 | PLL_USB1's `EN_USB_CLKS`                | USBPHY1          | `imx_ci_udc` |
|                      |          |                                         | (`imx_usb2_phy`) |              |
| `IMX8M_CM_USB1`      | USB1     | 8MQ: the USB bus, core reference and    | USB1's           | `dwc3_udc`   |
|                      |          | PHY reference clock roots, the gates of | (`imx8m_phy`)    |              |
|                      |          | USB_CTRL1 and USB_PHY1                  |                  |              |
|                      |          | 8MP: the HSIO AXI and the two USB       |                  |              |
|                      |          | reference clock roots, the gates of     |                  |              |
|                      |          | HSIO, USB and USB_PHY; HSIOMIX and the  |                  |              |
|                      |          | USB1 PHY's power domain (GPC), the NoC  |                  |              |
|                      |          | handshake, the BLK_CTRL's USB clock,    |                  |              |
|                      |          | the PHY's 24 MHz reference (F-115)      |                  |              |
| `IMX8X_CM_USB3`      | USB 3.0  | The SCFW: `SC_R_USB_2` and its PHY on,  | In the           | `imx_cdns3`  |
|                      | (QXP)    | its `PER`, `MISC` and `MST_BUS` clocks  | controller's     |              |
|                      |          |                                         | driver           |              |
| `IMX8X_CM_USB_OTG2`  | USB OTG2 | The SCFW: `SC_R_USB_1` and its PHY on   | USB PHY 2        | `imx_ci_udc` |
|                      | (DXL)    |                                         | (`imx_usb2_phy`) |              |
| `IMXRT_CM_USB_OTG1`  | USB OTG1 | PLL_USB1's `EN_USB_CLKS`, the gate      | USBPHY1          | `imx_ci_udc` |
|                      |          | `USBOH3` (CCGR6)                        | (`imx_usb2_phy`) |              |

The 8M's two SoCs differ in the clocks and the power only: `imx8m_soc_usb1_power_up()` in
`plat_imx8mq.c` and `plat_imx8mp.c`. An option exists for the port a board in the tree uses.
Without one: the 6UL's USB OTG2, the 8M's USB2, the 8QXP's and the 8DXL's USB OTG1, the
RT1060's USB OTG2.

## 4. Security

### 4.1 RoT and SLC

HABv4 on the 6UL and the RT (`imx_hab_fuses`, the fuse locations from the platform); the 8M once
its SRK_REVOKE word is verified (D-027, the 8MP RM marks it reserved). AHAB through the SECO on the
8X (`imx8x_revoke_key()`, `imx8x_slc_*()`). The fuse index macros: `IMX6UL_FUSE()`, ...

### 4.2 The TrustZone DRAM filter

| Port   | pb's part                                                            | Protects the enable      | Then   |
| ------ | -------------------------------------------------------------------- | ------------------------ | ------ |
| imx6ul | The DCD sets TZASC1_BYP (GPR9, RM 30.4.7), or the ROM with the       | No lock bit. pb's        | OP-TEE |
|        | TZASC_ENABLE fuse (RM p. 209, a board configuration fuse). pb checks | comments say it's sticky |        |
|        | it and opens DRAM in region 1                                        | until POR: not in the RM |        |
| imx8m  | Before `imx8m_ddr_init()`: GPR10 ID_SWAP_BYPASS, TZASC_EN and both   | GPR10[17:16]             | BL31,  |
|        | locks (8MP RM 4.9, p. 1328). After it the DDR window in region 1,    |                          | then   |
|        | region 0 left secure                                                 |                          | OP-TEE |
| imx8x  | Nothing: BL31 partitions memory through the SCFW                     | The SCFW                 | BL31   |
|        | (`imx8qx_bl31_setup.c`)                                              |                          |        |
| imxrt  | Nothing: the Cortex-M7 has no TrustZone                              | -                        | -      |

What the stages around pb do with it:

- **TF-A's BL31 (i.MX8M)**: `bl31_tzc380_setup()` returns when GPR10's TZASC_EN is clear, and
  otherwise opens region 0 (4 GiB, both worlds). It never enables the filter.
- **OP-TEE**: `imx_configure_tzasc()` panics with "TZC380 must be enabled before starting OP-TEE"
  when `CFG_TZASC_CHECK_ENABLED` is on, which `conf.mk` forces for the i.MX6 and i.MX8M unless
  `CFG_INSECURE=y` (its default). It checks GPR9 bit 0 on the 6UL and, on the 8M, GPR10's
  TZASC_EN, ID_SWAP_BYPASS and the TZASC_EN lock: the lock is required. On the 8M, unless
  insecure, it also panics when region 0 isn't secure only ("non-secure memory alias access
  possible"), so pb leaves region 0 alone and opens DRAM in region 1. Then it programs its regions
  and locks them down; on the 8M again after a resume (the TZASC isn't always on there).
- **U-Boot (i.MX8M)**: `enable_tzc380()` in the board's SPL (`imx8mp_evk/spl.c`, before
  `spl_dram_init()`) sets the GPR10 bits and locks and opens region 0, because "some masters like
  usb dwc3 controllers can't work with secure memory". The DWC3 is pb's USB on the 8MP too.

SoC facts for the driver:

- **Addresses.** The 8MP's TZASC counts from the start of DDR (0x40000000, 8MP RM 4.9.2.1; TF-A
  says the same), seen on the EVK: a refused access at PA 0x60000000 has FAIL_ADDRESS 0x20000000.
  Its TZC-380 has 16 regions and 33 bit addresses, enough for the 8 GiB window. The 6UL RM (52.3)
  says the DDR controller's "local addresses"; pb passes the physical base (F-241).
- **Aliasing.** With less DRAM than the controller addresses, DRAM repeats (both RMs): the default
  region stays secure so that the copies are covered.
- **Non-secure masters (8M).** Some DMA masters are non-secure, the DWC3 among them: without
  region 1 command mode's USB stops (the comment in `imx8m/plat.c`).
- **The 8MP's fuse.** Its RM (4.9.1.2) says a TZASC_ENABLE fuse must be blown. On the EVK, unfused,
  GPR10 alone takes it out of bypass (F-208, 2026-11-10): pb, at EL3 through a mapping with the
  NS attribute, had a read refused in a region made secure only, and the same read passed without
  it. The test is in the log of that day.
- **TZASC1_BOOT_LOCK** (6UL GPR1[23], RM p. 1262), presumably the TZC-380's `secure_boot_lock`
  input: what it locks needs the TZC-380 TRM (DDI0431), which isn't on the share.

### 4.3 Lockdown before the jump

The OCOTP's SW_STICKY register locks fuse areas, their shadow registers included, "unless a POR is
issued": SRK_REVOKE_LOCK and FIELD_RETURN_LOCK (6UL RM 35.5.6); the SCS register has a lock bit
(35.5.7). jiffy's CSF (`src/plat/imx6ul/pb.csf.template`) unlocks SRKREVOKE so that pb can revoke
keys, and CAAM's MID for OP-TEE.

- **i.MX6UL**: `plat_lockdown()` sets both SW_STICKY locks and SCS's (F-244). Seen on jiffy
  (closed, 2026-11-10): before pb runs SW_STICKY is 0x1c (HAB locked field return, bits 3 and 4
  are reserved) and SCS 0x80000000 (HAB locked it); the SRK_REVOKE shadow takes a write. After
  `plat_lockdown()` SW_STICKY is 0x1e and the shadow write is refused. A watchdog reset (Linux's
  `reboot`) clears SW_STICKY, so command mode can revoke keys after a reboot, and HAB still boots.
- **i.MX8M**: nothing locked yet; pb doesn't support its HAB (D-027) and its CSF leaves revocation
  locked. NXP's U-Boot (`secure_lockup()`, i.MX8MQ, SPL, closed devices) sets SW_STICKY's field
  return and SRK revoke locks and locks SCS. The 8MP's OCOTP registers aren't in its public RM.
- **i.MX8X** (SECO) and **i.MX RT**: unchecked.

### 4.4 Other first-stage duties

| Duty (`platform.md` 4.5) | imx6ul                             | imx8m                       | imx8x     | imxrt     |
| ------------------------ | ---------------------------------- | --------------------------- | --------- | --------- |
| Bus access control       | AIPS-TZ: "should be programmed     | BL31: CSU, AIPS-TZ, RDC,    | BL31,     | -         |
|                          | before use" (RM 14.6; reset MPR    | the OCRAM's secure range    | through   |           |
|                          | 0x77000000, OPACR 0x44444444);     | (GPR11)                     | the SCFW  |           |
|                          | U-Boot opens them (`init_aips()`), |                             |           |           |
|                          | pb doesn't (F-246). CSU: OP-TEE    |                             |           |           |
|                          | sets it and locks it; without it   |                             |           |           |
|                          | the reset defaults (SRM, CSU_SA)   |                             |           |           |
| Hardware RNG             | pb instantiates the CAAM's RNG4    | pb (boards)                 | unchecked | - (no     |
|                          | (in the boards today)              |                             |           | CAAM)     |
| Crypto engine            | OP-TEE sets the job rings to the   | BL31                        | The       | -         |
|                          | normal world                       |                             | SCFW/SECO |           |
| handover                 | (`imx_caam.c`); the CSF unlocks    | (`imx8m_caam_init()`)       |           |           |
|                          | CAAM MID for it                    |                             |           |           |
| Architected timer        | unchecked                          | pb: the system counter and  | unchecked | -         |
|                          |                                    | `CNTFRQ` in `plat_init()`   |           |           |
| SNVS                     | unchecked                          | U-Boot inits the glitch     | unchecked | unchecked |
|                          |                                    | detector, clears LPSR       |           |           |
|                          |                                    | (`imx8m_setup_snvs()`): who |           |           |
|                          |                                    | needs it, unchecked         |           |           |
| Cortex-A7 SMP bit        | F-135 (arch)                       | -                           | -         | -         |

## 5. Memory

- **Load windows.** i.MX6UL: DRAM after the DCD, or OCRAM from 0x908000 (ERR011115). i.MX8M Plus:
  the OCRAM's free area after the ROM's part, up to 0x97ffff (8MP RM, Figure 6-2). The others:
  unchecked, today's values are in the link scripts.
- **SoC regions.** i.MX8M: the umctl2 save area at the start of DRAM. i.MX8X: `reset`, 128 KiB at
  0x80000000, pb's reset stub and then BL31's load address.
- **`buffers`.** In DRAM on the 6UL, 8M and 8X; in OCRAM on the RT.
- **The RT's `MEMORY`** is in its board (`src/board/imxrt1060evk/link.lds`) and moves to
  `src/plat/imxrt/link.lds` (F-239).

## 6. Image stages

| Port   | Container                           | Image header           | Signing                   | Today in        |
| ------ | ----------------------------------- | ---------------------- | ------------------------- | --------------- |
| imx6ul | `mkimage -T imximage`, the board's  | IVT at                 | HABv4 CSF, CST; a serial  | jiffy           |
|        | DCD                                 | `PB_ENTRY - 0xC00`     | download CSF (0x00910000) |                 |
| imx8m  | `mkimage -T imx8mimage`             | 8MQ: `PB_ENTRY - 0x40` | HABv4 CSF (8MQ)           | the boards      |
| imx8x  | `mkimage_imx8`, the SCFW, the AHAB  | -                      | AHAB CSF, CST             | MEK, DXL (same) |
|        | container                           |                        |                           |                 |
| imxrt  | `mkimage -T imximage`, a flash      | -                      | -                         | the board       |
|        | image: FCB at 0, the image at 4 KiB |                        |                           |                 |
|        | (FCB from the board's NOR)          |                        |                           |                 |

Outputs today: `pb.imx`, `pb.csf`, `pb_signed.imx` (`pb_signed_uuu.imx` on the 6UL), the RT's
`pb_flash.bin`. Kconfig: `IMX6UL_CREATE_IMX_IMAGE`, `IMX6UL_SIGN_IMAGE`, `IMX8X_*`. Signing inputs
are Kconfig strings on the 6UL, make variables on the 8X (F-240).

## 7. Conformance

Where each port is (2026-11-11).

| Requirement                          | imx6ul       | imx8m             | imx8x      | imxrt     | Finding       |
| ------------------------------------ | ------------ | ----------------- | ---------- | --------- | ------------- |
| Tick and log time from early init    | yes          | yes               | yes        | yes       | F-238         |
| Watchdog armed in early init         | yes          | yes               | yes        | yes       | F-233         |
| Power-down counters disabled         | yes          | yes               | n/a        | yes       | F-245         |
| `plat_reset()` without the watchdog  | yes          | yes               | yes        | yes       | F-234         |
| `plat_reset()` before early init     | yes          | yes               | yes        | yes       | F-262         |
| `plat_wdog_kick()` before arming     | yes          | yes               | yes        | yes       | F-234         |
| `plat_get_boot_reason()`             | yes          | yes               | yes        | yes       | F-236         |
| The unique id fails on a failed read | yes          | no                | unchecked  | no        | F-200         |
| Serial download in                   | USB only     | no                | no         | no        | F-251         |
| `plat_get_boot_source()`             |              |                   |            |           |               |
| `plat_init()` errors only for        | yes          | yes               | yes        | yes       | F-235         |
| command mode                         |              |                   |            |           |               |
| `plat_get_status()`                  | no           | no                | no         | no        | F-248         |
| `plat_cm_init()`                     | one port     | one port          | one port   | one port  | F-402         |
| The board's regions in the map (5.2) | no           | no                | no         | n/a       | F-269, F-158  |
| SoC memory for image data,           | n/a          | partly            | yes        | yes       | F-294         |
| `bl_params`' region (5.3)            |              |                   |            |           |               |
| The system controller's API through  | n/a          | n/a               | partly     | n/a       | F-270         |
| the platform header (7)              |              |                   |            |           |               |
| TrustZone filter                     | partly       | partly            | n/a (BL31) | n/a       | F-241, F-208, |
|                                      |              |                   |            |           | F-261         |
| `plat_lockdown()`                    | yes          | yes (nothing yet) | unchecked  | unchecked | F-244         |
| Bus access control                   | no (AIPS-TZ) | n/a (BL31)        | n/a (BL31) | n/a       | F-246         |
| `plat_rot`, `plat_slc`               | no           | no (no HAB)       | no         | no        | F-247         |
| EOL as the SoC's own state           | no           | no                | no         | no        | F-263         |
| Crypto engine and RNG in             | no           | no                | no         | yes       | F-247         |
| `plat_init()`                        |              |                   |            |           |               |
| MMU from `pb_mmu_init()`             | no           | no                | no         | n/a       | F-158         |
| Link script without addresses        | no           | partly            | no         | no        | F-239         |
| Image stages                         | no           | no                | no         | no        | F-240         |
| `<p>_plat()` accessor                | yes          | no                | yes        | no        |               |
