/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The image handover (src/boot/image.c): device tree patching, the entry
 * and the arguments. The host board has no device tree part, so the
 * integration tests only reach the kernel entry point. The jump itself is
 * the delivery's: test_boot.c (directly), test_tfa.c (through TF-A).
 *
 * pb's information is the core's to collect (test_boot.c). The handover
 * writes what it is given: this test has no RoT, no SLC and no device
 * identity to read, so a handover that asks one of them doesn't link.
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
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

#define ID_KERNEL      0xec103b08
#define ID_DTB         0x56f91b86
#define ID_RAMDISK     0xf4cdac1f

#define KERNEL_ADDR    0x40400000
#define RAMDISK_ADDR   0x48000000
#define RAMDISK_SIZE   4096

#define KEY_ID_0       0xa90f9680
#define KEY_ID_1       0x25c6dd36
#define KEY_ID_2       0x52c1eda0

#define DEVICE_UUID    "02e49231-756e-35ee-a982-378e5ba866a9"
#define BOOT_PART_UUID "2af755d8-8de5-45d5-a862-014cfa735ce0"
#define VERSION        "3.0.0"

/* What the hook does to the tree, besides bootargs */
enum hook_action {
    HOOK_BOOTARGS,
    /* It writes every property of pb's, with values that aren't pb's */
    HOOK_SETS_PBS_PROPERTIES,
    /* It adds a node and a property before /chosen, which moves /chosen */
    HOOK_MOVES_CHOSEN,
    HOOK_REMOVES_CHOSEN,
};

/* The fakes */
static int patch_rc;
static int patch_calls;
static enum hook_action hook_action;
/* pb's properties that /chosen had when the hook ran */
static int hook_saw_pbs_properties;
static uintptr_t cleaned_start;
static size_t cleaned_len;
/* What prepare gives the delivery */
static struct boot_handover_entry next;
/* pb's information, as the core hands it to prepare */
static struct boot_info info;

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    cleaned_start = start;
    cleaned_len = len;
}

/* The board's image data regions (image_helpers.c): here one, the test's */
static struct boot_image_region region;

bool boot_image_in_regions(uint64_t addr, uint64_t size)
{
    return addr >= region.base && size <= region.size && addr - region.base <= region.size - size;
}

/* The candidate's source (boot.c): its partition is booted in place */
static bool in_place;

bool boot_image_in_place(void)
{
    return in_place;
}

static const char *const pbs_properties[] = {
    "pb,device-uuid",   "pb,slc",       "pb,slc-active-key", "pb,slc-available-keys",
    "pb,active-system", "pb,boot-mode", "pb,boot-partition", "pb,version",
};

static int patch_dtb(void *fdt, int offset)
{
    int rc;

    patch_calls++;

    if (patch_rc != PB_OK)
        return patch_rc;

    for (size_t i = 0; i < ARRAY_SIZE(pbs_properties); i++) {
        if (fdt_getprop(fdt, offset, pbs_properties[i], NULL) != NULL)
            hook_saw_pbs_properties++;
    }

    rc = fdt_setprop_string(fdt, offset, "bootargs", "console=ttyS0");

    switch (hook_action) {
    case HOOK_BOOTARGS:
        break;
    case HOOK_SETS_PBS_PROPERTIES:
        for (size_t i = 0; rc == 0 && i < ARRAY_SIZE(pbs_properties); i++)
            rc = fdt_setprop_string(fdt, offset, pbs_properties[i], "the board's");
        if (rc == 0)
            rc = fdt_setprop_string(fdt, offset, "linux,initrd-start", "the board's");
        if (rc == 0)
            rc = fdt_setprop_string(fdt, offset, "linux,initrd-end", "the board's");
        break;
    case HOOK_MOVES_CHOSEN:
        /* The hook has the whole tree. A new node is put first */
        if (rc == 0)
            rc = fdt_setprop_string(fdt, 0, "model", "the board's");
        if (rc == 0)
            rc = fdt_add_subnode(fdt, 0, "reserved-memory");
        if (rc > 0)
            rc = fdt_setprop_u32(fdt, rc, "size", 0x1000);
        break;
    case HOOK_REMOVES_CHOSEN:
        if (rc == 0)
            rc = fdt_del_node(fdt, fdt_path_offset(fdt, "/chosen"));
        break;
    }

    return rc;
}

static const struct boot_handover_image_config cfg_kernel_only = {
    .image_bpak_id = ID_KERNEL,
};

static const struct boot_handover_image_config cfg_dtb = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
    .ramdisk_bpak_id = ID_RAMDISK,
    .dtb_patch_cb = patch_dtb,
    .set_dtb_boot_arg = true,
};

/* No ramdisk, no callbacks */
static const struct boot_handover_image_config cfg_dtb_minimal = {
    .image_bpak_id = ID_KERNEL,
    .dtb_bpak_id = ID_DTB,
};

/* The board's object for the handover, the way a board has it in its data
 * (board.boot_handover, F-265): BOOT_HANDOVER_IMAGE() stays a constant
 * initializer */
static const struct boot_handover handover = BOOT_HANDOVER_IMAGE(&cfg_dtb);

/* The device tree part, and the headroom it grows into after it */
#define DTB_SIZE 4096
static uint8_t dtb[DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM];
static struct bpak_header *hdr;

/* The keys that aren't revoked, in the keystore's order */
static void available_keys(size_t count, uint32_t key0, uint32_t key1, uint32_t key2)
{
    info.no_of_available_keys = count;
    info.available_keys[0] = key0;
    info.available_keys[1] = key1;
    info.available_keys[2] = key2;
}

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

    patch_rc = PB_OK;
    patch_calls = 0;
    hook_action = HOOK_BOOTARGS;
    hook_saw_pbs_properties = 0;
    cleaned_start = 0;
    cleaned_len = 0;
    /* All there is: the parts are where the test binary has them */
    region.base = 0;
    region.size = UINT64_MAX;
    in_place = false;

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID_1;
    add_part(ID_KERNEL, KERNEL_ADDR, 8192);
    add_part(ID_DTB, (uintptr_t)dtb, DTB_SIZE);
    add_part(ID_RAMDISK, RAMDISK_ADDR, RAMDISK_SIZE);
    make_dtb("chosen");

    /* A locked device that booted System A, the policy's first candidate,
     * from an image signed with the second of three keys */
    memset(&info, 0, sizeof(info));
    info.version = VERSION;
    assert_int_equal(uuid_parse(DEVICE_UUID, info.device_uuid), 0);
    info.slc = SLC_CONFIGURATION_LOCKED;
    available_keys(3, KEY_ID_0, KEY_ID_1, KEY_ID_2);
    info.boot_mode = BOOT_POLICY_MODE_NORMAL;
    assert_int_equal(uuid_parse(BOOT_PART_UUID, info.boot_partition_uuid), 0);
    info.system_name = "A";
    info.active_key = hdr->key_id;
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

/* prepare as the core calls it: with an entry that is all zero */
static int prepare(void)
{
    memset(&next, 0, sizeof(next));
    return boot_handover_image_prepare(hdr, &info, &next);
}

static void assert_chosen_has_no(const char *name)
{
    assert_null(fdt_getprop(dtb, chosen(), name, NULL));
}

static void assert_next_args(uintptr_t a0, uintptr_t a1, uintptr_t a2, uintptr_t a3)
{
    assert_true(next.arg[0] == a0);
    assert_true(next.arg[1] == a1);
    assert_true(next.arg[2] == a2);
    assert_true(next.arg[3] == a3);
}

/* The Linux boot protocol's registers with a device tree (boot.md 5.2). The
 * test is built for both: AArch64's as the host is configured, and AArch32's
 * with CONFIG_ARCH_ARMV7 (test_image_armv7), which is jiffy's */
static void assert_next_passes_dtb(void)
{
#if defined(CONFIG_ARCH_ARMV7)
    /* r0 = 0, r1 = no machine type, r2 = the device tree */
    assert_next_args(0, 0xffffffff, (uintptr_t)dtb, 0);
#else
    /* x0 = the device tree, x1 to x3 = 0 */
    assert_next_args((uintptr_t)dtb, 0, 0, 0);
#endif
}

static void test_init_requires_image_id(void **state)
{
    static const struct boot_handover_image_config no_image = { 0 };
    (void)state;

    assert_int_equal(boot_handover_image_init(&no_image), -PB_ERR_PARAM);
    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
}

static void test_kernel_only(void **state)
{
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    /* The device tree is left alone */
    assert_int_equal(fdt_getprop(dtb, chosen(), "pb,slc", NULL), NULL);
    assert_int_equal(cleaned_len, 0);

    /* The kernel's load address, and without set_dtb_boot_arg four zeros */
    assert_int_equal(next.entry, KERNEL_ADDR);
    assert_next_args(0, 0, 0, 0);
    /* No device tree: a delivery has none to tell another stage of */
    assert_true(next.dt == 0);
}

static void test_kernel_needs_load_addr(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
}

/* F-065: the entry is the address the placement looked at and the load went
 * to, the meta's own bytes (boot_image_load_addr()). A meta shorter than a
 * u64 took the bytes after it into the entry, and one without bytes has no
 * address */
static void test_entry_is_the_metas_own_bytes(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_KERNEL, &meta), BPAK_OK);
    meta->size = 4;
    memset(&hdr->metadata[meta->offset + 4], 0xff, 4);
    assert_int_equal(bpak_valid_header(hdr), BPAK_OK);

    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == KERNEL_ADDR);

    meta->size = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
}

/* A kernel part of size 0 passes placement and the payload's hash with
 * nothing hashed at its load address: what is there isn't the image's.
 * Refused in prepare, before the device tree is touched, and with nothing to
 * jump to (security.md 4.1, F-292) */
static void test_empty_kernel_is_refused(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(bpak_get_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->size = 0;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
    assert_next_args(0, 0, 0, 0);
    assert_true(next.dt == 0);
    assert_null(fdt_getprop(dtb, chosen(), "pb,slc", NULL));
    assert_int_equal(patch_calls, 0);
    assert_int_equal(cleaned_len, 0);

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
}

/* A kernel with a load address and no part: nothing was hashed there either.
 * The handover asked for the address only */
static void test_kernel_without_a_part_is_refused(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(bpak_get_part(hdr, ID_KERNEL, &part), BPAK_OK);
    part->id = 0x12345678;

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
}

/* What counts is what was hashed at the load address, bpak_part_size(): the
 * content and its padding. The least is the instruction at the entry */
static void test_kernel_of_the_minimum(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(bpak_get_part(hdr, ID_KERNEL, &part), BPAK_OK);
    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);

    /* One byte of content is 512 bytes with its padding, all of them hashed */
    part->size = 1;
    part->pad_bytes = 511;
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == KERNEL_ADDR);

    /* A part that is stored without padding (transport encoded): its stored
     * size is what was loaded and hashed */
    part->size = 8192;
    part->pad_bytes = 0;
    part->flags = BPAK_FLAG_TRANSPORT;
    part->transport_size = 4;
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == KERNEL_ADDR);

    part->transport_size = 3;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);

    /* The size of the content says nothing when nothing of it was hashed */
    part->transport_size = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
}

/*
 * F-291: in place the device tree's part is in the flash, where pb can't grow
 * it or write /chosen: on a NOR the writes are lost or fault. Until it is
 * decided whether such a tree is copied to RAM, a board that names a device
 * tree part doesn't boot in place: refused before the tree is read, grown or
 * handed to the board's hook. It doesn't hang on the board's regions either:
 * here they hold the tree, with its headroom.
 */
static void test_dtb_in_place_is_refused(void **state)
{
    static uint8_t before[sizeof(dtb)];
    (void)state;

    in_place = true;
    memcpy(before, dtb, sizeof(dtb));

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_NOT_SUPPORTED);
    assert_memory_equal(dtb, before, sizeof(dtb));
    assert_int_equal(patch_calls, 0);
    assert_int_equal(cleaned_len, 0);
    assert_true(next.entry == 0);
    assert_next_args(0, 0, 0, 0);
    assert_true(next.dt == 0);

    /* Without a hook and a ramdisk it is the same tree in the same flash */
    assert_int_equal(boot_handover_image_init(&cfg_dtb_minimal), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_NOT_SUPPORTED);
    assert_memory_equal(dtb, before, sizeof(dtb));

    /* The same image and board, loaded: it boots, with pb's /chosen */
    in_place = false;
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
}

/* F-291: a board that names no device tree part boots in place as before,
 * which is every image the host's tests of F-253 have (test_in_place.py): the
 * kernel's part alone, started where it is. Nothing is written. */
static void test_kernel_only_in_place_boots(void **state)
{
    static uint8_t before[sizeof(dtb)];
    (void)state;

    in_place = true;
    memcpy(before, dtb, sizeof(dtb));

    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == KERNEL_ADDR);
    assert_next_args(0, 0, 0, 0);
    assert_true(next.dt == 0);
    assert_memory_equal(dtb, before, sizeof(dtb));
    assert_int_equal(cleaned_len, 0);
}

static void test_dtb_is_patched(void **state)
{
    int len;
    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_string_equal(chosen_string("pb,boot-partition"), BOOT_PART_UUID);
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");
    assert_string_equal(chosen_string("pb,version"), VERSION);
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
    assert_int_equal(next.entry, KERNEL_ADDR);
    assert_next_passes_dtb();
    assert_true(next.dt == (uintptr_t)dtb);
}

/* No list to replace: the keys are added (F-071) */
static void test_dtb_without_key_list(void **state)
{
    int len;
    (void)state;

    make_dtb_keys("chosen", false);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    const fdt32_t *keys = fdt_getprop(dtb, chosen(), "pb,slc-available-keys", &len);
    assert_non_null(keys);
    assert_int_equal(len, 12);
    assert_int_equal(fdt32_to_cpu(keys[0]), KEY_ID_0);
}

static void test_dtb_without_options(void **state)
{
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb_minimal), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    /* The system's name is pb's information, no longer the board's option */
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_chosen_has_no("linux,initrd-start");
    assert_chosen_has_no("bootargs");
    assert_int_equal(patch_calls, 0);

    /* The device tree is patched, and without set_dtb_boot_arg not passed:
     * the DXL EVK's handover until it went through TF-A. It is still the
     * handover's device tree */
    assert_int_equal(next.entry, KERNEL_ADDR);
    assert_next_args(0, 0, 0, 0);
    assert_true(next.dt == (uintptr_t)dtb);
}

/* A failed prepare leaves the entry as the core gave it: nothing to jump to */
static void test_failed_prepare_gives_no_entry(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_int_equal(next.entry, 0);
    assert_next_args(0, 0, 0, 0);
    assert_true(next.dt == 0);
}

static void test_dtb_errors(void **state)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);

    memset(dtb, 0, 64);
    assert_int_equal(prepare(), -PB_ERR);
    make_dtb("chosen");

    patch_rc = -PB_ERR_IO;
    assert_int_equal(prepare(), -PB_ERR_IO);
    patch_rc = PB_OK;

    /* A life cycle that can't be read never gets here: the core stops the
     * boot before the first candidate (test_boot.c). A mode without a name
     * is refused, not written as another one */
    make_dtb("chosen");
    info.boot_mode = BOOT_POLICY_MODE_SPECULATIVE + 1;
    assert_int_equal(prepare(), -PB_ERR_PARAM);
    assert_chosen_has_no("pb,boot-mode");
    info.boot_mode = BOOT_POLICY_MODE_NORMAL;

    /* The ramdisk has a load address, but no part */
    make_dtb("chosen");
    assert_int_equal(bpak_get_part(hdr, ID_RAMDISK, &part), BPAK_OK);
    part->id = 0x12345678;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    part->id = ID_RAMDISK;

    /* The ramdisk has no load address, which used to dereference NULL
     * (F-059) */
    make_dtb("chosen");
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_RAMDISK, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    meta->part_id_ref = ID_RAMDISK;

    make_dtb("chosen");
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_DTB, &meta), BPAK_OK);
    meta->part_id_ref = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    meta->part_id_ref = ID_DTB;

    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->id = 0x12345678;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    part->id = ID_DTB;
}

/* A board without an SLC boots, without pb,slc */
static void test_dtb_without_slc(void **state)
{
    (void)state;

    info.slc = BOOT_INFO_NO_SLC;
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,slc");
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
}

/* No /chosen: it is added (F-071) */
static void test_dtb_without_chosen(void **state)
{
    (void)state;

    make_dtb_keys("memory", false);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
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

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
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

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(fdt_totalsize(dtb), packed + CONFIG_BOOT_DTB_HEADROOM);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_string_equal(chosen_string("bootargs"), "console=ttyS0");
    assert_int_equal(chosen_u32("linux,initrd-start"), RAMDISK_ADDR);
    assert_non_null(fdt_getprop(dtb, chosen(), "pb,slc-available-keys", NULL));
}

/* One /chosen property's value, byte for byte */
struct chosen_bytes {
    const char *name;
    int len;
    const uint8_t *bytes;
};

static void assert_chosen_bytes(const struct chosen_bytes *props, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        int len = -1;
        const uint8_t *val = fdt_getprop(dtb, chosen(), props[i].name, &len);

        if (val == NULL || len != props[i].len || memcmp(val, props[i].bytes, len) != 0) {
            print_error("%s: %i bytes:", props[i].name, len);
            for (int n = 0; val != NULL && n < len; n++)
                print_error(" %02x", val[n]);
            print_error("\n");
            fail_msg("%s isn't what it was", props[i].name);
        }
    }
}

/*
 * D-008, boot.md 7: the /chosen properties a released pb wrote, with the
 * bytes it wrote them with. Taken from the code before pb's information was
 * collected by the core (a61a072a), which wrote what 2.x did. They are only
 * added to: a change here breaks a system in the field.
 */
static const struct chosen_bytes released_props[] = {
    /* A string: the UUID's 36 characters and a NUL */
    { "pb,device-uuid", 37, (const uint8_t *)DEVICE_UUID },
    /* A u32, big endian as every cell: SLC_CONFIGURATION_LOCKED, the wire's 3 */
    { "pb,slc", 4, (const uint8_t[]){ 0x00, 0x00, 0x00, 0x03 } },
    /* A u32: KEY_ID_1, the key the image is signed with */
    { "pb,slc-active-key", 4, (const uint8_t[]){ 0x25, 0xc6, 0xdd, 0x36 } },
    /* A u32 list in the keystore's order, without the revoked KEY_ID_0 */
    { "pb,slc-available-keys",
      8,
      (const uint8_t[]){ 0x25, 0xc6, 0xdd, 0x36, 0x52, 0xc1, 0xed, 0xa0 } },
    /* One cell each where both fit 32 bits */
    { "linux,initrd-start", 4, (const uint8_t[]){ 0x48, 0x00, 0x00, 0x00 } },
    { "linux,initrd-end", 4, (const uint8_t[]){ 0x48, 0x00, 0x10, 0x00 } },
    /* A string: "A", "B" or "?" */
    { "pb,active-system", 2, (const uint8_t[]){ 'A', 0x00 } },
    /* The board's hook's, not pb's: it is still there after pb's properties */
    { "bootargs", 14, (const uint8_t *)"console=ttyS0" },
};

static void test_chosen_released_bytes(void **state)
{
    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));

    /* The other two names of A/B, with the same encoding */
    make_dtb("chosen");
    info.system_name = "B";
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(
        &(struct chosen_bytes){ "pb,active-system", 2, (const uint8_t[]){ 'B', 0x00 } }, 1);

    make_dtb("chosen");
    info.system_name = "?";
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(
        &(struct chosen_bytes){ "pb,active-system", 2, (const uint8_t[]){ '?', 0x00 } }, 1);

    /* Every key revoked: no list, as it was */
    make_dtb("chosen");
    available_keys(0, 0, 0, 0);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,slc-available-keys");
}

/* The properties pb's information added (boot.md 5.6), which are fixed like
 * the released ones from the release that has them */
static void test_chosen_new_bytes(void **state)
{
    static const struct chosen_bytes new_props[] = {
        /* A string: the partition's UUID, 36 characters and a NUL */
        { "pb,boot-partition", 37, (const uint8_t *)BOOT_PART_UUID },
        { "pb,boot-mode", 7, (const uint8_t *)"normal" },
        /* A string: what the wire's version command answers */
        { "pb,version", 6, (const uint8_t *)VERSION },
    };
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(new_props, ARRAY_SIZE(new_props));
    /* pb's status isn't written until the boot checks its preconditions
     * (F-248): an empty list would say "nothing to report" */
    assert_chosen_has_no("pb,status");

    make_dtb("chosen");
    info.boot_mode = BOOT_POLICY_MODE_FALLBACK;
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(&(struct chosen_bytes){ "pb,boot-mode", 9, (const uint8_t *)"fallback" },
                        1);

    make_dtb("chosen");
    info.boot_mode = BOOT_POLICY_MODE_SPECULATIVE;
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_bytes(
        &(struct chosen_bytes){ "pb,boot-mode", 12, (const uint8_t *)"speculative" }, 1);
}

/* 'boot bpak' without a partition (boot.md 5.6): no pb,boot-partition, the
 * system is "?" as it was, the mode is normal */
static void test_chosen_without_a_partition(void **state)
{
    (void)state;

    uuid_clear(info.boot_partition_uuid);
    info.system_name = "?";
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,boot-partition");
    assert_string_equal(chosen_string("pb,active-system"), "?");
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");
}

/* A policy without systems (fixed, staggered chain) has no name: no
 * pb,active-system, the partition says what booted */
static void test_chosen_without_a_system_name(void **state)
{
    (void)state;

    info.system_name = NULL;
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,active-system");
    assert_string_equal(chosen_string("pb,boot-partition"), BOOT_PART_UUID);
}

/* boot.md 5.5: the board's hook runs first, on a tree that has none of pb's
 * properties yet */
static void test_hook_runs_before_pbs_properties(void **state)
{
    (void)state;

    make_dtb_keys("chosen", false);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(patch_calls, 1);
    assert_int_equal(hook_saw_pbs_properties, 0);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
}

/* boot.md 5.5: pb's properties are written after the hook, so pb's values
 * win over whatever the hook wrote under their names */
static void test_pbs_values_win_over_the_hook(void **state)
{
    (void)state;

    hook_action = HOOK_SETS_PBS_PROPERTIES;
    available_keys(2, KEY_ID_1, KEY_ID_2, 0);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(patch_calls, 1);

    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
    assert_string_equal(chosen_string("pb,boot-partition"), BOOT_PART_UUID);
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");
    assert_string_equal(chosen_string("pb,version"), VERSION);
}

/* A device without an SLC, booting with a policy that has no names, from
 * the host's stream without a partition: pb has no value for pb,slc,
 * pb,active-system and pb,boot-partition */
static void no_values(void)
{
    info.slc = BOOT_INFO_NO_SLC;
    info.system_name = NULL;
    uuid_clear(info.boot_partition_uuid);
}

/* D-008: a released property that pb has no value for is left as the hook
 * wrote it, as every release left it: an out-of-tree board whose hook
 * writes pb,active-system itself keeps it. pb's values win, pb doesn't
 * remove (F-348). The new pb,boot-partition, which no release wrote, isn't
 * left: what is under that name is pb's or nothing */
static void test_released_property_without_a_value_keeps_the_hooks(void **state)
{
    static const struct chosen_bytes the_hooks[] = {
        { "pb,slc", 12, (const uint8_t *)"the board's" },
        { "pb,active-system", 12, (const uint8_t *)"the board's" },
    };
    (void)state;

    hook_action = HOOK_SETS_PBS_PROPERTIES;
    no_values();
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_chosen_bytes(the_hooks, ARRAY_SIZE(the_hooks));
    assert_chosen_has_no("pb,boot-partition");
    /* What pb has a value for is pb's */
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_int_equal(chosen_u32("pb,slc-active-key"), KEY_ID_1);
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");
    assert_string_equal(chosen_string("pb,version"), VERSION);
    assert_string_equal(chosen_string("bootargs"), "console=ttyS0");

    /* The key list is removed and written again, as it always was: with
     * every key revoked the hook's is gone */
    make_dtb("chosen");
    available_keys(0, 0, 0, 0);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,slc-available-keys");
}

/* The same for what the image's own tree carries, e.g. the placeholders a
 * 2.x image has for pb's properties */
static void test_released_property_without_a_value_keeps_the_images(void **state)
{
    const struct chosen_bytes the_images[] = {
        { "pb,slc", 4, (const uint8_t[]){ 0x00, 0x00, 0x00, 0x07 } },
        { "pb,active-system", 2, (const uint8_t[]){ 'X', 0x00 } },
    };
    (void)state;

    assert_int_equal(fdt_setprop_u32(dtb, chosen(), "pb,slc", 7), 0);
    assert_int_equal(fdt_setprop_string(dtb, chosen(), "pb,active-system", "X"), 0);
    assert_int_equal(fdt_setprop_string(dtb, chosen(), "pb,boot-partition", BOOT_PART_UUID), 0);
    no_values();
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_chosen_bytes(the_images, ARRAY_SIZE(the_images));
    assert_chosen_has_no("pb,boot-partition");

    /* With a value, pb's wins over the image's */
    make_dtb("chosen");
    assert_int_equal(fdt_setprop_u32(dtb, chosen(), "pb,slc", 7), 0);
    assert_int_equal(fdt_setprop_string(dtb, chosen(), "pb,active-system", "X"), 0);
    info.slc = SLC_CONFIGURATION_LOCKED;
    info.system_name = "B";
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(chosen_u32("pb,slc"), SLC_CONFIGURATION_LOCKED);
    assert_string_equal(chosen_string("pb,active-system"), "B");
}

/* A version the core didn't give is no property, like any string pb has
 * no value for */
static void test_chosen_without_a_version(void **state)
{
    (void)state;

    info.version = NULL;
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_chosen_has_no("pb,version");
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");
}

/*
 * A device tree built the 2.x way: /chosen has a placeholder of the final
 * size for every property a release wrote and for the board's bootargs, and
 * the tree has 'spare' free bytes. With a headroom of 0 that is all the room
 * there is. 'new_too': also for the three new properties, as the README says
 * to build one now.
 */
static void make_dtb_with_placeholders(int spare, bool new_too)
{
    static const uint8_t zeros[40];

    assert_int_equal(fdt_create(dtb, sizeof(dtb)), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_begin_node(dtb, "chosen"), 0);
    assert_int_equal(fdt_property(dtb, "bootargs", zeros, 14), 0);
    assert_int_equal(fdt_property(dtb, "pb,device-uuid", zeros, 37), 0);
    assert_int_equal(fdt_property(dtb, "pb,slc", zeros, 4), 0);
    assert_int_equal(fdt_property(dtb, "pb,slc-active-key", zeros, 4), 0);
    assert_int_equal(fdt_property(dtb, "pb,slc-available-keys", zeros, 8), 0);
    assert_int_equal(fdt_property(dtb, "linux,initrd-start", zeros, 4), 0);
    assert_int_equal(fdt_property(dtb, "linux,initrd-end", zeros, 4), 0);
    assert_int_equal(fdt_property(dtb, "pb,active-system", zeros, 2), 0);

    if (new_too) {
        /* The mode's has room for the longest, "speculative" */
        assert_int_equal(fdt_property(dtb, "pb,boot-mode", zeros, 12), 0);
        assert_int_equal(fdt_property(dtb, "pb,boot-partition", zeros, 37), 0);
        assert_int_equal(fdt_property(dtb, "pb,version", zeros, sizeof(VERSION)), 0);
    }

    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    assert_int_equal(fdt_open_into(dtb, dtb, fdt_totalsize(dtb) + spare), 0);
}

/* /chosen as the image handover sets it, in the tree as it is: a headroom
 * of 0. The host's configuration has the default 4 KiB */
static int set_chosen(void)
{
    const struct boot_dtb_chosen chosen_cfg = {
        .patch_cb = patch_dtb,
        .ramdisk_id = ID_RAMDISK,
        .ramdisk_addr = RAMDISK_ADDR,
    };

    return boot_dtb_set_chosen(dtb, hdr, &info, &chosen_cfg);
}

static void assert_new_props(bool mode, bool partition, bool version)
{
    assert_true((fdt_getprop(dtb, chosen(), "pb,boot-mode", NULL) != NULL) == mode);
    assert_true((fdt_getprop(dtb, chosen(), "pb,boot-partition", NULL) != NULL) == partition);
    assert_true((fdt_getprop(dtb, chosen(), "pb,version", NULL) != NULL) == version);
}

/* D-008: an image whose device tree has room for the released properties
 * and not a byte more booted with a headroom of 0, and still does. The new
 * properties are left out, the released ones have their bytes */
static void test_tree_without_room_boots_without_the_new_properties(void **state)
{
    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);

    make_dtb_with_placeholders(0, false);
    assert_int_equal(set_chosen(), PB_OK);
    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
    assert_new_props(false, false, false);

    /* The same when the policy's second candidate boots */
    make_dtb_with_placeholders(0, false);
    info.boot_mode = BOOT_POLICY_MODE_FALLBACK;
    assert_int_equal(set_chosen(), PB_OK);
    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
    assert_new_props(false, false, false);
}

/* A released property that doesn't fit fails the candidate, as it did */
static void test_tree_without_room_for_a_released_property_fails(void **state)
{
    const struct boot_dtb_chosen no_hook = { 0 };
    (void)state;

    make_dtb_keys("chosen", false);
    assert_int_equal(fdt_pack(dtb), 0);
    assert_int_equal(boot_dtb_set_chosen(dtb, hdr, &info, &no_hook), -PB_ERR);
}

/*
 * The three new properties take 138 bytes in a tree that has none of them:
 * pb,boot-mode 37 with its longest string, pb,boot-partition 70, pb,version
 * 31 ("3.0.0"). With less room each is left out on its own, in that order
 * of preference, and which of them a next stage finds doesn't depend on the
 * mode: "normal" is four bytes shorter than the other two, and with 134 to
 * 137 spare bytes a tree would have the version in a normal boot and lose
 * it in the boot that fell back.
 */
static void test_new_properties_that_fit_do_not_depend_on_the_mode(void **state)
{
    static const struct {
        int spare;
        bool mode;
        bool partition;
        bool version;
    } rooms[] = {
        { 36, false, false, false },
        /* The mode alone */
        { 37, true, false, false },
        /* Not the partition: the version, which is smaller, still fits */
        { 106, true, false, true },
        { 107, true, true, false },
        { 134, true, true, false },
        { 135, true, true, false },
        { 136, true, true, false },
        { 137, true, true, false },
        { 138, true, true, true },
    };

    static const struct {
        enum boot_policy_mode mode;
        const char *name;
    } modes[] = {
        { BOOT_POLICY_MODE_NORMAL, "normal" },
        { BOOT_POLICY_MODE_FALLBACK, "fallback" },
        { BOOT_POLICY_MODE_SPECULATIVE, "speculative" },
    };

    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);

    for (size_t r = 0; r < ARRAY_SIZE(rooms); r++) {
        for (size_t m = 0; m < ARRAY_SIZE(modes); m++) {
            make_dtb_with_placeholders(rooms[r].spare, false);
            info.boot_mode = modes[m].mode;
            assert_int_equal(set_chosen(), PB_OK);

            assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
            assert_new_props(rooms[r].mode, rooms[r].partition, rooms[r].version);

            /* What is there is pb's value, whole */
            if (rooms[r].mode)
                assert_string_equal(chosen_string("pb,boot-mode"), modes[m].name);
            if (rooms[r].partition)
                assert_string_equal(chosen_string("pb,boot-partition"), BOOT_PART_UUID);
            if (rooms[r].version)
                assert_string_equal(chosen_string("pb,version"), VERSION);
        }
    }
}

/* A device tree built with placeholders for all of pb's properties (the
 * README) gets them all with a headroom of 0, in every mode */
static void test_tree_with_placeholders_for_the_new_properties(void **state)
{
    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);

    for (int mode = BOOT_POLICY_MODE_NORMAL; mode <= BOOT_POLICY_MODE_SPECULATIVE; mode++) {
        make_dtb_with_placeholders(0, true);
        info.boot_mode = mode;
        assert_int_equal(set_chosen(), PB_OK);
        assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
        assert_new_props(true, true, true);
        assert_string_equal(chosen_string("pb,boot-partition"), BOOT_PART_UUID);
        assert_string_equal(chosen_string("pb,version"), VERSION);
    }

    assert_string_equal(chosen_string("pb,boot-mode"), "speculative");
}

/* A placeholder that holds "normal" and no more has no room for the other
 * modes: the mode is left out in every boot, also the normal one, and the
 * image's placeholder doesn't stay in its place */
static void test_mode_placeholder_too_short_for_the_longest(void **state)
{
    (void)state;

    available_keys(2, KEY_ID_1, KEY_ID_2, 0);
    make_dtb_with_placeholders(0, false);
    assert_int_equal(fdt_open_into(dtb, dtb, fdt_totalsize(dtb) + 33), 0);
    assert_int_equal(fdt_setprop_string(dtb, chosen(), "pb,boot-mode", "normal"), 0);
    assert_int_equal(fdt_pack(dtb), 0);

    assert_int_equal(set_chosen(), PB_OK);
    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
    assert_new_props(false, false, false);
}

/* The hook has the whole tree: what it adds before /chosen moves /chosen,
 * and pb's properties still land in it */
static void test_hook_moves_chosen(void **state)
{
    int node;
    (void)state;

    hook_action = HOOK_MOVES_CHOSEN;
    available_keys(2, KEY_ID_1, KEY_ID_2, 0);
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_chosen_bytes(released_props, ARRAY_SIZE(released_props));
    assert_string_equal(chosen_string("pb,boot-mode"), "normal");

    /* The hook's node and property are as it left them */
    assert_string_equal(fdt_getprop(dtb, 0, "model", NULL), "the board's");
    node = fdt_path_offset(dtb, "/reserved-memory");
    assert_true(node >= 0);
    assert_non_null(fdt_getprop(dtb, node, "size", NULL));
    assert_null(fdt_getprop(dtb, node, "pb,device-uuid", NULL));
    assert_null(fdt_getprop(dtb, 0, "pb,device-uuid", NULL));
}

/* A hook that removes /chosen: pb adds it again, with its properties */
static void test_hook_removes_chosen(void **state)
{
    (void)state;

    hook_action = HOOK_REMOVES_CHOSEN;
    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_string_equal(chosen_string("pb,version"), VERSION);
    assert_chosen_has_no("bootargs");
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

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(prepare(), PB_OK);

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

/* Parts that nothing jumps to may be empty, as before (D-008). An empty
 * device tree that the board names isn't here: F-330 */
static void test_empty_parts_that_are_not_the_entry(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    /* An empty ramdisk: it starts and ends at its load address */
    check_initrd(RAMDISK_ADDR,
                 0,
                 (const uint8_t[]){ 0x48, 0x00, 0x00, 0x00 },
                 (const uint8_t[]){ 0x48, 0x00, 0x00, 0x00 },
                 4);
    assert_true(next.entry == KERNEL_ADDR);
    assert_next_passes_dtb();

    /* A board that names neither: the image's device tree and ramdisk are
     * empty and left alone */
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->size = 0;
    make_dtb("chosen");
    cleaned_len = 0;
    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == KERNEL_ADDR);
    assert_null(fdt_getprop(dtb, chosen(), "pb,slc", NULL));
    assert_int_equal(cleaned_len, 0);
}

/* The headroom must not reach another part or leave the part's image data
 * region (boot.md 4.3), which is checked before the device tree is read */
static void test_dtb_headroom_overlap(void **state)
{
    struct bpak_meta_header *meta;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);

    /* The ramdisk right after the device tree part */
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_RAMDISK, &meta), BPAK_OK);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = (uintptr_t)dtb + DTB_SIZE;
    assert_int_equal(prepare(), -PB_ERR_MEM);
    *bpak_get_meta_ptr(hdr, meta, uint64_t) = RAMDISK_ADDR;

    /* A region that ends one byte before the headroom does: the part was
     * inside it, the room it grows by isn't */
    region.base = (uintptr_t)dtb;
    region.size = DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM - 1;
    assert_int_equal(prepare(), -PB_ERR_MEM);

    /* Outside every region, as on a board without a list */
    region.size = 0;
    assert_int_equal(prepare(), -PB_ERR_MEM);

    /* A region that is the part and its headroom */
    region.size = DTB_SIZE + CONFIG_BOOT_DTB_HEADROOM;
    assert_int_equal(prepare(), PB_OK);
}

/* The device tree's header says it is larger than its part */
static void test_dtb_larger_than_part(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    part->size = DTB_SIZE / 2;
    assert_int_equal(prepare(), -PB_ERR);
}

/* An empty device tree part that the board names: nothing is hashed at its
 * load address, and a blob there whose header says totalsize 0 fitted the
 * part. It was grown, got pb's /chosen and was passed on. Refused before the
 * part is read, and so is a totalsize below the header (security.md 4.2,
 * F-330) */
static void test_dtb_smaller_than_its_header_is_refused(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    assert_int_equal(boot_handover_image_init(&cfg_dtb), PB_OK);
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);

    /* What an earlier load left there: a header that says 0 bytes */
    part->size = 0;
    fdt_set_totalsize(dtb, 0);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_true(next.entry == 0);
    assert_true(next.dt == 0);
    assert_int_equal(fdt_totalsize(dtb), 0);
    assert_int_equal(patch_calls, 0);
    assert_int_equal(cleaned_len, 0);

    /* A part stored without padding, one byte short of the header */
    make_dtb("chosen");
    part->size = sizeof(struct fdt_header) - 1;
    fdt_set_totalsize(dtb, sizeof(struct fdt_header) - 1);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_int_equal(patch_calls, 0);

    /* The part holds the header, the header says less than itself */
    part->size = DTB_SIZE;
    fdt_set_totalsize(dtb, sizeof(struct fdt_header) - 1);
    assert_int_equal(prepare(), -PB_ERR);
    fdt_set_totalsize(dtb, 0);
    assert_int_equal(prepare(), -PB_ERR);
    assert_true(next.dt == 0);
    assert_int_equal(patch_calls, 0);
    assert_int_equal(cleaned_len, 0);

    /* A whole tree in a part of the least size that holds it boots */
    make_dtb_keys("chosen", false);
    assert_int_equal(fdt_pack(dtb), 0);
    part->size = fdt_totalsize(dtb);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.dt == (uintptr_t)dtb);
    assert_int_equal(patch_calls, 1);
}

/* The handover the way src/board.c uses it: only through the board's object */
static void test_handover_object(void **state)
{
    static const struct boot_handover_image_config no_image = { 0 };
    static const struct boot_handover refused = BOOT_HANDOVER_IMAGE(&no_image);
    (void)state;

    assert_ptr_equal(handover.ops, &boot_handover_image_ops);
    assert_ptr_equal(handover.config, &cfg_dtb);

    /* An error of the module's is init's answer, also without a configuration */
    assert_int_equal(refused.ops->init(refused.config), -PB_ERR_PARAM);
    assert_int_equal(handover.ops->init(NULL), -PB_ERR_PARAM);

    /* The module has another configuration: the device tree is patched and
     * passed on only when the object's reached it */
    assert_int_equal(boot_handover_image_init(&cfg_kernel_only), PB_OK);

    assert_int_equal(handover.ops->init(handover.config), PB_OK);
    memset(&next, 0, sizeof(next));
    assert_int_equal(handover.ops->prepare(hdr, &info, &next), PB_OK);
    assert_string_equal(chosen_string("pb,device-uuid"), DEVICE_UUID);
    assert_string_equal(chosen_string("pb,active-system"), "A");
    assert_int_equal(patch_calls, 1);
    assert_int_equal(cleaned_start, (uintptr_t)dtb);

    assert_int_equal(next.entry, KERNEL_ADDR);
    assert_next_passes_dtb();
}

#define IMAGE_TEST(f) cmocka_unit_test_setup(f, setup)

int main(void)
{
    const struct CMUnitTest tests[] = {
        IMAGE_TEST(test_init_requires_image_id),
        IMAGE_TEST(test_kernel_only),
        IMAGE_TEST(test_kernel_needs_load_addr),
        IMAGE_TEST(test_entry_is_the_metas_own_bytes),
        IMAGE_TEST(test_dtb_in_place_is_refused),
        IMAGE_TEST(test_kernel_only_in_place_boots),
        IMAGE_TEST(test_empty_kernel_is_refused),
        IMAGE_TEST(test_kernel_without_a_part_is_refused),
        IMAGE_TEST(test_kernel_of_the_minimum),
        IMAGE_TEST(test_dtb_is_patched),
        IMAGE_TEST(test_dtb_without_key_list),
        IMAGE_TEST(test_dtb_without_options),
        IMAGE_TEST(test_failed_prepare_gives_no_entry),
        IMAGE_TEST(test_dtb_errors),
        IMAGE_TEST(test_dtb_without_slc),
        IMAGE_TEST(test_dtb_without_chosen),
        IMAGE_TEST(test_dtb_chosen_not_at_root),
        IMAGE_TEST(test_dtb_grown),
        IMAGE_TEST(test_chosen_released_bytes),
        IMAGE_TEST(test_chosen_new_bytes),
        IMAGE_TEST(test_chosen_without_a_partition),
        IMAGE_TEST(test_chosen_without_a_system_name),
        IMAGE_TEST(test_hook_runs_before_pbs_properties),
        IMAGE_TEST(test_pbs_values_win_over_the_hook),
        IMAGE_TEST(test_released_property_without_a_value_keeps_the_hooks),
        IMAGE_TEST(test_released_property_without_a_value_keeps_the_images),
        IMAGE_TEST(test_chosen_without_a_version),
        IMAGE_TEST(test_tree_without_room_boots_without_the_new_properties),
        IMAGE_TEST(test_tree_without_room_for_a_released_property_fails),
        IMAGE_TEST(test_new_properties_that_fit_do_not_depend_on_the_mode),
        IMAGE_TEST(test_tree_with_placeholders_for_the_new_properties),
        IMAGE_TEST(test_mode_placeholder_too_short_for_the_longest),
        IMAGE_TEST(test_hook_moves_chosen),
        IMAGE_TEST(test_hook_removes_chosen),
        IMAGE_TEST(test_initrd_32_bits),
        IMAGE_TEST(test_initrd_end_at_4_gib),
        IMAGE_TEST(test_initrd_above_4_gib),
        IMAGE_TEST(test_empty_parts_that_are_not_the_entry),
        IMAGE_TEST(test_dtb_headroom_overlap),
        IMAGE_TEST(test_dtb_larger_than_part),
        IMAGE_TEST(test_dtb_smaller_than_its_header_is_refused),
        IMAGE_TEST(test_handover_object),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
