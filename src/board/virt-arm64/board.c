/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <drivers/partition/gpt.h>
#include <drivers/virtio/virtio_block.h>
#include <drivers/virtio/virtio_serial.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/plat.h>
#include <pb/rot.h>
// TODO: PL061?
#include <plat/virt-arm64/uart.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

#include "partitions.h"

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

static int early_boot(void)
{
    return PB_OK;
}

static int late_boot(struct bpak_header *header, uuid_t boot_part_uu)
{
    LOG_DBG("Boot!");
    return -PB_ERR; /* Should not be reached */
}

static int board_set_slc_configuration(void)
{
    return PB_OK;
}

int board_init(void)
{
    int rc;
    LOG_INFO("Board init");
    return PB_OK;
}

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    size_t resp_buf_size = *response_size;
    char *response = (char *)response_bfr;

    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xc72b6e9e) { /* test-command */
        char *arg = (char *)bfr;
        arg[size] = 0;

        LOG_DBG("test-command (%s)", arg);

        (*response_size) = snprintf(response, resp_buf_size, "Hello test-command: %s\n", arg);

        return PB_OK;
    } else if (command == 0xdfa7c4ad) {
        (*response_size) = snprintf(response, resp_buf_size, "Should return error code -128\n");
        return -128;
    } else {
        LOG_ERR("Unknown command %x", command);
        (*response_size) = 0;
    }

    return -PB_ERR;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size, "Board status OK!\n");
    return PB_OK;
}

static void p(void)
{
}

int cm_board_init(void)
{
    int rc;

    // TODO: magic const
    rc = virtio_serial_init(0x0A003C00);

    if (rc != PB_OK) {
        LOG_ERR("Virtio serial failed (%i)", rc);
        return rc;
    }

    static const struct cm_config cfg = {
        .name = "virt-arm64",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .process = p,
        .tops = {
            .init = NULL,
            .connect = NULL,
            .disconnect = NULL,
            .read = virtio_serial_async_read,
            .write = virtio_serial_async_write,
            .complete = virtio_serial_async_complete,
        },
    };

    return cm_init(&cfg);
}
