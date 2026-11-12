/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The generic handover (src/boot/simple.c): the entry is the load address
 * of the board's entry part, the address pb hashed it at, whether the image
 * was loaded or is booted in place (F-219).
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/simple.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#define ID_APP     0x362fa9dd /* bpak_id("app") */
#define ID_OTHER   0xec103b08

/* A load address in RAM and one in a memory mapped flash: the handover
 * doesn't tell them apart */
#define APP_ADDR   0x20210000
#define FLASH_ADDR 0x60080000

/* The fakes */
static int name_calls;
static uuid_t name_uu;
/* What prepare gives the delivery, and the same as arch_jump() will get it:
 * the entry, then the four arguments. The jump itself is the core's
 * (test_boot.c) */
static struct boot_handover_entry next;
static void *jump_args[5];

static const char *part_name(uuid_t part_uu)
{
    name_calls++;
    uuid_copy(name_uu, part_uu);
    return "A";
}

static const struct boot_driver_simple_config cfg_named = {
    .boot_partition_id = ID_APP,
    .resolve_part_name = part_name,
};

static const struct boot_driver_simple_config cfg_no_name = {
    .boot_partition_id = ID_APP,
};

/* The board's object for the handover, the way a board has it in its data
 * (board.boot_handover, F-265): BOOT_HANDOVER_SIMPLE() stays a constant
 * initializer */
static const struct boot_handover handover = BOOT_HANDOVER_SIMPLE(&cfg_named);

static struct bpak_header *hdr;
static uuid_t boot_part_uu;

static void add_part(bpak_id_t id, size_t size)
{
    struct bpak_part_header *part;

    assert_int_equal(bpak_add_part(hdr, id, &part), BPAK_OK);
    part->size = size;
}

static void add_load_addr(bpak_id_t id, uint64_t load_addr)
{
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, id, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = load_addr;
}

static int setup(void **state)
{
    (void)state;

    name_calls = 0;
    uuid_clear(name_uu);
    memset(jump_args, 0xff, sizeof(jump_args));

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    assert_int_equal(uuid_parse("2af755d8-8de5-45d5-a862-014cfa735ce0", boot_part_uu), 0);
    return 0;
}

static int group_setup(void **state)
{
    (void)state;
    hdr = malloc(sizeof(*hdr));
    return hdr ? 0 : -1;
}

static int group_teardown(void **state)
{
    (void)state;
    free(hdr);
    return 0;
}

/* prepare as the core calls it: with an entry that is all zero */
static int prepare(void)
{
    memset(&next, 0, sizeof(next));
    return boot_driver_simple_prepare(hdr, boot_part_uu, &next);
}

/* What a delivery jumps with */
static void jump(void)
{
    jump_args[0] = (void *)next.entry;
    jump_args[1] = (void *)next.arg[0];
    jump_args[2] = (void *)next.arg[1];
    jump_args[3] = (void *)next.arg[2];
    jump_args[4] = (void *)next.arg[3];
    /* The generic handover has no device tree, whatever is in r0 */
    assert_true(next.dt == 0);
}

/* Runs first: the module keeps its configuration in a static */
static void test_prepare_before_init(void **state)
{
    (void)state;

    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);
    assert_int_equal(prepare(), -PB_ERR_STATE);
}

static void test_init_rejects(void **state)
{
    static const struct boot_driver_simple_config no_part = {
        .resolve_part_name = part_name,
    };
    (void)state;

    assert_int_equal(boot_driver_simple_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_driver_simple_init(&no_part), -PB_ERR_PARAM);

    /* A refused configuration isn't kept */
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);
    assert_int_equal(prepare(), -PB_ERR_STATE);

    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
}

static void test_entry_is_the_load_address(void **state)
{
    (void)state;

    /* The entry part isn't the first one, and its place in the payload is
     * not where it runs */
    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR + 0x100000);
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);

    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(name_calls, 1);
    assert_memory_equal(name_uu, boot_part_uu, sizeof(uuid_t));

    jump();
    assert_ptr_equal(jump_args[0], (void *)(uintptr_t)APP_ADDR);
    assert_string_equal(jump_args[1], "A");
    assert_memory_equal(jump_args[2], boot_part_uu, sizeof(uuid_t));
    assert_null(jump_args[3]);
    assert_null(jump_args[4]);
}

/* In place: the part's load address is its address in the map. The entry is
 * that address, not one computed from the part's offset in the payload:
 * here the entry part is second in the payload, 4096 bytes in, and its load
 * address is neither the flash's base plus that nor the other part's */
static void test_entry_in_place(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, FLASH_ADDR);
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, FLASH_ADDR + 0x3000);

    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    jump();
    assert_ptr_equal(jump_args[0], (void *)(uintptr_t)(FLASH_ADDR + 0x3000));
}

static void test_no_name(void **state)
{
    (void)state;

    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);

    /* A name from an earlier boot isn't kept */
    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_int_equal(boot_driver_simple_init(&cfg_no_name), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(name_calls, 1);

    jump();
    assert_ptr_equal(jump_args[0], (void *)(uintptr_t)APP_ADDR);
    assert_null(jump_args[1]);
    assert_memory_equal(jump_args[2], boot_part_uu, sizeof(uuid_t));
}

static void test_no_entry_part(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR);

    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_int_equal(name_calls, 0);
}

static void test_no_load_address(void **state)
{
    (void)state;

    /* Another part's load address isn't taken for it */
    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR);
    add_part(ID_APP, 8192);

    assert_int_equal(boot_driver_simple_init(&cfg_named), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_int_equal(name_calls, 0);
}

/* The handover the way src/board.c uses it: only through the board's object
 * (F-265) */
static void test_handover_object(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR + 0x100000);
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);

    assert_ptr_equal(handover.ops, &boot_handover_simple_ops);
    assert_ptr_equal(handover.config, &cfg_named);

    /* The module has the configuration without a name, so that the name
     * below shows that the object's reached it */
    assert_int_equal(boot_driver_simple_init(&cfg_no_name), PB_OK);

    /* An error of the module's is init's answer */
    assert_int_equal(handover.ops->init(NULL), -PB_ERR_PARAM);

    assert_int_equal(handover.ops->init(handover.config), PB_OK);
    memset(&next, 0, sizeof(next));
    assert_int_equal(handover.ops->prepare(hdr, boot_part_uu, &next), PB_OK);
    assert_int_equal(name_calls, 1);

    jump();
    /* The entry is the entry part's load address */
    assert_ptr_equal(jump_args[0], (void *)(uintptr_t)APP_ADDR);
    assert_string_equal(jump_args[1], "A");
    assert_memory_equal(jump_args[2], boot_part_uu, sizeof(uuid_t));
    assert_null(jump_args[3]);
    assert_null(jump_args[4]);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_prepare_before_init, setup),
        cmocka_unit_test_setup(test_init_rejects, setup),
        cmocka_unit_test_setup(test_entry_is_the_load_address, setup),
        cmocka_unit_test_setup(test_entry_in_place, setup),
        cmocka_unit_test_setup(test_no_name, setup),
        cmocka_unit_test_setup(test_no_entry_part, setup),
        cmocka_unit_test_setup(test_no_load_address, setup),
        cmocka_unit_test_setup(test_handover_object, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
