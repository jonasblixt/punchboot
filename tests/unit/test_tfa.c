/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The delivery through TF-A (src/boot/atf.c): what BL31 gets in x0, field
 * by field. No board is at hand and the host can't run BL31, so this is
 * where the bl_params list is pinned.
 *
 * The values are the ones the boot driver wrote before it became a
 * delivery (F-258), read from its code line by line, for an image with the
 * parts of the 8MP EVK's and the MEK's (atf, kernel, dt, ramdisk) and the
 * board's handover as the image handover gives it there (entry = the
 * kernel's load address, x0 = the device tree, x1 to x3 = 0):
 *
 *   The jump     arch_jump(BL31's load address, the list, 0, 0, 0), after
 *                arch_disable_mmu(), and the list is written after it
 *   bl_params    h = { PARAM_BL_PARAMS (5), version 2, size 16, attr 0 },
 *                head = BL33's node
 *   BL33's node  image_id 5, next = BL32's node or NULL
 *     info       h = { PARAM_IMAGE_BINARY (2), 2, 24, 0 }, base = the
 *                kernel's load address, size = its part's, max size 0
 *     ep         h = { PARAM_EP (1), 2, 88, EP_NON_SECURE | EP_EXECUTABLE
 *                (0x9) }, pc = the kernel's load address, spsr = 0x3c9
 *                (EL2h, DAIF masked) with EL2 and else 0x3c5 (EL1h),
 *                args = { the device tree or 0, 0, 0, 0, 0, 0, 0, 0 }
 *   BL32's node  image_id 4, next = NULL
 *     info       h as BL33's, base and size of BL32's part
 *     ep         h = { 1, 2, 88, EP_SECURE | EP_EXECUTABLE (0x8) }, pc =
 *                BL32's load address, args = { MODE_RW_64 (0) or, with the
 *                image's optee-arch = 32, MODE_RW_32 (1), 0, 0, the device
 *                tree or 0, 0, 0, 0, 0 }, spsr = 0x3c5 (EL1h) or 0x1d3
 *                (AArch32 svc, AIF masked)
 *
 * It also refused an image whose BL33 had a load address and no part.
 * test_tfa_image.c runs the image handover into the delivery with the two
 * boards' configurations.
 *
 * The test is built twice: with the list in pb's own data, and with the
 * platform's memory for it (PLAT_BL_PARAMS_SIZE, platform.md 5.3), as on
 * the i.MX8M.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "atf_interface.h"

#define ID_ATF       0xa697d988 /* bpak_id("atf") */
#define ID_KERNEL    0xec103b08 /* bpak_id("kernel") */
#define ID_DTB       0x56f91b86 /* bpak_id("dt") */
#define ID_RAMDISK   0xf4cdac1f /* bpak_id("ramdisk") */
#define ID_TEE       0x76b44bbb /* a part for BL32 */
#define ID_OPTEE     0x99e5e80a /* bpak_id("optee-arch") */

#define ATF_ADDR     0x00970000
#define ATF_SIZE     0x10000
#define KERNEL_ADDR  0x40400000
#define KERNEL_SIZE  0x00a00000
#define DTB_ADDR     0x43000000
#define DTB_SIZE     0x8000
#define RAMDISK_ADDR 0x48000000
#define RAMDISK_SIZE 0x100000
#define TEE_ADDR     0x56000000
#define TEE_SIZE     0x80000

/* The sum of what BL31 reads: the list's head, and for BL33 and BL32 a node,
 * an image info and an entry point */
#define LIST_SIZE    (16 + 2 * (32 + 24 + 88))

/* ID_AA64PFR0_EL1 of a core with EL2 (the i.MX8's Cortex-A53 and A35), and
 * of one without */
#define PFR0_EL2     0x1122
#define PFR0_NO_EL2  0x1012

#ifdef PLAT_BL_PARAMS_SIZE
uint8_t plat_bl_params[PLAT_BL_PARAMS_SIZE] __attribute__((aligned(PLAT_BL_PARAMS_ALIGN)));
#endif

/* The board's handover, as the image handover gives it with a device tree */
static struct boot_handover_entry next;

/* The fakes */
static uint64_t pfr0;
static int mmu_disabled;
static int jump_calls;
static int mmu_disabled_at_jump;
static void *jump_args[5];
/* The list as it was at the jump: the list itself, and where it is */
static const bl_params_t *list;
/* arch_jump() doesn't return: the fake leaves through jump() */
static jmp_buf jump_env;
/* Where the last jump's list was, to see that the next one is only
 * written with the MMU off */
static uint8_t *list_memory;
static bool list_untouched_at_mmu_off;

uint64_t read_id_aa64pfr0_el1(void)
{
    return pfr0;
}

static bool is_filled_with(const uint8_t *p, size_t len, uint8_t value)
{
    for (size_t i = 0; i < len; i++) {
        if (p[i] != value)
            return false;
    }
    return true;
}

void arch_disable_mmu(void)
{
    mmu_disabled++;

    if (list_memory)
        list_untouched_at_mmu_off = is_filled_with(list_memory, LIST_SIZE, 0xa5);
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
    list = p0;
    list_memory = p0;
    longjmp(jump_env, 1);
}

/* The delivery's jump, back here from the fake arch_jump() */
static void jump(void (*delivery_jump)(const struct boot_handover_entry *next))
{
    if (setjmp(jump_env) == 0) {
        delivery_jump(&next);
        fail_msg("The delivery's jump returned");
    }
}

/* The boards' configurations: the 8MP EVK's and the MEK's have no BL32 */
static const struct boot_delivery_tfa_config cfg_bl31 = {
    .bl31_bpak_id = ID_ATF,
};

static const struct boot_delivery_tfa_config cfg_bl32 = {
    .bl31_bpak_id = ID_ATF,
    .bl32_bpak_id = ID_TEE,
};

/* The board's object for the delivery, the way a board has it in its data
 * (board.boot_delivery): BOOT_DELIVERY_TFA() is a constant initializer */
static const struct boot_delivery delivery = BOOT_DELIVERY_TFA(&cfg_bl32);

static struct bpak_header *hdr;

static void add_part(bpak_id_t id, size_t size, uint64_t load_addr)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_part(hdr, id, &part), BPAK_OK);
    part->size = size;
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, id, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = load_addr;
}

static void add_optee_arch(uint32_t bits)
{
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_meta(hdr, ID_OPTEE, 0, 4, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint32_t) = bits;
}

/* An image with the boards' parts, and with a BL32 */
static void make_image(bool with_tee)
{
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, ATF_SIZE, ATF_ADDR);
    add_part(ID_KERNEL, KERNEL_SIZE, KERNEL_ADDR);
    add_part(ID_DTB, DTB_SIZE, DTB_ADDR);
    add_part(ID_RAMDISK, RAMDISK_SIZE, RAMDISK_ADDR);
    if (with_tee)
        add_part(ID_TEE, TEE_SIZE, TEE_ADDR);
}

static int setup(void **state)
{
    (void)state;

    pfr0 = PFR0_EL2;
    mmu_disabled = 0;
    jump_calls = 0;
    mmu_disabled_at_jump = -1;
    memset(jump_args, 0xff, sizeof(jump_args));
    list = NULL;
    list_untouched_at_mmu_off = false;

    /* Whatever an earlier boot left in the list's memory is gone before
     * BL31 reads it */
    if (list_memory)
        memset(list_memory, 0xa5, LIST_SIZE);
#ifdef PLAT_BL_PARAMS_SIZE
    memset(plat_bl_params, 0xa5, sizeof(plat_bl_params));
    list_memory = plat_bl_params;
#endif

    make_image(false);
    next = (struct boot_handover_entry){
        .entry = KERNEL_ADDR,
        .arg = { DTB_ADDR, 0, 0, 0 },
        .dt = DTB_ADDR,
    };
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

static void assert_head(const param_header_t *h, uint8_t type, uint16_t size, uint32_t attr)
{
    assert_int_equal(h->type, type);
    assert_int_equal(h->version, 2);
    assert_int_equal(h->size, size);
    assert_int_equal(h->attr, attr);
}

static void
assert_args(const entry_point_info_t *ep, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3)
{
    assert_true(ep->args.arg0 == a0);
    assert_true(ep->args.arg1 == a1);
    assert_true(ep->args.arg2 == a2);
    assert_true(ep->args.arg3 == a3);
    assert_true(ep->args.arg4 == 0);
    assert_true(ep->args.arg5 == 0);
    assert_true(ep->args.arg6 == 0);
    assert_true(ep->args.arg7 == 0);
}

/* The jump to BL31 and the list's head, the same for every image */
static const bl_params_node_t *assert_jump_and_bl33(uint32_t spsr)
{
    const bl_params_node_t *bl33;

    assert_int_equal(jump_calls, 1);
    assert_int_equal(mmu_disabled, 1);
    assert_int_equal(mmu_disabled_at_jump, 1);
    assert_ptr_equal(jump_args[0], (void *)(uintptr_t)ATF_ADDR);
    assert_non_null(jump_args[1]);
    assert_null(jump_args[2]);
    assert_null(jump_args[3]);
    assert_null(jump_args[4]);
#ifdef PLAT_BL_PARAMS_SIZE
    /* In the platform's memory, and in no more of it than the list needs */
    assert_ptr_equal(jump_args[1], plat_bl_params);
    assert_true(
        is_filled_with(plat_bl_params + LIST_SIZE, sizeof(plat_bl_params) - LIST_SIZE, 0xa5));
#endif

    assert_head(&list->h, PARAM_BL_PARAMS, sizeof(bl_params_t), 0);
    bl33 = list->head;
    assert_non_null(bl33);

    assert_int_equal(bl33->image_id, BL33_IMAGE_ID);
    assert_head(&bl33->image_info->h, PARAM_IMAGE_BINARY, sizeof(image_info_t), 0);
    assert_head(&bl33->ep_info->h, PARAM_EP, sizeof(entry_point_info_t), 0x9);
    assert_int_equal(bl33->ep_info->spsr, spsr);
    return bl33;
}

static int boot(const struct boot_delivery_tfa_config *cfg)
{
    int rc = boot_delivery_tfa_init(cfg);

    if (rc != PB_OK)
        return rc;

    rc = boot_delivery_tfa_prepare(hdr, &next);

    if (rc != PB_OK)
        return rc;

    jump(boot_delivery_tfa_jump);
    return PB_OK;
}

/* Runs first: the module keeps its configuration in a static */
static void test_not_initialized(void **state)
{
    (void)state;

    assert_int_equal(boot_delivery_tfa_prepare(hdr, &next), -PB_ERR_STATE);
}

static void test_init(void **state)
{
    static const struct boot_delivery_tfa_config no_bl31 = { .bl32_bpak_id = ID_TEE };
    (void)state;

    assert_int_equal(boot_delivery_tfa_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_delivery_tfa_prepare(hdr, &next), -PB_ERR_STATE);
    assert_int_equal(boot_delivery_tfa_init(&no_bl31), -PB_ERR_PARAM);
    assert_int_equal(boot_delivery_tfa_init(&cfg_bl31), PB_OK);
}

/* What BL31 reads is AArch64's, whatever compiles this: the offsets TF-A's
 * assembly uses, and the sizes in the headers */
static void test_layout(void **state)
{
    (void)state;

    assert_int_equal(sizeof(param_header_t), 8);
    assert_int_equal(sizeof(bl_params_t), 16);
    assert_int_equal(sizeof(bl_params_node_t), 32);
    assert_int_equal(sizeof(image_info_t), 24);
    assert_int_equal(sizeof(entry_point_info_t), 88);
    assert_int_equal(offsetof(entry_point_info_t, pc), ENTRY_POINT_INFO_PC_OFFSET);
    assert_int_equal(offsetof(entry_point_info_t, pc), 0x08);
    assert_int_equal(offsetof(entry_point_info_t, spsr), 0x10);
    assert_int_equal(offsetof(entry_point_info_t, args), ENTRY_POINT_INFO_ARGS_OFFSET);
    assert_int_equal(offsetof(entry_point_info_t, args), 0x18);

    /* The values behind the names, as in the table at the top */
    assert_int_equal(PARAM_EP, 1);
    assert_int_equal(PARAM_IMAGE_BINARY, 2);
    assert_int_equal(PARAM_BL_PARAMS, 5);
    assert_int_equal(BL32_IMAGE_ID, 4);
    assert_int_equal(BL33_IMAGE_ID, 5);
    assert_int_equal(EP_NON_SECURE | EP_EXECUTABLE, 0x9);
    assert_int_equal(EP_SECURE | EP_EXECUTABLE, 0x8);
    assert_int_equal(SPSR_64(MODE_EL2, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS), 0x3c9);
    assert_int_equal(SPSR_64(MODE_EL1, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS), 0x3c5);
    assert_int_equal(
        SPSR_MODE32(
            MODE32_svc, SPSR_T_ARM, SPSR_E_LITTLE, DAIF_FIQ_BIT | DAIF_IRQ_BIT | DAIF_ABT_BIT),
        0x1d3);
    assert_int_equal(MODE_RW_64, 0);
    assert_int_equal(MODE_RW_32, 1);
}

/* The 8MP EVK and the MEK: Linux as BL33 at EL2 with the device tree in x0,
 * no BL32 */
static void test_bl33(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);

    assert_null(bl33->next_params_info);
    assert_true(bl33->image_info->image_base == KERNEL_ADDR);
    assert_int_equal(bl33->image_info->image_size, KERNEL_SIZE);
    assert_int_equal(bl33->image_info->image_max_size, 0);
    assert_true(bl33->ep_info->pc == KERNEL_ADDR);
    assert_args(bl33->ep_info, DTB_ADDR, 0, 0, 0);
}

/* A CPU without EL2: BL33 at EL1 */
static void test_bl33_at_el1(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    pfr0 = PFR0_NO_EL2;
    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c5);
    assert_true(bl33->ep_info->pc == KERNEL_ADDR);
    assert_args(bl33->ep_info, DTB_ADDR, 0, 0, 0);
}

/* The image handover without a device tree: four zeros */
static void test_bl33_without_dt(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    next.arg[0] = 0;
    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_args(bl33->ep_info, 0, 0, 0, 0);
}

/* BL33's arguments are the handover's, all four (boot.md 5.3): the driver
 * knew Linux's only. Firmware Handoff's are x0 = the device tree, x1 = its
 * signature, x3 = the transfer list */
static void test_bl33_gets_the_handovers_arguments(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    next = (struct boot_handover_entry){
        .entry = 0x80200000,
        .arg = { 0x43000000, 0x14a0fb10b, 0, 0x42000000 },
        .dt = 0x43000000,
    };
    /* The image info is the part that is loaded at the entry, whatever its
     * name */
    add_part(0x362fa9dd, 0x4000, 0x80200000);
    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_true(bl33->ep_info->pc == 0x80200000);
    assert_true(bl33->image_info->image_base == 0x80200000);
    assert_int_equal(bl33->image_info->image_size, 0x4000);
    assert_args(bl33->ep_info, 0x43000000, 0x14a0fb10b, 0, 0x42000000);
}

/* BL33 has a part in the image. The driver refused an image whose BL33 had
 * a load address and no part, and so does the delivery: nothing of it runs */
static void test_bl33_without_a_part_is_refused(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    /* The handover's entry is at no part's load address */
    next.entry = KERNEL_ADDR + 0x1000;
    assert_int_equal(boot(&cfg_bl31), -PB_ERR_BAD_META);

    /* The kernel has a load address, which is all the image handover asks
     * for, and no part */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, ATF_SIZE, ATF_ADDR);
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = KERNEL_ADDR;
    add_part(ID_DTB, DTB_SIZE, DTB_ADDR);
    next.entry = KERNEL_ADDR;
    assert_int_equal(boot(&cfg_bl31), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

static const bl_params_node_t *assert_bl32(const bl_params_node_t *bl33, uint32_t spsr)
{
    const bl_params_node_t *bl32 = bl33->next_params_info;

    assert_non_null(bl32);
    assert_null(bl32->next_params_info);
    assert_int_equal(bl32->image_id, BL32_IMAGE_ID);
    assert_head(&bl32->image_info->h, PARAM_IMAGE_BINARY, sizeof(image_info_t), 0);
    assert_true(bl32->image_info->image_base == TEE_ADDR);
    assert_int_equal(bl32->image_info->image_size, TEE_SIZE);
    assert_int_equal(bl32->image_info->image_max_size, 0);
    assert_head(&bl32->ep_info->h, PARAM_EP, sizeof(entry_point_info_t), 0x8);
    assert_true(bl32->ep_info->pc == TEE_ADDR);
    assert_int_equal(bl32->ep_info->spsr, spsr);
    return bl32;
}

/* OP-TEE as BL32, in AArch64 without the image's attribute and with 64 */
static void test_bl32(void **state)
{
    const bl_params_node_t *bl33;
    const bl_params_node_t *bl32;
    (void)state;

    make_image(true);
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_true(bl33->ep_info->pc == KERNEL_ADDR);
    assert_int_equal(bl33->image_info->image_size, KERNEL_SIZE);
    assert_args(bl33->ep_info, DTB_ADDR, 0, 0, 0);
    bl32 = assert_bl32(bl33, 0x3c5);
    /* OP-TEE's: its state, no pageable part, no memory limit, the device tree */
    assert_args(bl32->ep_info, 0, 0, 0, DTB_ADDR);

    setup(NULL);
    make_image(true);
    add_optee_arch(64);
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl32 = assert_bl32(assert_jump_and_bl33(0x3c9), 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, DTB_ADDR);
}

/* OP-TEE in AArch32: the image says so */
static void test_bl32_aarch32(void **state)
{
    const bl_params_node_t *bl32;
    (void)state;

    make_image(true);
    add_optee_arch(32);
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl32 = assert_bl32(assert_jump_and_bl33(0x3c9), 0x1d3);
    assert_args(bl32->ep_info, 1, 0, 0, DTB_ADDR);
}

/* BL32's device tree is the one BL33 gets, so none without it */
static void test_bl32_without_dt(void **state)
{
    const bl_params_node_t *bl32;
    (void)state;

    make_image(true);
    next.arg[0] = 0;
    next.dt = 0;
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl32 = assert_bl32(assert_jump_and_bl33(0x3c9), 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, 0);
}

/* BL32's device tree is what the handover says it prepared, not what is in
 * BL33's x0: a handover without one has something else there (the generic
 * handover: the booted system's name), and the image handover without
 * set_dtb_boot_arg has a device tree and passes it in no register */
static void test_bl32_device_tree_is_the_handovers(void **state)
{
    const bl_params_node_t *bl33;
    const bl_params_node_t *bl32;
    (void)state;

    make_image(true);
    next.arg[0] = 0x20201000;
    next.arg[1] = 0x20202000;
    next.dt = 0;
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_args(bl33->ep_info, 0x20201000, 0x20202000, 0, 0);
    bl32 = assert_bl32(bl33, 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, 0);

    setup(NULL);
    make_image(true);
    next.arg[0] = 0;
    next.dt = DTB_ADDR;
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_args(bl33->ep_info, 0, 0, 0, 0);
    bl32 = assert_bl32(bl33, 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, DTB_ADDR);
}

/* The image's attribute for OP-TEE says nothing without a BL32 */
static void test_optee_arch_without_bl32(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    add_optee_arch(32);
    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_null(bl33->next_params_info);
}

/*
 * An optee-arch meta of 'size' bytes, with 'bytes' from its value's start:
 * the meta's and the ones after it, where a header bpak made has zeros
 */
static void add_optee_arch_bytes(uint16_t size, const uint8_t bytes[8])
{
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_meta(hdr, ID_OPTEE, 0, 8, &meta), BPAK_OK);
    memcpy(bpak_get_meta_ptr(hdr, meta, uint8_t), bytes, 8);
    meta->size = size;
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);
}

/* BL32's spsr after a boot of an image with BL32: 0x1d3 for AArch32 */
static uint32_t booted_bl32_spsr(void)
{
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    return (uint32_t)assert_jump_and_bl33(0x3c9)->next_params_info->ep_info->spsr;
}

/*
 * F-417: optee-arch is made of the meta's own bytes, whatever its size, as
 * a part's load address is. It was read as a u32 without a look at the
 * size: a shorter meta took the bytes after it into the value.
 */
static void test_optee_arch_is_the_metas_own_bytes(void **state)
{
    static const uint8_t u64_32[8] = { 32, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t u32_32[8] = { 32, 0, 0, 0, 0xff, 0xff, 0xff, 0xff };
    static const uint8_t short_32[8] = { 32, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    static const uint8_t none_then_32[8] = { 32, 0, 0, 0, 0, 0, 0, 0 };
    (void)state;

    /* The bpak CLI's integer encoder writes a u64: AArch32, as it was */
    make_image(true);
    add_optee_arch_bytes(8, u64_32);
    assert_int_equal(booted_bl32_spsr(), 0x1d3);

    /* A u32, as the tests and a raw meta have it: as it was */
    setup(NULL);
    make_image(true);
    add_optee_arch_bytes(4, u32_32);
    assert_int_equal(booted_bl32_spsr(), 0x1d3);

    /* Shorter: its bytes, zero extended, and not the bytes after it. They
     * made 0xffffff20 of it, AArch64 */
    setup(NULL);
    make_image(true);
    add_optee_arch_bytes(1, short_32);
    assert_int_equal(booted_bl32_spsr(), 0x1d3);

    setup(NULL);
    make_image(true);
    add_optee_arch_bytes(2, short_32);
    assert_int_equal(booted_bl32_spsr(), 0x3c5);

    /* None: 0, AArch64, as without the meta. The bytes after it made 32 */
    setup(NULL);
    make_image(true);
    add_optee_arch_bytes(0, none_then_32);
    assert_int_equal(booted_bl32_spsr(), 0x3c5);
}

/* What the delivery needs isn't in the image: prepare fails, nothing jumps */
static void test_missing_parts(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    /* BL31 has no load address */
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_ATF, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot(&cfg_bl31), -PB_ERR_BAD_META);

    /* The board names a BL32 and the image has none */
    make_image(false);
    assert_int_equal(boot(&cfg_bl32), -PB_ERR_BAD_META);

    /* A load address for it and no part */
    make_image(false);
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_TEE, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = TEE_ADDR;
    assert_int_equal(boot(&cfg_bl32), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* pb jumps to BL31's load address, and BL31 to BL32's in the secure world,
 * where pb says. A part of size 0 passes the payload's hash with nothing
 * hashed at its load address: what is there isn't the image's. Refused in
 * prepare, nothing jumps (security.md 4.1, F-292). BL33's entry is the
 * handover's to check: test_tfa_image.c */
static void test_empty_bl31_or_bl32_is_refused(void **state)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(bpak_get_part(hdr, ID_ATF, &part), BPAK_OK);
    part->size = 0;
    assert_int_equal(boot(&cfg_bl31), -PB_ERR_BAD_META);

    /* BL31 has a load address and no part: nothing was hashed there either */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_ATF, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = ATF_ADDR;
    add_part(ID_KERNEL, KERNEL_SIZE, KERNEL_ADDR);
    assert_int_equal(boot(&cfg_bl31), -PB_ERR_BAD_META);

    /* The board names a BL32, and the image's is empty */
    make_image(true);
    assert_int_equal(bpak_get_part(hdr, ID_TEE, &part), BPAK_OK);
    part->size = 0;
    assert_int_equal(boot(&cfg_bl32), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* What counts is what was hashed at the load address, bpak_part_size(): the
 * content and its padding. The least is the instruction at the entry */
static void test_bl31_and_bl32_of_the_minimum(void **state)
{
    const bl_params_node_t *bl32;
    struct bpak_part_header *atf;
    struct bpak_part_header *tee;
    (void)state;

    /* One byte of content is 512 bytes with its padding, all of them hashed */
    make_image(true);
    assert_int_equal(bpak_get_part(hdr, ID_ATF, &atf), BPAK_OK);
    assert_int_equal(bpak_get_part(hdr, ID_TEE, &tee), BPAK_OK);
    atf->size = 1;
    atf->pad_bytes = 511;
    tee->size = 1;
    tee->pad_bytes = 511;
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl32 = assert_jump_and_bl33(0x3c9)->next_params_info;
    assert_non_null(bl32);
    assert_true(bl32->ep_info->pc == TEE_ADDR);
    assert_int_equal(bl32->image_info->image_size, 512);

    /* Parts that are stored without padding (transport encoded): the stored
     * size is what was loaded and hashed */
    setup(NULL);
    make_image(true);
    atf->flags = BPAK_FLAG_TRANSPORT;
    atf->transport_size = 4;
    tee->flags = BPAK_FLAG_TRANSPORT;
    tee->transport_size = 4;
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    bl32 = assert_jump_and_bl33(0x3c9)->next_params_info;
    assert_non_null(bl32);
    assert_true(bl32->ep_info->pc == TEE_ADDR);

    setup(NULL);
    make_image(true);
    atf->flags = BPAK_FLAG_TRANSPORT;
    atf->transport_size = 3;
    assert_int_equal(boot(&cfg_bl32), -PB_ERR_BAD_META);

    atf->transport_size = 4;
    tee->flags = BPAK_FLAG_TRANSPORT;
    tee->transport_size = 3;
    assert_int_equal(boot(&cfg_bl32), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* Parts that nothing jumps to may be empty, as before (D-008): the device
 * tree, the ramdisk, and a BL32 that the board doesn't name */
static void test_empty_parts_that_are_not_an_entry(void **state)
{
    static const bpak_id_t ids[] = { ID_DTB, ID_RAMDISK, ID_TEE };
    const bl_params_node_t *bl33;
    struct bpak_part_header *part;
    (void)state;

    make_image(true);

    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        assert_int_equal(bpak_get_part(hdr, ids[i], &part), BPAK_OK);
        part->size = 0;
    }

    assert_int_equal(boot(&cfg_bl31), PB_OK);
    bl33 = assert_jump_and_bl33(0x3c9);
    assert_null(bl33->next_params_info);
    assert_true(bl33->ep_info->pc == KERNEL_ADDR);
    assert_int_equal(bl33->image_info->image_size, KERNEL_SIZE);
}

/* The list is written with the MMU off, all of it and nothing before: what
 * the last boot left there is gone, a BL32 that isn't in this image too */
static void test_list_is_written_after_the_mmu_is_off(void **state)
{
    const bl_params_node_t *bl33;
    const uint8_t *bl32_area;
    (void)state;

    make_image(true);
    assert_int_equal(boot(&cfg_bl32), PB_OK);
    assert_non_null(assert_jump_and_bl33(0x3c9)->next_params_info);

    /* The next boot, of an image without a BL32, on the same memory */
    setup(NULL);
    assert_int_equal(boot_delivery_tfa_init(&cfg_bl31), PB_OK);
    assert_int_equal(boot_delivery_tfa_prepare(hdr, &next), PB_OK);
    /* prepare wrote nothing there */
    assert_true(is_filled_with(list_memory, LIST_SIZE, 0xa5));
    jump(boot_delivery_tfa_jump);
    assert_true(list_untouched_at_mmu_off);

    bl33 = assert_jump_and_bl33(0x3c9);
    assert_null(bl33->next_params_info);
    /* BL32's node, info and entry point follow BL33's: all zero */
    bl32_area = (const uint8_t *)list + 16 + 32 + 24 + 88;
    assert_true(is_filled_with(bl32_area, 32 + 24 + 88, 0));
}

/* The delivery the way src/board.c and the core use it: through the object */
static void test_delivery_object(void **state)
{
    const bl_params_node_t *bl32;
    (void)state;

    assert_ptr_equal(delivery.ops, &boot_delivery_tfa_ops);
    assert_ptr_equal(delivery.config, &cfg_bl32);

    assert_int_equal(delivery.ops->init(NULL), -PB_ERR_PARAM);

    /* The module has the configuration without a BL32: BL32's node shows
     * that the object's reached it */
    assert_int_equal(boot_delivery_tfa_init(&cfg_bl31), PB_OK);
    make_image(true);
    assert_int_equal(delivery.ops->init(delivery.config), PB_OK);
    assert_int_equal(delivery.ops->prepare(hdr, &next), PB_OK);
    jump(delivery.ops->jump);
    bl32 = assert_bl32(assert_jump_and_bl33(0x3c9), 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, DTB_ADDR);
}

#define TFA_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        TFA_TEST(test_not_initialized),
        TFA_TEST(test_init),
        TFA_TEST(test_layout),
        TFA_TEST(test_bl33),
        TFA_TEST(test_bl33_at_el1),
        TFA_TEST(test_bl33_without_dt),
        TFA_TEST(test_bl33_gets_the_handovers_arguments),
        TFA_TEST(test_bl33_without_a_part_is_refused),
        TFA_TEST(test_bl32),
        TFA_TEST(test_bl32_aarch32),
        TFA_TEST(test_bl32_without_dt),
        TFA_TEST(test_bl32_device_tree_is_the_handovers),
        TFA_TEST(test_optee_arch_without_bl32),
        TFA_TEST(test_optee_arch_is_the_metas_own_bytes),
        TFA_TEST(test_missing_parts),
        TFA_TEST(test_empty_bl31_or_bl32_is_refused),
        TFA_TEST(test_bl31_and_bl32_of_the_minimum),
        TFA_TEST(test_empty_parts_that_are_not_an_entry),
        TFA_TEST(test_list_is_written_after_the_mmu_is_off),
        TFA_TEST(test_delivery_object),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
