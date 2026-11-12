/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The Linux boot driver (src/boot/linux.c): device tree patching and the
 * jump. The host board has no device tree part, so the integration tests
 * only reach the kernel entry point.
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <boot/linux.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/device_uuid.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>

#define ID_KERNEL    0xec103b08
#define ID_DTB       0x56f91b86
#define ID_RAMDISK   0xf4cdac1f

#define KERNEL_ADDR  0x40400000
#define RAMDISK_ADDR 0x48000000
#define RAMDISK_SIZE 4096

#define KEY_ID_0     0xa90f9680
#define KEY_ID_1     0x25c6dd36
#define KEY_ID_2     0x52c1eda0

#define DEVICE_UUID  "02e49231-756e-35ee-a982-378e5ba866a9"

/* The fakes */
static slc_t slc_status = SLC_CONFIGURATION_LOCKED;
static uint32_t revoked_keys; /* Bit per key index */
static int patch_rc;
static int patch_calls;
static uintptr_t cleaned_start;
static size_t cleaned_len;
static int mmu_disabled;
static jmp_buf jump_env;
static void *jump_args[2];

int device_uuid(uuid_t uu)
{
    return uuid_parse(DEVICE_UUID, uu);
}

slc_t slc_read_status(void)
{
    return slc_status;
}

size_t rot_no_of_keys(void)
{
    return 3;
}

int rot_read_key_status_by_idx(unsigned int index)
{
    return (revoked_keys & (1u << index)) ? -PB_ERR_KEY_REVOKED : PB_OK;
}

key_id_t rot_key_idx_to_id(unsigned int index)
{
    static const key_id_t ids[] = { KEY_ID_0, KEY_ID_1, KEY_ID_2 };
    return ids[index];
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    cleaned_start = start;
    cleaned_len = len;
}

void arch_disable_mmu(void)
{
    mmu_disabled++;
}

void arch_jump(void *addr, void *p0, void *p1, void *p2, void *p3)
{
    (void)p1;
    (void)p2;
    (void)p3;
    jump_args[0] = addr;
    jump_args[1] = p0;
    longjmp(jump_env, 1);
}

static int patch_dtb(void *fdt, int offset)
{
    patch_calls++;
    if (patch_rc == PB_OK)
        return fdt_setprop_string(fdt, offset, "bootargs", "console=ttyS0");
    return patch_rc;
}

static const char *part_name(uuid_t part_uu)
{
    (void)part_uu;
    return "A";
}

static const struct boot_driver_linux_config cfg_kernel_only = {
    .image_bpak_id = ID_KERNEL,
};

static const struct boot_driver_linux_config cfg_dtb = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
    .ramdisk_bpak_id = ID_RAMDISK,
    .dtb_patch_cb = patch_dtb,
    .resolve_part_name = part_name,
    .set_dtb_boot_arg = true,
};

/* No ramdisk, no callbacks */
static const struct boot_driver_linux_config cfg_dtb_minimal = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
};

/* The device tree part, and the headroom it grows into after it */
#define DTB_SIZE 4096
static uint8_t dtb[DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM];
static struct bpak_header *hdr;
static uuid_t boot_part_uu;

static void add_part(bpak_id_t id, uint64_t load_addr, size_t size)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;

    assert_int_equal(bpak_add_part(hdr, id, &part), BPAK_OK);
    part->size = size;
    assert_int_equal(bpak_add_meta(hdr, BPAK_ID_PB_LOAD_ADDR, id, 8, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = load_addr;
}

/* A device tree with a /chosen node (or another name), with or without an
 * old pb,slc-available-keys property */
static void make_dtb_keys(const char *node_name, bool keys)
{
    static const fdt32_t old_keys[] = { 0x11111111 };

    assert_int_equal(fdt_create(dtb, sizeof(dtb)), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_begin_node(dtb, node_name), 0);
    if (keys)
        assert_int_equal(fdt_property(dtb, "pb,slc-available-keys", old_keys, sizeof(old_keys)), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    /* Make the rest of the part free space */
    assert_int_equal(fdt_open_into(dtb, dtb, DTB_SIZE), 0);
}

static void make_dtb(const char *node_name)
{
    make_dtb_keys(node_name, true);
}

static int setup(void **state)
{
    (void)state;

    slc_status = SLC_CONFIGURATION_LOCKED;
    revoked_keys = 0;
    patch_rc = PB_OK;
    patch_calls = 0;
    cleaned_start = 0;
    cleaned_len = 0;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID_1;
    add_part(ID_KERNEL, KERNEL_ADDR, 8192);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, RAMDISK_ADDR, RAMDISK_SIZE);
    make_dtb("chosen");
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

static int chosen(void)
{
    int offset = fdt_path_offset(dtb, "/chosen");
    assert_true(offset >= 0);
    return offset;
}

static uint32_t chosen_u32(const char *name)
{
    int len;
    const fdt32_t *val = fdt_getprop(dtb, chosen(), name, &len);

    assert_non_null(val);
    assert_int_equal(len, 4);
    return fdt32_to_cpu(*val);
}

static const char *chosen_string(const char *name)
{
    const char *val = fdt_getprop(dtb, chosen(), name, NULL);
    assert_non_null(val);
    return val;
}

static void test_init_requires_image_id(void **state)
{
    static const struct boot_driver_linux_config no_image = { 0 };
    (void)state;

    assert_int_equal(boot_driver_linux_init(&no_image), -PB_ERR_PARAM);
    assert_int_equal(boot_driver_linux_init(&cfg_kernel_only), PB_OK);
}

static void test_kernel_only(void **state)
{
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    /* The device tree is left alone */
    assert_int_equal(fdt_getprop(dtb, chosen(), "pb,slc", NULL), NULL);
    assert_int_equal(cleaned_len, 0);

    mmu_disabled = 0;
    if (setjmp(jump_env) == 0) {
        boot_driver_linux_jump();
        fail_msg("arch_jump returned");
    }
    assert_ptr_equal(jump_args[0], (void *)KERNEL_ADDR);
    assert_null(jump_args[1]);
    assert_int_equal(mmu_disabled, 1);
}

static void test_kernel_needs_load_addr(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_BAD_META);
}

static void test_dtb_is_patched(void **state)
{
    int len;
    (void)state;

    revoked_keys = 1u << 0;
    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);

    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_string_equal(chosen_string("bootargs"), "console=ttyS0");
    assert_int_equal(patch_calls, 1);

    /* The old list is replaced by the keys that are not revoked */
    const fdt32_t *keys = fdt_getprop(dtb, chosen(), "pb,slc-available-keys", &len);
    assert_non_null(keys);
    assert_int_equal(len, 8);
    assert_int_equal(fdt32_to_cpu(keys[0]), KEY_ID_1);
    assert_int_equal(fdt32_to_cpu(keys[1]), KEY_ID_2);

    assert_int_equal(chosen_u32("linux,initrd-start"), RAMDISK_ADDR);
    assert_int_equal(chosen_u32("linux,initrd-end"), RAMDISK_ADDR + RAMDISK_SIZE);

    /* Grown by the headroom */
    assert_int_equal(cleaned_start, (uintptr_t)dtb);
    assert_int_equal(cleaned_len, DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM);

    /* The dtb address is passed to the kernel */
    if (setjmp(jump_env) == 0) {
        boot_driver_linux_jump();
        fail_msg("arch_jump returned");
    }
    assert_ptr_equal(jump_args[0], (void *)KERNEL_ADDR);
    assert_ptr_equal(jump_args[1], dtb);
}

/* No list to replace: the keys are added (F-071) */
static void test_dtb_without_key_list(void **state)
{
    int len;
    (void)state;

    make_dtb_keys("chosen", false);
    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);

    const fdt32_t *keys = fdt_getprop(dtb, chosen(), "pb,slc-available-keys", &len);
    assert_non_null(keys);
    assert_int_equal(len, 12);
    assert_int_equal(fdt32_to_cpu(keys[0]), KEY_ID_0);
}

static void test_dtb_without_options(void **state)
{
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_dtb_minimal), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_null(fdt_getprop(dtb, chosen(), "pb,active-system", NULL));
    assert_null(fdt_getprop(dtb, chosen(), "linux,initrd-start", NULL));
    assert_int_equal(patch_calls, 0);
}

static void test_dtb_errors(void **state)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);

    memset(dtb, 0, 64);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR);
    make_dtb("chosen");

    patch_rc = -PB_ERR_IO;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_IO);
    patch_rc = PB_OK;

    make_dtb("chosen");
    slc_status = -PB_ERR_IO;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_IO);
    slc_status = SLC_CONFIGURATION_LOCKED;

    /* The ramdisk has a load address, but no part */
    make_dtb("chosen");
    assert_int_equal(bpak_get_part(hdr, ID_RAMDISK, &part), BPAK_OK);
    part->id = 0x12345678;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_BAD_META);
    part->id = ID_RAMDISK;

    /* The ramdisk has no load address, which used to dereference NULL
     * (F-059) */
    make_dtb("chosen");
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_RAMDISK, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_BAD_META);
    meta->part_id_ref = ID_RAMDISK;

    make_dtb("chosen");
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_BAD_META);
    meta->part_id_ref = ID_DTB;

    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->id = 0x12345678;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_BAD_META);
    part->id = ID_DTB;
}

/* A board without an SLC boots, without pb,slc */
static void test_dtb_without_slc(void **state)
{
    (void)state;

    slc_status = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    assert_null(fdt_getprop(dtb, chosen(), "pb,slc", NULL));
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
}

/* No /chosen: it is added (F-071) */
static void test_dtb_without_chosen(void **state)
{
    (void)state;

    make_dtb_keys("memory", false);
    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_true(fdt_path_offset(dtb, "/memory") >= 0);
}

/* A node named chosen elsewhere isn't /chosen: /chosen is added, the other
 * one is left alone (the drivers took the first node named chosen) */
static void test_dtb_chosen_not_at_root(void **state)
{
    int soc_chosen;
    (void)state;

    assert_int_equal(fdt_create(dtb, sizeof(dtb)), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_begin_node(dtb, "soc"), 0);
    assert_int_equal(fdt_begin_node(dtb, "chosen"), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    assert_int_equal(fdt_open_into(dtb, dtb, DTB_SIZE), 0);

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    soc_chosen = fdt_path_offset(dtb, "/soc/chosen");
    assert_true(soc_chosen >= 0);
    assert_null(fdt_getprop(dtb, soc_chosen, "pb,device-uuid", NULL));
}

/* A device tree without free space and without pb's properties, as the
 * kernel builds them: it grows by the headroom and gets them all (F-071) */
static void test_dtb_grown(void **state)
{
    (void)state;

    make_dtb_keys("memory", false);
    assert_int_equal(fdt_pack(dtb), 0);
    int packed = fdt_totalsize(dtb);

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);
    assert_int_equal(fdt_totalsize(dtb), packed + CONFIG_BOOT_DTB_HEADROOM);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_string_equal(chosen_string("bootargs"), "console=ttyS0");
    assert_int_equal(chosen_u32("linux,initrd-start"), RAMDISK_ADDR);
    assert_non_null(fdt_getprop(dtb, chosen(), "pb,slc-available-keys", NULL));
}

/* Prepares with the ramdisk at 'addr' and checks the two properties' bytes */
static void
check_initrd(uint64_t addr, size_t size, const uint8_t *start, const uint8_t *end, int len)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;
    const void *val;
    int val_len;

    make_dtb("chosen");
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_RAMDISK, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = addr;
    assert_int_equal(bpak_get_part(hdr, ID_RAMDISK, &part), BPAK_OK);
    part->size = size;

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), PB_OK);

    val = fdt_getprop(dtb, chosen(), "linux,initrd-start", &val_len);
    assert_non_null(val);
    assert_int_equal(val_len, len);
    assert_memory_equal(val, start, len);

    val = fdt_getprop(dtb, chosen(), "linux,initrd-end", &val_len);
    assert_non_null(val);
    assert_int_equal(val_len, len);
    assert_memory_equal(val, end, len);
}

/* Both addresses fit 32 bits: one cell each, the bytes every release wrote
 * (D-008, boot.md 5.2) */
static void test_initrd_32_bits(void **state)
{
    (void)state;

    check_initrd(RAMDISK_ADDR,
                 RAMDISK_SIZE,
                 (const uint8_t[]){ 0x48, 0x00, 0x00, 0x00 },
                 (const uint8_t[]){ 0x48, 0x00, 0x10, 0x00 },
                 4);

    /* The last end that fits */
    check_initrd(0xfffff000,
                 0xfff,
                 (const uint8_t[]){ 0xff, 0xff, 0xf0, 0x00 },
                 (const uint8_t[]){ 0xff, 0xff, 0xff, 0xff },
                 4);
}

/* The end is 4 GiB, which doesn't fit: both get two cells. It was written
 * as 0 (F-255) */
static void test_initrd_end_at_4_gib(void **state)
{
    (void)state;

    check_initrd(0xfffff000,
                 0x1000,
                 (const uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xf0, 0x00 },
                 (const uint8_t[]){ 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 },
                 8);
}

/* A ramdisk above 4 GiB keeps its address (F-255) */
static void test_initrd_above_4_gib(void **state)
{
    (void)state;

    check_initrd(0x880000000,
                 0x2000,
                 (const uint8_t[]){ 0x00, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x00 },
                 (const uint8_t[]){ 0x00, 0x00, 0x00, 0x08, 0x80, 0x00, 0x20, 0x00 },
                 8);
}

/* The headroom must not reach another part or pb's memory, which is checked
 * before the device tree is read */
static void test_dtb_headroom_overlap(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);

    /* The ramdisk right after the device tree part */
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_RAMDISK, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = (uintptr_t)dtb + DTB_SIZE;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_MEM);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = RAMDISK_ADDR;

    /* The device tree part right before pb's code (_code_start, CMakeLists.txt) */
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = 0x40000000 - DTB_SIZE;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR_MEM);
}

/* The device tree's header says it is larger than its part */
static void test_dtb_larger_than_part(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(boot_driver_linux_init(&cfg_dtb), PB_OK);
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->size = DTB_SIZE / 2;
    assert_int_equal(boot_driver_linux_prepare(hdr, boot_part_uu), -PB_ERR);
}

#define LINUX_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        LINUX_TEST(test_init_requires_image_id),
        LINUX_TEST(test_kernel_only),
        LINUX_TEST(test_kernel_needs_load_addr),
        LINUX_TEST(test_dtb_is_patched),
        LINUX_TEST(test_dtb_without_key_list),
        LINUX_TEST(test_dtb_without_options),
        LINUX_TEST(test_dtb_errors),
        LINUX_TEST(test_dtb_without_slc),
        LINUX_TEST(test_dtb_without_chosen),
        LINUX_TEST(test_dtb_chosen_not_at_root),
        LINUX_TEST(test_dtb_grown),
        LINUX_TEST(test_initrd_32_bits),
        LINUX_TEST(test_initrd_end_at_4_gib),
        LINUX_TEST(test_initrd_above_4_gib),
        LINUX_TEST(test_dtb_headroom_overlap),
        LINUX_TEST(test_dtb_larger_than_part),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
