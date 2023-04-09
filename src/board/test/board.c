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
#include <pb/board.h>
#include <pb/fuse.h>
#include <pb/plat.h>
#include <pb/cm.h>
#include <boot/boot.h>
#include <boot/ab_state.h>
#include <boot/linux.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <drivers/block/bio.h>
#include <drivers/virtio/virtio_block.h>
#include <drivers/virtio/virtio_serial.h>
#include <drivers/partition/gpt.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <libfdt.h>
#include <uuid.h>
#include <platform_defs.h>
#include <plat/qemu/qemu.h>

#include "partitions.h"

struct fuse fuses[] =
{
    TEST_FUSE_BANK_WORD_VAL(0,  "SRK0",  0x5020C7D7),
    TEST_FUSE_BANK_WORD_VAL(1,  "SRK1",  0xBB62B945),
    TEST_FUSE_BANK_WORD_VAL(2,  "SRK2",  0xDD97C8BE),
    TEST_FUSE_BANK_WORD_VAL(3,  "SRK3",  0xDC6710DD),
    TEST_FUSE_BANK_WORD_VAL(4,  "SRK4",  0x2756B777),
    TEST_FUSE_BANK_WORD_VAL(5, "BOOT0", 0x12341234),
    TEST_FUSE_BANK_WORD_VAL(6, "BOOT1", 0xCAFEEFEE),
    TEST_FUSE_END,
};

const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

static const struct gpt_part_table gpt_tbl[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_kB(512),
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_kB(512),
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
        .size = SZ_MB(1),
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

int board_init(void)
{
    int rc;
    LOG_INFO("Board init");

    bio_dev_t disk = virtio_block_init(0x0A003C00,
                                  UUID_1eacedf3_3790_48c7_8ed8_9188ff49672b);

    if (disk < 0)
        return disk;

    rc = gpt_ptbl_init(disk, gpt_tbl, ARRAY_SIZE(gpt_tbl));

    if (rc != PB_OK) {
        LOG_WARN("GPT Init failed (%i)", rc);
    }

    bio_dev_t readable_part = bio_get_part_by_uu(UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5);
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

    rc = virtio_serial_init(0x0A003E00);

    if (rc != PB_OK) {
        LOG_ERR("Virtio serial failed (%i)", rc);
        return rc;
    }

    bio_dev_t fusebox_dev = bio_get_part_by_uu(UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95);

    if (fusebox_dev < 0)
        return fusebox_dev;

    rc = test_fuse_init(fusebox_dev);

    if (rc != PB_OK) {
        LOG_ERR("Fusebox init failed");
        return rc;
    }

    rc = mbedtls_pb_init();

    if (rc != PB_OK)
        return rc;

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

static int board_status(uint8_t *response_bfr,
                    size_t *response_size)
{
    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size,
                            "Board status OK!\n");
    return PB_OK;
}

const struct cm_config * cm_board_init(void)
{
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

    return &cfg;
}
