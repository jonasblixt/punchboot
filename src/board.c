/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Sets up the board from its description, 'board' (pb/board.h).
 */

#include <boot/boot.h>
#include <boot/image_regions.h>
#include <pb-tools/wire.h>
#include <pb/bio.h>
#include <pb/board.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_BOOT_DTB
#include <libfdt.h>
#endif

static int setup_partitions(void)
{
    bio_dev_t dev;
    int rc;

    if (board.ptbl.ops == NULL)
        return PB_OK;

    dev = bio_get_part_by_uu(board.ptbl.device);

    if (dev < 0) {
        LOG_ERR("No device for the partition table (%i)", dev);
        return dev;
    }

    rc = board.ptbl.ops->init(&board.ptbl, dev);
    /* The device itself is only visible, to report its capacity */
    (void)bio_set_flags(dev, BIO_FLAG_VISIBLE);

    if (rc != PB_OK) {
        LOG_WARN("No partition table (%i)", rc);
        return rc;
    }

    return PB_OK;
}

static int setup_boot_driver(void)
{
    static struct boot_driver driver;
    int rc;

    memset(&driver, 0, sizeof(driver));
    driver.default_boot_source = BOOT_SOURCE_BIO;
    driver.early_boot_cb = board.early_boot;
    driver.late_boot_cb = board.late_boot;

    driver.policy = &board.boot_policy;

    /* A handover is mandatory. Without a delivery the core jumps itself */
    if (board.boot_handover.ops == NULL || board.boot_handover.ops->init == NULL) {
        LOG_ERR("Board data: no boot_handover");
        rc = -PB_ERR_PARAM;
    } else {
        rc = board.boot_handover.ops->init(board.boot_handover.config);
    }

    if (rc == PB_OK && board.boot_delivery.ops) {
        if (board.boot_delivery.ops->init == NULL)
            rc = -PB_ERR_PARAM;
        else
            rc = board.boot_delivery.ops->init(board.boot_delivery.config);
    }

    if (rc != PB_OK) {
        LOG_ERR("Boot driver init failed (%i)", rc);
        return rc;
    }

    driver.handover = &board.boot_handover;
    driver.delivery = board.boot_delivery.ops ? &board.boot_delivery : NULL;

    return boot_init(&driver);
}

/*
 * A failed check of the board's data is a failed step (board.md 3): the
 * steps after it are skipped, and the handover is set up when its own data
 * passed, so that command mode has its boot commands.
 */
static int board_data_failed(const char *member)
{
    LOG_ERR("Board data: no %s", member);
    (void)setup_boot_driver();
    return -PB_ERR_PARAM;
}

/*
 * The board's data is checked before it is used (board.md 3): image_regions,
 * rot and slc first, boot_policy and boot_handover when their step comes, a
 * partition table's by its driver. The name and the rest of command mode's
 * are checked by cm_board_init(): nothing here uses them.
 *
 * A step that fails ends the setup (board.md 2.1): the steps after it are
 * skipped, the handover is set up all the same, so that command mode has its
 * boot commands, and the step's error is the one returned.
 */
int board_setup(void)
{
    int rc;

    /* Before any image is read, also by a boot command after a later step
     * failed. It logs which region and why; a list that fails is no list, so
     * nothing is loaded */
    rc = boot_image_set_regions(board.image_regions);

    if (rc != PB_OK) {
        (void)setup_boot_driver();
        return rc;
    }

    if (board.rot == NULL)
        return board_data_failed("rot");

    /* A board without a life cycle has an slc without read_status */
    if (board.slc == NULL)
        return board_data_failed("slc");

    /* A failed RoT or SLC fails closed (security.md 4.4), also with the
     * handover set up: the SLC isn't set up after either, so the life cycle
     * is unknown. Command mode asks for authentication and no boot is
     * started; without the RoT no image verifies */
    rc = rot_init(board.rot);

    if (rc != PB_OK) {
        LOG_ERR("RoT init failed (%i)", rc);
        (void)setup_boot_driver();
        return rc;
    }

    rc = slc_init(board.slc);

    if (rc != PB_OK) {
        LOG_ERR("SLC init failed (%i)", rc);
        (void)setup_boot_driver();
        return rc;
    }

    if (board.init) {
        rc = board.init();

        if (rc != PB_OK) {
            LOG_ERR("Board init failed (%i)", rc);
            /* The handover all the same (board.md 2.1): a boot from RAM
             * needs neither the storage nor its table */
            (void)setup_boot_driver();
            return rc;
        }
    }

    rc = setup_partitions();

    if (rc != PB_OK) {
        /* Command mode installs the table, and its boot commands go through
         * the driver: they report the policy's missing state */
        (void)setup_boot_driver();
        return rc;
    }

    if (board.boot_policy.ops == NULL)
        return board_data_failed("boot_policy");

    rc = boot_policy_init(&board.boot_policy);

    if (rc != PB_OK) {
        LOG_ERR("Boot policy init failed (%i)", rc);
        /* The handover all the same (board.md 2.1): a boot from RAM needs
         * no policy. The policy itself isn't called after its init failed:
         * A/B's functions would write its state through partitions the init
         * never looked up. Until they can be (F-178), 'boot activate'
         * answers -PB_ERR_NOT_SUPPORTED, as it did without a boot driver */
        (void)setup_boot_driver();
        boot_policy_set(NULL);
        return rc;
    }

    if (board.late_init) {
        rc = board.late_init();

        if (rc != PB_OK) {
            LOG_ERR("Board late init failed (%i)", rc);
            /* As after init(): the table and the policy are there */
            (void)setup_boot_driver();
            return rc;
        }
    }

    return setup_boot_driver();
}

#ifdef CONFIG_BOOT_DTB
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

/* The name fills the wire's board_id, which has no terminator: 1 to 16
 * characters. Only the field's size and one more byte of it are read. */
#define BOARD_ID_SIZE sizeof(((struct pb_result_device_identifier *)NULL)->board_id)

static bool name_is_valid(const char *name)
{
    return name != NULL && name[0] != '\0' && memchr(name, '\0', BOARD_ID_SIZE + 1) != NULL;
}

/*
 * Command mode's part of the board's data, checked before any of it is used
 * (board.md 3). A failed check means no command mode: cm_run() returns the
 * error and pb resets.
 */
static int cm_check_board_data(void)
{
    if (!name_is_valid(board.name)) {
        LOG_ERR("Board data: name is not 1 to %u characters", (unsigned int)BOARD_ID_SIZE);
        return -PB_ERR_PARAM;
    }

#ifdef CONFIG_CM_AUTH_PASSWORD
    if (board.password_auth == NULL) {
        LOG_ERR("Board data: no password_auth with CONFIG_CM_AUTH_PASSWORD");
        return -PB_ERR_PARAM;
    }
#endif

    /* Only that it is there: it is copied below. The transport leaves the
     * board, and this check with it (F-267) */
    if (board.cm_transport == NULL) {
        LOG_ERR("Board data: no cm_transport");
        return -PB_ERR_PARAM;
    }

    return PB_OK;
}

int cm_board_init(void)
{
    static struct cm_config cfg;
    int rc;

    rc = cm_check_board_data();

    if (rc != PB_OK)
        return rc;

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
