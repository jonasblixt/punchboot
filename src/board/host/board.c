/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Test board for the host platform. Mirrors src/board/test (QEMU) so the same
 * partition layout, keys and board commands are available to the tests.
 */

#include <board_defs.h>
#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/image.h>
#include <boot/image_helpers.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <drivers/partition/gpt.h>
#include <drivers/partition/static.h>
#include <libfdt.h>
#include <pb/bio.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/command.h>
#include <pb/plat.h>
#include <pb/slc.h>
#include <plat/host/host.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

#include "partitions.h"

/* The configuration fuses of the test fuse driver's layout, same as on the
 * QEMU test board. The revocation's and the life cycle's are the platform's */
#define FUSE_SRK0  (0)
#define FUSE_SRK1  (1)
#define FUSE_SRK2  (2)
#define FUSE_SRK3  (3)
#define FUSE_SRK4  (4)
#define FUSE_BOOT0 (5)
#define FUSE_BOOT1 (6)

static const struct gpt_part_table gpt_tbl_default[] = {
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_KiB(512),
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_KiB(512),
    },
    {
        .uu = UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e,
        .description = "PB State Primary",
        .size = 512,
    },
    {
        .uu = UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee,
        .description = "PB State Backup",
        .size = 512,
    },
    {
        /* Where the fuses were until F-217, kept so that the layout stays:
         * they are a file of their own now (the host platform's) */
        .uu = UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95,
        .description = "Fuse array",
        .size = 512,
    },
    {
        .uu = UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5,
        .description = "Readable",
        .size = SZ_MiB(1),
    },
};

static const struct gpt_part_table gpt_tbl_var1[] = {
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_KiB(512),
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_KiB(512),
    },
    {
        .uu = UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e,
        .description = "PB State Primary",
        .size = 512,
    },
    {
        .uu = UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee,
        .description = "PB State Backup",
        .size = 512,
    },
    {
        .uu = UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95,
        .description = "Fuse array",
        .size = 512,
    },
    {
        .uu = UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5,
        .description = "Readable",
        .size = SZ_MiB(8),
    },
};

/* Variant 2, the disk is 32M, the largest partition
 * is 32MB - 2 * 34 512b blocks for the GPT table,
 * == 32734 kB */
static const struct gpt_part_table gpt_tbl_var2[] = {
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "Large",
        .size = SZ_KiB(32734),
    },
};

/* Variant 3, too large partition */
static const struct gpt_part_table gpt_tbl_var3[] = {
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "Too large",
        .size = SZ_KiB(32735),
    },
};

static const struct gpt_table_list gpt_tables[] = {
    {
        .name = "Default",
        .variant = 0,
        .table = gpt_tbl_default,
        .table_length = ARRAY_SIZE(gpt_tbl_default),
    },
    {
        .name = "Variant 1",
        .variant = 1,
        .table = gpt_tbl_var1,
        .table_length = ARRAY_SIZE(gpt_tbl_var1),
    },
    {
        .name = "Variant 2",
        .variant = 2,
        .table = gpt_tbl_var2,
        .table_length = ARRAY_SIZE(gpt_tbl_var2),
    },
    {
        .name = "Variant 3",
        .variant = 3,
        .table = gpt_tbl_var3,
        .table_length = ARRAY_SIZE(gpt_tbl_var3),
    },
};

/*
 * The memory mapped flash (--nor), mapped at BOARD_NOR_BASE: its first
 * partition is booted in place, the second is loaded like the disk's, the
 * third is in place and never made bootable.
 */
static const struct static_part_table nor_table[] = {
    {
        .uu = PART_nor_in_place,
        .description = "NOR In place",
        .first_lba = 8,
        .size = SZ_KiB(128),
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    {
        .uu = PART_nor_loaded,
        .description = "NOR Loaded",
        .size = SZ_KiB(128),
    },
    {
        .uu = PART_nor_not_bootable,
        .description = "NOR Not bootable",
        .size = SZ_KiB(64),
        .flags = STATIC_PART_FLAG_IN_PLACE,
    },
    {
        .uu = NULL,
    },
};

#define HOST_DT_ID 0x56f91b86 /* bpak_id("dt") */

/*
 * The board's device tree hook (boot.md 5.5): 'bootargs' is the board's. It
 * also writes a pb,version of its own, so that the tests see pb's value win:
 * pb writes its properties after the hook.
 */
static int dt_patch(void *fdt, int offset)
{
    int rc = fdt_setprop_string(fdt, offset, "bootargs", "console=pb-host");

    if (rc == 0)
        rc = fdt_setprop_string(fdt, offset, "pb,version", "the board's");

    return (rc == 0) ? PB_OK : -PB_ERR;
}

/*
 * The image handover. With --dt it also names a device tree part, which then
 * every image needs, and which an image in place can't have (F-291): the in
 * place tests run without it. Set in init(), before the handover's init
 */
static struct boot_handover_image_config boot_image = {
    .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_patch_cb = dt_patch,
};

/*
 * For the tests of a stop in a boot that command mode started (board.md
 * 5.4): the named command 'test-boot-stop' arms one, in the hook and with
 * the error that its request names. It stops the next boot and no other.
 */
static int early_boot_stop;
static int late_boot_stop;

static const struct {
    const char *request;
    int *stop;
    int rc;
} boot_stops[] = {
    { "early:abort", &early_boot_stop, -PB_ERR_ABORT },
    { "early:fail", &early_boot_stop, -PB_ERR_IO },
    { "late:abort", &late_boot_stop, -PB_ERR_ABORT },
    { "late:fail", &late_boot_stop, -PB_ERR_IO },
};

static int boot_stop_take(int *stop, const char *hook)
{
    int rc = *stop;

    *stop = PB_OK;

    if (rc != PB_OK)
        LOG_INFO("Stopping the boot in %s (%i)", hook, rc);

    return rc;
}

static int early_boot(void)
{
    int rc = boot_stop_take(&early_boot_stop, "early_boot");

    if (rc != PB_OK)
        return rc;

    if (boot_get_flags() & BOOT_FLAG_CMD) {
        return PB_OK;
    } else {
        if (host_opts.force_command_mode) {
            LOG_INFO("Aborting boot");
            return -PB_ERR_ABORT;
        }
    }

    return PB_OK;
}

/*
 * With --dt the image handover names a device tree part, and the boot writes
 * the tree it prepared to the file: what the next stage would read, pb's
 * /chosen with it (boot.md 5.5, 5.6). The tree is where the handover left
 * it, at the part's load address in the verified header.
 */
static int write_dt(void)
{
    const struct bpak_header *hdr = boot_image_get_bpak_header();
    uint64_t addr;
    const void *fdt;
    size_t length;
    FILE *fp;

    if (host_opts.dt_path == NULL)
        return PB_OK;

    if (hdr == NULL || boot_image_load_addr((struct bpak_header *)hdr, HOST_DT_ID, &addr) != PB_OK)
        return -PB_ERR_STATE;

    fdt = (const void *)(uintptr_t)addr;

    if (fdt_check_header(fdt) != 0)
        return -PB_ERR_STATE;

    length = fdt_totalsize(fdt);
    fp = fopen(host_opts.dt_path, "wb");

    if (fp == NULL)
        return -PB_ERR_IO;

    if (fwrite(fdt, 1, length, fp) != length) {
        fclose(fp);
        return -PB_ERR_IO;
    }

    return (fclose(fp) == 0) ? PB_OK : -PB_ERR_IO;
}

static int late_boot(uuid_t boot_part_uu)
{
    int rc = boot_stop_take(&late_boot_stop, "late_boot");

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Boot!");

    rc = write_dt();

    if (rc != PB_OK) {
        LOG_ERR("Could not write the device tree (%i)", rc);
        return rc;
    }

    char name;

    if (uuid_compare(boot_part_uu, PART_sys_a) == 0)
        name = 'A';
    else if (uuid_compare(boot_part_uu, PART_sys_b) == 0)
        name = 'B';
    else if (uuid_compare(boot_part_uu, PART_nor_in_place) == 0)
        name = 'X';
    else if (uuid_compare(boot_part_uu, PART_nor_loaded) == 0)
        name = 'N';
    else
        name = '?';

    FILE *fp = fopen(host_opts.boot_status_path, "w");

    if (fp == NULL)
        return -PB_ERR_IO;

    fputc(name, fp);
    fclose(fp);
    plat_reset();
    return -PB_ERR; /* Should not be reached */
}

/* The configuration: a made up SRK hash and boot configuration */
static const struct slc_fuse config_fuses[] = {
    { FUSE_SRK0, 0x5020C7D7 },  { FUSE_SRK1, 0xBB62B945 }, { FUSE_SRK2, 0xDD97C8BE },
    { FUSE_SRK3, 0xDC6710DD },  { FUSE_SRK4, 0x2756B777 }, { FUSE_BOOT0, 0x12341234 },
    { FUSE_BOOT1, 0xCAFEEFEE },
};

static int nor_init(void)
{
    int rc;
    bio_dev_t nor = host_nor_init(PART_nor, "Host NOR");

    if (nor < 0)
        return nor;

    rc = static_ptbl_init_mapped(nor, BOARD_NOR_BASE, nor_table);
    /* As the core does for its table's device: a host writes the partitions */
    (void)bio_set_flags(nor, BIO_FLAG_VISIBLE);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

/*
 * The storage. The crypto backend is the platform's, set up before this
 * (platform.md 2.2): a failed init() leaves what verifies a RAM boot
 * (board.md 2.1, F-394)
 */
static int board_hw_init(void)
{
    /* Before anything that can fail: the handover is set up also after a
     * failed init() (board.md 2.1) */
    if (host_opts.dt_path != NULL)
        boot_image.dtb_bpak_id = HOST_DT_ID;

    bio_dev_t disk = host_disk_init(host_opts.disk_path, SZ_MiB(32), PART_disk, "Host disk");

    if (disk < 0)
        return disk;

    if (host_opts.nor_path != NULL)
        return nor_init();

    return PB_OK;
}

static int board_late_init(void)
{
    int rc;
    bio_dev_t readable_part = bio_get_part_by_uu(PART_readable);

    if (readable_part >= 0)
        (void)bio_clear_set_flags(readable_part, 0, BIO_FLAG_READABLE);

    // For 'test_board_regs'
    // If bit 0 in register 0 is set, we should set bit 0 in register 3
    uint32_t board_reg;
    rc = boot_ab_state_read_board_reg(0, &board_reg);

    if (rc != PB_OK)
        return rc;

    if (board_reg & 1)
        return boot_ab_state_write_board_reg(3, 1);

    return PB_OK;
}

/* snprintf() returns the length of the whole text, also when only room - 1
 * bytes of it and the NUL fit: the size is what it wrote */
static size_t written(int n, size_t room)
{
    if (n < 0)
        return 0;

    return ((size_t)n < room) ? (size_t)n : room;
}

/* The board's named commands (command-mode.md 5.6). Their ids are those
 * the board answered before 3.0.0, pb_id() of each name */

static int cmd_test_command(const uint8_t *request,
                            size_t request_size,
                            uint8_t *response,
                            size_t *response_size)
{
    size_t room = *response_size;
    /* The request isn't terminated: at most its size, up to a NUL. The core
     * refuses one larger than 1024 bytes */
    const char *arg = (const char *)request;
    int arg_length = (int)request_size;

    LOG_DBG("test-command (%.*s)", arg_length, arg);

    *response_size = written(
        snprintf((char *)response, room, "Hello test-command: %.*s\n", arg_length, arg), room);

    return PB_OK;
}

DEFINE_COMMAND("test-command", "Echoes its request", cmd_test_command);

static int cmd_test_command2(const uint8_t *request,
                             size_t request_size,
                             uint8_t *response,
                             size_t *response_size)
{
    size_t room = *response_size;

    (void)request;
    (void)request_size;

    *response_size =
        written(snprintf((char *)response, room, "Should return error code -128\n"), room);
    return -128;
}

DEFINE_COMMAND("test-command2", "Fails with -128", cmd_test_command2);

static int cmd_test_boot_stop(const uint8_t *request,
                              size_t request_size,
                              uint8_t *response,
                              size_t *response_size)
{
    (void)response;
    *response_size = 0;

    for (size_t i = 0; i < ARRAY_SIZE(boot_stops); i++) {
        if (request_size == strlen(boot_stops[i].request) &&
            memcmp(request, boot_stops[i].request, request_size) == 0) {
            *boot_stops[i].stop = boot_stops[i].rc;
            return PB_OK;
        }
    }

    return -PB_ERR_PARAM;
}

DEFINE_COMMAND("test-boot-stop",
               "Stops the next boot: early:abort, early:fail, late:abort or late:fail",
               cmd_test_boot_stop);

static const struct boot_ab_state_config boot_state_cfg = {
    .primary_state_part_uu = PART_primary_state,
    .backup_state_part_uu = PART_backup_state,
    .sys_a_uu = PART_sys_a,
    .sys_b_uu = PART_sys_b,
};

/*
 * The tests' boot source policy: A/B on the disk, and with a flash (--nor)
 * its two bootable partitions as well. A policy marks what can be booted
 * (boot.md 3.1): the flash's partitions are candidates that the policy
 * never hands out, so only 'boot part' boots them.
 */
static int host_policy_init(const struct boot_policy *policy)
{
    int rc = boot_policy_ab_ops.init(policy);
    bio_dev_t dev;

    dev = bio_get_part_by_uu(PART_nor_in_place);

    if (dev >= 0)
        (void)bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

    dev = bio_get_part_by_uu(PART_nor_loaded);

    if (dev >= 0)
        (void)bio_clear_set_flags(dev, 0, BIO_FLAG_BOOTABLE);

    return rc;
}

static int host_policy_next(uuid_t part_uu)
{
    return boot_policy_ab_ops.next(part_uu);
}

static void host_policy_report(const uuid_t part_uu, int result)
{
    boot_policy_ab_ops.report(part_uu, result);
}

static enum boot_policy_mode host_policy_mode(void)
{
    return boot_policy_ab_ops.mode();
}

static int host_policy_activate(uuid_t part_uu)
{
    return boot_policy_ab_ops.activate(part_uu);
}

static void host_policy_status(uuid_t part_uu)
{
    boot_policy_ab_ops.status(part_uu);
}

static const struct boot_policy_ops host_policy_ops = {
    .init = host_policy_init,
    .next = host_policy_next,
    .report = host_policy_report,
    .mode = host_policy_mode,
    .activate = host_policy_activate,
    .status = host_policy_status,
    .system_name = boot_ab_part_uu_to_name,
};

/*
 * Where an image's parts may be loaded (board.md 2.9): the emulated RAM above
 * the fake pb at its start, 'pb' and 'buffers' to 0x40120000 (CMakeLists.txt).
 * The flash is booted in place and needs no region.
 */
#define HOST_PB_SIZE 0x120000

static const struct boot_image_region image_regions[] = {
    BOOT_IMAGE_REGION(BOARD_RAM_BASE + HOST_PB_SIZE, BOARD_RAM_SIZE - HOST_PB_SIZE),
    BOOT_IMAGE_REGION_END,
};

const struct board board = {
    .name = "host test",
    .slc_fuses = config_fuses,
    .slc_fuses_length = ARRAY_SIZE(config_fuses),
    .init = board_hw_init,
    .ptbl = PTBL_GPT(PART_disk, gpt_tables),
    .boot_policy = { .ops = &host_policy_ops, .config = &boot_state_cfg },
    .late_init = board_late_init,
    .boot_handover = BOOT_HANDOVER_IMAGE(&boot_image),
    .image_regions = image_regions,
    .early_boot = early_boot,
    .late_boot = late_boot,
};
