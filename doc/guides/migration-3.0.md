# From 2.x to 3.0.0: a board, a driver, a platform

What an out-of-tree board changes from 2.x, and what an out-of-tree driver or platform does. A
guide: it explains and decides nothing. Each section says what it was, what it is, shows the
change in a board of the tree, and names the decision or the contract's section it rests on.

3.0.0 is a major release for the porting API (D-018). The wire protocol and the bpak format
didn't change (D-008): a board that is ported answers the hosts it answered and boots the
images it booted. Section 19 lists what to check for that.

"2.x" is the tag `v2.7.1`, the last release. The excerpts are jiffy's and the MEK's
(`src/board/jiffy/board.c`, `src/board/imx8qxmek/board.c`) at that tag and now, shortened; where
neither has the code, another board's or the header's, and the section says which.
`git show v2.7.1:src/board/jiffy/board.c` has the whole file.

| Section | Change                                                  | Rests on               |
| ------- | ------------------------------------------------------- | ---------------------- |
| 1       | The board is `const struct board board`                 | D-026, `board.md` 2    |
| 2       | The partition table, the policy and the handover are    | D-046, `board.md` 1    |
|         | objects                                                 |                        |
| 3       | TF-A is a delivery                                      | `boot.md` 5.3          |
| 4       | The generic handover is Firmware Handoff                | D-039, `boot.md` 5.4   |
| 5       | `late_boot()` gets no header                            | D-047, `board.md` 5.3  |
| 6       | In place replaces the in memory and the custom source   | D-048, D-035           |
| 7       | Image data regions                                      | D-055, `board.md` 2.9  |
| 8       | Command mode: no `cm_board_init()`, no transport        | D-049, `board.md` 4.1  |
| 9       | Named commands                                          | D-076                  |
| 10      | Tasks                                                   | D-051, `board.md` 4.5  |
| 11      | `status()` and `password_auth()`                        | `board.md` 4.3, 4.4    |
| 12      | The serial download stop                                | D-031, `board.md` 5.1  |
| 13      | The keys, the life cycle and the fuses                  | D-027                  |
| 14      | What a board includes and asks the platform             | D-056, `board.md` 6    |
| 15      | Kconfig and the defconfig, with the board's addresses   | `board.md` 7           |
| 16      | An out-of-tree driver                                   | `drivers.md`           |
| 17      | An out-of-tree platform                                 | D-030, `platform.md`   |
| 18      | `pb/dma.h`, and not in the code yet: two changes that   | D-066, F-247, F-240,   |
|         | change a board again, and one that is added             | F-269                  |
| 19      | Before the ported pb goes to a device in the field      | `board.md` 8           |

## 1. The board is `const struct board board`

**Was.** The platform called the board: `board_console_init()` and `board_init()`, with the
platform's own struct as the argument, a signature per platform. Command mode called
`cm_board_init()`. `board_init()` did everything in the board's own order, the root of trust and
the life cycle last.

**Is.** A board defines one object, `const struct board board` (`include/pb/board.h`). `main()`
calls `board.console_init()`, and `board_setup()` (`src/board.c`) runs the steps in one order
for every board: `rot` and `slc`, `init()`, `ptbl`, `boot_policy`, `late_init()`, the handover.
The hooks take no argument and are `static`. A board that needs the platform's data asks for it
(`imx6ul_plat()`, `imx8x_ipc()`, `imx8x_plat()`).

2.x:

```c
void board_console_init(struct imx6ul_platform *plat_)
...
int board_init(struct imx6ul_platform *plat_)
{
    plat = plat_;
    ...
    return imx_usdhc_init(&cfg, MHz(plat->usdhc1_clk_MHz));
```

3.0.0:

```c
static void board_console_init(void)
...
    return imx_usdhc_init(&cfg, MHz(imx6ul_plat()->usdhc1_clk_MHz));
...
const struct board board = {
    .name = "jiffy",
    .console_init = board_console_init,
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .image_regions = image_regions,
    .bootargs = "console=ttymxc1,115200 quiet ",
    .bootargs_verbose = "console=ttymxc1,115200 earlyprintk ",
    .status = board_status,
};
```

- `init()` keeps the hardware the boot needs: the storage, and for now the crypto engine
  (section 18.1). It returns an error only for what the boot can't go on without; the rest is
  logged and reported with `board_report_degraded()` (`board.md` 2.3).
- A step that fails stops the boot and not command mode (`board.md` 2.1). In 2.x a failed
  `board_init()` left command mode without a boot driver.
- `console_init()` runs before `plat_init()`: it uses nothing `plat_init()` sets up
  (`board.md` 2.2).
- The kernel command line is data: `bootargs` and `bootargs_verbose`, which the core's
  `board_patch_bootargs()` sets when the board names it as its DT hook. jiffy's own
  `board_patch_bootargs()` with `fdt_setprop_string()` is gone. A board may still give a hook of
  its own.

Rests on: D-026, D-031, `board.md` 1, 2 and 3.

## 2. The partition table, the policy and the handover are objects

**Was.** Calls from `board_init()`: `gpt_ptbl_init()`, `boot_ab_state_init()`,
`boot_driver_linux_init()`, then a `struct boot_driver` of callbacks for `boot_init()`.

**Is.** One member per concept in `struct board`, filled with an object that the module's macro
makes from the board's configuration. The core calls through it.

2.x:

```c
    rc = gpt_ptbl_init(user_part, gpt_tables, ARRAY_SIZE(gpt_tables));
    /* eMMC User partition now only has the visible flag to report capacity */
    (void)bio_set_flags(user_part, BIO_FLAG_VISIBLE);
    ...
            .rollback_mode = AB_ROLLBACK_MODE_NORMAL,
        };

        rc = boot_ab_state_init(&boot_state_cfg);
    ...
    static const struct boot_driver_linux_config linux_boot_cfg = {
        .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
        .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
        .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
        .dtb_patch_cb = board_patch_bootargs,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg = true,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);
    ...
    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition = boot_ab_state_set_boot_partition,
        .get_boot_partition = boot_ab_state_get_boot_partition,
        .prepare = boot_driver_linux_prepare,
        .late_boot_cb = NULL,
        .jump = boot_driver_linux_jump,
    };

    rc = boot_init(&boot_driver);
```

3.0.0:

```c
static const struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .set_dtb_boot_arg = true,
};
...
static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};
...
    .ptbl = PTBL_GPT(PART_user, gpt_tables),
    .boot_policy = BOOT_POLICY_AB(&boot_state_cfg),
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
```

| 2.x                                        | 3.0.0                                               |
| ------------------------------------------ | --------------------------------------------------- |
| `gpt_ptbl_init()` from `board_init()`      | `.ptbl = PTBL_GPT(device, tables)`                  |
| `static_ptbl_init()` from `board_init()`   | `.ptbl = PTBL_STATIC(device, table)`                |
| `boot_ab_state_init()`, the three A/B      | `.boot_policy = BOOT_POLICY_AB(&config)`            |
| callbacks in `struct boot_driver`          |                                                     |
| `boot_driver_linux_init()`, `prepare` and  | `.boot_handover = BOOT_HANDOVER_IMAGE(&config)`     |
| `jump` in `struct boot_driver`             |                                                     |
| `struct boot_driver_linux_config`,         | `struct boot_handover_image_config`,                |
| `<boot/linux.h>`                           | `<boot/image.h>`                                    |
| `early_boot_cb`, `late_boot_cb`            | `.early_boot`, `.late_boot` (section 5)             |
| `rollback_mode`                            | Gone: `CONFIG_BOOT_AB_SPECULATIVE` is the only      |
|                                            | switch for speculative mode (D-040, `boot.md` 3.6)  |
| `resolve_part_name`                        | Gone: the system's name is the policy's             |
|                                            | (`board.md` 2.7)                                    |

- The board doesn't take `WRITABLE` from the device its table is on: the core does
  (`board.md` 2.4). It doesn't set `BIO_FLAG_BOOTABLE` either: the policy marks its candidates
  (`board.md` 2.5).
- A macro takes its module's configuration and nothing else: another module's, or NULL, doesn't
  compile. A member the board doesn't have is left out, not set to a macro of NULL.
- The module must be in the build (section 15): a board that names one that isn't doesn't link.
- Two policies are new: `BOOT_POLICY_FIXED(part)` and `BOOT_POLICY_CHAIN(parts)`
  (`boot.md` 3.5).

Rests on: D-046, `board.md` 1, 2.4, 2.5 and 2.7.

## 3. TF-A is a delivery

**Was.** Two ways to start BL31. The MEK named BL31's part as the Linux driver's image, so pb
jumped to it with every register 0. The other was the TF-A boot driver
(`boot_driver_atf_init()`, `struct boot_driver_atf_config` in `<boot/atf.h>`), which had its
own fields for BL33, the device tree, the ramdisk and the DT hook.

**Is.** The handover says what the next stage gets, and the delivery says how it gets there
(`boot.md` 5.1). `BOOT_DELIVERY_TFA(&config)` names BL31's part and BL32's. BL33 is the
handover's entry, with the handover's arguments.

2.x, the MEK:

```c
    static const struct boot_driver_linux_config linux_boot_cfg = {
        .image_bpak_id = 0xa697d988, /* bpak_id("atf") */
        .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt")  */
        .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
        .dtb_patch_cb = patch_bootargs,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg = false,
    };
```

3.0.0:

```c
static const struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt") */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .set_dtb_boot_arg = true,
};

static const struct boot_delivery_tfa_config boot_tfa = {
    .bl31_bpak_id = 0xa697d988, /* bpak_id("atf") */
};
...
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .boot_delivery = BOOT_DELIVERY_TFA(&boot_tfa),
```

- A board that used `struct boot_driver_atf_config` moves `bl33_bpak_id`, `dtb_bpak_id`,
  `ramdisk_bpak_id` and `dtb_patch_cb` into the handover's configuration (`image_bpak_id` is
  BL33's part) and keeps `atf_bpak_id` and `bl32_bpak_id` in the delivery's, as `bl31_bpak_id`
  and `bl32_bpak_id`.
- `CONFIG_BOOT_ATF` is the delivery alone. The defconfig has the handover's option too
  (`CONFIG_BOOT_LINUX`), or the board doesn't link (`board.md` 7).
- With the delivery BL31 gets a `bl_params` list in x0, and an image needs the part the handover
  names as its entry (`boot.md` 5.3). An image that a released pb booted gets the same entry
  and arguments (`board.md` 8): a product whose BL31 was started as the MEK's was in 2.x keeps
  that with `BOOT_HANDOVER_IMAGE()` alone, BL31's part as `image_bpak_id` and
  `set_dtb_boot_arg` false. The DXL EVK in the tree moved to the delivery, and F-295 has what
  that changes for an image.
- Where the list may live is the platform's to say (`platform.md` 5.3).

Rests on: `boot.md` 5.1 and 5.3, `board.md` 2.7 and 8.

## 4. The generic handover is Firmware Handoff

**Was.** `boot_driver_armv7m_baremetal_*()` (`<boot/armv7m_baremetal.h>`,
`CONFIG_BOOT_ARMV7M_BAREMETAL`), for ARMv7-M only. It passed the image nothing. Its
configuration, from the header (the RT1060 EVK's board file of 2.x included it and called none
of it):

```c
struct boot_driver_armv7m_baremetal_config {
    uintptr_t image_base_address;
    size_t image_size;
    bpak_id_t boot_partition_id;
};
```

**Is.** `BOOT_HANDOVER_FH(&config)` (`<boot/fh.h>`, `CONFIG_BOOT_FH`), on every arch: Arm's
Firmware Handoff, a transfer list in a region the defconfig gives. The RT1060 EVK:

```c
static const struct boot_handover_fh_config boot_fh = {
    .entry_bpak_id = 0x362fa9dd, /* bpak_id("app") */
};
...
    .boot_handover = BOOT_HANDOVER_FH(&boot_fh),
```

```
CONFIG_BOOT_FH=y
CONFIG_BOOT_FH_REGION_BASE=0x20208000
CONFIG_BOOT_FH_REGION_SIZE=0x1000
```

- The entry is the part's load address, with every source. `image_base_address` and
  `image_size` are gone (`board.md` 2.4).
- The next stage gets the specification's registers and the list (`boot.md` 5.4). An image
  that ignores its registers keeps working.
- The region holds nothing else: no image data region overlaps it (section 7), and the image's
  memory map keeps it free until the list is read (`board.md` 8).
- Every arch hands over in the same state (D-028). On ARMv7-M that is new: the image starts
  with the D-cache off and PRIMASK set, and turns them on itself.
- **Not yet in the code (F-259)**: the tag of pb's structure, which the list holds when the
  board names no device tree part, isn't allocated in the specification. Until it is,
  `PB_FH_TAG_HANDOVER` is a number from the non-standard range, and it will change.

Rests on: D-028, D-039, `boot.md` 5.4, `board.md` 2.7.

## 5. `late_boot()` gets no header

**Was.** `late_boot_cb` in `struct boot_driver`, with the image's header as an argument.

**Is.** `.late_boot` in `struct board`, with the partition's UUID alone. A board that needs
the verified header asks for it: `boot_image_get_bpak_header()` (`<boot/image_helpers.h>`),
valid from prepare to the jump, so in the DT hook too. It is NULL before the payload's hash is
checked. The RT1060 EVK, the one board in the tree with the hook:

2.x:

```c
static int board_late_boot(struct bpak_header *header, uuid_t boot_part_uu)
```

3.0.0:

```c
static int board_late_boot(uuid_t boot_part_uu)
...
    .early_boot = board_early_boot,
    .late_boot = board_late_boot,
```

- `-PB_ERR_ABORT` from either hook still means "stop, enter command mode". Each stop check
  decides for `BOOT_FLAG_CMD` itself (`board.md` 5.2).
- `early_boot()` runs after the boot's preconditions (`board.md` 5.2). A check whose answer is
  there before the load belongs in it, not in `late_boot()`: a stop there comes after the
  policy wrote its state (`board.md` 5.3).
- `late_boot()` stops what the board started that must not run in the next stage
  (`board.md` 5.3).

Rests on: D-047, `board.md` 5.2 and 5.3.

## 6. In place replaces the in memory and the custom source

**Was.** `BOOT_SOURCE_IN_MEM` with the board's `get_in_mem_image()`, which handed pb a header's
address, and `BOOT_SOURCE_CUSTOM` with the board's `authenticate_image()`. From 2.x's
`struct boot_driver`:

```c
    int (*get_in_mem_image)(struct bpak_header **header);
    int (*authenticate_image)(struct bpak_header **header_ptr);
```

**Is.** Both are gone. A partition that is memory mapped is booted in place: a flag in its
entry of the board's static table, on a device the board says is mapped. No board of a SoC in
the tree has one; the host board's table (`src/board/host/board.c`):

```c
    {
        .uu = PART_nor_in_place,
        .description = "NOR In place",
        .first_lba = 8,
        .size = SZ_KiB(128),
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
```

- The table's object gets the address of the device's first byte:
  `PTBL_STATIC_MAPPED(device, base, table)`.
- The header is the partition's last 4 KiB, as for a partition that is loaded. The board gives
  no address for it.
- Every part is at its `pb-load-addr`, inside the partition's mapped range (`boot.md` 4.1).
- The image handover refuses a device tree part in place: pb grows and patches the tree where
  it is, which here is the flash (F-291, open for the decision). A kernel alone boots. A 2.x
  board that booted from memory with the Linux driver and a device tree has no equivalent
  until F-291 is decided.
- The custom source has no replacement: it was the one way around the core's verification
  (D-035).
- Where the arch has an MMU the mapped range must be in the map: the platform's, or the board's
  `mmap` (section 18.4).

Rests on: D-035, D-048, `board.md` 2.4, `boot.md` 4.1.

## 7. Image data regions

**Was.** Nothing from the board. pb refused a part that lay over pb itself.

**Is.** The board lists the only memory an image's parts are loaded into. There is no default:
a board without a list loads nothing. jiffy:

```c
#define BOARD_PB_END      ((uint64_t)CONFIG_PB_ENTRY + CONFIG_PB_SIZE)
#define BOARD_BUFFERS_END ((uint64_t)CONFIG_PB_BUFFERS_BASE + CONFIG_PB_BUFFERS_SIZE)
...
static const struct boot_image_region image_regions[] = {
    /* Between pb and its buffers */
    BOOT_IMAGE_REGION(BOARD_PB_END, CONFIG_PB_BUFFERS_BASE - BOARD_PB_END),
    BOOT_IMAGE_REGION(BOARD_BUFFERS_END, BOARD_RAM_END - BOARD_BUFFERS_END),
    BOOT_IMAGE_REGION_END,
};
...
    .image_regions = image_regions,
```

- No region holds anything of pb's: `board_setup()` checks the list against what is linked,
  before any image is read. A list that fails is a failed step with `-PB_ERR_PARAM`
  (`board.md` 3).
- Where pb runs in DRAM the regions go round it, as above. A region starts where pb's region
  or its buffers' ends, from the defconfig's values (section 15), which the link uses too: the
  link fails when pb or its buffers grow into a region.
- SoC memory that a part is linked for is named in the platform header: the MEK lists BL31's
  place as `IMX8X_ATF_BASE` and `IMX8X_ATF_SIZE` (`platform.md` 5.3).
- The list covers every load address of the product's images in the field, and is only made
  larger afterwards (`board.md` 8). The boards in the tree leave out the room next to pb and
  its buffers, which 2.x loaded into (F-392).

Rests on: D-055, `board.md` 2.9 and 3, `boot.md` 4.3.

## 8. Command mode: no `cm_board_init()`, no transport

**Was.** The board's `cm_board_init()` set up the USB port's clocks, PHY and controller and the
class, and handed command mode a `struct cm_config` with the transport's six functions.

**Is.** The board sets up nothing for command mode. The platform sets up the port in
`plat_cm_init()`, when command mode first runs, and the transport is linked. The port is an
option of the platform's in the board's defconfig.

2.x:

```c
int cm_board_init(void)
{
    int rc;
    /* Enable USB PLL */
    mmio_clrsetbits_32(IMX6UL_CCM_ANALOG_PLL_USB1, 0, CCM_ANALOG_PLL_USB1_EN_USB_CLKS);

    imx_usb2_phy_init(IMX6UL_USBPHY1_BASE);
    rc = imx_ci_udc_init(IMX6UL_USB1_BASE);
    ...
    static const struct cm_config cfg = {
        .name = "jiffy",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .tops = {
            .init = usbd_init,
            .connect = usbd_connect,
            .disconnect = usbd_disconnect,
            .read = pb_dev_cls_read,
            .write = pb_dev_cls_write,
            .complete = pb_dev_cls_xfer_complete,
        },
    };

    return cm_init(&cfg);
}
```

3.0.0, in `configs/jiffy_defconfig`:

```
CONFIG_IMX6UL_CM_USB_OTG1=y
```

- `name`, `status` and `password_auth` are members of `struct board`. `command` is section 9,
  `process` section 10.
- The options per platform are in `plat-imx.md` 3. A port without an option needs one in the
  platform first (F-402).
- A board whose command mode needs something of the PCB (a mux, a VBUS switch) has no hook for
  it: none was needed so far, and one can be added (`board.md` 4.1).
- pb has no other transport than USB (`board.md` 4.2).

Rests on: D-049, `board.md` 4.1 and 4.2, `platform.md` 3.10.

## 9. Named commands

**Was.** One function per board, `command` in `struct cm_config`, which switched on the id the
host sent.

**Is.** Each command is declared by its name with `DEFINE_COMMAND()` (`<pb/command.h>`), in the
board, a platform or a driver. The id on the wire is `pb_id()` of the name, as before, so a
command keeps its id when it is declared under the name hosts call it by. imx8mevk, whose
command is the same before and after:

2.x:

```c
static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xf93ba110) {
        ...
    } else {
        *response_size = 0;
    }

    return PB_OK;
}
...
        .command = board_command,
```

3.0.0:

```c
static int
cmd_test(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    ...
}

DEFINE_COMMAND("test", "Answers with a greeting", cmd_test);
```

- A name is 1 to 31 characters of `[a-z0-9-]` and not `list`, the help line at most 79. A bad
  list means no command mode (`command-mode.md` 5.6).
- An id the build doesn't have answers `NOT_SUPPORTED`. A 2.x board whose function answered
  OK to every id, as the one above, answers differently there.
- The function sets `*response_size` also when it fails, and writes no more than the room it
  was given.
- `cm_board_cmd_request_reboot_on_success()` is `cm_request_reset_on_success()`. A command
  that returns `-PB_ERR_ABORT` no longer resets the device: the error is the host's answer.
- jiffy's `dump-rom-log` isn't a command any more: the ROM's log is a log (`punchboot log rom`).
- A link script of the board's own keeps the section `pb_commands` (section 17).
- A released command's name, id and bytes are fixed (`board.md` 8).

Rests on: D-076, `command-mode.md` 5.6.

## 10. Tasks

**Was.** `process` in `struct cm_config`, called from command mode's loop. No board in the tree
set it. From 2.x's `<pb/cm.h>`:

```c
    void (*process)(void);
```

**Is.** `DEFINE_TASK(name, run)` (`<pb/task.h>`). The core calls every task in turn while pb
is stopped. Command mode is one of them (`src/cm/cm_main.c`):

```c
DEFINE_TASK(cm_main, cm_main_task);
```

- `run` returns `PB_OK` to be called again. An error ends the loop, and pb resets.
- A task returns quickly, takes no input from the host and relies on no order (`board.md` 4.5).
- A link script of the board's own keeps the section `pb_tasks` (section 17).

Rests on: D-051, `board.md` 4.5.

## 11. `status()` and `password_auth()`

**`status()`.** The board writes text and its length. The core clamps the length to the buffer
and appends the NUL; a board that appends one sends two. jiffy:

2.x:

```c
    response[(*response_size)++] = 0;

    return PB_OK;
```

3.0.0: the line is gone. Rests on: `board.md` 4.3.

**`password_auth()`** is mandatory with `CONFIG_CM_AUTH_PASSWORD`, and the board's data is
checked for it: a build with the option and no function has no command mode (`board.md` 3). A
2.x defconfig that had the option on without a function turns it off (jiffy's did). The
function compares the whole password, in constant time, and logs nothing. The MEK:

2.x:

```c
    LOG_DBG("Got password '%s', length = %zu", password, length);

    if (strncmp("imx8qxpmek", password, length) == 0 && length > 0)
        return PB_OK;
    else
        return -PB_ERR_AUTHENTICATION_FAILED;
```

3.0.0:

```c
    static const char expected[] = "imx8qxpmek";

    return board_password_check(expected, sizeof(expected) - 1, password, length);
```

- `board_password_check()` is `src/board/password.c`, which the board's `makefile.mk` adds
  (`src-y += src/board/password.c`).
- The 2.x code accepted any prefix of the password. A host that sent one is refused now; the
  password as released hosts send it works.
- A build with `CONFIG_CM_AUTH` selects at least one method, or it stops at an `#error` in
  `<pb/cm.h>` (`command-mode.md` 3.2).

Rests on: D-031, D-059, `board.md` 4.4.

## 12. The serial download stop

**Was.** A board that wanted command mode after the ROM's serial download read the ROM's boot
mode itself, in a boot hook.

**Is.** The core does it: with `CONFIG_CM_ON_SERIAL_DOWNLOAD` (default y) a pb that the ROM's
serial downloader loaded stops in command mode, unless command mode started the boot. The
platform reports how pb was loaded (`plat_get_boot_source()`). The MEK:

2.x:

```c
    sc_misc_get_boot_type(plat->ipc, &boot_type);

    /* Don't stop boot flow when command mode requested the boot */
    if (!(boot_get_flags() & BOOT_FLAG_CMD) && (boot_type == SC_MISC_BT_SERIAL)) {
        return -PB_ERR_ABORT;
    }
```

3.0.0: the lines are gone, `early_boot()` keeps its button.

- The option is on by default, also for a board that had no such check: jiffy stops in
  command mode after `uuu`, where 2.x booted from storage. `punchboot boot partition` or
  `boot bpak` boots from there. A product that wants the old behaviour turns the option off.
- What each port can tell is in `plat-imx.md` 3.

Rests on: D-031, `board.md` 5.1, `platform.md` 3.9.

## 13. The keys, the life cycle and the fuses

**Was.** The board had a key map (a name, an id and a fuse bit per key), the SRK hash as words
in the board file, and a function that wrote the SRK and the boot fuses.

**Is.** The core owns the rules, the board gives data.

2.x:

```c
    static const struct imx6ul_srk_fuses srk_fuses = {
        .srk[0] = 0x5020C7D7,
        ...
    rc = imx6ul_fuse_write_srk(&srk_fuses);
    ...
    rc = imx_ocotp_write(0, 5, 0x0000c060);
    ...
    static const struct rot_config rot_config = {
        .revoke_key = imx6ul_revoke_key,
        .read_key_status = imx6ul_read_key_status,
        .key_map_length = 3,
        .key_map = {
            {
                .name = "pb-development",
                .id = 0xa90f9680,
                .param1 = 0,
            },
            ...
    static const struct slc_config slc_config = {
        .read_status = imx6ul_slc_read_status,
        .set_configuration = board_set_slc_configuration,
        .set_configuration_locked = imx6ul_slc_set_configuration_locked,
        .set_eol = NULL,
    };
```

3.0.0:

```c
static const struct slc_fuse config_fuses[] = {
    { IMX6UL_FUSE(IMX6UL_FUSE_BOOT_CFG_BANK, IMX6UL_FUSE_BOOT_CFG_WORD),
      IMX6UL_BOOT_CFG_USDHC | IMX6UL_BOOT_CFG_MMC | IMX6UL_BOOT_CFG_USDHC_PORT(0) |
          IMX6UL_BOOT_CFG_BUS_WIDTH(IMX6UL_BOOT_BUS_8BIT_DDR) },
    { IMX6UL_FUSE(IMX6UL_FUSE_SEC_CONFIG_BANK, IMX6UL_FUSE_SEC_CONFIG_WORD),
      IMX6UL_FUSE_BT_FUSE_SEL },
};
...
static const struct rot_config rot = {
    .revoke_key = imx_hab_revoke_key,
    .read_key_status = imx_hab_read_key_status,
};

static const struct slc_config slc = {
    .read_status = imx_hab_slc_read_status,
    .set_configuration = imx_hab_slc_set_configuration,
    .set_configuration_locked = imx_hab_slc_set_configuration_locked,
    .fuses = config_fuses,
    .fuses_length = ARRAY_SIZE(config_fuses),
    .fuse_read = imx_ocotp_fuse_read,
    .fuse_write = imx_ocotp_fuse_write,
};
```

```
CONFIG_KEYSTORE_SLOTS="a90f9680 25c6dd36 52c1eda0"
CONFIG_SRK_FUSE="pki/imx6ul_hab_testkeys/SRK_1_2_3_4_fuse.bin"
```

- **The key map is gone.** The keys are the keystore's. `CONFIG_KEYSTORE_SLOTS` lists the key
  ids in the order of their revoke slots: write the ids of the 2.x map there in the order of
  their `param1`. Empty means the keystore's order. A key that is dropped from the keystore
  keeps its place in the list.
- **The SRK hash comes from the build**: `CONFIG_SRK_FUSE` names srktool's fuse file. No words
  are copied into the board. A wrong hash closes the device to every image.
- **The configuration fuses are data**, `slc.fuses`, by the platform's fuse index macro. The
  board writes no fuse itself (`board.md` 6). `slc_set_configuration()` sets the bits that
  aren't set yet.
- **HABv4 has one driver** (`<drivers/fuse/imx_hab_fuses.h>`): `imx6ul_revoke_key()` and the
  other `imx6ul_*` functions of the 6UL are `imx_hab_*`. The 8X keeps its own (`imx8x_*`), with
  `imx8x_slc_set_configuration` in place of the board's function.
- **`rot` and `slc` are mandatory** members (`board.md` 3). A board without a life cycle has an
  `slc` without `read_status`.
- **EOL is the product's option**: `set_eol` is called only with `CONFIG_SLC_EOL`, off by
  default (`system.md` 4). Without it `slc eol` answers `NOT_SUPPORTED`.
- **`rot_no_of_keys()` and `rot_key_idx_to_id()`** return an error code and give their value
  through a pointer: `int rot_no_of_keys(size_t *count)`,
  `int rot_key_idx_to_id(unsigned int index, key_id_t *id)`.

This wiring changes once more: section 18.1.

Rests on: D-027, `board.md` 3, 6 and 8.

## 14. What a board includes and asks the platform

**Was.** Whatever compiled: the arch's headers, the system controller's API by its own path, the
USB drivers.

**Is.** `include/pb/`, `include/drivers/`, `include/boot/` and `include/plat/<p>/`, and
`<bpak/bpak.h>` for a board that asks for the verified header. Nothing else. The MEK:

2.x:

```c
#include <arch/arch_helpers.h>
#include <arch/armv8a/timer.h>
...
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <drivers/usb/usbd.h>
#include <libfdt.h>
...
#include <pb/timestamp.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/seco/sci_seco_api.h>
```

3.0.0:

```c
#include <pb/board.h>
...
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
```

- **The 8X**: `<plat/imx8x/imx8x.h>` includes the SCFW services a board needs. A board names
  no header under `sci/` itself. The handle is `imx8x_ipc()`, where it was `plat->ipc`.
- **The arch**: what a board needs of it comes through `<pb/arch.h>`.
- **Gone**: `<pb/timestamp.h>`, `<pb/self_test.h>`, `<drivers/fs/lfs.h>`.
- **Pins** can be tables: `struct imx_pin` with `IMX6UL_PIN()`, `IMX8M_PIN()` or `IMXRT_PIN()`
  and `imx_iomuxc_setup()`; `struct imx8x_pad` and `imx8x_pads_setup()`. The boards in the tree
  use them. The i.MX8M's pins are in `pins_imx8mq.h` and `pins_imx8mp.h`, which
  `<plat/imx8m/imx8m.h>` includes; `pins.h` is gone there.

What a board asks the platform changed too:

| 2.x                                         | 3.0.0                                             |
| ------------------------------------------- | ------------------------------------------------- |
| `plat_boot_reason()`,                       | `plat_get_boot_reason(struct pb_boot_reason *)`:  |
| `plat_boot_reason_str()`                    | a kind that is the same on every platform, the    |
|                                             | SoC's raw value, a name (`platform.md` 3.4)       |
| `unsigned int plat_get_us_tick()`           | `uint32_t`; use differences only                  |
|                                             | (`platform.md` 3.2)                               |
| `struct pb_timeout` with `ts_end` and       | `ts_start` and `timeout_us`; code that only calls |
| `rollover`                                  | `pb_timeout_init_us()` and                        |
|                                             | `pb_timeout_has_expired()` doesn't change         |
| The platform's struct as an argument        | `imx6ul_plat()`, `imx8x_plat()`, `imx8x_ipc()`    |
| Nothing                                     | `plat_get_status()`: what `plat_init()` couldn't  |
|                                             | do (`platform.md` 3.8)                            |
| Nothing                                     | `board_report_degraded()`: the board's own, for   |
|                                             | what `init()` logged and didn't fail on           |
|                                             | (`board.md` 2.3)                                  |

`board_report_degraded()` has no caller in the tree yet. Its declaration (`<pb/board.h>`):

```c
void board_report_degraded(void);
```

Rests on: D-050, D-056, `board.md` 6, `platform.md` 7.

## 15. Kconfig and the defconfig

A name that is gone drops out of a defconfig without an error, and what it switched is then at
its default (`platform.md` 6.2). Compare the defconfig with the closest one in `configs/`.

| 2.x                                    | 3.0.0                                             |
| -------------------------------------- | ------------------------------------------------- |
| `PLAT_IMX8M`                           | `PLAT_IMX8MQ` or `PLAT_IMX8MP`; both select       |
|                                        | `PLAT_IMX8M_COMMON` (D-018)                       |
| `BOOT_AB_DRIVER`                       | `BOOT_POLICY_AB`. The old name still selects it   |
| `BOOT_ARMV7M_BAREMETAL`                | `BOOT_FH`, `BOOT_FH_REGION_BASE` and `_SIZE`      |
|                                        | (section 4)                                       |
| `BOOT_ATF`, as the whole driver        | `BOOT_ATF` and the handover's option (section 3)  |
| None                                   | The platform's port for command mode, such as     |
|                                        | `IMX6UL_CM_USB_OTG1` (section 8)                  |
| None                                   | `KEYSTORE_SLOTS`, `SRK_FUSE` (section 13)         |
| None                                   | `CM_ON_SERIAL_DOWNLOAD`, default y (section 12)   |
| None                                   | `SLC_EOL`, default n (section 13)                 |
| None                                   | `CM_TRANSFER_TIMEOUT`, 10 s: a transfer the host  |
|                                        | doesn't complete ends the session                 |
|                                        | (`command-mode.md` 4.6)                           |
| None                                   | `BOOT_AB_SPECULATIVE`, default n (section 2)      |
| `PB_ENTRY` in `makefile.mk`            | `PB_ENTRY`, with no default: the build stops      |
|                                        | without it                                        |
| `-DBOARD_RAM_BASE`, `-DBOARD_RAM_END`  | `BOARD_DRAM_BASE` and `BOARD_DRAM_SIZE`           |
| in `makefile.mk`                       |                                                   |
| None                                   | `PB_SIZE`: where pb's region ends, for a board    |
|                                        | with an image data region after pb                |
| None                                   | `PB_BUFFERS_BASE` and `_SIZE`, with the           |
|                                        | platform's default                                |
| `CM_AUTH_PASSWORD` without a function  | Off (section 11)                                  |
| `BIO_CORE`, `CRYPTO`, `DEVICE_UUID`,   | Gone: always built                                |
| `BOOT_CORE`, `BOOT_BPAK_IMAGE_HELPERS` |                                                   |
| `LIB_BPAK`, `LIB_UUID`, `LIB_UUID3`,   |                                                   |
| `LIB_ZLIB_CRC`                         |                                                   |
| `SELF_TEST`, `ENABLE_TIMESTAMPING`,    | Gone, with what they switched                     |
| `PRINT_TIMESTAMPS`, `NO_OF_TIMESTAMPS` |                                                   |
| `LITTLE_FS`, `STACK_USAGE`,            |                                                   |
| `IMX_USDHC_XTRA_DEBUG`,                |                                                   |
| `IMX6UL_MKIMAGE_TOOL`, `IMXRT_FLASH`   |                                                   |

- The build stops when `KCONFIG_CONFIG` names no file. 2.x built the Kconfig defaults.
- A board's own `Kconfig` is read from its directory (`board.md` 7). The MEK's `choice BOARD`
  is gone: `BOARD=<path>` on the make command line selects the board, as before.

**The board's addresses** were make variables and `-D` flags in the board's `makefile.mk`, and
the link scripts repeated them. They are Kconfig values in the defconfig, with one source for
the linker, the image recipes and C. jiffy:

2.x, `makefile.mk`:

```
PB_ENTRY     = 0x80001000

cflags-y += -DBOARD_RAM_BASE=0x80000000
cflags-y += -DBOARD_RAM_END=0xc0000000
```

3.0.0, `configs/jiffy_defconfig`:

```
CONFIG_PB_ENTRY=0x80001000
CONFIG_PB_SIZE=0x100000
CONFIG_BOARD_DRAM_BASE=0x80000000
CONFIG_BOARD_DRAM_SIZE=0x40000000
CONFIG_PB_BUFFERS_BASE=0x81021000
CONFIG_PB_BUFFERS_SIZE=0xfdf000
```

- The three lines leave `makefile.mk`, and its recipes read `$(CONFIG_PB_ENTRY)`. The rest of
  the file is as in 2.x, for now (section 18.2).
- `BOARD_RAM_BASE` and `BOARD_RAM_END` are still defined for C, by the Makefile, from the two
  DRAM values.
- The platform's link script checks the values: pb is where the ROM may load it, its buffers
  are in the board's DRAM or the SoC's on-chip RAM, and the two don't overlap. A board with a
  wrong address doesn't link, where 2.x built it.

Rests on: `board.md` 7, `platform.md` 5.1.

## 16. An out-of-tree driver

`drivers.md` is what every driver does; `bio.md`, `crypto.md` and `usb.md` are the three
classes with a contract. A driver that was written for 2.x is read against their checklists
(`drivers.md` 7). What changed in the headers:

**An MMC host controller** (`<drivers/mmc/mmc_core.h>`). The HAL is one request function, and
it carries the controller's instance:

2.x:

```c
struct mmc_hal {
    mmc_init_t init; /*!< Initilize HAL */
    mmc_send_cmd_t send_cmd; /*!< Send MMC command */
    mmc_set_bus_clock_t set_bus_clock; /*!< Set bus clock rate */
    mmc_set_bus_width_t set_bus_width; /*!< Set bus with */
    mmc_io_t prepare; /*!< Prepare DMA and start xfer, this is optional */
    mmc_io_t read; /*!< Perform read op */
    mmc_io_t write; /*!< Perform write op */
    int (*set_delay_tap)(unsigned int tap); /*!< Select bus delay tap */
    size_t max_chunk_bytes; /*!< Maximum bytes per iop */
};
```

3.0.0:

```c
struct mmc_hal {
    void *priv; /*!< The host controller instance */
    mmc_init_t init; /*!< Initilize HAL */
    mmc_request_t request; /*!< Send a command and move its data */
    mmc_set_bus_clock_t set_bus_clock; /*!< Set bus clock rate */
    mmc_set_bus_width_t set_bus_width; /*!< Set bus with */
    int (*set_delay_tap)(void *priv, unsigned int tap); /*!< Select bus delay tap */
    unsigned int delay_taps; /*!< Taps set_delay_tap() takes, 0: no tuning */
    unsigned int delay_tap_default; /*!< The board's tap, without tuning */
    bool (*card_present)(void *priv); /*!< Card detect, optional (SD) */
    size_t max_chunk_bytes; /*!< Maximum bytes per request */
    bool hs400; /*!< The host runs HS400: 8 bit DDR at 200 MHz, reads on the data strobe */
    bool hs400_es; /*!< And HS400 with Enhanced Strobe, MMC_BUS_WIDTH_8BIT_DDR_STROBE */
};
```

- `send_cmd`, `prepare`, `read` and `write` are one `request` with a `struct mmc_cmd`: the
  command, its data and whether CMD12 follows. Every function gets `priv`.
- A host that can't run HS400 leaves `hs400` and `hs400_es` false: the mode is then refused
  before the card is switched.
- A command timeout is `-PB_ERR_TIMEOUT`, not `-PB_ERR_IO`: the core tells an SD card's
  version by it (`drivers.md` 3).
- `mmc_init(hal, cfg, &dev)` gives a card, and `mmc_read()`, `mmc_write()` and the EXT_CSD
  functions take it: a build can have several. `mmc_part_switch()` and `mmc_extcsd_reload()`
  aren't in the header any more.
- A board's `struct imx_usdhc_config` has `card_detect`, and its `mmc_config` has `card`
  (`MMC_CARD_TYPE_EMMC` is 0, so a 2.x configuration means what it meant). `imx_usdhc_init()`
  is called as before.
- The i.MX8M's DRAM: the controller and the PHY are drivers in `src/drivers/ddr` (D-014), and
  the board gives its timing as `dram_timing` and `dram_rates` (`<plat/imx8m/dram.h>`,
  `src/board/imx8mevk/lpddr4_timing.c`).

**A block device** (`bio.md`):

- A driver's read and write get whole blocks only; the core owns a length that isn't
  (`bio.md` 3.1 and 3.2). A block is at most `CONFIG_BIO_MAX_BLOCK_SIZE` (`bio.md` 2.3).
- `bio_get_first_block()`, `bio_get_last_block()` and `bio_get_no_of_blocks()` return
  `int64_t`.
- A UUID that another device has is refused at registration (`-PB_ERR_PARAM`).

**A USB device controller** (`usb.md` 4): every operation of `struct usbd_hal_ops` is
mandatory, `stop` too, and `usbd_init_hal_ops()` refuses a driver that leaves one out. 2.x
checked five of the nine. A driver with one transfer for all its endpoints says so with
`usbd_hal_one_xfer()`. The platform's `plat_cm_init()` sets the driver up, not a board.

**A crypto backend** (`crypto.md`):

- An algorithm comes from one backend: a second registration of it is refused (`crypto.md` 2).
- A backend's `final` checks the room for the digest: less than the algorithm's is
  `-PB_ERR_BUF_TOO_SMALL`, and nothing is written (`crypto.md` 3.5).
- `hash_print()` is gone.
- `der_ec_public_key_data()` takes the key's length as its second argument and reads nothing
  past it.

Each of them that does DMA goes through `pb/dma.h`, and the cores refuse a buffer a device
writes that doesn't start on a cache line: section 18.3.

Rests on: D-018, D-064 to D-073, `drivers.md`, `bio.md`, `crypto.md`, `usb.md`.

## 17. An out-of-tree platform

2.x had no platform contract. `platform.md` is one, and a port is read against its checklist
(`platform.md` 8); `plat-imx.md` shows how the i.MX ports meet it. `<pb/plat.h>` from 2.x to
3.0.0:

| 2.x                                 | 3.0.0                                                 |
| ----------------------------------- | ----------------------------------------------------- |
| `plat_init()`                       | `plat_early_init()` and `plat_init()`, with the       |
|                                     | board's console between them (`platform.md` 2)        |
| `plat_board_init()`                 | Gone: the core sets the board up (section 1)          |
| `plat_boot_reason()`,               | `plat_get_boot_reason()` (`platform.md` 3.4)          |
| `plat_boot_reason_str()`            |                                                       |
| `unsigned int plat_get_us_tick()`   | `uint32_t plat_get_us_tick()`, valid from             |
|                                     | `plat_early_init()`                                   |
| `void plat_reset()`                 | `noreturn`, and it works before `plat_early_init()`   |
|                                     | (`platform.md` 3.1)                                   |
| None                                | `plat_get_status()` (`platform.md` 3.8)               |
| None                                | `plat_cm_init()` with `CONFIG_CM` (`platform.md`      |
|                                     | 3.10), and a Kconfig choice for the port              |
| None                                | `plat_lockdown()` (`platform.md` 4.4)                 |
| `plat_get_boot_source()`, optional  | Mandatory where the ROM has a serial downloader       |
|                                     | (`platform.md` 3.9)                                   |
| `plat_get_unique_id()`              | The same. A failed read returns its error and writes  |
|                                     | nothing (`platform.md` 3.5)                           |

- A failed `plat_early_init()` or `plat_init()` resets. `plat_init()` returns an error only
  for what command mode needs and records the rest in `plat_get_status()` (`platform.md` 2.2).
- **A bypassed TrustZone DRAM filter refuses every boot** (`platform.md` 2.3). The port takes
  the filter out of bypass or reports `PLAT_TZ_DRAM_NONE` or `PLAT_TZ_DRAM_LATER_STAGE`. The
  core counts a field the port leaves unset as failed.
- The platform no longer calls the board and hands it no struct: it gives an accessor
  (`platform.md` 7).
- `imx_ocotp_init(base, words_per_bank, ipg_rate_hz)` has a third argument, the controller's
  clock rate, which a write's timing is computed from. With 0 the fuses are read and a write is
  `-PB_ERR_NOT_SUPPORTED`.
- A link script of the port's or the board's own keeps the sections `pb_tasks` and
  `pb_commands` and provides their start and end, and provides `_pb_start`, `_pb_end`,
  `_buffers_start` and `_buffers_end`, as `src/link.lds` does (`platform.md` 5.1).
- The platform's `link.lds` makes `MEMORY` from the board's Kconfig values (section 15) and
  asserts where pb and its buffers lie. The RT's `MEMORY`, which was its board's, is the
  platform's.

Rests on: D-030, D-053, `platform.md`.

## 18. `pb/dma.h`, and not in the code yet

`pb/dma.h` (18.3) came into the code last, and is written as the sections above are. The other
three are changes that the contracts decided and the code doesn't have yet: 18.1 and 18.2 change
a board's files again, 18.4 is new and optional. What each of them will ask is the contract's
text today; the names can still move with the code.

### 18.1 The RoT, the SLC and the crypto engine move to the platform

**Not yet in the code (F-247)**: the board sets `rot` and `slc` to the SoC's functions, and
its `init()` sets up the crypto engine and its RNG (section 13, jiffy's `board_hw_init()`).
The change is written, on the branch `pkg-plat-rot`, and this section is rewritten with a
before and an after when it lands.

What `platform.md` 2.2, 4.1 and 4.2 ask of a board, and the branch does:

- `rot` leaves `struct board`: the platform provides `plat_rot`.
- `slc`'s functions leave it: the platform provides `plat_slc`. The board keeps its
  configuration fuses, as `slc_fuses` and `slc_fuses_length`.
- `imx_caam_init()` and `imx_caam_rng_init()` leave the board's `init()`: `plat_init()` sets up
  the SoC's crypto engine and RNG behind a Kconfig option. A board that needs the software
  implementation turns the option off in its defconfig.

### 18.2 The build's stages

**Not yet in the code (F-240)**: the boards hold the SoC's image recipes.

What `platform.md` 6.2 will ask of a board: the recipes that package and sign pb leave its
`makefile.mk`. The platform provides the stages (`<p>_image`, `<p>_sign_desc`,
`<p>_sign_image`), each behind a Kconfig option. The board sets their inputs, and overrides a
stage at one of three levels.

### 18.3 `pb/dma.h`

**Was.** A driver whose device reads or writes memory included its arch's header
(`<arch/arch.h>`) and called `arch_clean_cache_range()` and `arch_invalidate_cache_range()`
itself, each with its own alignment (64, 4096, or none) and its own order. Its descriptors and
buffers were aligned each in its own way, some next to other data. Nothing checked a caller's
buffer: a block read or a USB transfer into one that shared a cache line with other data
cleaned and invalidated the neighbour's bytes with it (F-081, F-205).

**Is.** The rule is `drivers.md` 4, and `<pb/dma.h>` is its home:

- `PB_DMA_ALIGN`, 64: the line that buffers are declared and checked with. `arch_init()` logs
  an error on a core whose D-cache line is longer.
- `PB_DMA_BUFFER(type, name)`, `PB_DMA_ARRAY(type, name, n)` and `PB_DMA_ARRAY_ALIGNED()`
  declare an object that starts on a line and is padded to whole lines, in `.no_init`: not
  initialised, and usable after `plat_init()`.
- `dma_out()` before the device reads; `dma_in_begin()` before it writes and `dma_in_end()`
  after, before the CPU reads. `dma_owns_lines()` checks a range.
- The arch's clean, invalidate, and clean and invalidate by range are declared in
  `<pb/arch.h>` on every arch, for `pb/dma.h`; a driver calls none of them.

2.x (`src/drivers/mmc/imx_usdhc.c`, a transfer through ADMA2):

```c
#include <arch/arch.h>
...
static struct usdhc_adma2_desc tbl[512] __section(".no_init") __aligned(64);
...
    if (buf && length) {
        arch_clean_cache_range(buf, length);
    }
...
    arch_clean_cache_range((uintptr_t)tbl, sizeof(struct usdhc_adma2_desc) * n_descriptors);
...
static int imx_usdhc_read(unsigned int lba, size_t length, uintptr_t buf)
{
    /* All transfers are performed with ADMA2 */
    if (length && buf) {
        arch_invalidate_cache_range(buf, length);
    }
```

3.0.0:

```c
#include <pb/dma.h>
...
typedef struct usdhc_adma2_desc usdhc_adma2_tbl[ADMA2_DESCS];
PB_DMA_ARRAY(usdhc_adma2_tbl, tbls, USDHC_MAX_INSTANCES);
...
    /* The caller's buffer: a read's owns its lines (bio.md 3.1) */
    if (write)
        dma_out(buf, length);
    else
        dma_in_begin(buf, length);
...
    dma_out(u->tbl, sizeof(u->tbl[0]) * n);
...
    /* The controller is done with a read's buffer, or was reset */
    if (data && !cmd->write)
        dma_in_end(cmd->buf, cmd->block_size * cmd->blocks);
```

- **A driver** includes `<pb/dma.h>` and no arch header, and declares its own DMA memory with
  the macros. What its device writes is begun before the device starts and ended on every
  path that ends the transfer (done, an error, a timeout, a cancel, a stop), and a copy out of
  a bounce buffer comes after the end (`drivers.md` 4.2, the checklist in 7).
- **A board**: a buffer that a device fills, a `bio_read()` or a USB OUT transfer of its own,
  starts on a cache line and owns its lines: declared with `PB_DMA_BUFFER()` or
  `PB_DMA_ARRAY()`, or with `__aligned(PB_DMA_ALIGN)` and whole lines long where it must stay
  initialised. A buffer on the stack never owns its lines. The class's core refuses one that
  doesn't start on a line with `-PB_ERR_ALIGN`, on its first read, where 2.x read into it and
  shared the line without a word. The boards of the tree read into nothing of their own.
- **An image**: nothing. A part whose load address doesn't start on a line is read a chunk at
  a time into the boot's own buffer and copied to its address, with the same reads as
  before; one that does is read straight to its address, as in 2.x. The buffer is one
  `CONFIG_BOOT_LOAD_CHUNK_kB` in `.no_init`, which `CONFIG_PB_BUFFERS_SIZE` holds.

Rests on: D-065, D-066, `drivers.md` 4, `bio.md` 3.1, `usb.md` 2.3.

### 18.4 The board's `mmap`

**Not yet in the code (F-269)**: `struct board` has no `mmap`.

What `board.md` 2.8 and `platform.md` 5.2 will ask: a board that reaches memory the platform
didn't map (a device on an external bus, a memory mapped flash) lists it as data, `mmap`, with
`PB_MAP_DEVICE()`, `PB_MAP_MEMORY_RO()` and `PB_MAP_MEMORY_RW()` from `<pb/mmu.h>`. A board that
needs no such memory doesn't change.

## 19. Before the ported pb goes to a device in the field

The port is done when the board passes `board.md` 9. A product in the field also keeps what
`board.md` 8 fixes. What the sections above can change by mistake:

- The board's `name`, the partitions' UUIDs, the GPT's layouts.
- The revoke slot of each key: `CONFIG_KEYSTORE_SLOTS` against the 2.x key map (section 13).
- The configuration fuses' words and bits, and `CONFIG_SRK_FUSE` against the SRK words of 2.x.
- The entry and the arguments the product's images get: the handover, its parts,
  `set_dtb_boot_arg`, and whether BL31 is started directly or through the delivery (section 3).
- The image data regions against every load address of the images in the field (section 7).
- The ids and the answers of the board's commands (section 9).
- The password (section 11).
- Whether the factory's flow relies on a boot from storage after a serial download
  (section 12).
- On a SoC with a TrustZone DRAM filter: that the filter is out of bypass when pb's
  preconditions are checked, or pb refuses every boot (section 17).
