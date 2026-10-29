# Firmware architecture

## Boot flow

1. Reset entry: `src/arch/{armv8a/entry.S, armv7a/entry_armv7a.S, armv7m/entry.c}`, then `bl main`.
2. `src/main.c` `main()` runs `arch_init()`, `plat_init()`, then `plat_board_init()`, which calls `board_init(&plat)`.
   Then `boot(NULL)`. On failure: "Boot aborted", then `cm_run()` (command mode), then `plat_reset()`.
3. `src/board/<b>/board.c` `board_init()` wires up drivers in this order:
   - console, mmc/usdhc
   - `gpt_ptbl_init`, `rot_init`, `slc_init`
   - `boot_ab_state_init`
   - the boot driver: `boot_driver_linux_init`, `boot_driver_atf_init` or armv7m baremetal
   - `boot_init()`

   `cm_board_init()` sets up the USB phy/UDC, `pb_dev_cls_init` and `cm_init()`.
4. `src/boot/boot.c` `boot_load()` runs:
   - `early_boot_cb`
   - source selection (`BIO` / `IN_MEM` / `CB` / `CUSTOM`)
   - the bpak header read from the **end** of the partition
   - the functions in `src/boot/image_helpers.c`: `boot_image_auth_header`, `verify_parts` (overlap check against pb's own segments), `load_and_hash`, `verify_payload`
   - `prepare`

   `boot_jump()` runs `late_boot_cb`, then `jump()`.
5. Jump targets: `src/boot/linux.c` (kernel + DTB + ramdisk, libfdt), `src/boot/atf.c` (TF-A bl_params, BL32 OP-TEE), `src/boot/armv7m_baremetal.c`.

## Core modules (`src/`)

| File | Role |
|---|---|
| `bio.c` | Block-device layer; partitions are child bios |
| `crypto.c` | Hash/DSA op registry. Backends: `drivers/crypto/mbedtls`, `drivers/crypto/caam` (HW hash) |
| `rot.c` | Root of trust: key map and revocation. Keys come from the generated `keystore.c` |
| `log.c` | The start of every LOG line: `[ms.us] level function:`, the time since `log_time_start()`, which each platform calls once its timer runs (0 before). There are no separate timestamps any more: compare `punchboot log` outputs to find where boot time went |
| `ramlog.c` | `CONFIG_RAMLOG`: a copy of the console output in a ring buffer in `.ramlog` (NOLOAD, in the `ram` region pb runs from: on the i.MX8M `.no_init` is DRAM, trained after `ramlog_init()`; symbol `ramlog`, `struct pb_ramlog_hdr`). Read it over JTAG/FEL, from a RAM dump or with `punchboot log` (`PB_CMD_LOG_READ`, positions count bytes since `ramlog_init()`, so they survive a wrap). Prints before `console_init()` only go to the log, and `console_init()` writes them to the UART. `CONFIG_RAMLOG_LOGLEVEL` (default debug) puts levels above `CONFIG_LOGLEVEL` in the log only: the LOG macros mute the UART for them (`console_uart_mute()`), and the replay leaves out the ones muted before `console_init()`. Guard code that only log messages need with `LOG_MAX_LEVEL`, not `LOGLEVEL`. `CONFIG_RAMLOG_SYNC` cleans the D-cache per character, so the log is current even after a hang |
| `log_source.c` | The logs `PB_CMD_LOG_READ` reads, selected by `pb_id()` of their name (`struct log_source`: `head`, `length`, `read`, like the RAM log). Built in: `pb` (the RAM log, also log 0) and `list` (one line per log). Platforms and boards add others with `log_source_register()` (up to 4), e.g. `rom`. A log that doesn't change has `length() == head()`. The result echoes the log that was read; see `compatibility.md` for old hosts and devices |
| `slc.c` | Security life cycle. Thin dispatch to `plat/*/slc_helpers.c`, `fusebox.c`, `drivers/fuse/imx_ocotp.c` |
| `cm/cm_main.c` | Command mode: `cm_run()` loop, `pb_command_parse()` switch over `PB_CMD_*` |
| `wire.c` | Wire protocol helpers, shared with the host lib |
| `boot/ab_state.c` | A/B state (primary + backup, CRC32), attempt counters, rollback |
| `boot/linux_ramoops.c` | `CONFIG_BOOT_LINUX_RAMOOPS` (off by default): the console that Linux's pstore/ramoops left in a reserved RAM region, as the log `linux`. The Kconfig values mirror the DT ramoops node (address, size, record-, console-, ftrace-, pmsg-size; ecc-size must be 0), and the console zone is found the way Linux 6.18 lays the region out (`ramoops_init_przs()`: `size / record_size` dmesg zones of an even share each, then console). Read when the log is read, never copied, so read it before anything is loaded there. On the 8MP EVK, DRAM survives pb's watchdog reset and its DRAM training, except the first 4 KiB + 64 bytes at 0x80000000 (the second rank's start, training patterns) and pb's own buffers at 0x40060000; a PMIC that power cycles the DRAM on WDOG_B (Linux's restart) would lose it, not checked |
| `drivers/partition/gpt.c` | GPT validate/repair/install from variant list |
| `drivers/rom_log/imx_rom_log.c` | The i.MX boot ROM's event log (NXP AN12853, in `~/work/manuals`). The platform passes a `struct imx_rom_log_config` (pointer address, OCRAM range, size, event definition version) to `imx_rom_log_save()` early in `plat_init()`. Decodes to text, read as the log `rom` (`punchboot log rom`, registered when the save finds a log, decoded at the first read) or jiffy's `board command dump-rom-log`. `imx_rom_log_has_event()` tells `plat_get_boot_source()` how the ROM booted. Event tables per definition version: 0 (i.MX6, IDs in bits 23:0) and 2 (i.MX 8M, IDs in bits 31:24, ticks with an undocumented unit; `0x82` has a tick the table doesn't list). Events have up to two typed parameters. i.MX6UL (pointer 0x1E0, 64 words) and i.MX8M (0x9E0, 128 words, in the ROM's part of the OCRAM below pb) save it. Not yet: i.MX 8QXP/8DXL (four buffers) and i.MX93 (fixed address) |
| `drivers/rom_log/imx_hab_log.c` | The boot ROM's HABv4 status and events as the log `hab` (i.MX6UL, i.MX8M, i.MX RT10xx), through the ROM's HAB API (`report_status`, `report_event` in its RVT; `struct imx_hab_log_config`: the RVT's address and pointer size, 4 or 8). `imx_hab_log_init()` checks the RVT header (tag 0xdd) before anything is called; the ROM is called at the first read. RVTs: i.MX6UL 0x100, RT1060 0x00200300 (RM 9.12.1), i.MX8M Plus 0x900 and 8MQ 0x880 with 64-bit pointers (U-Boot's; the 8MP one checked on the EVK). The ROM's data must be mapped: on the 8MP the whole 252 KiB ROM |

## Platforms and boards (last touched)

| | Status |
|---|---|
| Archs | armv7a, armv8a (2026); armv7m (2023) |
| Plats | imx6ul, imx8m (2026; `PLAT_IMX8MQ`/`PLAT_IMX8MP` + `PLAT_IMX8M_COMMON`, per-SoC clocks in `plat_<soc>.c`, DDR init: `imx8m-ddr.md`); imx8x QXP/DXL (2026-08); imxrt (2023, stale); host (test only, `testing.md`) |
| Boards | imx8dxlevk, imx8qxmek (2025, the only boards in the Kconfig `choice BOARD`); jiffy (imx6ul), imxrt1060evk (2024); imx8mevk (unfinished, see F-033); imx8mpevk (2026-10, DRAM up on the board, no USB or eMMC yet); host (test only). jiffy, imx8dxlevk, imx8mevk and imx8mpevk (the last two with placeholder DDR firmware in CI) build today (F-051) |

## Security model (short)

- **Image auth.** Hash the 4 KiB bpak header with the signature zeroed, then `dsa_verify` it with the key from `rot_get_dsa_key(hdr->key_id)`. After that, the streamed payload hash must match `hdr->payload_hash`. Only ECDSA P-256/384/521 is supported.
- **Command mode auth.** Either a token (a signature over the device-UUID string) or a board password callback. It is only enforced in the `CONFIGURATION_LOCKED` SLC; open in EOL is intended (D-011).
- **No anti-rollback version counter.** Only key revocation.

## Conventions

- C99, `-ffreestanding -nostdinc`, own libc (`src/lib/libc`, from TF-A).
- Return `PB_OK` or `-PB_ERR_*` (`include/pb/errors.h`). Errors map to the wire with `error_to_wire()`.
- Logging: `LOG_ERR/WARN/INFO/DBG` gated by `CONFIG_LOGLEVEL` (`include/pb/log_def.h`).
- Headers: `include/pb/`, `include/boot/`, `include/drivers/<x>/`, `include/plat/<p>/`.
- Prefix-namespaced snake_case. SPDX BSD-3-Clause headers. Doxygen in headers.
