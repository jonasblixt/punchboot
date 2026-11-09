/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The board setup (src/board.c) with fake RoT, SLC, bio, GPT, A/B state,
 * boot driver and command mode. It records the order of the calls.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/ab_state.h>
#include <boot/boot.h>
#include <boot/linux.h>
#include <drivers/partition/gpt.h>
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
    int rot, slc, init, part, gpt, ab, late_init, linux_init, cm_init, cm;
} rc;

static const struct rot_config *rot_cfg;
static const struct slc_config *slc_cfg;
static const struct boot_driver *driver;
static const struct cm_config *cm_cfg;
static uint16_t disk_flags;
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
    return (rc.part != PB_OK) ? rc.part : 7;
}

int bio_set_flags(bio_dev_t dev, uint16_t flags)
{
    assert_int_equal(dev, 7);
    disk_flags = flags;
    return PB_OK;
}

int gpt_ptbl_init(bio_dev_t dev, const struct gpt_table_list *tables, size_t length)
{
    (void)tables;
    assert_int_equal(dev, 7);
    assert_int_equal(length, 2);
    call("gpt");
    return rc.gpt;
}

int boot_ab_state_init(const struct boot_ab_state_config *cfg)
{
    (void)cfg;
    call("ab");
    return rc.ab;
}

bio_dev_t boot_ab_state_get(void)
{
    return 0;
}

int boot_ab_state_set_boot_partition(uuid_t part_uu)
{
    (void)part_uu;
    return PB_OK;
}

void boot_ab_state_get_boot_partition(uuid_t part_uu)
{
    (void)part_uu;
}

int boot_driver_linux_init(const struct boot_driver_linux_config *cfg)
{
    (void)cfg;
    call("linux");
    return rc.linux_init;
}

int boot_driver_linux_prepare(struct bpak_header *hdr, uuid_t boot_part_uu)
{
    (void)hdr;
    (void)boot_part_uu;
    return PB_OK;
}

void boot_driver_linux_jump(void)
{
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

static int transport_read(void *buf, size_t length)
{
    (void)buf;
    (void)length;
    return PB_OK;
}

static const struct rot_config test_rot = { 0 };
static const struct slc_config test_slc = { 0 };
static const struct gpt_table_list test_tables[2];
static const struct boot_ab_state_config test_ab;
static const struct boot_driver_linux_config test_linux;
static const struct cm_transport_ops test_transport = { .read = transport_read };

static int setup(void **state)
{
    (void)state;
    memset(calls, 0, sizeof(calls));
    memset(&rc, 0, sizeof(rc));
    driver = NULL;
    cm_cfg = NULL;
    disk_flags = 0;
    boot_flags = 0;

    board = (struct board_decl){
        .name = "test",
        .rot = &test_rot,
        .slc = &test_slc,
        .init = board_hw_init,
        .gpt_dev_uu = DISK_UU,
        .gpt_tables = test_tables,
        .gpt_tables_length = 2,
        .ab = &test_ab,
        .late_init = board_late_init,
        .boot_linux = &test_linux,
        .early_boot = early_boot,
        .bootargs = "quiet",
        .bootargs_verbose = "verbose",
        .cm_init = board_cm_init,
        .cm_transport = &test_transport,
    };
    return 0;
}

static void test_setup(void **state)
{
    (void)state;

    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc init gpt ab late linux boot ");
    assert_ptr_equal(rot_cfg, &test_rot);
    assert_ptr_equal(slc_cfg, &test_slc);
    /* The device with the table is only visible */
    assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);

    assert_int_equal(driver->default_boot_source, BOOT_SOURCE_BIO);
    assert_ptr_equal(driver->early_boot_cb, early_boot);
    assert_null(driver->late_boot_cb);
    assert_ptr_equal(driver->get_boot_bio_device, boot_ab_state_get);
    assert_ptr_equal(driver->set_boot_partition, boot_ab_state_set_boot_partition);
    assert_ptr_equal(driver->get_boot_partition, boot_ab_state_get_boot_partition);
    assert_ptr_equal(driver->prepare, boot_driver_linux_prepare);
    assert_ptr_equal(driver->jump, boot_driver_linux_jump);
}

/* Each failure stops the setup there, the RoT and SLC are always set up */
static void test_setup_failures(void **state)
{
    static const struct {
        int *rc;
        const char *calls;
    } cases[] = {
        { &rc.rot, "rot " },
        { &rc.slc, "rot slc " },
        { &rc.init, "rot slc init " },
        { &rc.ab, "rot slc init gpt ab " },
        { &rc.late_init, "rot slc init gpt ab late " },
        { &rc.linux_init, "rot slc init gpt ab late linux " },
    };

    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        setup(NULL);
        *cases[i].rc = -PB_ERR_IO;
        assert_int_equal(board_setup(), -PB_ERR_IO);
        assert_string_equal(calls, cases[i].calls);
    }
}

/* Command mode installs the table, its boot commands need the driver */
static void test_no_partition_table(void **state)
{
    (void)state;

    rc.gpt = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init gpt linux boot ");
    assert_int_equal(disk_flags, BIO_FLAG_VISIBLE);

    setup(NULL);
    rc.part = -PB_ERR_NOT_FOUND;
    assert_int_equal(board_setup(), -PB_ERR_NOT_FOUND);
    assert_string_equal(calls, "rot slc init linux boot ");
}

static void test_optional(void **state)
{
    (void)state;

    board.init = NULL;
    board.gpt_dev_uu = NULL;
    board.ab = NULL;
    board.late_init = NULL;
    assert_int_equal(board_setup(), PB_OK);
    assert_string_equal(calls, "rot slc linux boot ");
    assert_null(driver->get_boot_bio_device);
    assert_null(driver->set_boot_partition);
    assert_null(driver->get_boot_partition);

    /* No boot driver */
    setup(NULL);
    board.boot_linux = NULL;
    assert_int_equal(board_setup(), -PB_ERR_PARAM);
    assert_string_equal(calls, "rot slc init gpt ab late ");
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
    assert_null(cm_cfg->password_auth);

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
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_setup, setup),
        cmocka_unit_test_setup(test_setup_failures, setup),
        cmocka_unit_test_setup(test_no_partition_table, setup),
        cmocka_unit_test_setup(test_optional, setup),
        cmocka_unit_test_setup(test_patch_bootargs, setup),
        cmocka_unit_test_setup(test_cm, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
