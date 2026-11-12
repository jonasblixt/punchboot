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
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/boot.h>
#include <libfdt.h>
#include <pb/bio.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
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

static const unsigned char *DISK_UU = (const unsigned char *)"disk-uuid-012345";

/* The calls, in order */
static char calls[256];

static void call(const char *name)
{
    strcat(calls, name);
    strcat(calls, " ");
}

static struct {
    int rot, slc, init, part, ptbl, policy, late_init, handover, delivery, cm_init, cm;
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

static int board_cm_init(void)
{
    call("cm_init");
    return rc.cm_init;
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

static int transport_read(void *buf, size_t length)
{
    (void)buf;
    (void)length;
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
static const struct cm_transport_ops test_transport = { .read = transport_read };

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

static int setup(void **state)
{
    (void)state;
    memset(calls, 0, sizeof(calls));
    memset(&rc, 0, sizeof(rc));
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

    board = (struct board_decl){
        .name = "test",
        .rot = &test_rot,
        .slc = &test_slc,
        .init = board_hw_init,
        .ptbl = { .ops = &test_ptbl_ops, .device = DISK_UU, .table = test_table, .length = 2 },
        .boot_policy = { .ops = &test_policy_ops, .config = &test_policy_config },
        .late_init = board_late_init,
        .boot_handover = { .ops = &test_handover_ops, .config = &test_handover_config },
        .early_boot = early_boot,
        .late_boot = late_boot,
        .bootargs = "quiet",
        .bootargs_verbose = "verbose",
        .cm_init = board_cm_init,
        .cm_transport = &test_transport,
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

/* Each failure stops the setup there, the RoT and SLC are always set up.
 * None of them gets to boot_init(). */
static void test_setup_failures(void **state)
{
    static const struct {
        int *rc;
        const char *calls;
    } cases[] = {
        { &rc.rot, "rot " },
        { &rc.slc, "rot slc " },
        { &rc.init, "rot slc init " },
        { &rc.policy, "rot slc init ptbl policy " },
        { &rc.late_init, "rot slc init ptbl policy late " },
        { &rc.handover, "rot slc init ptbl policy late handover " },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        setup(NULL);
        *cases[i].rc = -PB_ERR_IO;
        assert_int_equal(board_setup(), -PB_ERR_IO);
        assert_string_equal(calls, cases[i].calls);
        assert_null(driver);
    }
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
    assert_string_equal(calls, "rot slc init ");
    assert_null(driver);
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

static int board_status(uint8_t *buf, size_t *length)
{
    (void)buf;
    *length = 0;
    return PB_OK;
}

static void test_cm(void **state)
{
    uint8_t buf[64];
    size_t length = sizeof(buf);
    (void)state;

    assert_int_equal(cm_board_init(), PB_OK);
    assert_string_equal(calls, "cm_init cm ");
    assert_string_equal(cm_cfg->name, "test");
    assert_ptr_equal(cm_cfg->tops.read, transport_read);
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

    /* The board's */
    setup(NULL);
    board.status = board_status;
    assert_int_equal(cm_board_init(), PB_OK);
    assert_ptr_equal(cm_cfg->status, board_status);

    /* A failed hook stops it */
    setup(NULL);
    rc.cm_init = -PB_ERR_IO;
    assert_int_equal(cm_board_init(), -PB_ERR_IO);
    assert_string_equal(calls, "cm_init ");

    /* No hook: optional */
    setup(NULL);
    board.cm_init = NULL;
    assert_int_equal(cm_board_init(), PB_OK);
    assert_string_equal(calls, "cm ");
}

/* A failed check in cm_board_init() means no command mode: -PB_ERR_PARAM,
 * and neither the board's hook nor cm_init() is called */
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
        assert_string_equal(calls, "cm_init cm ");
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

/* The transport is copied: checked only for being there, until it leaves the
 * board (F-267) */
static void test_cm_no_transport(void **state)
{
    (void)state;

    board.cm_transport = NULL;
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
    assert_string_equal(calls, "cm_init cm ");
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
    assert_string_equal(calls, "cm_init cm ");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_setup, setup),
        cmocka_unit_test_setup(test_setup_failures, setup),
        cmocka_unit_test_setup(test_no_partition_table, setup),
        cmocka_unit_test_setup(test_ptbl_device_flags, setup),
        cmocka_unit_test_setup(test_no_ptbl, setup),
        cmocka_unit_test_setup(test_no_policy, setup),
        cmocka_unit_test_setup(test_no_policy_no_handover, setup),
        cmocka_unit_test_setup(test_optional, setup),
        cmocka_unit_test_setup(test_no_handover, setup),
        cmocka_unit_test_setup(test_no_rot_or_slc, setup),
        cmocka_unit_test_setup(test_slc_without_read_status, setup),
        cmocka_unit_test_setup(test_setup_ignores_the_name, setup),
        cmocka_unit_test_setup(test_delivery, setup),
        cmocka_unit_test_setup(test_delivery_failures, setup),
        cmocka_unit_test_setup(test_patch_bootargs, setup),
        cmocka_unit_test_setup(test_cm, setup),
        cmocka_unit_test_setup(test_cm_name, setup),
        cmocka_unit_test_setup(test_cm_name_is_read_bounded, setup),
        cmocka_unit_test_setup(test_cm_no_transport, setup),
        cmocka_unit_test_setup(test_cm_password_auth, setup),
        cmocka_unit_test_setup(test_cm_after_a_failed_setup, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
