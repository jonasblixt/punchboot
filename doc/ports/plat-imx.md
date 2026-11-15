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
| Who sets up DRAM | The ROM, from the board's DCD | The SPL trains it       | The SCFW (container)    | None used  |
|                  |                               | (D-088)                 |                         |            |
| pb runs from     | DRAM (jiffy: 0x80001000)      | DRAM; the SPL from the  | DRAM, a reset stub at   | OCRAM      |
|                  |                               | TCM (8MQ), the OCRAM    | 0x80000000              |            |
|                  |                               | (8MP)                   |                         |            |
| Watchdog         | WDOG1 (`imx_wdog`)            | WDOG1                   | The SCFW (`sc_timer_*`) | WDOG1      |
| Reset reason     | SRC_SRSR                      | SRC_SRSR                | `sc_pm_reset_reason()`  | WDOG1 WRSR |
| ROM state saved  | ROM log, the serial           | ROM log                 | -                       | -          |
|                  | downloader's USB address      |                         |                         |            |
| HAB log          | yes                           | yes                     | - (AHAB)                | yes        |
| SECO log         | -                             | -                       | yes                     | -          |

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
  doesn't list. The 8X's can't be: its ROM runs on the SCU, and its log (four buffers, Table 13)
  is in the SCU's TCM, which the SCFW locks to all but the MUs (8QXP RM 2.2.8.1); the SCFW API
  has no call that reads it.
- **The SECO log** (the log `seco`, 8X): the life cycle (`sc_seco_chip_info()`) and the SECO's
  events (`sc_seco_get_event()`), the responses other than success to any request since it
  started, as U-Boot's `ahab_status` prints them: the request in bits 23:16, the indication in
  15:8, the status in 7:0, named by the SECO API Reference Guide's tables ("Responses").
  Registered by `plat_cm_init()` (D-023) and read through the SCFW at the first read, once. On
  the MEK (open, no SRK fused): life cycle 0x0020, one event, `authenticate container`,
  `bad key hash`, status success.
- **The HAB log** (the log `hab`) is read through the ROM's HAB API, `report_status` and
  `report_event` in its vector table (RVT): at 0x100 on the 6UL, 0x00200300 on the RT1060 (RM
  9.12.1), 0x900 on the 8M Plus and 0x880 on the 8MQ. The 8M's have 64-bit pointers and are
  U-Boot's addresses (the RMs don't have them); the 8MP's is checked on the EVK.
  `imx_hab_log_init()` checks the RVT's header (tag 0xdd) before anything is called, and the ROM
  is called at the first read, so its data must be mapped: the 8M maps the whole ROM (252 KiB
  on the 8MP, 128 KiB on the 8MQ).
- **Warm resets.** The 6UL's `plat_early_init()` clears SRC_SCR's warm reset enable, as U-Boot's
  `init_src()` does. The others: unchecked.
- **The 8M's two stages** (D-088, `platform.md` 2). The SPL (`src/plat/imx8m/spl/`) sets up the
  GPT for its tick, the TZASC's enable and its DRAM region (4.2), trains DRAM (`imx8m-ddr.md`)
  and leaves the copy for TF-A at the start of DRAM, with its RAM log in on-chip RAM. pb proper's
  `plat_init()` doesn't train again: it reads the TZASC's state back into `plat_get_status()`.
  The SPL fails without a word on the console: the ROM then goes to its serial downloader, and
  the RAM log of pb proper loaded over USB has the SPL's lines.
  - **8MQ, a HABv4 plugin** (8MQ RM 6.1.7). The ROM calls the SPL at its entry with three
    pointers, the start, the size and the IVT offset it returns, at EL3 on the ROM's stack and
    with the ROM's MMU state. The SPL saves the ROM's registers and stack pointer, runs on its own
    stack, and returns 1 with pb proper's values, 0 when DRAM failed. pb proper's IVT follows the
    plugin's CSF (imx-mkimage's layout): the SPL reads its boot data there and checks the IVT's
    entry and address against `PB_ENTRY` and the load range against the DRAM below pb proper's
    region, then calls `pu_irom_hwcnfg_setup(&start, &size, &boot_data)` (the ROM's API table at
    0x900, entry 2, 0xa8cc on the pico8ml's ROM; it returns 0xf0). The function asks HAB's
    `check_target` for the range, copies what the ROM has read, and reads the rest from the boot
    device. The ROM has left HAB before it runs the plugin: the SPL calls HAB's `entry` before
    the function and its `exit` after it (the RVT, 0x888 and 0x890), or `check_target` fails
    with "function called out of sequence" (HAB's event log on the pico8ml). The SPL cleans the
    data cache to the point of coherency before it returns: the ROM may run it with the D-cache
    on. Its `.bss` and stack are at the top of its window, below the RAM log: the ROM loads the
    plugin's CSF and pb proper's IVT right after the SPL's image.
    - **Over USB the SPL is a fastboot device** (D-089, F-451). The serial downloader takes one
      image: "JUMP_ADDRESS is the last command that the host can send" (8MQ RM 6.1.8.2.1.7). A
      plugin that returns 1 there makes the ROM go on to pb proper, which the host hasn't sent,
      and the ROM serves USB no more; one that returns 0 leaves HAB failed (0x33), and the ROM
      refuses every command after it (both on the pico8ml). So in a serial download (the ROM
      log's event 0xd0) the SPL doesn't return: it brings up DRAM, takes USB1 from the ROM (its
      clocks, its PHY and the DWC3, as `plat_cm_init()`) and is a fastboot device, which the
      host sends pb proper to (`spl/fb_usb.c`, `spl/fb_proto.c`, `spl/hab.c`).
      - **The device**: pb's vendor id with the product id 0x0001, pid.codes' test id, which
        the SPL reuses (D-089; `PB_SPL_USB_PID` in the board's `makefile.mk` or on the command
        line gives another); the interface fastboot's hosts look for (0xff, 0x42, 3), the bulk
        endpoints pb's device has, no serial number. The commands: `getvar:` of `version`,
        `product` and `max-download-size`, `download:<size>`, `boot` and `continue`. Any other
        is `FAIL`. Nothing of the device is told but that it is pb's SPL (`security.md` 4.7).
      - **The host downloads the image the plugin came from**, `pb_signed_uuu.imx`, whole. It
        goes to DRAM where pb proper's IVT in it is at its own address (`PB_ENTRY - 0x40` less
        the IVT's offset in the image, a cache line: the build checks it), at most to the end
        of pb proper's region, in whole packets.
      - **What starts** (`security.md` 1). The header in the download is the one the ROM loaded
        and authenticated with the plugin, which the SPL checked against its build; the
        download reaches its CSF; and HAB authenticates pb proper where it is
        (`authenticate_image` between HAB's `entry` and `exit`, RVT at 0x880, as the 8M Plus's
        SPL). On a closed device nothing starts that HAB refused. On one that isn't, HAB's
        failure is its events only, as for the ROM's own image. What was refused isn't tried
        again: the host downloads again. Then the host gets its `OKAY`, the device leaves the
        bus, the SPL cleans the D-cache and jumps to pb proper's entry. pb proper finds the ROM's
        event 0xd0 and stops in command mode (`CONFIG_CM_ON_SERIAL_DOWNLOAD`). HAB takes 3.7 s
        on the pico8ml (RSA 4096, the caches off as the ROM has them) and 2.3 KiB of the SPL's
        stack. Its `report_status` writes two 32 bit words.
      - **Its waits**: 10 s for a host to configure the device and 10 s for a transfer inside a
        command, none for a command. A session that is lost (a bus reset, a host that stopped)
        starts the device over, and what was downloaded is dropped. No host, or a controller
        that failed: the SPL resets through WDOG1, and the ROM is in its serial downloader
        again. It never returns to the ROM. Where the build has the watchdog
        (`CONFIG_ENABLE_WATCHDOG`) WDOG1 is armed for the session, 30 s, and kicked every round:
        a call into the ROM that hangs resets too. pb proper arms it again with its own time.
      - **Its memory**: what the controller reads and writes of the SPL's own is in DRAM, at
        the start of pb's buffers (`.no_init` in `spl/link.lds`), and the DWC3's driver sends
        endpoint 0's replies from there (`usb.md` 5). Nothing says the controller reaches the
        TCM.
      - **Only with `IMX8M_CM_USB1`**: a board without the port has no USB recovery. Its SPL
        returns 0 in a serial download, and the ROM stays there.
      - The host, `uuu` (`build-and-test.md`, "The i.MX8MQ over USB"); Android's `fastboot`
        speaks it too (`stage`, `continue`). uuu's `FB: boot` sends `boot:`.
      - On the closed pico8ml (2026-11-15): both hosts, about 9 s from the ROM to pb proper's
        command mode; a download that is cut short, one with another header and one with a
        changed byte of pb proper are refused, and the right one starts after them.
    - **A board may give the SPL a console**: `imx8m_spl_console_init()`
      (`include/plat/imx8m/spl.h`), the pico8ml's UART1, and sets `PB_SPL_CONSOLE` in its
      `makefile.mk`: pb proper then doesn't print the SPL's lines again (`ramlog_kept()`); they
      stay in the RAM log. Without it the SPL's lines are in the RAM log only, which pb proper
      prints.
  - **8MP, an image of the v2 ROM** (no plugins). The SPL reads pb proper's IVT and image with
    the ROM API at 0x980 (`download_image`, `query_boot_infor` as U-Boot's
    `arch/arm/mach-imx/romapi.c`: the arguments XORed into the last one), checks the IVT as the
    8MQ's SPL does, calls HAB's `authenticate_image` (RVT at 0x900, `spl/hab.c`) and jumps to pb
    proper only when it returns pb proper's entry. Over USB the ROM API streams: the image is
    read in order. The v2 ROM
    takes no return from the SPL: a failure resets through WDOG1 (`platform.md` 2).
  - **pb proper** is linked at the board's `PB_ENTRY` in DRAM, after its buffers. Its entry turns
    the MMU and the D-cache off and invalidates the cache (F-207), as before.

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
- **What the SoC is** (`plat_get_info()`, `platform.md` 3.11, D-077):

| Port   | Variant, revision                  | CPU clock                  | DRAM clock                  |
| ------ | ---------------------------------- | -------------------------- | --------------------------- |
| imx6ul | USB_ANALOG_DIGPROG (RM 55.4.11):   | PLL1, ARM_PODF             | mmdc_clk_root: periph2,     |
|        | 0x64 6UL, 0x65 6ULL (Linux's)      | (RM 18.7.1, 18.6.4)        | FABRIC_MMDC_PODF (RM 18.6)  |
| imx8m  | The build; DIGPROG's MINOR (0x6C   | The ARM PLL (8MP: as the   | Half the board's first      |
|        | on the 8MQ, 0x800 on the 8MP)      | ROM left it)               | trained rate                |
| imx8x  | SC_C_ID: the SoC id, A0 + rev      | SC_R_A35's CPU clock       | Twice SC_R_DRC_0's bus      |
|        |                                    |                            | clock (the DFI's)           |
| imxrt  | "i.MX RT1060"; DIGPROG             | AHB_CLK_ROOT               | None set up                 |
|        | (RM 43.5.13)                       | (RM 14.5)                  |                             |

  The specifics: the 6UL's speed grade and boot configuration (SBMR1, SBMR2, the BOOT_CFG fuse),
  the RT's speed grade, the 8X's SCFW and SECO builds and temperature; none on the 8M. On the
  MEK: `SoC i.MX8QXP rev B0, CPU 1200 MHz, memory 1200 MHz`, ~70 µs of the boot. jiffy: i.MX6UL
  1.2, 528 MHz, 396 MHz; the 8MP EVK: 1.1, 1000 MHz, 2000 MHz; the RT1060: 1.0, 600 MHz. The boards
  have no status (D-077). F-432, F-433 for what isn't checked.
- **Unique id**: two OCOTP words (6UL, 8M, RT; the 8M's are words 1 and 2, F-169), which
  `imx_ocotp_read_unique_id()` makes into the id for all three; `sc_misc_unique_id()` on the 8X. 8
  bytes each.
- **Boot source**: the 6UL, from the ROM log (AN12853 Table 7) and the eMMC's boot partition
  configuration. A serial download is USB when a host gave the ROM's USB device an address, which
  `plat_early_init()` saves, and else UART: the serial downloader listens on both. The 8M, from
  the ROM log too: its event 0xd0, "Serial download" (AN12853 Table 9), is reported as USB, and
  a log without it as unknown. Table 9 is the source: the event hasn't been seen in a board's
  log (the 8MP EVK's one log in the tests is of an eMMC boot). The 8X, from
  the SCFW's boot type (`sc_misc_get_boot_type()`), asked at the call: serial is reported as USB,
  the other types as unknown. The RT, from SRC_SBMR2's BMOD: 0b01, the serial downloader, is
  reported as USB (its UART isn't told apart), the fuses (0b00) and internal boot (0b10, the EVK
  on the bench) as unknown. Not read: a blank IVT at the NOR's start, the sign of a ROM that fell
  back to the serial downloader by itself (F-251).
- **Tick**: GPT1 on the 6UL, 8M and RT, GPT0 through the SCFW on the 8X. The 6UL's and the RT's
  run from the 24 MHz crystal (F-084, F-238). `plat_early_init()` opens GPT1's gates before it
  starts the timer, on the 8M as on the 6UL and the RT (F-416). On the RT they are open out of
  reset (CCGR1 resets to FFFF_FFFFh, RT RM 14.7.22) and the ROM leaves them open (Table 9-6 lists
  neither CG10 nor CG11); on the 6UL they are CCGR1's CG10 and CG11 (Linux's `clk-imx6ul.c`), open
  out of jiffy's ROM, the 6UL RM's reset value not checked here. The writes change nothing on such
  a board.
- **Command mode's USB** (`plat_cm_init()`, `platform.md` 3.10). The port is a Kconfig choice,
  `<P>_CM_USB`, in the port's `Kconfig`; an option selects the USB stack, pb's class and the
  drivers its set up calls. What `plat_cm_init()` does for each, in this order, as the boards'
  `cm_init` hooks did before it:

| Option               | Port     | Clocks and power                        | PHY              | Controller   |
| -------------------- | -------- | --------------------------------------- | ---------------- | ------------ |
| `IMX6UL_CM_USB_OTG1` | USB OTG1 | PLL_USB1 powered, locked, out of bypass | USBPHY1          | `imx_ci_udc` |
|                      |          | and `EN_USB_CLKS`; the charger detector | (`imx_usb2_phy`, |              |
|                      |          | off (F-139)                             | no PLL_SIC)      |              |
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

HABv4 on the 6UL, the RT and the 8M (`imx_hab_fuses`, the fuse locations from the platform). AHAB
through the SECO on the 8X (`imx8x_revoke_key()`, `imx8x_slc_*()`). The fuse index macros:
`IMX6UL_FUSE()`, ...

The 8M's (`include/plat/imx8m/fusebox.h`, 2026-11-14): SEC_CONFIG[1] at bank 1 word 3 bit 25 and
SRK_HASH in banks 6 and 7, from the 8MQ RM's fuse map and NXP's U-Boot for the whole family
(`mx8m_secure_boot.txt`); the 8M Plus's public RM has no security fuses. SRK_REVOKE[2:0] on the
8MQ only, bank 9 word 3: the RM's OCOTP register "Value of OTP Bank9 Word3 (SRK Revoke)" and its
boot fuses, while its fuse map calls 0x670 reserved; not verified on a part (F-447). The 8M
Plus's SRK_REVOKE isn't public: its keys read as active and none is revoked. The 8M's CSF
unlocks SRKREVOKE, as the 6UL's. The lock and a revocation wait for a boot without a HAB
failure (`imx_hab_no_failure()`): warnings pass, the 8MQ's ROM reports one for the CAAM. On the
pico8ml (HAB open, SRK fused): `slc show` reads CONFIGURATION and the three development keys
active.

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
- **i.MX8M** (2026-11-14, D-084): as the 6UL, `plat_lockdown()` locks SW_STICKY's SRK_REVOKE_LOCK
  and FIELD_RETURN_LOCK and SCS (HAB_JDE), then stops the CAAM; its CSF unlocks SRKREVOKE for
  command mode. The 8MQ RM (OCOTP 0x50, 0x60; SCS's lock holds until a POR); NXP's U-Boot does
  the same on a closed 8MQ (`secure_lockup()`). The 8MP's OCOTP registers aren't in its public
  RM: U-Boot's `struct ocotp_regs` has the family's at the same offsets. Not run on a board yet:
  the pico8ml has no boot image, the 8MP EVK wasn't attached.
- **i.MX8X** (SECO) and **i.MX RT**: unchecked.

### 4.4 Other first-stage duties

| Duty (`platform.md` 4.5) | imx6ul                             | imx8m                       | imx8x     | imxrt     |
| ------------------------ | ---------------------------------- | --------------------------- | --------- | --------- |
| Bus access control       | AIPS-TZ: "should be programmed     | BL31: CSU, AIPS-TZ, RDC,    | BL31,     | -         |
|                          | before use" (RM 14.6; reset MPR    | the OCRAM's secure range    | through   |           |
|                          | 0x77000000, OPACR 0x44444444);     | (GPR11)                     | the SCFW  |           |
|                          | pb opens them as U-Boot's          |                             |           |           |
|                          | `init_aips()` (F-246). CSU: OP-TEE |                             |           |           |
|                          | sets it and locks it; without it   |                             |           |           |
|                          | the reset defaults (SRM, CSU_SA)   |                             |           |           |
| Hardware RNG             | pb instantiates the CAAM's RNG4    | pb (`plat_init()`)          | unchecked | - (no     |
|                          | (`plat_init()`)                    |                             |           | CAAM)     |
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
- **The 8M's SPL window**: the ROM's load window, the platform's (`IMX8M_SPL_ENTRY`, D-088). 8MQ:
  the TCM from 0x7e1000 to 0x819000, the SPL, then its RAM log and `bl_params` at its end. 8MP:
  the OCRAM from 0x920000 to 0x980000.
- **SoC regions.** i.MX8M: the umctl2 save area at the start of DRAM. i.MX8X: `reset`, 128 KiB at
  0x80000000, pb's reset stub and then BL31's load address.
- **`buffers`.** In DRAM on the 6UL, 8M and 8X; in OCRAM on the RT.
- **The RT's `MEMORY`** is in `src/plat/imxrt/link.lds`.

## 6. Image stages

| Port   | Container                           | Image header           | Signing                   |
| ------ | ----------------------------------- | ---------------------- | ------------------------- |
| imx6ul | `mkimage -T imximage`, the board's  | IVT at                 | HABv4 CSF, CST; a serial  |
|        | DCD                                 | `PB_ENTRY - 0xC00`     | download CSF (0x00910000) |
| imx8m  | 8MQ: NXP's `mkimage_imx8m`, the SPL | 8MQ: the SPL's at its  | HABv4: 8MQ a CSF for the  |
|        | as the plugin, pb proper as the     | entry - 0x40, pb       | plugin and pb proper's    |
|        | loader; 8MP: `mkimage -T            | proper's at `PB_ENTRY  | IVT, one for pb proper;   |
|        | imx8mimage` for the SPL, pb proper  | - 0x40`                | 8MP one for the SPL, one  |
|        | with its IVT after it               |                        | for pb proper             |
| imx8x  | `mkimage_imx8`, the SCFW, the AHAB  | -                      | AHAB CSF, CST (8QXP)      |
|        | container                           |                        |                           |
| imxrt  | `mkimage -T imximage`, a flash      | -                      | -                         |
|        | image: FCB at 0, the image at 4 KiB |                        |                           |
|        | (FCB from the board's NOR)          |                        |                           |

The stages are in `src/plat/<p>/makefile.mk` (`platform.md` 6.2), their files in `$(BUILD_DIR)`:

| Stage               | Kconfig                    | Makes                                                   |
| ------------------- | -------------------------- | ------------------------------------------------------- |
| `imx6ul_image`      | `IMX6UL_CREATE_IMX_IMAGE`  | `pb.imx`                                                |
| `imx6ul_sign_desc`  | `IMX6UL_SIGN_IMAGE`        | `pb.csf`; `pb_uuu.csf` for the serial downloader        |
| `imx6ul_sign_image` | `IMX6UL_SIGN_IMAGE`        | `pb_csf.bin`, `pb_signed.imx` (two zero blocks before); |
|                     |                            | `pb_csf_uuu.bin`, `pb_signed_uuu.imx`                   |
| `imx8m_image`       | `IMX8M_CREATE_IMX_IMAGE`   | `pb.imx` from `spl.bin` and `pb.bin`, its offsets in    |
|                     |                            | `pb.imx.offsets`; 8MQ: `pb_pad.bin`, `pb_pad.imx`; 8MP: |
|                     |                            | `spl.imx`, `pb_part.bin`                                |
| `imx8m_sign_desc`   | `IMX8M_SIGN_IMAGE`         | `spl.csf`, `pb.csf`                                     |
| `imx8m_sign_image`  | `IMX8M_SIGN_IMAGE`         | `spl_csf.bin`, `pb_csf.bin`, `pb_signed.imx`; 8MQ: with |
|                     |                            | 33 KiB of zeros before, the ROM's IVT offset, and       |
|                     |                            | `pb_signed_uuu.imx` without                             |
| `imx8x_image`       | `IMX8X_CREATE_IMX_IMAGE`   | `pb.imx`                                                |
| `imx8x_sign_desc`   | `IMX8X_SIGN_IMAGE`         | `pb.csf`; on the DXL it stops the build (not supported) |
| `imx8x_sign_image`  | `IMX8X_SIGN_IMAGE`         | `pb_signed.imx`                                         |
| `imxrt_image`       | `IMXRT_CREATE_IMX_IMAGE`   | `pb.imx`                                                |
| `imxrt_flash_image` | `IMXRT_CREATE_FLASH_IMAGE` | `fcb.bin`, `pb_flash.bin`                               |

`FINAL_OUTPUT` is the signed image, else `pb.imx` (the RT: `pb_flash.bin`). The inputs, make
variables set with `?=`:

| Port   | Inputs                                                                                    |
| ------ | ----------------------------------------------------------------------------------------- |
| all    | `CST_TOOL` (`cst`): the 6UL calls it `--o <out> --i <description>`, the 8M and 8X         |
|        | `-i <description> -o <out>`                                                               |
| imx6ul | `MKIMAGE`; `IMX6UL_IMAGE_CFG`, the board's `imximage.cfg` (the DCD); `IMX6UL_SRK_TABLE`,  |
|        | `IMX6UL_KEY_INDEX`, `IMX6UL_SIGN_CERT`, `IMX6UL_IMAGE_SIGN_CERT`                          |
| imx8m  | `MKIMAGE` (8MQ: `mkimage_imx8m`); `IMX8M_BOOT_FROM` (8MP, `sd`); `IMX8M_SRK_TABLE`,       |
|        | `IMX8M_KEY_INDEX`, `IMX8M_SIGN_CERT`, `IMX8M_IMAGE_SIGN_CERT`                             |
| imx8x  | `IMX8X_MKIMAGE`; `IMX8X_AHAB_IMAGE` (the SECO firmware, per variant); `IMX8X_SCFW_IMAGE`, |
|        | the board's; `IMX8X_BOOT_DEVICE` (`emmc_fast`); `IMX8X_SRK_TABLE`, `IMX8X_SIGN_CERT`,     |
|        | `IMX8X_KEY_INDEX`, `IMX8X_KEY_REVOKE_MASK`                                                |
| imxrt  | `MKIMAGE`; `IMXRT_IMAGE_CFG`, the board's `imximage.cfg`; `IMXRT_FCB_SRC`, the board's    |
|        | FCB: a C file with its 512 bytes in `.fcb`                                                |

The i.MX8M's two CSFs are made from one template (`pb.csf.template`). The 8MQ's plugin has no
`[Unlock]`: they are pb proper's, the image HAB exits into. The 8M Plus's SPL and pb proper both
have them: its ROM leaves HAB after the SPL, and what it locks then stays locked. The 8M Plus's
signed image is built, not run: the EVK is open and its fuses are never written (D-087).

The keys default to the Kconfig strings of the same names. A board at level 3 starts from `pb.bin`,
linked at `PB_ENTRY` and entered at its first byte, and puts around it what the first table says.

## 7. Conformance

Where each port is (2026-11-15).

| Requirement                          | imx6ul       | imx8m             | imx8x      | imxrt     | Finding       |
| ------------------------------------ | ------------ | ----------------- | ---------- | --------- | ------------- |
| Tick and log time from early init    | yes          | yes               | yes        | yes       | F-238         |
| Watchdog armed in early init         | yes          | yes               | yes        | yes       | F-233         |
| Power-down counters disabled         | yes          | yes               | n/a        | yes       | F-245         |
| `plat_reset()` without the watchdog  | yes          | yes               | yes        | yes       | F-234         |
| `plat_reset()` before early init     | yes          | yes               | yes        | yes       | F-262         |
| `plat_wdog_kick()` before arming     | yes          | yes               | yes        | yes       | F-234         |
| `plat_get_boot_reason()`             | yes          | yes               | yes        | yes       | F-236         |
| The unique id fails on a failed read | yes          | yes               | yes        | yes       | F-416         |
| Serial download in                   | yes          | yes               | yes        | pins only | F-251         |
| `plat_get_boot_source()`             |              |                   |            |           |               |
| `plat_init()` errors only for        | yes          | yes               | yes        | yes       | F-235         |
| command mode                         |              |                   |            |           |               |
| `plat_get_status()`                  | yes          | yes               | yes        | yes       | F-248         |
| `plat_cm_init()`                     | one port     | one port          | one port   | one port  | F-402         |
| The board's regions in the map (5.2) | yes          | yes               | yes        | n/a       | F-269         |
| SoC memory for image data,           | n/a          | yes (D-088)       | yes        | yes       | F-294         |
| `bl_params`' region (5.3)            |              |                   |            |           |               |
| The system controller's API through  | n/a          | n/a               | yes        | n/a       |               |
| the platform header (7)              |              |                   |            |           |               |
| TrustZone filter                     | partly       | partly            | n/a (BL31) | n/a       | F-241, F-261  |
| `plat_lockdown()`                    | yes          | yes, not on a     | unchecked  | unchecked | F-244, D-084  |
|                                      |              | board             |            |           |               |
| Bus access control                   | yes (jiffy)  | n/a (BL31)        | n/a (BL31) | n/a       | F-246         |
| `plat_rot`, `plat_slc`               | yes          | yes (HABv4; the   | yes        | yes       | F-447         |
|                                      |              | 8MP revokes none) |            |           |               |
| `plat_rot.revoke_slots` (4.1)        | yes (3)      | yes (3)           | yes (4)    | yes (3)   | F-439         |
| EOL as the SoC's own state           | no           | no                | no         | no        | F-263, D-084  |
| Crypto engine and RNG in             | yes          | yes               | yes (no    | yes (Mbed |               |
| `plat_init()`                        |              |                   | RNG)       | TLS)      |               |
| MMU from `pb_mmu_init()`             | yes          | yes               | yes        | n/a       | F-158, F-269  |
| Link script without addresses        | yes          | yes               | yes        | yes       | F-239         |
| Image stages                         | yes          | yes               | yes        | yes       | F-240         |
| Two stages, pb proper in DRAM        | n/a          | 8MQ from the eMMC | n/a        | n/a       | D-088, D-089  |
| (`platform.md` 2)                    |              | and over USB, the |            |           |               |
|                                      |              | SPL's fastboot    |            |           |               |
|                                      |              | (pico8ml); 8MP    |            |           |               |
|                                      |              | built, not run    |            |           |               |
| The SPL's TZASC read back            | n/a          | yes               | n/a        | n/a       | D-088         |
| Functions for boards, not a          | yes          | n/a               | yes        | n/a       | F-440         |
| structure (7)                        |              |                   |            |           |               |
| `plat_get_info()`                    | yes (jiffy)  | 8MP EVK; 8MQ not  | yes (MEK)  | yes       | F-432, F-433  |
| The header: the tick's accuracy, the | no           | no                | no         | no        |               |
| longest stretch without a kick, what |              |                   |            |           |               |
| `plat_lockdown()` locks (7)          |              |                   |            |           |               |
