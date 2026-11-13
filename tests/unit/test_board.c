/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The board setup (src/board.c) with fake RoT, SLC, bio, boot module and
 * command mode, and a board whose partition table, boot policy and handover
 * are fake objects. It records the order of the calls.
 *
 * Built twice: as pb-host is, without the password method, and with it
 * (test_board_password), where the board's password_auth is mandatory.
 *
 * And the boards' password comparison (src/board/password.c).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <board/password.h>
#include <boot/boot.h>
#include <libfdt.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdio.h>
#include <string.h>

/*
 * src/board.c reads 'const struct board board', which a board defines. The
 * tests change it, so here it is defined without const: the macro renames
 * the header's declaration (and its struct tag) out of the way.
 */
#define board board_decl
#include <pb/board.h>
#undef board
struct board_decl board;

/* D-047 (F-326): a board that asks for nothing of the image's format gets
 * nothing of it. boot/boot.h, which a board includes for boot_get_flags(),
 * and pb/board.h bring no <bpak/bpak.h>, as this file's other headers */
#ifdef BPAK_HEADER_MAGIC
#error "boot/boot.h or pb/board.h includes <bpak/bpak.h>"
#endif

static const unsigned char *DISK_UU = (const unsigned char *)"disk-uuid-012345";

/* The calls, in order */
static char calls[256];

static void call(const char *name)
{
    strcat(calls, name);
    strcat(calls, " ");
}

static struct {
    int regions, rot, slc, init, part, ptbl, policy, late_init, handover, delivery, cm;
} rc;

static const struct rot_config *rot_cfg;
static const struct slc_config *slc_cfg;
static const struct ptbl *ptbl_arg;
static bio_dev_t ptbl_dev;
static const void *policy_cfg;
static const void *handover_cfg;
static const void *delivery_cfg;
static const struct boot_driver *driver;
static const struct cm_config *cm_cfg;
static uint16_t disk_flags;
static unsigned int lookups, flags_set;
static uint32_t boot_flags;
/* The board's status(): its text, its result and how often it was called */
static const char *status_text;
static int status_rc;
static unsigned int status_calls;

int rot_init(const struct rot_config *cfg)
{
    call("rot");
    rot_cfg = cfg;
    return rc.rot;
}

int slc_init(const struct slc_config *cfg)
{
    call("slc");
    slc_cfg = cfg;
    return rc.slc;
}

bio_dev_t bio_get_part_by_uu(const uuid_t uu)
{
    assert_ptr_equal(uu, DISK_UU);
    lookups++;
    return (rc.part != PB_OK) ? rc.part : 7;
}

int bio_set_flags(bio_dev_t dev, uint16_t flags)
{
    assert_int_equal(dev, 7);
    disk_flags = flags;
    flags_set++;
    return PB_OK;
}

/*
 * The partition table. It stands for a table of any kind: src/board.c knows
 * none. Like a driver it leaves the device with flags of its own.
 */
static int ptbl_init(const struct ptbl *ptbl, bio_dev_t dev)
{
    call("ptbl");
    ptbl_arg = ptbl;
    ptbl_dev = dev;
    disk_flags = BIO_FLAG_BOOTABLE | BIO_FLAG_READABLE | BIO_FLAG_WRITABLE | BIO_FLAG_VISIBLE;
    return rc.ptbl;
}

/* The boot source policy. The core's calls to it (src/boot/policy.c) are
 * linked in: board_setup() sets it up through boot_policy_init() */
static int policy_init(const struct boot_policy *policy)
{
    call("policy");
    assert_ptr_equal(policy, &board.boot_policy);
    policy_cfg = policy->config;
    return rc.policy;
}

static int policy_next(uuid_t part_uu)
{
    (void)part_uu;
    return -PB_ERR_NOT_FOUND;
}

static void policy_report(const uuid_t part_uu, int result)
{
    (void)part_uu;
    (void)result;
}

static enum boot_policy_mode policy_mode(void)
{
    return BOOT_POLICY_MODE_NORMAL;
}

static int policy_activate(uuid_t part_uu)
{
    (void)part_uu;
    return PB_OK;
}

static void policy_status(uuid_t part_uu)
{
    (void)part_uu;
}

/* The handover */
static int handover_init(const void *config)
{
    call("handover");
    handover_cfg = config;
    return rc.handover;
}

static int handover_prepare(struct bpak_header *hdr,
                            const struct boot_info *info,
                            struct boot_handover_entry *next)
{
    (void)hdr;
    (void)info;
    (void)next;
    return PB_OK;
}

/* The delivery */
static int delivery_init(const void *config)
{
    call("delivery");
    delivery_cfg = config;
    return rc.delivery;
}

static void delivery_jump(const struct boot_handover_entry *next)
{
    (void)next;
}

int boot_init(const struct boot_driver *driver_)
{
    call("boot");
    driver = driver_;
    return PB_OK;
}

uint32_t boot_get_flags(void)
{
    return boot_flags;
}

int cm_init(const struct cm_config *cfg)
{
    call("cm");
    cm_cfg = cfg;
    return rc.cm;
}

/* The board's hooks */
static int board_hw_init(void)
{
    call("init");
    return rc.init;
}

static int board_late_init(void)
{
    call("late");
    return rc.late_init;
}

static int early_boot(void)
{
    return PB_OK;
}

/* The hook's type (board.md 5.3): the partition, no image header (D-047) */
static int late_boot(uuid_t boot_part_uu)
{
    (void)boot_part_uu;
    return PB_OK;
}

#ifdef CONFIG_CM_AUTH_PASSWORD
static int password_auth(const char *password, size_t length)
{
    (void)password;
    (void)length;
    return -PB_ERR_AUTHENTICATION_FAILED;
}
#endif

static const struct rot_config test_rot = { 0 };
static const struct slc_config test_slc = { 0 };

static const struct ptbl_ops test_ptbl_ops = { .init = ptbl_init };
static const struct boot_policy_ops test_policy_ops = {
    .init = policy_init,
    .next = policy_next,
    .report = policy_report,
    .mode = policy_mode,
    .activate = policy_activate,
    .status = policy_status,
};
static const struct boot_handover_ops test_handover_ops = {
    .init = handover_init,
    .prepare = handover_prepare,
};
static const struct boot_delivery_ops test_delivery_ops = {
    .init = delivery_init,
    .jump = delivery_jump,
};
/* What the objects point at: only the addresses matter */
static const int test_table[2];
static const int test_policy_config;
static const int test_handover_config;
static const int test_delivery_config;
static const struct boot_image_region test_regions[] = { BOOT_IMAGE_REGION_END };

/* The check of the board's list (image_helpers.c, tested with it): it gets
 * the board's list, before every other step */
static const struct boot_image_region *regions_arg;
static unsigned int regions_calls;

int boot_image_set_regions(const struct boot_image_region *regions)
{
    assert_string_equal(calls, "");
    regions_arg = regions;
    regions_calls++;
    return rc.regions;
}

static int setup(void **state)
{
    (void)state;
    memset(calls, 0, sizeof(calls));
    memset(&rc, 0, sizeof(rc));
    regions_arg = NULL;
    regions_calls = 0;
    ptbl_arg = NULL;
    ptbl_dev = -1;
    policy_cfg = NULL;
    handover_cfg = NULL;
    delivery_cfg = NULL;
    driver = NULL;
    cm_cfg = NULL;
    disk_flags = 0;
    lookups = 0;
    flags_set = 0;
    boot_flags = 0;
    status_text = "";
    status_rc = PB_OK;
    status_calls = 0;

    board = (struct board_decl){
        .name = "test",
        .rot = &test_rot,
        .slc = &test_slc,
        .init = board_hw_init,
        .ptbl = { .ops = &test_ptbl_ops, .device = DISK_UU, .table = test_table, .length = 2 },
        .boot_policy = { .ops = &test_policy_ops, .config = &test_policy_config },
        .late_init = board_late_init,
        .boot_handover = { .ops = &test_handover_ops, .config = &test_handover_config },
        .image_regions = test_regions,
        .early_boot = early_boot,
        .late_boot = late_boot,
        .bootargs = "quiet",
        .bootargs_verbose = "verbose",
#ifdef CONFIG_CM_AUTH_PASSWORD
        .password_auth = password_auth,
#endif
    };
    return 0;
}

/* The boot driver boot_init() got: the board's hooks, its handover, no
 * delivery (the core jumps), and the board's policy, which has no functions
 * when the board has none */
static void assert_driver(bool policy)
{
    assert_non_null(driver);
    assert_int_equal(driver->default_boot_source, BOOT_SOURCE_BIO);
    assert_ptr_equal(driver->early_boot_cb, early_boot);
    assert_ptr_equal(driver->late_boot_cb, late_boot);
    assert_ptr_equal(driver->handover, &board.boot_handover);
    assert_null(driver->delivery);
    assert_ptr_equal(driver->policy, &board.boot_policy);

    if (policy)
        assert_ptr_equal(driver->policy->ops, &test_policy_ops);
    else
        assert_null(driver->policy->ops);
}

static void test_setup(void **state)
{
    (void)state;

    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc init ptbl policy late handover boot ");
    assert_ptr_equal(rot_cfg, &test_rot);
    assert_ptr_equal(slc_cfg, &test_slc);
    /* The image data regions are checked once, the board's list */
    assert_int_equal(regions_calls, 1);
    assert_ptr_equal(regions_arg, test_regions);

    /* The table's init gets the board's object and the device it names */
    assert_int_equal(lookups, 1);
    assert_ptr_equal(ptbl_arg, &board.ptbl);
    assert_int_equal(ptbl_dev, 7);
    /* The device with the table is only visible */
    assert_int_equal(flags_set, 1);
    assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);

    /* Each object's init gets its configuration */
    assert_ptr_equal(policy_cfg, &test_policy_config);
    assert_ptr_equal(handover_cfg, &test_handover_config);

    assert_driver(true);
}

/* The handover is the last step: when it fails there is no boot driver, and
 * its error is the one returned */
static void test_failed_handover(void **state)
{
    (void)state;

    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy late handover ");
    assert_null(driver);
}

/* board.md 2.1 (F-216): a failed RoT or SLC ends the setup like any step: the
 * steps after it are skipped, the handover is set up all the same and the
 * step's error is returned.
 *
 * It fails closed (security.md 4.4) by what isn't set up, not by a missing
 * handover: slc_init() isn't reached or didn't take the board's data, so the
 * life cycle is unknown (test_rot.c: -PB_ERR_STATE), no boot is started
 * (test_boot.c) and command mode asks for authentication (test_cm.c). */
static void test_failed_rot_or_slc_sets_up_the_handover(void **state)
{
    (void)state;

    /* The RoT: the SLC isn't set up, nor anything of the board's */
    rc.rot = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* The SLC: no init(), no table, no policy, no late_init() */
    setup(NULL);
    rc.slc = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* With a delivery: both are set up */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    rc.slc = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* The step's error is the one returned, also when the handover can't be
     * set up either */
    setup(NULL);
    rc.rot = -PB_ERR_IO;
    rc.handover = -PB_ERR_TIMEOUT;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot handover ");
    assert_null(driver);

    setup(NULL);
    rc.slc = -PB_ERR_IO;
    rc.handover = -PB_ERR_TIMEOUT;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc handover ");
    assert_null(driver);
}

/* board.md 2.1 (F-216): a failed init() or late_init() skips the steps after
 * it and sets up the handover all the same. Command mode then has its boot
 * commands: a boot from RAM needs neither the storage nor its table. The
 * hook's error is the one returned. */
static void test_failed_hook_sets_up_the_handover(void **state)
{
    (void)state;

    /* init(): no table, no policy, no late_init() */
    rc.init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(ptbl_arg);
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* late_init(): the table and the policy are set up before it */
    setup(NULL);
    rc.late_init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy late handover boot ");
    assert_ptr_equal(policy_cfg, &test_policy_config);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* With a delivery: both are set up */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    rc.init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    rc.late_init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy late handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* The hook's error is the one returned, also when the handover can't be
     * set up either */
    setup(NULL);
    rc.init = -PB_ERR_IO;
    rc.handover = -PB_ERR_TIMEOUT;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init handover ");
    assert_null(driver);

    setup(NULL);
    rc.late_init = -PB_ERR_IO;
    rc.handover = -PB_ERR_TIMEOUT;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy late handover ");
    assert_null(driver);

    /* A board without a handover (the imx8mevk, F-300) has none to set up:
     * the hook's error, and what was set up before it */
    setup(NULL);
    board.boot_handover.ops = NULL;
    rc.init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ");
    assert_null(driver);
}

/* board.md 2.1 (F-178): a boot source policy whose init fails (a partition it
 * names isn't in the table, its state can't be read or repaired) skips
 * late_init() and sets up the handover all the same: command mode boots an
 * image from RAM. The policy's error is the one returned.
 *
 * The policy stays the board's: 'boot activate' reaches it, and what it
 * answers after a failed init is its own (A/B: test_ab_state.c,
 * test_failed_init_leaves_the_functions_safe). */
static void test_failed_policy_sets_up_the_handover(void **state)
{
    (void)state;

    rc.policy = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy handover boot ");
    assert_ptr_equal(policy_cfg, &test_policy_config);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* With a delivery: both are set up */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    rc.policy = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* The policy's error is the one returned, also when the handover can't
     * be set up either */
    setup(NULL);
    rc.policy = -PB_ERR_IO;
    rc.handover = -PB_ERR_TIMEOUT;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy handover ");
    assert_null(driver);

    /* A board without a table: the same */
    setup(NULL);
    board.ptbl.ops = NULL;
    rc.policy = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init policy handover boot ");
    assert_driver(true);
}

/* Command mode installs the table, its boot commands need the driver */
static void test_no_partition_table(void **state)
{
    (void)state;

    /* The table's init fails: no policy init, no late_init, a boot driver */
    rc.ptbl = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ptbl handover boot ");
    assert_ptr_equal(ptbl_arg, &board.ptbl);
    assert_int_equal(ptbl_dev, 7);
    assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    /* The table's device does not exist: the lookup's error */
    setup(NULL);
    rc.part = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init handover boot ");
    assert_int_equal(lookups, 1);
    assert_null(ptbl_arg);
    assert_int_equal(flags_set, 0);
    assert_driver(true);

    /* The table's error is the one returned, also when the boot driver
     * cannot be set up either */
    setup(NULL);
    rc.ptbl = -PB_ERR_NOT_FOUND;
    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ptbl handover ");
    assert_null(driver);

    setup(NULL);
    rc.part = -PB_ERR_NOT_FOUND;
    board.boot_handover.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ");
    assert_null(driver);
}

/* After the table's init its device has only BIO_FLAG_VISIBLE, whatever the
 * init left there and whether or not it found a table. The same for a table
 * of any kind: src/board.c sees only the object. */
static void test_ptbl_device_flags(void **state)
{
    static const int results[] = { PB_OK, -PB_ERR_NOT_FOUND, -PB_ERR_IO };
    const struct ptbl tables[] = {
        /* The shape of a GPT: layouts and their number */
        { .ops = &test_ptbl_ops, .device = DISK_UU, .table = test_table, .length = 2 },
        /* The shape of a static table on a mapped device */
        { .ops = &test_ptbl_ops, .device = DISK_UU, .table = test_table, .base = 0x60000000 },
    };

    (void)state;

    for (size_t t = 0; t < ARRAY_SIZE(tables); t++) {
        for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
            setup(NULL);
            board.ptbl = tables[t];
            rc.ptbl = results[i];
            disk_flags = BIO_FLAG_WRITABLE;
            assert_int_equal(board_setup(), results[i]);
            assert_ptr_equal(ptbl_arg, &board.ptbl);
            assert_int_equal(flags_set, 1);
            assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);
        }
    }
}

/* No table: init() created the partitions, or there are none */
static void test_no_ptbl(void **state)
{
    (void)state;

    board.ptbl.ops = NULL;
    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc init policy late handover boot ");
    /* Nothing is looked up, no device is touched */
    assert_int_equal(lookups, 0);
    assert_int_equal(flags_set, 0);
    assert_driver(true);
}

/* The policy is mandatory, checked when its step comes: what is before it
 * is set up, late_init() isn't called, and the handover is set up all the
 * same, with a boot driver whose policy has no functions. Command mode then
 * has the table and "boot bpak". */
static void test_no_policy(void **state)
{
    (void)state;

    board.boot_policy.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl handover boot ");
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(false);

    /* With a delivery: both are set up */
    setup(NULL);
    board.boot_policy.ops = NULL;
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* The check's error is the one returned, also when the handover can't
     * be set up */
    setup(NULL);
    board.boot_policy.ops = NULL;
    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl handover ");
    assert_null(driver);

    /* A step before it that fails comes first: the table's error */
    setup(NULL);
    board.boot_policy.ops = NULL;
    rc.ptbl = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ptbl handover boot ");
    assert_driver(false);

    setup(NULL);
    board.boot_policy.ops = NULL;
    rc.init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init handover boot ");
    assert_driver(false);
}

/* Neither a policy nor a handover, with a table (the imx8mevk, F-300): the
 * RoT, the SLC, init() and the table are set up as they were before the
 * check, and no boot driver. The error is the policy's check. */
static void test_no_policy_no_handover(void **state)
{
    (void)state;

    board.boot_policy.ops = NULL;
    board.boot_handover.ops = NULL;
    board.late_init = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl ");
    assert_ptr_equal(rot_cfg, &test_rot);
    assert_ptr_equal(slc_cfg, &test_slc);
    assert_ptr_equal(ptbl_arg, &board.ptbl);
    assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);
    assert_null(driver);

    /* On a blank disk: the table's error, as before */
    setup(NULL);
    board.boot_policy.ops = NULL;
    board.boot_handover.ops = NULL;
    rc.ptbl = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ptbl ");
    assert_null(driver);
}

/* The hooks and the table are optional */
static void test_optional(void **state)
{
    (void)state;

    board.init = NULL;
    board.ptbl.ops = NULL;
    board.late_init = NULL;
    board.early_boot = NULL;
    board.late_boot = NULL;
    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc policy handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(driver->early_boot_cb);
    assert_null(driver->late_boot_cb);
    assert_ptr_equal(driver->policy, &board.boot_policy);
    assert_ptr_equal(driver->policy->ops, &test_policy_ops);
    assert_ptr_equal(driver->handover, &board.boot_handover);
    assert_null(driver->delivery);
}

/* The handover is mandatory, checked when its step comes */
static void test_no_handover(void **state)
{
    (void)state;

    board.boot_handover.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl policy late ");
    assert_null(driver);
}

/* rot and slc are mandatory, checked before anything is set up: neither
 * rot_init() nor slc_init() is called, no hook, no table, no policy. The
 * handover is set up all the same, its own data passed. */
static void test_no_rot_or_slc(void **state)
{
    (void)state;

    board.rot = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(policy_cfg);
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_driver(true);

    setup(NULL);
    board.slc = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "handover boot ");
    assert_int_equal(lookups, 0);
    assert_driver(true);

    /* With a delivery */
    setup(NULL);
    board.slc = NULL;
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* A handover whose own data didn't pass isn't set up */
    setup(NULL);
    board.rot = NULL;
    board.boot_handover.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "");
    assert_null(driver);

    /* The check's error, not the handover's */
    setup(NULL);
    board.slc = NULL;
    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "handover ");
    assert_null(driver);
}

/* The image data regions (board.md 2.9, 3) are checked before any other
 * step: a boot command after a step that failed loads into them too. A list
 * that fails is a failed step, and the handover is set up all the same */
static void test_image_regions(void **state)
{
    (void)state;

    rc.regions = -PB_ERR_PARAM;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_int_equal(regions_calls, 1);
    assert_string_equal(calls, "handover boot ");
    assert_int_equal(lookups, 0);
    assert_null(policy_cfg);
    assert_driver(true);

    /* The check's error, not the handover's */
    setup(NULL);
    rc.regions = -PB_ERR_PARAM;
    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "handover ");
    assert_null(driver);

    /* A board without a list: the check hears it, and it isn't a failure */
    setup(NULL);
    board.image_regions = NULL;
    assert_int_equal(board_setup(), PB_OK);
    assert_int_equal(regions_calls, 1);
    assert_null(regions_arg);

    /* Checked also when a step before the partitions fails */
    setup(NULL);
    rc.init = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_int_equal(regions_calls, 1);
    assert_ptr_equal(regions_arg, test_regions);

    setup(NULL);
    board.rot = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_int_equal(regions_calls, 1);
}

/* A board without a life cycle has an slc without read_status: that is
 * valid data (its command mode is open) */
static void test_slc_without_read_status(void **state)
{
    (void)state;

    assert_null(test_slc.read_status);
    assert_int_equal(board_setup(), PB_OK);
    assert_ptr_equal(slc_cfg, &test_slc);
}

/* The name is command mode's (the wire's board_id): board_setup() doesn't
 * use it, and a bad one stops neither the setup nor the boot */
static void test_setup_ignores_the_name(void **state)
{
    static const char *const names[] = { NULL, "", "17 characters....", "test" };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
        setup(NULL);
        board.name = names[i];
        assert_int_equal(board_setup(), PB_OK);
        assert_string_equal(calls, "rot slc init ptbl policy late handover boot ");
        assert_driver(true);
    }
}

/* A board with a delivery: it is set up after the handover, with the
 * board's configuration for it, and the boot driver gets both */
static void test_delivery(void **state)
{
    (void)state;

    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc init ptbl policy late handover delivery boot ");
    assert_ptr_equal(handover_cfg, &test_handover_config);
    assert_ptr_equal(delivery_cfg, &test_delivery_config);
    assert_non_null(driver);
    assert_ptr_equal(driver->handover, &board.boot_handover);
    assert_ptr_equal(driver->delivery, &board.boot_delivery);

    /* Also when the table failed: command mode's boot commands go through it */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_delivery.config = &test_delivery_config;
    rc.ptbl = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init ptbl handover delivery boot ");
    assert_ptr_equal(driver->delivery, &board.boot_delivery);
}

/* A delivery that can't be set up, or a handover: no boot driver */
static void test_delivery_failures(void **state)
{
    (void)state;

    board.boot_delivery.ops = &test_delivery_ops;
    rc.delivery = -PB_ERR_PARAM;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl policy late handover delivery ");
    assert_null(driver);

    /* The handover's failure comes first: the delivery isn't set up */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    rc.handover = -PB_ERR_IO;
    assert_int_equal(board_setup(), -PB_ERR_IO);
    assert_string_equal(calls, "rot slc init ptbl policy late handover ");
    assert_null(delivery_cfg);
    assert_null(driver);

    /* A delivery, or a handover, without an init is the board's mistake:
     * refused, not called */
    static const struct boot_delivery_ops delivery_without_init = { .jump = delivery_jump };
    static const struct boot_handover_ops handover_without_init = { .prepare = handover_prepare };

    setup(NULL);
    board.boot_delivery.ops = &delivery_without_init;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl policy late handover ");
    assert_null(driver);

    setup(NULL);
    board.boot_handover.ops = &handover_without_init;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl policy late ");
    assert_null(driver);

    /* Without a handover there is nothing to deliver */
    setup(NULL);
    board.boot_delivery.ops = &test_delivery_ops;
    board.boot_handover.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init ptbl policy late ");
    assert_null(driver);
}

static void test_patch_bootargs(void **state)
{
    uint8_t fdt[512];
    const char *bootargs;
    int offset;
    (void)state;

    /* A DTB with /chosen */
    assert_int_equal(fdt_create(fdt, sizeof(fdt)), 0);
    assert_int_equal(fdt_finish_reservemap(fdt), 0);
    assert_int_equal(fdt_begin_node(fdt, ""), 0);
    assert_int_equal(fdt_begin_node(fdt, "chosen"), 0);
    assert_int_equal(fdt_end_node(fdt), 0);
    assert_int_equal(fdt_end_node(fdt), 0);
    assert_int_equal(fdt_finish(fdt), 0);
    assert_int_equal(fdt_open_into(fdt, fdt, sizeof(fdt)), 0);
    offset = fdt_path_offset(fdt, "/chosen");
    assert_true(offset >= 0);

    assert_int_equal(board_patch_bootargs(fdt, offset), 0);
    bootargs = fdt_getprop(fdt, offset, "bootargs", NULL);
    assert_string_equal(bootargs, "quiet");

    boot_flags = BOOT_FLAG_VERBOSE;
    assert_int_equal(board_patch_bootargs(fdt, offset), 0);
    bootargs = fdt_getprop(fdt, offset, "bootargs", NULL);
    assert_string_equal(bootargs, "verbose");

    /* No verbose command line: the normal one */
    board.bootargs_verbose = NULL;
    assert_int_equal(board_patch_bootargs(fdt, offset), 0);
    bootargs = fdt_getprop(fdt, offset, "bootargs", NULL);
    assert_string_equal(bootargs, "quiet");

    /* None: the DTB's own */
    board.bootargs = NULL;
    assert_int_equal(fdt_delprop(fdt, offset, "bootargs"), 0);
    assert_int_equal(board_patch_bootargs(fdt, offset), PB_OK);
    assert_null(fdt_getprop(fdt, offset, "bootargs", NULL));
}

/* A status() as the boards write it: snprintf() into the buffer, and the
 * length it returns, which is the whole text's also when it didn't fit */
static int board_status(uint8_t *buf, size_t *length)
{
    status_calls++;
    *length = snprintf((char *)buf, *length, "%s", status_text);
    return status_rc;
}

#define STATUS_GUARD 0xa5

/* Calls command mode's status with 'size' bytes of a larger buffer, and
 * checks that nothing after them was written */
static int cm_status(uint8_t *buf, size_t buf_size, size_t size, size_t *length)
{
    int result;

    memset(buf, STATUS_GUARD, buf_size);
    *length = size;
    result = cm_cfg->status(buf, length);

    for (size_t i = size; i < buf_size; i++)
        assert_int_equal(buf[i], STATUS_GUARD);

    return result;
}

static void test_cm(void **state)
{
    uint8_t buf[64];
    size_t length = sizeof(buf);
    (void)state;

    /* The board sets up nothing for command mode (board.md 4.1): its data
     * is checked and handed to cm_init(), and no hook of the board's runs */
    assert_int_equal(cm_board_init(), PB_OK);
    assert_string_equal(calls, "cm ");
    assert_string_equal(cm_cfg->name, "test");
#ifdef CONFIG_CM_AUTH_PASSWORD
    assert_ptr_equal(cm_cfg->password_auth, password_auth);
#else
    assert_null(cm_cfg->password_auth);
#endif

    /* The defaults */
    assert_int_equal(cm_cfg->status(buf, &length), PB_OK);
    assert_string_equal((char *)buf, "Board status OK!\n");
    assert_int_equal(length, strlen("Board status OK!\n") + 1);
    length = sizeof(buf);
    assert_int_equal(cm_cfg->command(0x1234, buf, 0, buf, &length), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(length, 0);

    /* The board's: its text, and the core's NUL */
    setup(NULL);
    status_text = "Fine\n";
    board.status = board_status;
    assert_int_equal(cm_board_init(), PB_OK);
    length = sizeof(buf);
    assert_int_equal(cm_cfg->status(buf, &length), PB_OK);
    assert_int_equal(status_calls, 1);
    assert_string_equal((char *)buf, "Fine\n");
    assert_int_equal(length, strlen("Fine\n") + 1);
}

/* board.md 4.3 (F-250): the board writes at most *length - 1 bytes of text,
 * the core clamps the length and appends the NUL. A text that didn't fit is
 * cut, and nothing is written past the buffer. */
static void test_cm_status_is_clamped(void **state)
{
    static const char text[] = "Board status OK!\n";
    uint8_t buf[64];
    size_t length;

    (void)state;

    /* The default, in a buffer too small for its text */
    assert_int_equal(cm_board_init(), PB_OK);
    assert_int_equal(cm_status(buf, sizeof(buf), 8, &length), PB_OK);
    assert_int_equal(length, 8);
    assert_memory_equal(buf, "Board s", 8);

    /* A buffer the text and its NUL fill, and one that is a byte short */
    assert_int_equal(cm_status(buf, sizeof(buf), sizeof(text), &length), PB_OK);
    assert_int_equal(length, sizeof(text));
    assert_memory_equal(buf, text, sizeof(text));

    assert_int_equal(cm_status(buf, sizeof(buf), sizeof(text) - 1, &length), PB_OK);
    assert_int_equal(length, sizeof(text) - 1);
    assert_memory_equal(buf, "Board status OK!", sizeof(text) - 1);

    /* A board's status() that reports more than the buffer holds */
    setup(NULL);
    status_text = "0123456789";
    board.status = board_status;
    assert_int_equal(cm_board_init(), PB_OK);
    assert_int_equal(cm_status(buf, sizeof(buf), 8, &length), PB_OK);
    assert_int_equal(length, 8);
    assert_memory_equal(buf, "0123456", 8);

    /* One byte holds the NUL alone */
    assert_int_equal(cm_status(buf, sizeof(buf), 1, &length), PB_OK);
    assert_int_equal(length, 1);
    assert_int_equal(buf[0], 0);

    /* None has no room for it: refused, and the board isn't asked */
    status_calls = 0;
    assert_int_equal(cm_status(buf, sizeof(buf), 0, &length), -PB_ERR_PARAM);
    assert_int_equal(status_calls, 0);

    /* A status() that fails is passed on, without a NUL of the core's */
    status_text = "";
    status_rc = -PB_ERR_IO;
    assert_int_equal(cm_status(buf, sizeof(buf), 8, &length), -PB_ERR_IO);
    assert_int_equal(status_calls, 1);
    assert_int_equal(buf[1], STATUS_GUARD);
}

/* A failed check in cm_board_init() means no command mode: -PB_ERR_PARAM,
 * and cm_init() isn't called */
static void assert_no_cm(void)
{
    assert_int_equal(cm_board_init(), -PB_ERR_PARAM);
    assert_string_equal(calls, "");
    assert_null(cm_cfg);
}

/* The name is the wire's board_id, 16 bytes without a terminator: 1 to 16
 * characters */
static void test_cm_name(void **state)
{
    static const char *const valid[] = { "a", "test", "16 characters..." };
    static const char *const invalid[] = {
        NULL,
        "",
        "17 characters....",
        "a much longer name than fits",
    };

    (void)state;

    assert_int_equal(strlen(valid[2]), 16);
    assert_int_equal(strlen(invalid[2]), 17);

    for (size_t i = 0; i < ARRAY_SIZE(valid); i++) {
        setup(NULL);
        board.name = valid[i];
        assert_int_equal(cm_board_init(), PB_OK);
        assert_string_equal(calls, "cm ");
        assert_ptr_equal(cm_cfg->name, valid[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(invalid); i++) {
        setup(NULL);
        board.name = invalid[i];
        assert_no_cm();
    }
}

/* The check reads the field's 16 bytes and one more, never past them: a name
 * that ends with its array (the sanitizer build sees a longer read) */
static void test_cm_name_is_read_bounded(void **state)
{
    static const char exact[17] = "exactly sixteen.";
    static const char unterminated[17] = { 'n', 'o', ' ', 't', 'e', 'r', 'm', 'i', 'n',
                                           'a', 't', 'o', 'r', ' ', '.', '.', '.' };

    (void)state;

    board.name = exact;
    assert_int_equal(cm_board_init(), PB_OK);
    assert_ptr_equal(cm_cfg->name, exact);

    setup(NULL);
    board.name = unterminated;
    assert_no_cm();
}

/* password_auth is mandatory with CONFIG_CM_AUTH_PASSWORD, and optional
 * without the method */
static void test_cm_password_auth(void **state)
{
    (void)state;

    board.password_auth = NULL;
#ifdef CONFIG_CM_AUTH_PASSWORD
    assert_no_cm();
#else
    assert_int_equal(cm_board_init(), PB_OK);
    assert_string_equal(calls, "cm ");
    assert_null(cm_cfg->password_auth);
#endif
}

/* The boot's data isn't checked by cm_board_init(): a board that can't boot
 * still gets its command mode */
static void test_cm_after_a_failed_setup(void **state)
{
    (void)state;

    board.rot = NULL;
    board.slc = NULL;
    board.boot_policy.ops = NULL;
    board.boot_handover.ops = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "");
    assert_int_equal(cm_board_init(), PB_OK);
    assert_string_equal(calls, "cm ");
}

/* The host's password, length bytes in a buffer of its own: no NUL after it */
static int check_password(const char *expected, const char *password, size_t length)
{
    char sent[32];

    assert_true(length <= sizeof(sent));
    memcpy(sent, password, length);
    return board_password_check(expected, strlen(expected), sent, length);
}

/*
 * board.md 4.5 (F-004): the whole password, the lengths match too. The 8X
 * boards compared with strncmp(expected, password, length): any prefix of
 * the password was accepted, "i" too, and so was the password with a NUL
 * and anything after it.
 */
static void test_password_check(void **state)
{
    const char *pw = "imx8qxpmek";

    (void)state;

    assert_int_equal(check_password(pw, "imx8qxpmek", 10), PB_OK);

    /* Prefixes */
    assert_int_equal(check_password(pw, "i", 1), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qxpme", 9), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "", 0), -PB_ERR_AUTHENTICATION_FAILED);

    /* Longer */
    assert_int_equal(check_password(pw, "imx8qxpmek\0", 11), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qxpmek\0zz", 13), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qxpmekz", 11), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qxpmekimx8qxpmek", 20), -PB_ERR_AUTHENTICATION_FAILED);

    /* The same length, a byte differs: the first, one inside, the last */
    assert_int_equal(check_password(pw, "Imx8qxpmek", 10), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qXpmek", 10), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(check_password(pw, "imx8qxpmeK", 10), -PB_ERR_AUTHENTICATION_FAILED);

    /* No expected password, no password */
    assert_int_equal(board_password_check(pw, 0, "", 0), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(board_password_check(pw, 0, "i", 1), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(board_password_check(NULL, 0, pw, 10), -PB_ERR_AUTHENTICATION_FAILED);
    assert_int_equal(board_password_check(pw, 10, NULL, 10), -PB_ERR_AUTHENTICATION_FAILED);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_setup, setup),
        cmocka_unit_test_setup(test_failed_handover, setup),
        cmocka_unit_test_setup(test_failed_rot_or_slc_sets_up_the_handover, setup),
        cmocka_unit_test_setup(test_failed_hook_sets_up_the_handover, setup),
        cmocka_unit_test_setup(test_failed_policy_sets_up_the_handover, setup),
        cmocka_unit_test_setup(test_no_partition_table, setup),
        cmocka_unit_test_setup(test_ptbl_device_flags, setup),
        cmocka_unit_test_setup(test_no_ptbl, setup),
        cmocka_unit_test_setup(test_no_policy, setup),
        cmocka_unit_test_setup(test_no_policy_no_handover, setup),
        cmocka_unit_test_setup(test_optional, setup),
        cmocka_unit_test_setup(test_no_handover, setup),
        cmocka_unit_test_setup(test_no_rot_or_slc, setup),
        cmocka_unit_test_setup(test_image_regions, setup),
        cmocka_unit_test_setup(test_slc_without_read_status, setup),
        cmocka_unit_test_setup(test_setup_ignores_the_name, setup),
        cmocka_unit_test_setup(test_delivery, setup),
        cmocka_unit_test_setup(test_delivery_failures, setup),
        cmocka_unit_test_setup(test_patch_bootargs, setup),
        cmocka_unit_test_setup(test_cm, setup),
        cmocka_unit_test_setup(test_cm_status_is_clamped, setup),
        cmocka_unit_test_setup(test_cm_name, setup),
        cmocka_unit_test_setup(test_cm_name_is_read_bounded, setup),
        cmocka_unit_test_setup(test_cm_password_auth, setup),
        cmocka_unit_test_setup(test_cm_after_a_failed_setup, setup),
        cmocka_unit_test(test_password_check),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
