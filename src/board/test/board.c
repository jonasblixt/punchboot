/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/plat.h>
#include <pb/cm.h>
#include <pb/rot.h>
#include <pb/bio.h>
#include <boot/boot.h>
#include <boot/ab_state.h>
#include <boot/linux.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <drivers/virtio/virtio_block.h>
#include <drivers/virtio/virtio_serial.h>
#include <drivers/partition/gpt.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <drivers/crypto/mbedtls.h>
#include <uuid.h>
#include <platform_defs.h>
#include <plat/qemu/qemu.h>

#include "partitions.h"

static const struct gpt_part_table gpt_tbl_default[]=
{
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
        .size = SZ_MiB(1),
    },
};

static const struct gpt_part_table gpt_tbl_var1[]=
{
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
static const struct gpt_part_table gpt_tbl_var2[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "Large",
        .size = SZ_KiB(32734),
    },
};

/* Variant 3, too large partition */
static const struct gpt_part_table gpt_tbl_var3[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "Too large",
        .size = SZ_KiB(32735),
    },
};

static const struct gpt_table_list gpt_tables[] =
{
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

static int early_boot(void)
{
    if (boot_get_flags() & BOOT_FLAG_CMD) {
        return PB_OK;
    } else {
        long fd = semihosting_file_open("/tmp/pb_force_command_mode", 0);

        LOG_DBG("Checking for /tmp/pb_force_command_mode");

        if (fd != -1) {
            semihosting_file_close(fd);
            LOG_INFO("Aborting boot");
            return -PB_ERR_ABORT;
        }
    }

    return PB_OK;
}

static int late_boot(struct bpak_header *header, uuid_t boot_part_uu)
{
    LOG_DBG("Boot!");

    long fd = semihosting_file_open("/tmp/pb_boot_status", 6);

    if (fd < 0)
        return PB_ERR;

    size_t bytes_to_write = 1;
    char name = '?';

    if (uuid_compare(boot_part_uu, PART_sys_a) == 0)
        name = 'A';
    else if (uuid_compare(boot_part_uu, PART_sys_b) == 0)
        name = 'B';
    else
        name = '?';

    semihosting_file_write(fd, &bytes_to_write,
                            (const uintptr_t) &name);

    semihosting_file_close(fd);
    plat_reset();
    return -PB_ERR; /* Should not be reached */
}

static int board_set_slc_configuration(void)
{
    int rc;

    rc = test_fuse_write(FUSE_SRK0, 0x5020C7D7);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_SRK1, 0xBB62B945);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_SRK2, 0xDD97C8BE);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_SRK3, 0xDC6710DD);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_SRK4, 0x2756B777);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_BOOT0, 0x12341234);
    if (rc != PB_OK)
        return rc;

    rc = test_fuse_write(FUSE_BOOT1, 0xCAFEEFEE);
    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

int board_init(void)
{
    int rc;
    LOG_INFO("Board init");

    bio_dev_t disk = virtio_block_init(0x0A003C00, PART_virtio_disk);

    if (disk < 0)
        return disk;

    rc = mbedtls_pb_init();

    if (rc != PB_OK)
        return rc;

    rc = gpt_ptbl_init(disk, gpt_tables, ARRAY_SIZE(gpt_tables));

    if (rc != PB_OK) {
        LOG_WARN("GPT Init failed (%i)", rc);
    }

    bio_dev_t readable_part = bio_get_part_by_uu(PART_readable);
    if (readable_part)
        (void) bio_clear_set_flags(readable_part, 0, BIO_FLAG_READABLE);

    if (rc == PB_OK) {
        static const struct boot_ab_state_config boot_state_cfg = {
            .primary_state_part_uu = PART_primary_state,
            .backup_state_part_uu = PART_backup_state,
            .sys_a_uu = PART_sys_a,
            .sys_b_uu = PART_sys_b,
            .rollback_mode = AB_ROLLBACK_MODE_NORMAL,
        };

        rc = boot_ab_state_init(&boot_state_cfg);

        if (rc != PB_OK) {
            goto err_out;
        }
    }

    static const struct boot_driver_linux_config linux_boot_cfg = {
        .image_bpak_id     = 0xec103b08,    /* bpak_id("kernel") */
        .dtb_bpak_id       = 0,
        .ramdisk_bpak_id   = 0,
        .dtb_patch_cb      = NULL,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg  = false,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb       = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition  = boot_ab_state_set_boot_partition,
        .get_boot_partition  = boot_ab_state_get_boot_partition,
        .prepare             = boot_driver_linux_prepare,
        .late_boot_cb        = late_boot,
        .jump                = boot_driver_linux_jump,
    };

    rc = boot_init(&boot_driver);

    if (rc != PB_OK) {
        goto err_out;
    }

    bio_dev_t fusebox_dev = bio_get_part_by_uu(PART_fusebox);

    if (fusebox_dev < 0)
        return fusebox_dev;

    rc = test_fuse_init(fusebox_dev);

    if (rc != PB_OK) {
        LOG_ERR("Fusebox init failed");
        return rc;
    }

    static const struct rot_config rot_config = {
        .revoke_key = qemu_revoke_key,
        .read_key_status = qemu_read_key_status,
        .key_map_length = 3,
        .key_map = {
            {
                .name = "pb-development",
                .id = 0xa90f9680,
                .param1 = 0,
            },
            {
                .name = "pb-development2",
                .id = 0x25c6dd36,
                .param1 = 1,
            },
            {
                .name = "pb-development3",
                .id = 0x52c1eda0,
                .param1 = 2,
            },
        },
    };

    rc = rot_init(&rot_config);

    if (rc != PB_OK) {
        LOG_ERR("RoT init failed (%i)", rc);
        return rc;
    }

    static const struct slc_config slc_config = {
        .read_status = qemu_slc_read_status,
        .set_configuration = board_set_slc_configuration,
        .set_configuration_locked = qemu_slc_set_configuration_locked,
        .set_eol = qemu_slc_set_eol,
    };

    rc = slc_init(&slc_config);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        return rc;
    }

    // For 'test_board_regs'
    // If bit 0 in register 0 is set, we should set bit 0 in register 3
    uint32_t board_reg;
    rc = boot_ab_state_read_board_reg(0, &board_reg);

    if (rc != PB_OK) {
        return rc;
    }

    if (board_reg & 1) {
        rc = boot_ab_state_write_board_reg(3, 1);

        if (rc != PB_OK) {
            return rc;
        }
    }

err_out:
    return rc;
}

static int board_command(uint32_t command,
                          uint8_t *bfr,
                          size_t size,
                          uint8_t *response_bfr,
                          size_t *response_size)
{
    size_t resp_buf_size = *response_size;
    char *response = (char *) response_bfr;

    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xc72b6e9e) { /* test-command */
        char *arg = (char *) bfr;
        arg[size] = 0;

        LOG_DBG("test-command (%s)", arg);

        (*response_size) = snprintf(response, resp_buf_size,
                                "Hello test-command: %s\n", arg);

        return PB_OK;
    } else if (command == 0xdfa7c4ad) {

        (*response_size) = snprintf(response, resp_buf_size,
                                "Should return error code -128\n");
        return -128;
    } else {
        LOG_ERR("Unknown command %x", command);
        (*response_size) = 0;
    }

    return -PB_ERR;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size, "Board status OK!\n");
    return PB_OK;
}

int cm_board_init(void)
{
    int rc;

    rc = virtio_serial_init(0x0A003E00);

    if (rc != PB_OK) {
        LOG_ERR("Virtio serial failed (%i)", rc);
        return rc;
    }

    static const struct cm_config cfg = {
        .name = "qemu test",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .tops = {
            .init = NULL,
            .connect = NULL,
            .disconnect = NULL,
            .read = virtio_serial_read,
            .write = virtio_serial_write,
        },
    };

    return cm_init(&cfg);
}
