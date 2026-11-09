/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Sets up the board from its description, 'board' (pb/board.h).
 */

#include <boot/ab_state.h>
#include <boot/armv7m_baremetal.h>
#include <boot/atf.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/partition/gpt.h>
#include <pb/bio.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdio.h>
#include <string.h>

#if defined(CONFIG_BOOT_LINUX) || defined(CONFIG_BOOT_ATF)
#include <libfdt.h>
#endif

static int setup_partitions(void)
{
#ifdef CONFIG_PARTITION_GPT
    bio_dev_t dev;
    int rc;

    if (board.gpt_dev_uu == NULL)
        return PB_OK;

    dev = bio_get_part_by_uu(board.gpt_dev_uu);

    if (dev < 0) {
        LOG_ERR("No device for the partition table (%i)", dev);
        return dev;
    }

    rc = gpt_ptbl_init(dev, board.gpt_tables, board.gpt_tables_length);
    /* The device itself is only visible, to report its capacity */
    (void)bio_set_flags(dev, BIO_FLAG_VISIBLE);

    if (rc != PB_OK) {
        LOG_WARN("No partition table (%i)", rc);
        return rc;
    }
#endif
    return PB_OK;
}

static int setup_boot_driver(void)
{
    static struct boot_driver driver;
    int rc = -PB_ERR_PARAM;

    driver.default_boot_source = BOOT_SOURCE_BIO;
    driver.early_boot_cb = board.early_boot;
    driver.late_boot_cb = board.late_boot;
#ifdef CONFIG_BOOT_AB_DRIVER
    driver.get_boot_bio_device = boot_ab_state_get;
    driver.set_boot_partition = boot_ab_state_set_boot_partition;
    driver.get_boot_partition = boot_ab_state_get_boot_partition;
#endif
#ifdef CONFIG_BOOT_LINUX
    if (board.boot_linux) {
        rc = boot_driver_linux_init(board.boot_linux);
        driver.prepare = boot_driver_linux_prepare;
        driver.jump = boot_driver_linux_jump;
    }
#endif
#ifdef CONFIG_BOOT_ATF
    if (board.boot_atf) {
        rc = boot_driver_atf_init(board.boot_atf);
        driver.prepare = boot_driver_atf_prepare;
        driver.jump = boot_driver_atf_jump;
    }
#endif
#ifdef CONFIG_BOOT_ARMV7M_BAREMETAL
    if (board.boot_armv7m) {
        rc = boot_driver_armv7m_baremetal_init(board.boot_armv7m);
        driver.prepare = boot_driver_armv7m_baremetal_prepare;
        driver.jump = boot_driver_armv7m_baremetal_jump;
    }
#endif

    if (rc != PB_OK) {
        LOG_ERR("Boot driver init failed (%i)", rc);
        return rc;
    }

    return boot_init(&driver);
}

int board_setup(void)
{
    int rc;

    rc = rot_init(board.rot);

    if (rc != PB_OK) {
        LOG_ERR("RoT init failed (%i)", rc);
        return rc;
    }

    rc = slc_init(board.slc);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        return rc;
    }

    if (board.init) {
        rc = board.init();

        if (rc != PB_OK) {
            LOG_ERR("Board init failed (%i)", rc);
            return rc;
        }
    }

    rc = setup_partitions();

    if (rc != PB_OK) {
        /* Command mode installs the table, and its boot commands go through
         * the driver: they report the missing A/B state */
        (void)setup_boot_driver();
        return rc;
    }

#ifdef CONFIG_BOOT_AB_DRIVER
    rc = boot_ab_state_init(board.ab);

    if (rc != PB_OK) {
        LOG_ERR("Boot state init failed (%i)", rc);
        return rc;
    }
#endif

    if (board.late_init) {
        rc = board.late_init();

        if (rc != PB_OK) {
            LOG_ERR("Board late init failed (%i)", rc);
            return rc;
        }
    }

    return setup_boot_driver();
}

#if defined(CONFIG_BOOT_LINUX) || defined(CONFIG_BOOT_ATF)
int board_patch_bootargs(void *fdt, int offset)
{
    const char *bootargs = board.bootargs;

    if ((boot_get_flags() & BOOT_FLAG_VERBOSE) && board.bootargs_verbose)
        bootargs = board.bootargs_verbose;

    if (bootargs == NULL)
        return PB_OK;

    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);
}
#endif

static int default_status(uint8_t *buf, size_t *length)
{
    size_t size = *length;

    *length = snprintf((char *)buf, size, "Board status OK!\n");
    buf[(*length)++] = 0;
    return PB_OK;
}

static int
default_command(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size)
{
    (void)bfr;
    (void)bfr_length;
    (void)rsp;
    LOG_ERR("Unknown command %x", cmd);
    *rsp_size = 0;
    return -PB_ERR_NOT_SUPPORTED;
}

int board_cm_setup(void)
{
    static struct cm_config cfg;
    int rc;

    if (board.cm_init) {
        rc = board.cm_init();

        if (rc != PB_OK) {
            LOG_ERR("Command mode init failed (%i)", rc);
            return rc;
        }
    }

    cfg.name = board.name;
    cfg.status = board.status ? board.status : default_status;
    cfg.command = board.command ? board.command : default_command;
    cfg.password_auth = board.password_auth;
    cfg.tops = *board.cm_transport;

    return cm_init(&cfg);
}
