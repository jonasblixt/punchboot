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

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <drivers/partition/gpt.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/host/host.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>

#include "partitions.h"

/* Fuse layout of the test fuse driver, same as on the QEMU test board */
#define FUSE_SRK0   (0)
#define FUSE_SRK1   (1)
#define FUSE_SRK2   (2)
#define FUSE_SRK3   (3)
#define FUSE_SRK4   (4)
#define FUSE_BOOT0  (5)
#define FUSE_BOOT1  (6)
#define FUSE_REVOKE (8)
#define FUSE_SEC    (9)

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

static int late_boot(struct bpak_header *header, uuid_t boot_part_uu)
{
    LOG_DBG("Boot!");

    char name;

    if (uuid_compare(boot_part_uu, PART_sys_a) == 0)
        name = 'A';
    else if (uuid_compare(boot_part_uu, PART_sys_b) == 0)
        name = 'B';
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

#define SEC_CONF_LOCKED BIT(0)
#define SEC_CONF_EOL    BIT(1)

static int board_revoke_key(const struct rot_key *key)
{
    return test_fuse_write(FUSE_REVOKE, (uint32_t)(1 << key->param1));
}

static int board_read_key_status(const struct rot_key *key)
{
    int val = test_fuse_read(FUSE_REVOKE);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    if (val & (1 << key->param1))
        return -PB_ERR_KEY_REVOKED;
    return PB_OK;
}

static slc_t board_slc_read_status(void)
{
    int val = test_fuse_read(FUSE_SEC);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    if (val & SEC_CONF_EOL)
        return SLC_EOL;
    else if (val & SEC_CONF_LOCKED)
        return SLC_CONFIGURATION_LOCKED;
    else
        return SLC_CONFIGURATION;
}

static int board_slc_set_configuration_locked(void)
{
    return test_fuse_write(FUSE_SEC, SEC_CONF_LOCKED);
}

static int board_slc_set_eol(void)
{
    return test_fuse_write(FUSE_SEC, SEC_CONF_EOL);
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

    bio_dev_t disk = host_disk_init(host_opts.disk_path, SZ_MiB(32), PART_disk, "Host disk");

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
    if (readable_part >= 0)
        (void)bio_clear_set_flags(readable_part, 0, BIO_FLAG_READABLE);

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
        .image_bpak_id = 0xec103b08, /* bpak_id("kernel") */
        .dtb_bpak_id = 0,
        .ramdisk_bpak_id = 0,
        .dtb_patch_cb = NULL,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg = false,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition = boot_ab_state_set_boot_partition,
        .get_boot_partition = boot_ab_state_get_boot_partition,
        .prepare = boot_driver_linux_prepare,
        .late_boot_cb = late_boot,
        .jump = boot_driver_linux_jump,
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
        .revoke_key = board_revoke_key,
        .read_key_status = board_read_key_status,
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
        .read_status = board_slc_read_status,
        .set_configuration = board_set_slc_configuration,
        .set_configuration_locked = board_slc_set_configuration_locked,
        .set_eol = board_slc_set_eol,
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

int cm_board_init(void)
{
    static const struct cm_config cfg = {
        .name = "host test",
        .status = board_status,
        .password_auth = NULL,
        .command = board_command,
        .tops = {
            .init = host_socket_init,
            .connect = NULL,
            .disconnect = NULL,
            .read = host_socket_read,
            .write = host_socket_write,
            .complete = host_socket_complete,
        },
    };

    return cm_init(&cfg);
}
