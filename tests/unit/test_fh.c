/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The generic handover, Arm's Firmware Handoff (src/boot/fh.c, boot.md 5.4):
 * the entry, the registers and the transfer list, with pb's structure or
 * the image's device tree in it. The jump itself is the delivery's
 * (test_boot.c).
 *
 * The list is read back by a receiver of the test's own, which knows
 * nothing of pb's headers: it follows the reference library's checks of a
 * list (TF-Shared's transfer-list-library, transfer_list_check_header() and
 * transfer_list_next()) and the specification's tables for the offsets.
 * The reference library itself is neither in the tree nor in the container.
 *
 * The region is where the build says, CONFIG_BOOT_FH_REGION_BASE: the test
 * maps memory there. Built for AArch64's registers, as the host is
 * configured, and for AArch32's with CONFIG_ARCH_ARMV7 and with
 * CONFIG_ARCH_ARMV7M.
 *
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <boot/boot.h>
#include <boot/fh.h>
#include <boot/pb_fh.h>
#include <bpak/bpak.h>
#include <bpak/id.h>
#include <libfdt.h>
#include <pb/arch.h>
#include <pb/pb.h>
#include <pb/slc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define ID_APP             0x362fa9dd /* bpak_id("app") */
#define ID_OTHER           0xec103b08
#define ID_DTB             0x56f91b86

/* A load address in RAM and one in a memory mapped flash: the handover
 * doesn't tell them apart */
#define APP_ADDR           0x20210000
#define FLASH_ADDR         0x60080000

#define KEY_ID_0           0xa90f9680
#define KEY_ID_1           0x25c6dd36
#define KEY_ID_2           0x52c1eda0

#define DEVICE_UUID        "02e49231-756e-35ee-a982-378e5ba866a9"
#define BOOT_PART_UUID     "2af755d8-8de5-45d5-a862-014cfa735ce0"
#define VERSION            "3.0.0"

/* The board's region */
#define REGION             ((uint8_t *)(uintptr_t)CONFIG_BOOT_FH_REGION_BASE)
#define REGION_SIZE        ((size_t)CONFIG_BOOT_FH_REGION_SIZE)
/* A page after it that isn't the region's: nothing is written there */
#define GUARD_SIZE         4096
#define GUARD_BYTE         0x5a

/* The specification's numbers, written out: not pb's header's */
#define TL_SIGNATURE       0x4a0fb10b
#define TL_VERSION         2
#define TL_HDR_SIZE        0x18
#define TE_HDR_SIZE        8
#define TAG_FDT            1
#define REG_X1             0x14a0fb10bull /* The signature, 1 in bits 39:32 */
#define REG_R1             0x010fb10bul /* 0x0fb10b, 1 in bits 31:24 */

/* The header's offsets (the specification's table) */
#define TL_OFF_SIGNATURE   0x00
#define TL_OFF_CHECKSUM    0x04
#define TL_OFF_VERSION     0x05
#define TL_OFF_HDR_SIZE    0x06
#define TL_OFF_ALIGNMENT   0x07
#define TL_OFF_USED_SIZE   0x08
#define TL_OFF_TOTAL_SIZE  0x0c
#define TL_OFF_FLAGS       0x10
#define TL_OFF_RESERVED    0x14

/* The one entry's data, after the two headers */
#define DATA_OFFSET        (TL_HDR_SIZE + TE_HDR_SIZE)
#define ROOM               (REGION_SIZE - DATA_OFFSET)

/* pb's structure's offsets (boot.md 5.4) */
#define PB_OFF_BOOT_MODE   0
#define PB_OFF_DEVICE_UUID 4
#define PB_OFF_PART_UUID   20
#define PB_OFF_SYSTEM_NAME 36
#define PB_OFF_SLC         44
#define PB_OFF_ACTIVE_KEY  48
#define PB_OFF_NO_OF_KEYS  52
#define PB_OFF_KEYS        56
#define PB_OFF_STATUS      120
#define PB_OFF_VERSION     124
#define PB_SIZE            156
/* With the padding to the next multiple of 8 */
#define PB_LIST_SIZE       (DATA_OFFSET + 160)

/* The device tree's part, and what follows it in memory, which isn't its */
#define DTB_PART_SIZE      4096
#define DTB_BUF_SIZE       (128 * 1024)

/* What the hook does to the tree it is given, besides its own bootargs */
enum hook_action {
    HOOK_BOOTARGS,
    HOOK_SETS_PBS_PROPERTIES,
    /* It makes the tree larger than the room the region has for it */
    HOOK_GROWS_PAST_THE_REGION,
    /* It makes the tree larger than the room and fills it to the room's last
     * bytes: its own writes stay inside the region, pb's wouldn't */
    HOOK_FILLS_TO_THE_REGIONS_END,
    /* It takes the free space out */
    HOOK_PACKS,
    /* It takes the free space out and leaves 1 KiB */
    HOOK_SHRINKS,
};

/* The fakes */
static int patch_rc;
static int patch_calls;
static void *patch_fdt;
static enum hook_action hook_action;
static int hook_saw_pbs_properties;
static uintptr_t cleaned_start;
static size_t cleaned_len;
/* The boot's flags (boot.c): BOOT_FLAG_VERBOSE picks the verbose command line */
static uint32_t boot_flags;

static struct boot_handover_entry next;
static struct boot_info info;
static struct bpak_header *hdr;
static uint8_t *dtb;
static size_t dtb_part_size;

uint32_t boot_get_flags(void)
{
    return boot_flags;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    cleaned_start = start;
    cleaned_len = len;
}

/* dtb.c's, for the image handover: not asked by this one */
bool boot_image_in_regions(uint64_t addr, uint64_t size)
{
    (void)addr;
    (void)size;
    fail_msg("Firmware Handoff asks placement nothing");
    return false;
}

static const char *const pbs_properties[] = {
    "pb,device-uuid",        "pb,slc",           "pb,slc-active-key",
    "pb,slc-available-keys", "pb,active-system", "pb,boot-mode",
    "pb,boot-partition",     "pb,version",       "pb,status",
};

/* What a tree holds: its blocks end with the strings */
static size_t fdt_used(const void *fdt)
{
    return fdt_off_dt_strings(fdt) + fdt_size_dt_strings(fdt);
}

/* A property that leaves 'left' bytes, or up to 3 more, between the tree's
 * blocks and 'end'. It costs its header, its value in whole cells and its
 * name among the strings */
static int fill_to(void *fdt, int offset, size_t end, size_t left)
{
    static uint8_t value[CONFIG_BOOT_FH_REGION_SIZE];
    const size_t cost = 12 + sizeof("fill");
    size_t used = fdt_used(fdt);
    size_t length;

    if (used + cost + left > end)
        return -FDT_ERR_NOSPACE;

    length = (end - left - used - cost) & ~(size_t)3;
    memset(value, 0x11, sizeof(value));
    return fdt_setprop(fdt, offset, "fill", value, (int)length);
}

static int patch_dtb(void *fdt, int offset)
{
    int rc;

    patch_calls++;
    patch_fdt = fdt;

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
        break;
    case HOOK_GROWS_PAST_THE_REGION:
        /* Only the header changes: nothing is written past the old size */
        if (rc == 0)
            rc = fdt_open_into(fdt, fdt, (int)(ROOM + 8));
        break;
    case HOOK_FILLS_TO_THE_REGIONS_END:
        if (rc == 0)
            rc = fdt_open_into(fdt, fdt, (int)(ROOM + 4096));
        if (rc == 0)
            rc = fill_to(fdt, offset, ROOM, 16);
        /* The hook wrote nothing past the region itself */
        if (rc == 0 && (fdt_used(fdt) > ROOM || fdt_used(fdt) < ROOM - 20))
            rc = -FDT_ERR_INTERNAL;
        break;
    case HOOK_PACKS:
        if (rc == 0)
            rc = fdt_pack(fdt);
        break;
    case HOOK_SHRINKS:
        if (rc == 0)
            rc = fdt_pack(fdt);
        if (rc == 0)
            rc = fdt_open_into(fdt, fdt, (int)fdt_totalsize(fdt) + 1024);
        break;
    }

    return rc;
}

static const struct boot_handover_fh_config cfg_app = {
    .entry_bpak_id = ID_APP,
};

/* A board whose entry is in the other part */
static const struct boot_handover_fh_config cfg_other = {
    .entry_bpak_id = ID_OTHER,
};

/* A board that names a device tree part: the list holds the tree */
static const struct boot_handover_fh_config cfg_dt = {
    .entry_bpak_id = ID_APP,
    .dtb_bpak_id = ID_DTB,
    .dtb_patch_cb = patch_dtb,
};

static const struct boot_handover_fh_config cfg_dt_no_hook = {
    .entry_bpak_id = ID_APP,
    .dtb_bpak_id = ID_DTB,
};

/* The board's object for the handover, the way a board has it in its data
 * (board.boot_handover): BOOT_HANDOVER_FH() is a constant initializer */
static const struct boot_handover handover = BOOT_HANDOVER_FH(&cfg_app);

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t sum_of(const uint8_t *p, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; i++)
        sum += p[i];

    return sum;
}

/*
 * The receiver: a list that the reference library takes for all operations
 * (transfer_list_check_header() answers TL_OPS_ALL), and that keeps the
 * specification's rules the library doesn't check.
 */
static bool tl_is_valid(const uint8_t *tl)
{
    uint32_t used = le32(tl + TL_OFF_USED_SIZE);
    uint32_t total = le32(tl + TL_OFF_TOTAL_SIZE);

    /* The library's */
    if (tl == NULL || le32(tl + TL_OFF_SIGNATURE) != TL_SIGNATURE)
        return false;
    if (total == 0 || used > total)
        return false;
    if (tl[TL_OFF_HDR_SIZE] != TL_HDR_SIZE)
        return false;
    if ((le32(tl + TL_OFF_FLAGS) & 1) && sum_of(tl, used) != 0)
        return false;
    if (tl[TL_OFF_VERSION] != TL_VERSION)
        return false;

    /* The specification's: R1, the sizes, the flags' reserved bits */
    if ((uintptr_t)tl % 8 != 0 || used % 8 != 0 || total % 8 != 0 || used < TL_HDR_SIZE)
        return false;
    if (le32(tl + TL_OFF_FLAGS) & ~UINT32_C(1))
        return false;

    return true;
}

/* The library's transfer_list_next(): the entry after 'last', the first one
 * with NULL, or NULL at the end of the used part */
static const uint8_t *tl_next(const uint8_t *tl, const uint8_t *last)
{
    uintptr_t end = (uintptr_t)tl + le32(tl + TL_OFF_USED_SIZE);
    uintptr_t va;

    if (last != NULL) {
        va = (uintptr_t)last + last[3] + le32(last + 4);
        va = (va + 7) & ~(uintptr_t)7;
    } else {
        va = (uintptr_t)tl + tl[TL_OFF_HDR_SIZE];
    }

    const uint8_t *te = (const uint8_t *)va;

    if (va + TE_HDR_SIZE > end || te[3] < TE_HDR_SIZE || va + te[3] + le32(te + 4) > end)
        return NULL;

    return te;
}

static uint32_t te_tag(const uint8_t *te)
{
    return le32(te) & 0xffffff;
}

static const uint8_t *tl_find(const uint8_t *tl, uint32_t tag)
{
    for (const uint8_t *te = tl_next(tl, NULL); te != NULL; te = tl_next(tl, te)) {
        if (te_tag(te) == tag)
            return te;
    }

    return NULL;
}

static unsigned int tl_count(const uint8_t *tl)
{
    unsigned int count = 0;

    for (const uint8_t *te = tl_next(tl, NULL); te != NULL; te = tl_next(tl, te))
        count++;

    return count;
}

/* The one entry of the list in the region, which a receiver accepts */
static const uint8_t *the_entry(uint32_t tag)
{
    const uint8_t *te;

    assert_true(tl_is_valid(REGION));
    assert_int_equal(tl_count(REGION), 1);
    te = tl_find(REGION, tag);
    assert_non_null(te);
    assert_int_equal(te[3], TE_HDR_SIZE);
    /* The data is 8-byte aligned */
    assert_int_equal((uintptr_t)(te + TE_HDR_SIZE) % 8, 0);
    return te;
}

/* pb's structure, as a receiver finds it */
static const uint8_t *pbs_structure(void)
{
    const uint8_t *te = the_entry(PB_FH_TAG_HANDOVER);

    assert_int_equal(le32(te + 4), PB_SIZE);
    return te + TE_HDR_SIZE;
}

static void assert_no_list(void)
{
    assert_false(tl_is_valid(REGION));
    assert_int_equal(le32(REGION + TL_OFF_SIGNATURE), 0);
}

static void assert_guard_is_untouched(void)
{
    for (size_t i = 0; i < GUARD_SIZE; i++)
        assert_int_equal(REGION[REGION_SIZE + i], GUARD_BYTE);
}

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

static void add_app(void)
{
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, APP_ADDR);
}

/* A device tree of 'size' bytes with /chosen, in a part of 'part_size' */
static void add_dtb(size_t size, size_t part_size)
{
    assert_true(part_size <= DTB_BUF_SIZE);
    memset(dtb, 0xee, DTB_BUF_SIZE);
    assert_int_equal(fdt_create(dtb, (int)size), 0);
    assert_int_equal(fdt_finish_reservemap(dtb), 0);
    assert_int_equal(fdt_begin_node(dtb, ""), 0);
    assert_int_equal(fdt_property_string(dtb, "model", "the image's"), 0);
    assert_int_equal(fdt_begin_node(dtb, "chosen"), 0);
    assert_int_equal(fdt_property_string(dtb, "pb,version", "the image's"), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_end_node(dtb), 0);
    assert_int_equal(fdt_finish(dtb), 0);
    assert_int_equal(fdt_open_into(dtb, dtb, (int)size), 0);

    dtb_part_size = part_size;
    add_part(ID_DTB, part_size);
    add_load_addr(ID_DTB, (uintptr_t)dtb);
}

static void available_keys(size_t count)
{
    info.no_of_available_keys = count;

    for (size_t i = 0; i < count; i++)
        info.available_keys[i] = 0x1000 + (uint32_t)i;
}

static int setup(void **state)
{
    (void)state;

    patch_rc = PB_OK;
    patch_calls = 0;
    patch_fdt = NULL;
    hook_action = HOOK_BOOTARGS;
    hook_saw_pbs_properties = 0;
    boot_flags = 0;
    cleaned_start = 0;
    cleaned_len = 0;

    /* What an earlier boot left in the region isn't zero */
    memset(REGION, 0xa5, REGION_SIZE);
    memset(REGION + REGION_SIZE, GUARD_BYTE, GUARD_SIZE);

    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    hdr->key_id = KEY_ID_1;

    /* A locked device that booted System A, the policy's first candidate,
     * from an image signed with the second of three keys */
    memset(&info, 0, sizeof(info));
    info.version = VERSION;
    assert_int_equal(uuid_parse(DEVICE_UUID, info.device_uuid), 0);
    info.slc = SLC_CONFIGURATION_LOCKED;
    info.no_of_available_keys = 2;
    info.available_keys[0] = KEY_ID_1;
    info.available_keys[1] = KEY_ID_2;
    info.boot_mode = BOOT_POLICY_MODE_NORMAL;
    assert_int_equal(uuid_parse(BOOT_PART_UUID, info.boot_partition_uuid), 0);
    info.system_name = "A";
    info.active_key = hdr->key_id;
    return 0;
}

static int group_setup(void **state)
{
    void *region;

    (void)state;

    /* The region, where the build has it, and the guard after it */
    region = mmap(REGION,
                  REGION_SIZE + GUARD_SIZE,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                  -1,
                  0);

    if (region != REGION)
        return -1;

    hdr = malloc(sizeof(*hdr));
    /* malloc()'s alignment is a device tree's, 8 bytes, and more */
    dtb = malloc(DTB_BUF_SIZE);
    return (hdr && dtb) ? 0 : -1;
}

static int group_teardown(void **state)
{
    (void)state;
    free(hdr);
    free(dtb);
    munmap(REGION, REGION_SIZE + GUARD_SIZE);
    return 0;
}

/* prepare as the core calls it: with an entry that is all zero */
static int prepare(void)
{
    memset(&next, 0, sizeof(next));
    return boot_handover_fh_prepare(hdr, &info, &next);
}

static void assert_next_is_empty(void)
{
    static const struct boot_handover_entry zeros;

    assert_memory_equal(&next, &zeros, sizeof(next));
}

/* The specification's registers (boot.md 5.4), with the device tree 'dt' in
 * the list or 0 */
static void assert_registers(uintptr_t dt)
{
#if defined(CONFIG_ARCH_ARMV7) || defined(CONFIG_ARCH_ARMV7M)
    /* AArch32, and pb's extension to ARMv7-M: r0 = 0, r1 = the signature
     * and the convention's version, r2 = the device tree, r3 = the list */
    assert_true(next.arg[0] == 0);
    assert_true(next.arg[1] == REG_R1);
    assert_true(next.arg[2] == dt);
    assert_true(next.arg[3] == (uintptr_t)REGION);
#else
    /* AArch64: x0 = the device tree, x1 = the signature and the
     * convention's version, x2 = 0, x3 = the list */
    assert_true(next.arg[0] == dt);
    assert_true(next.arg[1] == REG_X1);
    assert_true(next.arg[2] == 0);
    assert_true(next.arg[3] == (uintptr_t)REGION);
#endif
    assert_true(next.dt == dt);
}

/* Runs first: the module keeps its configuration in a static */
static void test_prepare_before_init(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(prepare(), -PB_ERR_STATE);
    assert_next_is_empty();
}

static void test_init_rejects(void **state)
{
    static const struct boot_handover_fh_config no_part = { .dtb_bpak_id = ID_DTB };
    (void)state;

    assert_int_equal(boot_handover_fh_init(NULL), -PB_ERR_PARAM);
    assert_int_equal(boot_handover_fh_init(&no_part), -PB_ERR_PARAM);

    /* A refused configuration isn't kept */
    add_app();
    assert_int_equal(prepare(), -PB_ERR_STATE);

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
}

/* The entry is the entry part's load address (boot.md 5.4) */
static void test_entry_is_the_load_address(void **state)
{
    (void)state;

    /* The entry part isn't the first one, and its place in the payload is
     * not where it runs */
    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR + 0x100000);
    add_app();

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == APP_ADDR);
}

/* In place: the part's load address is its address in the map. The entry is
 * that address, not one computed from the part's offset in the payload */
static void test_entry_in_place(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, FLASH_ADDR);
    add_part(ID_APP, 8192);
    add_load_addr(ID_APP, FLASH_ADDR + 0x3000);

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == FLASH_ADDR + 0x3000);
}

static void test_no_entry_part(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR);

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    /* Nothing to jump to, and no arguments */
    assert_next_is_empty();
}

static void test_no_load_address(void **state)
{
    (void)state;

    /* Another part's load address isn't taken for it */
    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR);
    add_part(ID_APP, 8192);

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();
}

/* An entry part of size 0 passes placement and the payload's hash with
 * nothing hashed at its load address: what is there isn't the image's, and
 * on ARMv7-M the jump reads the stack pointer and the reset vector from it.
 * Refused in prepare, with nothing to jump to (security.md 4.1, F-292) */
static void test_empty_entry_part_is_refused(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR + 0x100000);
    add_part(ID_APP, 0);
    add_load_addr(ID_APP, APP_ADDR);

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();
}

/* What counts is what was hashed at the load address, bpak_part_size(): the
 * content and its padding. The least is the 8 bytes ARMv7-M's jump reads */
static void test_entry_part_of_the_minimum(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    add_part(ID_APP, 1);
    add_load_addr(ID_APP, APP_ADDR);
    assert_int_equal(bpak_get_part(hdr, ID_APP, &part), BPAK_OK);
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    /* One byte of content is 512 bytes with its padding, all of them hashed */
    part->pad_bytes = 511;
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == APP_ADDR);

    /* A part that is stored without padding (transport encoded): its stored
     * size is what was loaded and hashed. 8 bytes hold the two words */
    part->size = 8192;
    part->pad_bytes = 0;
    part->flags = BPAK_FLAG_TRANSPORT;
    part->transport_size = 8;
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == APP_ADDR);

    part->transport_size = 7;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();

    /* The size of the content says nothing when nothing of it was hashed */
    part->transport_size = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();
}

/* A part that nothing jumps to may be empty, as before (D-008) */
static void test_empty_part_that_is_not_the_entry(void **state)
{
    (void)state;

    add_part(ID_OTHER, 0);
    /* Also one whose load address is the region's: nothing of it is there */
    add_load_addr(ID_OTHER, CONFIG_BOOT_FH_REGION_BASE);
    add_app();

    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(next.entry == APP_ADDR);
}

/* boot.md 5.4, the registers: without a device tree part its register is 0 */
static void test_registers_without_a_device_tree(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_registers(0);
}

/* The header, byte for byte: the specification's table. Version 2, with
 * checksums on (boot.md 5.4) */
static void test_list_header(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    /* Little-endian: 0x4a0fb10b */
    assert_int_equal(REGION[0], 0x0b);
    assert_int_equal(REGION[1], 0xb1);
    assert_int_equal(REGION[2], 0x0f);
    assert_int_equal(REGION[3], 0x4a);
    assert_int_equal(REGION[TL_OFF_VERSION], 2);
    assert_int_equal(REGION[TL_OFF_HDR_SIZE], 0x18);
    /* 8 bytes, as a power of two */
    assert_int_equal(REGION[TL_OFF_ALIGNMENT], 3);
    /* The header, the entry's header, pb's structure and its padding: a
     * multiple of 8 */
    assert_int_equal(le32(REGION + TL_OFF_USED_SIZE), PB_LIST_SIZE);
    assert_int_equal(PB_LIST_SIZE % 8, 0);
    /* The region */
    assert_int_equal(le32(REGION + TL_OFF_TOTAL_SIZE), REGION_SIZE);
    /* The list uses checksums, and no other flag */
    assert_int_equal(le32(REGION + TL_OFF_FLAGS), 1);
    assert_int_equal(le32(REGION + TL_OFF_RESERVED), 0);

    /* What the next stage reads is in memory, not in the cache */
    assert_true(cleaned_start == (uintptr_t)REGION);
    assert_int_equal(cleaned_len, PB_LIST_SIZE);

    assert_guard_is_untouched();
}

/* The checksum as the reference library computes and checks it: the used
 * bytes sum to 0 modulo 256, the checksum among them. Not the sum of the
 * other bytes, the specification's other reading (firmware-handoff.md) */
static void test_checksum_makes_the_used_bytes_sum_to_zero(void **state)
{
    uint8_t others;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    assert_int_equal(sum_of(REGION, PB_LIST_SIZE), 0);

    others = (uint8_t)(sum_of(REGION, PB_LIST_SIZE) - REGION[TL_OFF_CHECKSUM]);
    assert_int_equal(REGION[TL_OFF_CHECKSUM], (uint8_t)(0x100 - others));
    /* The two readings differ for this list, so the test tells them apart */
    assert_int_not_equal(REGION[TL_OFF_CHECKSUM], others);

    /* Every used byte counts: the padding after the entry too */
    REGION[PB_LIST_SIZE - 1] ^= 0x01;
    assert_false(tl_is_valid(REGION));
    REGION[PB_LIST_SIZE - 1] ^= 0x01;
    assert_true(tl_is_valid(REGION));

    /* Another boot's information, another checksum that holds */
    info.boot_mode = BOOT_POLICY_MODE_FALLBACK;
    info.system_name = "B";
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(sum_of(REGION, PB_LIST_SIZE), 0);
}

/* Without a device tree part the list holds pb's structure and nothing
 * else: one entry with pb's tag (boot.md 5.4) */
static void test_list_holds_pbs_structure(void **state)
{
    const uint8_t *te;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    te = the_entry(PB_FH_TAG_HANDOVER);
    /* The first entry follows the header */
    assert_ptr_equal(te, REGION + TL_HDR_SIZE);
    /* tag_id in three bytes, then hdr_size, then data_size: the data's own
     * size, which is the structure's */
    assert_int_equal(te[0], PB_FH_TAG_HANDOVER & 0xff);
    assert_int_equal(te[1], (PB_FH_TAG_HANDOVER >> 8) & 0xff);
    assert_int_equal(te[2], (PB_FH_TAG_HANDOVER >> 16) & 0xff);
    assert_int_equal(te[3], 8);
    assert_int_equal(le32(te + 4), PB_SIZE);
    assert_int_equal(sizeof(struct pb_fh_handover), PB_SIZE);

    /* No device tree, so none in the list either */
    assert_null(tl_find(REGION, TAG_FDT));

    /* The padding to the used part's end is zero */
    for (size_t i = DATA_OFFSET + PB_SIZE; i < PB_LIST_SIZE; i++)
        assert_int_equal(REGION[i], 0);
}

/* The tag isn't allocated: until it is, it is in the range the
 * specification leaves to use without allocation, never in the standard
 * range, where an id is allocated before it is used */
static void test_pbs_tag_is_not_a_standard_one(void **state)
{
    (void)state;

    assert_in_range(PB_FH_TAG_HANDOVER, 0xfff000, 0xffffff);
}

/* pb's structure, field by field (boot.md 5.4, 5.6): little-endian, at the
 * offsets of the contract's layout */
static void test_pbs_structure(void **state)
{
    static const uint8_t device_uuid[16] = { 0x02, 0xe4, 0x92, 0x31, 0x75, 0x6e, 0x35, 0xee,
                                             0xa9, 0x82, 0x37, 0x8e, 0x5b, 0xa8, 0x66, 0xa9 };
    static const uint8_t part_uuid[16] = { 0x2a, 0xf7, 0x55, 0xd8, 0x8d, 0xe5, 0x45, 0xd5,
                                           0xa8, 0x62, 0x01, 0x4c, 0xfa, 0x73, 0x5c, 0xe0 };
    static const uint8_t zeros[32];
    const uint8_t *h;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    h = pbs_structure();

    assert_int_equal(le32(h + PB_OFF_BOOT_MODE), 0);
    /* The bytes in the order of the UUID's string */
    assert_memory_equal(h + PB_OFF_DEVICE_UUID, device_uuid, 16);
    assert_memory_equal(h + PB_OFF_PART_UUID, part_uuid, 16);
    /* NUL terminated, and nothing after it */
    assert_string_equal((const char *)h + PB_OFF_SYSTEM_NAME, "A");
    assert_memory_equal(h + PB_OFF_SYSTEM_NAME + 1, zeros, 7);
    assert_int_equal(le32(h + PB_OFF_SLC), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(le32(h + PB_OFF_ACTIVE_KEY), KEY_ID_1);
    assert_int_equal(le32(h + PB_OFF_NO_OF_KEYS), 2);
    assert_int_equal(le32(h + PB_OFF_KEYS), KEY_ID_1);
    assert_int_equal(le32(h + PB_OFF_KEYS + 4), KEY_ID_2);

    /* The keys that aren't there are zero */
    for (size_t i = 2; i < 16; i++)
        assert_int_equal(le32(h + PB_OFF_KEYS + 4 * i), 0);

    /* Nothing pb booted without */
    assert_int_equal(le32(h + PB_OFF_STATUS), 0);
    assert_string_equal((const char *)h + PB_OFF_VERSION, VERSION);
    assert_memory_equal(h + PB_OFF_VERSION + strlen(VERSION), zeros, 32 - strlen(VERSION));
}

/* How the boot was chosen: 0, 1, 2 (boot.md 5.6) */
static void test_boot_mode(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    info.boot_mode = BOOT_POLICY_MODE_FALLBACK;
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_BOOT_MODE), 1);

    info.boot_mode = BOOT_POLICY_MODE_SPECULATIVE;
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_BOOT_MODE), 2);

    /* A mode that has no value isn't handed over as another one */
    info.boot_mode = (enum boot_policy_mode)3;
    assert_int_equal(prepare(), -PB_ERR_PARAM);
    assert_next_is_empty();
    assert_no_list();
}

/* What a boot doesn't have (boot.md 5.4, 5.6): a policy without names, a
 * stream the host named no partition for, a board without a life cycle */
static void test_what_a_boot_does_not_have(void **state)
{
    static const uint8_t zeros[16];
    const uint8_t *h;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    info.system_name = NULL;
    uuid_clear(info.boot_partition_uuid);
    info.slc = BOOT_INFO_NO_SLC;
    info.no_of_available_keys = 0;
    assert_int_equal(prepare(), PB_OK);
    h = pbs_structure();

    /* Empty */
    assert_memory_equal(h + PB_OFF_SYSTEM_NAME, zeros, 8);
    /* All zero: none */
    assert_memory_equal(h + PB_OFF_PART_UUID, zeros, 16);
    /* -1 */
    assert_int_equal(le32(h + PB_OFF_SLC), 0xffffffff);
    assert_int_equal(le32(h + PB_OFF_NO_OF_KEYS), 0);

    /* A partition that is no system of the policy's */
    info.system_name = "?";
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal((const char *)pbs_structure() + PB_OFF_SYSTEM_NAME, "?");
}

/* pb's status is the core's to collect (F-248): the handover writes the
 * bits it is given (boot.md 5.6). The next stage's names for them are
 * pb_fh.h's, with the bits boot.md 5.6 and D-050 give them: fixed like the
 * layout */
static void test_status_is_the_cores(void **state)
{
    (void)state;

    assert_int_equal(PB_FH_STATUS_WATCHDOG_NOT_ARMED, 1u << 0);
    assert_int_equal(PB_FH_STATUS_BOARD_DEGRADED, 1u << 1);

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    /* Nothing to report */
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_STATUS), 0);

    info.status = BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED;
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_STATUS), PB_FH_STATUS_WATCHDOG_NOT_ARMED);

    info.status = BOOT_INFO_STATUS_BOARD_DEGRADED;
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_STATUS), PB_FH_STATUS_BOARD_DEGRADED);

    info.status = BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED | BOOT_INFO_STATUS_BOARD_DEGRADED;
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(le32(pbs_structure() + PB_OFF_STATUS), 3);
}

/* pb's structure has room for 16 keys (boot.md 5.4). A build with more
 * doesn't compile; one that got past that doesn't hand over a part of them */
static void test_sixteen_keys(void **state)
{
    const uint8_t *h;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    available_keys(16);
    assert_int_equal(prepare(), PB_OK);
    h = pbs_structure();
    assert_int_equal(le32(h + PB_OFF_NO_OF_KEYS), 16);
    assert_int_equal(le32(h + PB_OFF_KEYS), 0x1000);
    assert_int_equal(le32(h + PB_OFF_KEYS + 4 * 15), 0x100f);
    /* The field after the keys is its own */
    assert_int_equal(le32(h + PB_OFF_STATUS), 0);

    available_keys(17);
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_next_is_empty();
    assert_no_list();
}

/* The strings are NUL terminated in their fields: one that doesn't fit
 * isn't cut */
static void test_strings_that_do_not_fit(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    /* 31 characters and the NUL fit */
    info.version = "3.0.0-0123456789012345678901234";
    assert_int_equal(strlen(info.version), 31);
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal((const char *)pbs_structure() + PB_OFF_VERSION, info.version);

    info.version = "3.0.0-01234567890123456789012345";
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_next_is_empty();
    assert_no_list();

    info.version = VERSION;
    info.system_name = "1234567";
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal((const char *)pbs_structure() + PB_OFF_SYSTEM_NAME, "1234567");

    info.system_name = "12345678";
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_next_is_empty();
    assert_no_list();
}

/* Prepare is part of a candidate's attempt (boot.md 2): what the candidate
 * before left in the region is no list when this one fails, and this one's
 * when it succeeds */
static void test_a_failed_candidate_leaves_no_list(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_true(tl_is_valid(REGION));

    info.boot_mode = (enum boot_policy_mode)3;
    assert_int_equal(prepare(), -PB_ERR_PARAM);
    assert_no_list();

    /* The next candidate, System B */
    info.boot_mode = BOOT_POLICY_MODE_FALLBACK;
    info.system_name = "B";
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal((const char *)pbs_structure() + PB_OFF_SYSTEM_NAME, "B");

    /* One that has no entry */
    assert_int_equal(bpak_get_part(hdr, ID_APP, &part), BPAK_OK);
    part->size = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();
    assert_no_list();
}

/* The region is the handover's: an image with a part in it is refused
 * before the list is written, so what was hashed there stays (boot.md 4.3,
 * 5.4). In place, the part is where the board's map has it */
static void test_a_part_in_the_region_is_refused(void **state)
{
    struct bpak_meta_header *meta;

    static const struct {
        uint64_t addr;
        int rc;
    } cases[] = {
        /* Inside, over its start and over its end */
        { CONFIG_BOOT_FH_REGION_BASE + 0x100, -PB_ERR_MEM },
        { CONFIG_BOOT_FH_REGION_BASE - 4096 + 1, -PB_ERR_MEM },
        { CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE - 1, -PB_ERR_MEM },
        /* Before it and after it */
        { CONFIG_BOOT_FH_REGION_BASE - 4096, PB_OK },
        { CONFIG_BOOT_FH_REGION_BASE + CONFIG_BOOT_FH_REGION_SIZE, PB_OK },
        /* One that wraps is nowhere */
        { UINT64_MAX - 100, -PB_ERR_MEM },
    };

    (void)state;

    add_app();
    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, 0);
    assert_int_equal(bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, ID_OTHER, &meta), BPAK_OK);
    assert_int_equal(boot_handover_fh_init(&cfg_app), PB_OK);

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        memset(REGION, 0xa5, REGION_SIZE);
        *bpak_get_meta_ptr(hdr, meta, uint64_t) = cases[i].addr;
        assert_int_equal(prepare(), cases[i].rc);

        if (cases[i].rc == PB_OK) {
            assert_true(tl_is_valid(REGION));
            continue;
        }

        assert_next_is_empty();

        /* Nothing was written */
        for (size_t n = 0; n < REGION_SIZE; n++)
            assert_int_equal(REGION[n], 0xa5);
    }
}

/* The device tree in the list, as a receiver finds it */
static const void *the_device_tree(size_t *size)
{
    const uint8_t *te = the_entry(TAG_FDT);
    const void *fdt = te + TE_HDR_SIZE;

    assert_int_equal(fdt_check_header(fdt), 0);
    /* The entry's data is the tree, all of it */
    assert_int_equal(fdt_totalsize(fdt), le32(te + 4));

    if (size)
        *size = le32(te + 4);

    return fdt;
}

static const char *chosen_string(const void *fdt, const char *name)
{
    int offset = fdt_path_offset(fdt, "/chosen");
    const char *value;

    assert_true(offset >= 0);
    value = fdt_getprop(fdt, offset, name, NULL);
    assert_non_null(value);
    return value;
}

/* With a device tree part the list holds the image's device tree, copied
 * into the list's FDT entry with the headroom, and nothing else: never both
 * (boot.md 5.4) */
static void test_list_holds_the_device_tree(void **state)
{
    uint8_t *before;
    const void *fdt;
    size_t size;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    before = malloc(DTB_BUF_SIZE);
    assert_non_null(before);
    memcpy(before, dtb, DTB_BUF_SIZE);

    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);
    assert_int_equal(prepare(), PB_OK);

    fdt = the_device_tree(&size);
    /* The first entry's data: the register points into the list */
    assert_ptr_equal(fdt, REGION + DATA_OFFSET);
    assert_registers((uintptr_t)fdt);
    assert_true(next.entry == APP_ADDR);
    /* The tree and the headroom, which is the region's */
    assert_int_equal(size, DTB_PART_SIZE + CONFIG_BOOT_DTB_HEADROOM);
    assert_int_equal(le32(REGION + TL_OFF_USED_SIZE), DATA_OFFSET + size);
    assert_int_equal(le32(REGION + TL_OFF_TOTAL_SIZE), REGION_SIZE);
    /* The whole tree is cleaned out of the cache with the list */
    assert_true(cleaned_start == (uintptr_t)REGION);
    assert_int_equal(cleaned_len, DATA_OFFSET + size);

    /* Never both: pb's information is in the tree */
    assert_null(tl_find(REGION, PB_FH_TAG_HANDOVER));

    /* The image's tree, with what the board and pb added to the copy */
    assert_string_equal(fdt_getprop(fdt, 0, "model", NULL), "the image's");
    assert_string_equal(chosen_string(fdt, "bootargs"), "console=ttyS0");
    assert_string_equal(chosen_string(fdt, "pb,device-uuid"), DEVICE_UUID);
    assert_string_equal(chosen_string(fdt, "pb,active-system"), "A");
    assert_string_equal(chosen_string(fdt, "pb,boot-partition"), BOOT_PART_UUID);
    assert_string_equal(chosen_string(fdt, "pb,boot-mode"), "normal");
    /* pb's value, not the one the image's tree came with */
    assert_string_equal(chosen_string(fdt, "pb,version"), VERSION);

    /* The part is as it was hashed, and so is the memory after it: this
     * handover needs no headroom at the part's load address */
    assert_memory_equal(dtb, before, DTB_BUF_SIZE);
    free(before);
    assert_guard_is_untouched();
}

/* With a device tree in the list pb's status is in its /chosen, like the
 * rest of pb's information (boot.md 5.4, 5.6): pb,status, a list of strings,
 * there and empty when there is nothing to report */
static void test_status_in_the_device_tree(void **state)
{
    const void *fdt;
    const char *list;
    int len = -1;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);

    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(NULL);
    assert_non_null(fdt_getprop(fdt, fdt_path_offset(fdt, "/chosen"), "pb,status", &len));
    assert_int_equal(len, 0);

    info.status = BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED | BOOT_INFO_STATUS_BOARD_DEGRADED;
    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(NULL);
    list = fdt_getprop(fdt, fdt_path_offset(fdt, "/chosen"), "pb,status", &len);
    assert_non_null(list);
    assert_int_equal(len, 34);
    assert_memory_equal(list, "watchdog-not-armed\0board-degraded", 34);
}

/* The board's hook runs on the copy, with the whole tree, before pb writes
 * its properties: pb's values win (boot.md 5.5) */
static void test_hook_runs_on_the_copy_before_pbs_properties(void **state)
{
    const void *fdt;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    hook_action = HOOK_SETS_PBS_PROPERTIES;

    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(NULL);

    assert_int_equal(patch_calls, 1);
    assert_ptr_equal(patch_fdt, fdt);
    /* Only the image's own pb,version was there */
    assert_int_equal(hook_saw_pbs_properties, 1);
    assert_string_equal(chosen_string(fdt, "pb,device-uuid"), DEVICE_UUID);
    assert_string_equal(chosen_string(fdt, "pb,active-system"), "A");
    assert_string_equal(chosen_string(fdt, "pb,boot-mode"), "normal");
    assert_string_equal(chosen_string(fdt, "pb,version"), VERSION);
    assert_string_equal(chosen_string(fdt, "bootargs"), "console=ttyS0");
}

/* The hook is optional */
static void test_device_tree_without_a_hook(void **state)
{
    const void *fdt;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);

    assert_int_equal(boot_handover_fh_init(&cfg_dt_no_hook), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(NULL);
    assert_int_equal(patch_calls, 0);
    assert_string_equal(chosen_string(fdt, "pb,device-uuid"), DEVICE_UUID);
}

/* The kernel command line is the handover's data (boot.md 5.5), written into
 * the copy before the hook: the hook's own wins */
static void test_bootargs_in_the_copy(void **state)
{
    static const struct boot_handover_fh_config cfg = {
        .entry_bpak_id = ID_APP,
        .dtb_bpak_id = ID_DTB,
        .bootargs = "quiet",
        .bootargs_verbose = "verbose",
    };
    static const struct boot_handover_fh_config with_hook = {
        .entry_bpak_id = ID_APP,
        .dtb_bpak_id = ID_DTB,
        .bootargs = "quiet",
        .dtb_patch_cb = patch_dtb,
    };
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);

    assert_int_equal(boot_handover_fh_init(&cfg), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal(chosen_string(the_device_tree(NULL), "bootargs"), "quiet");

    boot_flags = BOOT_FLAG_VERBOSE;
    assert_int_equal(prepare(), PB_OK);
    assert_string_equal(chosen_string(the_device_tree(NULL), "bootargs"), "verbose");

    assert_int_equal(boot_handover_fh_init(&with_hook), PB_OK);
    assert_int_equal(prepare(), PB_OK);
    assert_int_equal(patch_calls, 1);
    assert_string_equal(chosen_string(the_device_tree(NULL), "bootargs"), "console=ttyS0");
}

/* pb's structure has no command line: one without a device tree part is
 * refused */
static void test_bootargs_need_a_device_tree(void **state)
{
    static const struct boot_handover_fh_config bootargs_only = {
        .entry_bpak_id = ID_APP,
        .bootargs = "quiet",
    };
    static const struct boot_handover_fh_config verbose_only = {
        .entry_bpak_id = ID_APP,
        .bootargs_verbose = "verbose",
    };
    (void)state;

    assert_int_equal(boot_handover_fh_init(&bootargs_only), -PB_ERR_PARAM);
    assert_int_equal(boot_handover_fh_init(&verbose_only), -PB_ERR_PARAM);
}

/* A failure in the hook is the candidate's (boot.md 2) */
static void test_hook_fails(void **state)
{
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    patch_rc = -PB_ERR_IO;

    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_IO);
    assert_next_is_empty();
    assert_no_list();
}

/* The hook has the whole tree. What it leaves isn't handed over when it is
 * larger than the region's room, and is the entry's data when it is smaller */
static void test_hook_changes_the_trees_size(void **state)
{
    const void *fdt;
    size_t size;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);

    hook_action = HOOK_GROWS_PAST_THE_REGION;
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_next_is_empty();
    assert_no_list();
    assert_guard_is_untouched();

    /* Packed: pb's properties need room again, which a packed tree doesn't
     * have. The released ones fail the boot when they don't fit (boot.md
     * 5.5), also here */
    hook_action = HOOK_PACKS;
    assert_int_equal(prepare(), -PB_ERR);
    assert_no_list();

    /* Smaller, with room for pb's: the entry's data is the tree as the hook
     * left it */
    hook_action = HOOK_SHRINKS;
    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(&size);
    assert_true(size < DTB_PART_SIZE);
    assert_int_equal(le32(REGION + TL_OFF_USED_SIZE), DATA_OFFSET + ((size + 7) & ~(size_t)7));
    assert_string_equal(chosen_string(fdt, "pb,version"), VERSION);
    assert_registers((uintptr_t)fdt);

    /* A tree of a size that is no multiple of 8: the entry's data_size is
     * the tree's, the used part ends at the next multiple of 8 and the
     * padding is zero and counted */
    hook_action = HOOK_BOOTARGS;
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_app();
    add_dtb(DTB_PART_SIZE - 3, DTB_PART_SIZE);
    assert_int_equal(prepare(), PB_OK);
    fdt = the_device_tree(&size);
    assert_int_equal(size, DTB_PART_SIZE - 3 + CONFIG_BOOT_DTB_HEADROOM);
    assert_int_equal(le32(REGION + TL_OFF_USED_SIZE), DATA_OFFSET + size + 3);
    assert_int_equal(REGION[DATA_OFFSET + size], 0);
    assert_int_equal(REGION[DATA_OFFSET + size + 2], 0);
    assert_int_equal(sum_of(REGION, DATA_OFFSET + size + 3), 0);
    assert_string_equal(chosen_string(fdt, "pb,version"), VERSION);
}

/* The hook may make the tree larger than the region's room and fill it up
 * to the room's end. pb's properties are written with the size the tree
 * says: they would move the strings past the region. Such a tree is refused
 * before pb writes to it, and nothing is written after the region (boot.md
 * 5.4: the list is in the region) */
static void test_hook_fills_the_tree_past_the_room(void **state)
{
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);

    hook_action = HOOK_FILLS_TO_THE_REGIONS_END;
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_int_equal(patch_calls, 1);
    assert_guard_is_untouched();
    assert_next_is_empty();
    assert_no_list();

    /* What the hook left is as it left it: no property of pb's was added */
    assert_int_equal(fdt_totalsize(REGION + DATA_OFFSET), ROOM + 4096);
    assert_true(fdt_used(REGION + DATA_OFFSET) <= ROOM);
    assert_null(fdt_getprop(REGION + DATA_OFFSET,
                            fdt_path_offset(REGION + DATA_OFFSET, "/chosen"),
                            "pb,device-uuid",
                            NULL));
}

/* An image without the part the board names fails to boot (boot.md 5.4) */
static void test_image_without_the_device_tree_part(void **state)
{
    (void)state;

    add_app();
    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_next_is_empty();
    assert_no_list();

    /* A part without a load address, and one that is no device tree */
    add_part(ID_DTB, DTB_PART_SIZE);
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_no_list();

    memset(dtb, 0, DTB_BUF_SIZE);
    add_load_addr(ID_DTB, (uintptr_t)dtb);
    assert_int_equal(prepare(), -PB_ERR);
    assert_next_is_empty();
    assert_no_list();
    assert_int_equal(patch_calls, 0);
}

/* The tree's header is read in bytes the payload's hash covers: a part that
 * is smaller than the header, or than the tree says it is, is refused
 * (security.md 4.2, F-330) */
static void test_device_tree_is_inside_its_part(void **state)
{
    struct bpak_part_header *part;
    (void)state;

    add_app();
    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);
    assert_int_equal(bpak_get_part(hdr, ID_DTB, &part), BPAK_OK);
    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);

    part->size = DTB_PART_SIZE - 512;
    assert_int_equal(prepare(), -PB_ERR);
    assert_no_list();

    part->size = 0;
    assert_int_equal(prepare(), -PB_ERR_BAD_META);
    assert_no_list();
    assert_int_equal(patch_calls, 0);
}

/* A device tree that doesn't fit the region with its headroom fails to
 * boot (boot.md 5.4). The room is the region less the list's two headers */
static void test_device_tree_that_does_not_fit_the_region(void **state)
{
    const size_t largest = ROOM - CONFIG_BOOT_DTB_HEADROOM;
    size_t size;
    (void)state;

    assert_int_equal(boot_handover_fh_init(&cfg_dt), PB_OK);

    /* To the region's last byte */
    add_app();
    add_dtb(largest, largest);
    assert_int_equal(prepare(), PB_OK);
    the_device_tree(&size);
    assert_int_equal(size, ROOM);
    assert_int_equal(le32(REGION + TL_OFF_USED_SIZE), REGION_SIZE);
    assert_guard_is_untouched();

    /* One byte more */
    assert_int_equal(bpak_init_header(hdr), BPAK_OK);
    add_app();
    add_dtb(largest + 1, largest + 512);
    assert_int_equal(prepare(), -PB_ERR_MEM);
    assert_next_is_empty();
    assert_no_list();
    assert_guard_is_untouched();
    assert_int_equal(patch_calls, 1);
}

/* dtb.c's part of it (boot.md 5.5): the copy goes where the handover says,
 * with room that is all its own, and a part that lies there isn't read
 * while it is written */
static void test_open_into(void **state)
{
    uint8_t *dest = REGION + DATA_OFFSET;
    (void)state;

    add_dtb(DTB_PART_SIZE, DTB_PART_SIZE);

    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, 256, dest, DTB_PART_SIZE + 256), PB_OK);
    assert_int_equal(fdt_check_header(dest), 0);
    assert_int_equal(fdt_totalsize(dest), DTB_PART_SIZE + 256);
    /* What the tree doesn't use of its room is zero, not what was there */
    assert_int_equal(dest[DTB_PART_SIZE + 255], 0);
    assert_int_equal(dest[DTB_PART_SIZE + 256], 0xa5);

    /* One byte short */
    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, 256, dest, DTB_PART_SIZE + 255), -PB_ERR_MEM);
    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, SIZE_MAX, dest, ROOM), -PB_ERR_MEM);
    assert_int_equal(boot_dtb_open_into(hdr, ID_OTHER, 256, dest, ROOM), -PB_ERR_BAD_META);

    /* The room is in the part, at its start or over its last byte */
    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, 256, dtb, DTB_BUF_SIZE), -PB_ERR_MEM);
    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, 256, dtb + DTB_PART_SIZE - 1, 8192),
                     -PB_ERR_MEM);
    /* Right after the part it isn't */
    assert_int_equal(boot_dtb_open_into(hdr, ID_DTB, 256, dtb + DTB_PART_SIZE, 8192), PB_OK);
    assert_int_equal(fdt_check_header(dtb), 0);
    assert_int_equal(fdt_totalsize(dtb), DTB_PART_SIZE);
}

/* The handover the way src/board.c uses it: only through the board's object */
static void test_handover_object(void **state)
{
    (void)state;

    add_part(ID_OTHER, 4096);
    add_load_addr(ID_OTHER, APP_ADDR + 0x100000);
    add_app();

    assert_ptr_equal(handover.ops, &boot_handover_fh_ops);
    assert_ptr_equal(handover.config, &cfg_app);

    /* The module has the configuration with the other entry part, so that
     * the entry below shows that the object's reached it */
    assert_int_equal(boot_handover_fh_init(&cfg_other), PB_OK);

    /* An error of the module's is init's answer */
    assert_int_equal(handover.ops->init(NULL), -PB_ERR_PARAM);

    assert_int_equal(handover.ops->init(handover.config), PB_OK);
    memset(&next, 0, sizeof(next));
    assert_int_equal(handover.ops->prepare(hdr, &info, &next), PB_OK);

    assert_true(next.entry == APP_ADDR);
    assert_registers(0);
    assert_string_equal((const char *)pbs_structure() + PB_OFF_SYSTEM_NAME, "A");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_prepare_before_init, setup),
        cmocka_unit_test_setup(test_init_rejects, setup),
        cmocka_unit_test_setup(test_entry_is_the_load_address, setup),
        cmocka_unit_test_setup(test_entry_in_place, setup),
        cmocka_unit_test_setup(test_no_entry_part, setup),
        cmocka_unit_test_setup(test_no_load_address, setup),
        cmocka_unit_test_setup(test_empty_entry_part_is_refused, setup),
        cmocka_unit_test_setup(test_entry_part_of_the_minimum, setup),
        cmocka_unit_test_setup(test_empty_part_that_is_not_the_entry, setup),
        cmocka_unit_test_setup(test_registers_without_a_device_tree, setup),
        cmocka_unit_test_setup(test_list_header, setup),
        cmocka_unit_test_setup(test_checksum_makes_the_used_bytes_sum_to_zero, setup),
        cmocka_unit_test_setup(test_list_holds_pbs_structure, setup),
        cmocka_unit_test_setup(test_pbs_tag_is_not_a_standard_one, setup),
        cmocka_unit_test_setup(test_pbs_structure, setup),
        cmocka_unit_test_setup(test_boot_mode, setup),
        cmocka_unit_test_setup(test_what_a_boot_does_not_have, setup),
        cmocka_unit_test_setup(test_status_is_the_cores, setup),
        cmocka_unit_test_setup(test_status_in_the_device_tree, setup),
        cmocka_unit_test_setup(test_sixteen_keys, setup),
        cmocka_unit_test_setup(test_strings_that_do_not_fit, setup),
        cmocka_unit_test_setup(test_a_failed_candidate_leaves_no_list, setup),
        cmocka_unit_test_setup(test_a_part_in_the_region_is_refused, setup),
        cmocka_unit_test_setup(test_list_holds_the_device_tree, setup),
        cmocka_unit_test_setup(test_hook_runs_on_the_copy_before_pbs_properties, setup),
        cmocka_unit_test_setup(test_device_tree_without_a_hook, setup),
        cmocka_unit_test_setup(test_bootargs_in_the_copy, setup),
        cmocka_unit_test_setup(test_bootargs_need_a_device_tree, setup),
        cmocka_unit_test_setup(test_hook_fails, setup),
        cmocka_unit_test_setup(test_hook_changes_the_trees_size, setup),
        cmocka_unit_test_setup(test_hook_fills_the_tree_past_the_room, setup),
        cmocka_unit_test_setup(test_image_without_the_device_tree_part, setup),
        cmocka_unit_test_setup(test_device_tree_is_inside_its_part, setup),
        cmocka_unit_test_setup(test_device_tree_that_does_not_fit_the_region, setup),
        cmocka_unit_test_setup(test_open_into, setup),
        cmocka_unit_test_setup(test_handover_object, setup),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
