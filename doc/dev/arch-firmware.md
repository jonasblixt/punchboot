# Firmware architecture

## Boot flow

1. Reset entry: `src/arch/{armv8a/entry.S, armv7a/entry_armv7a.S, armv7m/entry.c}`, then `bl main`.
2. `src/main.c` `main()` runs `arch_init()`, `plat_init()`, then `plat_board_init()`, which calls `board_init(&plat)`.
   Then the optional `self_test()`, then `boot(NULL)`. On failure: "Boot aborted", then `cm_run()` (command mode), then `plat_reset()`.
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
| `slc.c` | Security life cycle. Thin dispatch to `plat/*/slc_helpers.c`, `fusebox.c`, `drivers/fuse/imx_ocotp.c` |
| `cm/cm_main.c` | Command mode: `cm_run()` loop, `pb_command_parse()` switch over `PB_CMD_*` |
| `wire.c` | Wire protocol helpers, shared with the host lib |
| `boot/ab_state.c` | A/B state (primary + backup, CRC32), attempt counters, rollback |
| `drivers/partition/gpt.c` | GPT validate/repair/install from variant list |

## Platforms and boards (last touched)

| | Status |
|---|---|
| Archs | armv7a, armv8a (2026); armv7m (2023) |
| Plats | imx6ul, imx8m (2026; `PLAT_IMX8MQ` + `PLAT_IMX8M_COMMON`, DDR init: `imx8m-ddr.md`); imx8x QXP/DXL (2026-08); imxrt (2023, stale); host (test only, `testing.md`) |
| Boards | imx8dxlevk, imx8qxmek (2025, the only boards in the Kconfig `choice BOARD`); jiffy (imx6ul), imxrt1060evk (2024); imx8mevk (unfinished, see F-033); host (test only). Only jiffy and imx8dxlevk build today (F-051) |

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
