/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The image handover through the OP-TEE delivery (boot.md 5.7), as jiffy
 * would boot with it: the real src/boot/image.c prepares, with AArch32's
 * registers (built with CONFIG_ARCH_ARMV7), the real src/boot/optee.c
 * delivers. OP-TEE's entry without TF-A, from OP-TEE 4.9.0's entry_a32.S
 * and boot_save_args():
 *
 *   pb enters OP-TEE's part at its load address, after arch_disable_mmu(),
 *   with r0 = 0, r1 = 0xffffffff, r2 = the device tree, r3 = 0 and lr =
 *   the kernel's load address.
 *
 * An image without OP-TEE's part is jumped to as the image handover alone
 * would be: arch_jump(the kernel, 0, 0xffffffff, the device tree, 0).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <boot/dtb.h>
#include <boot/image.h>
#include <boot/image_regions.h>
#include <boot/optee.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>

#define ID_KERNEL      0xec103b08 /* bpak_id("kernel") */
#define ID_DTB         0x56f91b86 /* bpak_id("dt") */
#define ID_TEE         0x76b44bbb /* OP-TEE's part */

/* jiffy's DRAM: the kernel at 0x82000000, OP-TEE at the end of 512 MiB */
#define KERNEL_ADDR    0x82000000
#define KERNEL_SIZE    0x00800000
#define TEE_ADDR       0x9e100000
#define TEE_SIZE       0x00100000

#define KEY_ID         0xa90f9680
#define DEVICE_UUID    "0da92402-437e-3979-bedc-d596539340b5"

/* The device tree part. OP-TEE's room for it is the test's memory, and so is
 * what the handover grows it into */
#define DTB_SIZE       4096
#define OPTEE_DTB_ROOM 0x4000
static uint8_t dtb[OPTEE_DTB_ROOM] __attribute__((aligned(8)));

/* The fakes */
static int mmu_disabled;
static int jump_calls;
static int mmu_disabled_at_jump;
static void *jump_args[5];
static int enter_calls;
static uintptr_t enter_args[4];
static int mmu_disabled_at_enter;
static jmp_buf jump_env;

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_disable_mmu(void)
{
    mmu_disabled++;
}

/* Inside the board's regions: what the test checks is the parts in the room */
bool boot_image_in_regions(uint64_t addr, uint64_t size)
{
    (void)addr;
    (void)size;
    return true;
}

bool boot_image_in_place(void)
{
    return false;
}

uint32_t boot_get_flags(void)
{
    return 0;
}

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
{
    jump_calls++;
    mmu_disabled_at_jump = mmu_disabled;
    jump_args[0] = addr;
    jump_args[1] = p0;
    jump_args[2] = p1;
    jump_args[3] = p2;
    jump_args[4] = p3;
    longjmp(jump_env, 1);
}

void boot_optee_enter(uintptr_t entry, uintptr_t ns_entry, uintptr_t r1, uintptr_t r2)
{
    enter_calls++;
    mmu_disabled_at_enter = mmu_disabled;
    enter_args[0] = entry;
    enter_args[1] = ns_entry;
    enter_args[2] = r1;
    enter_args[3] = r2;
    longjmp(jump_env, 1);
}

/* jiffy's image handover */
static const struct boot_handover_image_config board_image = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
    .bootargs = "console=ttymxc0,115200",
    .set_dtb_boot_arg = true,
};

static const struct boot_delivery_optee_config board_optee = {
    .bl32_bpak_id = ID_TEE,
    .dtb_max_size = OPTEE_DTB_ROOM,
};

static const struct boot_handover handover = BOOT_HANDOVER_IMAGE(&board_image);
static const struct boot_delivery delivery = BOOT_DELIVERY_OPTEE(&board_optee);

static struct bpak_header *hdr;
static struct boot_info info;
static struct boot_handover_entry next;

static void add_part(bpak_id_t id, uint64_t load_addr, size_t size)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_part(hdr, id, &part), BPAK_OK);
    part->size = size;
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, id, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = load_addr;
}

static void make_dtb(void)
{
    assert_int_equal(fdt_create(dtb, DTB_SIZE), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_begin_node(dtb, "chosen"), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    assert_int_equal(fdt_open_into(dtb, dtb, DTB_SIZE), 0);
}

/* kernel, dt and, with 'tee', OP-TEE */
static void make_image(bool tee)
{
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID;
    add_part(ID_KERNEL, KERNEL_ADDR, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    if (tee)
        add_part(ID_TEE, TEE_ADDR, TEE_SIZE);
    make_dtb();
}

static int setup(void **state)
{
    (void)state;

    mmu_disabled = 0;
    jump_calls = 0;
    mmu_disabled_at_jump = -1;
    enter_calls = 0;
    mmu_disabled_at_enter = -1;
    memset(jump_args, 0xff, sizeof(jump_args));
    memset(enter_args, 0xff, sizeof(enter_args));

    make_image(true);

    memset(&info, 0, sizeof(info));
    info.version = PB_VERSION;
    assert_int_equal(uuid_parse(DEVICE_UUID, info.device_uuid), 0);
    info.slc = SLC_CONFIGURATION_LOCKED;
    info.no_of_available_keys = 1;
    info.available_keys[0] = KEY_ID;
    info.boot_mode = BOOT_POLICY_MODE_NORMAL;
    info.system_name = "A";
    info.active_key = KEY_ID;
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

/* The handover's prepare, the delivery's, then its jump: as boot.c runs
 * them. PB_OK when something was started */
static int run_boot(const struct boot_delivery *d)
{
    int rc = handover.ops->init(handover.config);

    if (rc != PB_OK)
        return rc;

    rc = d->ops->init(d->config);

    if (rc != PB_OK)
        return rc;

    memset(&next, 0, sizeof(next));
    rc = handover.ops->prepare(hdr, &info, &next);

    if (rc != PB_OK)
        return rc;

    rc = d->ops->prepare(hdr, &next);

    if (rc != PB_OK)
        return rc;

    if (setjmp(jump_env) == 0) {
        d->ops->jump(&next);
        fail_msg("The delivery's jump returned");
    }

    return PB_OK;
}

/* OP-TEE's entry, with the normal world's in lr and the handover's r1 and r2 */
static void test_enters_optee(void **state)
{
    (void)state;

    assert_int_equal(run_boot(&delivery), PB_OK);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(enter_calls, 1);
    assert_int_equal(mmu_disabled, 1);
    assert_int_equal(mmu_disabled_at_enter, 1);
    assert_int_equal(enter_args[0], TEE_ADDR);
    assert_int_equal(enter_args[1], KERNEL_ADDR);
    assert_int_equal(enter_args[2], 0xffffffff);
    assert_int_equal(enter_args[3], (uintptr_t)dtb);

    /* The device tree has pb's /chosen, written before OP-TEE gets it */
    int chosen = fdt_path_offset(dtb, "/chosen");
    assert_true(chosen >= 0);
    assert_non_null(fdt_getprop(dtb, chosen, "pb,device-uuid", NULL));
    assert_non_null(fdt_getprop(dtb, chosen, "bootargs", NULL));
}

/* Without OP-TEE's part: the image handover alone, directly. A released
 * image with OP-TEE in the kernel's part keeps booting (D-008) */
static void test_without_optee_part_directly(void **state)
{
    (void)state;

    make_image(false);
    assert_int_equal(run_boot(&delivery), PB_OK);

    assert_int_equal(enter_calls, 0);
    assert_int_equal(jump_calls, 1);
    assert_int_equal(mmu_disabled_at_jump, 1);
    assert_ptr_equal(jump_args[0], (void *)KERNEL_ADDR);
    assert_null(jump_args[1]);
    assert_ptr_equal(jump_args[2], (void *)0xffffffff);
    assert_ptr_equal(jump_args[3], (void *)dtb);
    assert_null(jump_args[4]);
}

/* An OP-TEE part with no bytes the hash covers has no entry */
static void test_empty_optee_part(void **state)
{
    (void)state;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID;
    add_part(ID_KERNEL, KERNEL_ADDR, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_TEE, TEE_ADDR, 0);
    make_dtb();

    assert_int_equal(run_boot(&delivery), -PB_ERR_BAD_META);
    assert_int_equal(enter_calls + jump_calls, 0);
}

/* OP-TEE grows the device tree in place to its CFG_DTB_MAX_SIZE: a part in
 * that room is refused, as for the handover's own headroom (boot.md 4.3) */
static void test_part_in_optees_dtb_room(void **state)
{
    (void)state;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID;
    add_part(ID_KERNEL, KERNEL_ADDR, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_TEE, (uintptr_t)dtb + OPTEE_DTB_ROOM - 16, 16);
    make_dtb();

    assert_int_equal(run_boot(&delivery), -PB_ERR_MEM);
    assert_int_equal(enter_calls + jump_calls, 0);
}

/* A device tree the handover grew past OP-TEE's room: OP-TEE couldn't open
 * it, so pb doesn't enter it */
static void test_dtb_larger_than_optees_room(void **state)
{
    static const struct boot_delivery_optee_config small = {
        .bl32_bpak_id = ID_TEE,
        .dtb_max_size = DTB_SIZE,
    };
    static const struct boot_delivery small_room = BOOT_DELIVERY_OPTEE(&small);
    (void)state;

    assert_int_equal(run_boot(&small_room), -PB_ERR_MEM);
    assert_int_equal(enter_calls + jump_calls, 0);
}

/* The image handover only: OP-TEE takes no normal world entry with a
 * transfer list on AArch32 (boot.md 5.7) */
static void test_takes_the_image_handover_only(void **state)
{
    static const struct boot_handover_ops other = { 0 };
    (void)state;

    assert_true(boot_delivery_optee_ops.takes(&boot_handover_image_ops));
    assert_false(boot_delivery_optee_ops.takes(&other));
}

static void test_init_needs_optees_part(void **state)
{
    static const struct boot_delivery_optee_config no_part = { 0 };
    (void)state;

    assert_int_equal(boot_delivery_optee_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_delivery_optee_init(&no_part), -PB_ERR_PARAM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_enters_optee, setup),
        cmocka_unit_test_setup(test_without_optee_part_directly, setup),
        cmocka_unit_test_setup(test_empty_optee_part, setup),
        cmocka_unit_test_setup(test_part_in_optees_dtb_room, setup),
        cmocka_unit_test_setup(test_dtb_larger_than_optees_room, setup),
        cmocka_unit_test_setup(test_takes_the_image_handover_only, setup),
        cmocka_unit_test_setup(test_init_needs_optees_part, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
