/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The image handover through the TF-A delivery, as the 8MP EVK and the MEK
 * boot: the real src/boot/image.c prepares, the real src/boot/atf.c
 * delivers, with the two boards' configurations and an image with their
 * parts. What BL31 gets is what the one boot driver gave it before it became
 * a handover and a delivery (F-258): test_tfa.c has the list field by field,
 * this has the two modules together.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <arch/arch_helpers.h>
#include <boot/atf.h>
#include <boot/image.h>
#include <boot/image_regions.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>

#include "atf_interface.h"

/* The part ids of the boards' files */
#define ID_ATF       0xa697d988 /* bpak_id("atf") */
#define ID_KERNEL    0xec103b08 /* bpak_id("kernel") */
#define ID_DTB       0x56f91b86 /* bpak_id("dt") */
#define ID_RAMDISK   0xf4cdac1f /* bpak_id("ramdisk") */
#define ID_TEE       0x76b44bbb /* a part for BL32 */

#define KERNEL_SIZE  0x00a00000
#define RAMDISK_SIZE 0x00100000
#define ATF_SIZE     0x00010000
#define TEE_ADDR     0x56000000
#define TEE_SIZE     0x00080000

#define KEY_ID       0xa90f9680
#define DEVICE_UUID  "02e49231-756e-35ee-a982-378e5ba866a9"

/* Where a board's image has its parts. The device tree's address is the
 * test's memory, pb writes it */
struct layout {
    uintptr_t atf;
    uintptr_t kernel;
    uintptr_t ramdisk;
};

/* The 8MP EVK: BL31 in the OCRAM. The MEK: BL31 at the start of the DRAM */
static const struct layout imx8mpevk = { 0x00970000, 0x40400000, 0x48000000 };
static const struct layout imx8qxmek = { 0x80000000, 0x80200000, 0x88000000 };

/* The device tree part, and the headroom it grows into after it */
#define DTB_SIZE 4096
static uint8_t dtb[DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM];

/* The fakes */
static int mmu_disabled;
static int cleaned_calls;
static uintptr_t cleaned_start;
static size_t cleaned_len;
static int mmu_disabled_at_clean;
static int jump_calls;
static void *jump_args[5];
static int mmu_disabled_at_jump;
static const bl_params_t *list;
static jmp_buf jump_env;

/* Both cores of the boards have EL2 */
uint64_t read_id_aa64pfr0_el1(void)
{
    return 0x1122;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    cleaned_calls++;
    cleaned_start = start;
    cleaned_len = len;
    mmu_disabled_at_clean = mmu_disabled;
}

void arch_disable_mmu(void)
{
    mmu_disabled++;
}

/* Placement isn't this suite's: the device tree is where the test binary has
 * it, inside a region (test_image.c has the room it grows by) */
bool boot_image_in_regions(uint64_t addr, uint64_t size)
{
    (void)addr;
    (void)size;
    return true;
}

/* Loaded, as the boards that deliver through TF-A boot (test_image.c has a
 * device tree in place) */
bool boot_image_in_place(void)
{
    return false;
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
    longjmp(jump_env, 1);
}

/* The boards' hook sets the kernel command line (board_patch_bootargs()) */
static int patch_bootargs(void *fdt, int offset)
{
    return fdt_setprop_string(fdt, offset, "bootargs", "console=ttymxc1,115200");
}

/* src/board/imx8mpevk/board.c and src/board/imx8qxmek/board.c: the same */
static const struct boot_handover_image_config board_image = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
    .ramdisk_bpak_id = ID_RAMDISK,
    .dtb_patch_cb = patch_bootargs,
    .set_dtb_boot_arg = true,
};

static const struct boot_delivery_tfa_config board_tfa = {
    .bl31_bpak_id = ID_ATF,
};

/* The objects, as the boards' data has them */
static const struct boot_handover handover = BOOT_HANDOVER_IMAGE(&board_image);
static const struct boot_delivery delivery = BOOT_DELIVERY_TFA(&board_tfa);

/* A board that names no device tree, and one with a BL32 */
static const struct boot_handover_image_config image_without_dt = {
    .image_bpak_id = ID_KERNEL,
    .set_dtb_boot_arg = true,
};
static const struct boot_handover handover_without_dt = BOOT_HANDOVER_IMAGE(&image_without_dt);

static const struct boot_delivery_tfa_config tfa_with_bl32 = {
    .bl31_bpak_id = ID_ATF,
    .bl32_bpak_id = ID_TEE,
};
static const struct boot_delivery delivery_with_bl32 = BOOT_DELIVERY_TFA(&tfa_with_bl32);

static struct bpak_header *hdr;
/* pb's information, as the core collects it for the handover */
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
    assert_int_equal(fdt_create(dtb, sizeof(dtb)), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_begin_node(dtb, "chosen"), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    /* Make the rest of the part free space */
    assert_int_equal(fdt_open_into(dtb, dtb, DTB_SIZE), 0);
}

/* The image the test bench builds for the boards: atf, kernel, dt, ramdisk */
static void make_image(const struct layout *at)
{
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID;
    add_part(ID_ATF, at->atf, ATF_SIZE);
    add_part(ID_KERNEL, at->kernel, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, at->ramdisk, RAMDISK_SIZE);
    make_dtb();
}

static int setup(void **state)
{
    (void)state;

    mmu_disabled = 0;
    cleaned_calls = 0;
    cleaned_start = 0;
    cleaned_len = 0;
    mmu_disabled_at_clean = -1;
    jump_calls = 0;
    mmu_disabled_at_jump = -1;
    memset(jump_args, 0xff, sizeof(jump_args));
    list = NULL;

    make_image(&imx8mpevk);

    /* A locked device with one key, booting System A */
    memset(&info, 0, sizeof(info));
    info.version = PB_VERSION;
    assert_int_equal(uuid_parse(DEVICE_UUID, info.device_uuid), 0);
    info.slc = SLC_CONFIGURATION_LOCKED;
    info.no_of_available_keys = 1;
    info.available_keys[0] = KEY_ID;
    info.boot_mode = BOOT_POLICY_MODE_NORMAL;
    assert_int_equal(uuid_parse("2af755d8-8de5-45d5-a862-014cfa735ce0", info.boot_partition_uuid),
                     0);
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

/* A boot as src/board.c sets it up and src/boot/boot.c runs it: the
 * handover's prepare from an entry that is all zero, then the delivery's,
 * then its jump. PB_OK when BL31 was started */
static int run_boot(const struct boot_handover *h, const struct boot_delivery *d)
{
    int rc;

    rc = h->ops->init(h->config);

    if (rc != PB_OK)
        return rc;

    rc = d->ops->init(d->config);

    if (rc != PB_OK)
        return rc;

    memset(&next, 0, sizeof(next));
    rc = h->ops->prepare(hdr, &info, &next);

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

/* pb's jump: BL31's load address, x0 = the list, x1 to x3 = 0, with the
 * cache cleaned and off. BL33's node: the kernel in the normal world at EL2,
 * all exceptions masked */
static const bl_params_node_t *assert_bl31_and_bl33(const struct layout *at)
{
    const bl_params_node_t *bl33;

    assert_int_equal(jump_calls, 1);
    assert_int_equal(mmu_disabled, 1);
    assert_int_equal(mmu_disabled_at_jump, 1);
    assert_ptr_equal(jump_args[0], (void *)at->atf);
    assert_non_null(jump_args[1]);
    assert_null(jump_args[2]);
    assert_null(jump_args[3]);
    assert_null(jump_args[4]);

    assert_head(&list->h, PARAM_BL_PARAMS, 16, 0);
    bl33 = list->head;
    assert_non_null(bl33);
    assert_int_equal(bl33->image_id, 5);
    assert_head(&bl33->image_info->h, PARAM_IMAGE_BINARY, 24, 0);
    assert_true(bl33->image_info->image_base == at->kernel);
    assert_int_equal(bl33->image_info->image_size, KERNEL_SIZE);
    assert_int_equal(bl33->image_info->image_max_size, 0);
    assert_head(&bl33->ep_info->h, PARAM_EP, 88, 0x9);
    assert_true(bl33->ep_info->pc == at->kernel);
    assert_int_equal(bl33->ep_info->spsr, 0x3c9);
    return bl33;
}

static const void *chosen_prop(const char *name, int *len)
{
    int offset = fdt_path_offset(dtb, "/chosen");

    assert_true(offset >= 0);
    return fdt_getprop(dtb, offset, name, len);
}

/* The boards' boot: Linux as BL33 with the device tree in x0, no BL32, and
 * the device tree with pb's /chosen */
static void boards_boot(const struct layout *at)
{
    const bl_params_node_t *bl33;
    const fdt32_t *initrd;
    int len;

    make_image(at);
    assert_int_equal(run_boot(&handover, &delivery), PB_OK);

    bl33 = assert_bl31_and_bl33(at);
    assert_null(bl33->next_params_info);
    /* x0 = the device tree, x1 to x3 = 0: the Linux boot protocol */
    assert_args(bl33->ep_info, (uintptr_t)dtb, 0, 0, 0);

    /* The device tree BL33 gets is the image's, with the board's hook's and
     * pb's properties, and it was cleaned to memory before the cache went
     * off */
    assert_string_equal(chosen_prop("bootargs", NULL), "console=ttymxc1,115200");
    assert_string_equal(chosen_prop("pb,device-uuid", NULL), DEVICE_UUID);
    assert_string_equal(chosen_prop("pb,active-system", NULL), "A");
    assert_non_null(chosen_prop("pb,slc", NULL));
    assert_string_equal(chosen_prop("pb,boot-mode", NULL), "normal");
    assert_string_equal(chosen_prop("pb,version", NULL), PB_VERSION);
    /* pb's status: there, and empty with nothing to report (boot.md 5.6) */
    assert_non_null(chosen_prop("pb,status", &len));
    assert_int_equal(len, 0);
    initrd = chosen_prop("linux,initrd-start", &len);
    assert_non_null(initrd);
    assert_int_equal(len, 4);
    assert_true(fdt32_to_cpu(*initrd) == at->ramdisk);
    assert_int_equal(cleaned_calls, 1);
    assert_true(cleaned_start == (uintptr_t)dtb);
    assert_int_equal(cleaned_len, DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM);
    assert_int_equal(mmu_disabled_at_clean, 0);
}

static void test_imx8mpevk(void **state)
{
    (void)state;
    boards_boot(&imx8mpevk);
}

static void test_imx8qxmek(void **state)
{
    (void)state;
    boards_boot(&imx8qxmek);
}

/* The boards name a device tree and a ramdisk: an image without one of them
 * doesn't boot, as before. Nothing is started */
static void test_image_without_a_named_part(void **state)
{
    (void)state;

    /* No device tree */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, imx8mpevk.atf, ATF_SIZE);
    add_part(ID_KERNEL, imx8mpevk.kernel, KERNEL_SIZE);
    add_part(ID_RAMDISK, imx8mpevk.ramdisk, RAMDISK_SIZE);
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);

    /* No ramdisk */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, imx8mpevk.atf, ATF_SIZE);
    add_part(ID_KERNEL, imx8mpevk.kernel, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    make_dtb();
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);

    /* No BL31: the handover is prepared, the delivery refuses */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_KERNEL, imx8mpevk.kernel, KERNEL_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, imx8mpevk.ramdisk, RAMDISK_SIZE);
    make_dtb();
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);

    /* No kernel */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, imx8mpevk.atf, ATF_SIZE);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, imx8mpevk.ramdisk, RAMDISK_SIZE);
    make_dtb();
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* A kernel with a load address and no part is refused, as the one driver
 * did: by the image handover now, which asked for the address only and left
 * it to the delivery (F-292) */
static void test_kernel_without_a_part(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_part(ID_ATF, imx8mpevk.atf, ATF_SIZE);
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = imx8mpevk.kernel;
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, imx8mpevk.ramdisk, RAMDISK_SIZE);
    make_dtb();

    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);
    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* An entry in an empty part is refused, whoever jumps there: nothing was
 * hashed at its load address (security.md 4.1, F-292). The kernel is BL33,
 * the handover's entry; BL31 and BL32 are the delivery's. Nothing is started */
static void test_empty_entry_parts(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    /* The kernel: before the device tree is touched */
    assert_int_equal(bpak_get_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->size = 0;
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);
    assert_null(chosen_prop("pb,slc", NULL));
    assert_int_equal(cleaned_calls, 0);

    make_image(&imx8mpevk);
    assert_int_equal(bpak_get_part(hdr, ID_ATF, &part), BPAK_OK);
    part->size = 0;
    assert_int_equal(run_boot(&handover, &delivery), -PB_ERR_BAD_META);

    /* BL32, on a board that names one */
    make_image(&imx8mpevk);
    add_part(ID_TEE, TEE_ADDR, 0);
    assert_int_equal(run_boot(&handover, &delivery_with_bl32), -PB_ERR_BAD_META);

    assert_int_equal(jump_calls, 0);
    assert_int_equal(mmu_disabled, 0);
}

/* Parts that nothing jumps to may be empty, as before (D-008): the boards'
 * ramdisk, which then starts and ends at its load address, and a BL32 that
 * the board doesn't name */
static void test_empty_parts_that_are_not_an_entry(void **state)
{
    const bl_params_node_t *bl33;
    struct bpak_part_header *part;
    const fdt32_t *initrd;
    int len;
    (void)state;

    assert_int_equal(bpak_get_part(hdr, ID_RAMDISK, &part), BPAK_OK);
    part->size = 0;
    add_part(ID_TEE, TEE_ADDR, 0);
    assert_int_equal(run_boot(&handover, &delivery), PB_OK);

    bl33 = assert_bl31_and_bl33(&imx8mpevk);
    assert_null(bl33->next_params_info);
    assert_args(bl33->ep_info, (uintptr_t)dtb, 0, 0, 0);
    initrd = chosen_prop("linux,initrd-start", &len);
    assert_non_null(initrd);
    assert_int_equal(len, 4);
    assert_true(fdt32_to_cpu(*initrd) == imx8mpevk.ramdisk);
    initrd = chosen_prop("linux,initrd-end", &len);
    assert_non_null(initrd);
    assert_int_equal(len, 4);
    assert_true(fdt32_to_cpu(*initrd) == imx8mpevk.ramdisk);
}

/* A board whose handover names no device tree: BL33 gets four zeros, and
 * the image's device tree part is left as it is */
static void test_board_without_a_device_tree(void **state)
{
    const bl_params_node_t *bl33;
    (void)state;

    assert_int_equal(run_boot(&handover_without_dt, &delivery), PB_OK);
    bl33 = assert_bl31_and_bl33(&imx8mpevk);
    assert_null(bl33->next_params_info);
    assert_args(bl33->ep_info, 0, 0, 0, 0);
    assert_null(chosen_prop("pb,slc", NULL));
    assert_int_equal(cleaned_calls, 0);
}

/* With OP-TEE as BL32: its device tree is the image handover's, and none
 * when the board's handover has none */
static void test_bl32(void **state)
{
    const bl_params_node_t *bl33;
    const bl_params_node_t *bl32;
    (void)state;

    add_part(ID_TEE, TEE_ADDR, TEE_SIZE);
    assert_int_equal(run_boot(&handover, &delivery_with_bl32), PB_OK);
    bl33 = assert_bl31_and_bl33(&imx8mpevk);
    assert_args(bl33->ep_info, (uintptr_t)dtb, 0, 0, 0);
    bl32 = bl33->next_params_info;
    assert_non_null(bl32);
    assert_null(bl32->next_params_info);
    assert_int_equal(bl32->image_id, 4);
    assert_true(bl32->image_info->image_base == TEE_ADDR);
    assert_int_equal(bl32->image_info->image_size, TEE_SIZE);
    assert_head(&bl32->ep_info->h, PARAM_EP, 88, 0x8);
    assert_true(bl32->ep_info->pc == TEE_ADDR);
    assert_int_equal(bl32->ep_info->spsr, 0x3c5);
    assert_args(bl32->ep_info, 0, 0, 0, (uintptr_t)dtb);

    setup(NULL);
    add_part(ID_TEE, TEE_ADDR, TEE_SIZE);
    assert_int_equal(run_boot(&handover_without_dt, &delivery_with_bl32), PB_OK);
    bl33 = assert_bl31_and_bl33(&imx8mpevk);
    assert_args(bl33->ep_info, 0, 0, 0, 0);
    bl32 = bl33->next_params_info;
    assert_non_null(bl32);
    assert_args(bl32->ep_info, 0, 0, 0, 0);
}

#define TFA_IMAGE_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        TFA_IMAGE_TEST(test_imx8mpevk),
        TFA_IMAGE_TEST(test_imx8qxmek),
        TFA_IMAGE_TEST(test_image_without_a_named_part),
        TFA_IMAGE_TEST(test_kernel_without_a_part),
        TFA_IMAGE_TEST(test_empty_entry_parts),
        TFA_IMAGE_TEST(test_empty_parts_that_are_not_an_entry),
        TFA_IMAGE_TEST(test_board_without_a_device_tree),
        TFA_IMAGE_TEST(test_bl32),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
