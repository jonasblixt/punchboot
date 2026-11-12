/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Room in the device tree for /chosen and pb's properties, so that a board's
 * device tree doesn't need placeholders or free space for them (F-071), and
 * the properties: pb's information in a device tree (boot.md 5.5, 5.6). The
 * core collected it; nothing here reads the RoT, the SLC or the policy.
 */

#include <boot/dtb.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <libfdt.h>
#include <limits.h>
#include <pb/pb.h>

IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _zero_region_start, bss_start);
IMPORT_SYM(uintptr_t, _zero_region_end, bss_end);
IMPORT_SYM(uintptr_t, _no_init_start, no_init_start);
IMPORT_SYM(uintptr_t, _no_init_end, no_init_end);

/* [a, a + a_len) and [b, b + b_len) share a byte */
static bool overlaps(uintptr_t a, size_t a_len, uintptr_t b, size_t b_len)
{
    return a < b + b_len && b < a + a_len;
}

static bool overlaps_pb(uintptr_t addr, size_t len)
{
    const struct {
        uintptr_t start;
        uintptr_t end;
    } regions[] = {
        { code_start, code_end }, { data_start, data_end },       { stack_start, stack_end },
        { bss_start, bss_end },   { no_init_start, no_init_end },
    };

    for (size_t i = 0; i < ARRAY_SIZE(regions); i++) {
        if (overlaps(addr, len, regions[i].start, regions[i].end - regions[i].start))
            return true;
    }

    return false;
}

int boot_dtb_grow(struct bpak_header *hdr, bpak_id_t dtb_id, size_t headroom, void **fdt)
{
    int rc;
    struct bpak_part_header *ph;
    struct bpak_meta_header *mh;

    if (bpak_get_part(hdr, dtb_id, &ph) != BPAK_OK) {
        LOG_ERR("Could not read dtb bpak part meta");
        return -PB_ERR_BAD_META;
    }

    if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, dtb_id, &mh) != BPAK_OK) {
        LOG_ERR("Could not read dtb load addr meta");
        return -PB_ERR_BAD_META;
    }

    uintptr_t addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);
    size_t part_size = bpak_part_size(ph);
    size_t room = part_size + headroom;

    if (room < part_size || room > INT_MAX || addr + room < addr) {
        LOG_ERR("Device tree headroom too large");
        return -PB_ERR_MEM;
    }

    /* Before the device tree is read: the parts are only checked against pb */
    if (overlaps_pb(addr, room)) {
        LOG_ERR("Device tree with headroom overlaps pb");
        return -PB_ERR_MEM;
    }

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (p->id == dtb_id || bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, p->id, &mh) != BPAK_OK)
            continue;

        uintptr_t p_addr = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);

        if (overlaps(addr, room, p_addr, bpak_part_size(p))) {
            LOG_ERR("Device tree with headroom overlaps part 0x%x", p->id);
            return -PB_ERR_MEM;
        }
    }

    *fdt = (void *)addr;
    rc = fdt_check_header(*fdt);

    if (rc < 0) {
        LOG_ERR("Invalid device tree");
        return -PB_ERR;
    }

    size_t size = fdt_totalsize(*fdt);

    if (size > part_size) {
        LOG_ERR("Device tree (%zu bytes) is larger than its part (%zu)", size, part_size);
        return -PB_ERR;
    }

    rc = fdt_open_into(*fdt, *fdt, (int)(size + headroom));

    if (rc < 0) {
        LOG_ERR("Could not grow the device tree (%i)", rc);
        return -PB_ERR;
    }

    LOG_DBG("Device tree at 0x%" PRIxPTR ", %zu -> %zu bytes", addr, size, size + headroom);
    return PB_OK;
}

/* /chosen, added to a tree that has none */
static int chosen_node(void *fdt)
{
    int offset = fdt_subnode_offset(fdt, 0, "chosen");

    if (offset == -FDT_ERR_NOTFOUND)
        offset = fdt_add_subnode(fdt, 0, "chosen");

    if (offset < 0)
        LOG_ERR("Could not add /chosen (%i)", offset);

    return offset;
}

static int prop_result(const char *name, int rc)
{
    if (rc == 0)
        return PB_OK;

    LOG_ERR("fdt error: %s (%i)", name, rc);
    return -PB_ERR;
}

/* Not there is fine */
static int del_prop(void *fdt, int offset, const char *name)
{
    int rc = fdt_delprop(fdt, offset, name);

    return prop_result(name, rc == -FDT_ERR_NOTFOUND ? 0 : rc);
}

/*
 * A string every release wrote. NULL: pb has no value for it and writes
 * none. What the image's tree or the board's hook has under the name then
 * stays, as it did: pb's values win, pb doesn't remove (D-008, F-348). No
 * room for it fails the boot, as it did.
 */
static int set_string(void *fdt, int offset, const char *name, const char *value)
{
    if (value == NULL)
        return PB_OK;

    return prop_result(name, fdt_setprop_string(fdt, offset, name, value));
}

/*
 * A string that is new with pb's information (boot.md 5.6). A device tree
 * built with room for the released properties only (placeholders and a
 * headroom of 0, the 2.x way) has none for it: the property is left out with
 * a warning and the boot goes on, so that an image that booted still boots
 * (D-008). Left out, or NULL (no value): no property of the name is in the
 * tree, also not one the image or the hook brought. It isn't pb's, and no
 * release wrote the name.
 */
static int set_new_string(void *fdt, int offset, const char *name, const char *value)
{
    if (value != NULL) {
        int rc = fdt_setprop_string(fdt, offset, name, value);

        if (rc != -FDT_ERR_NOSPACE)
            return prop_result(name, rc);

        LOG_WARN("No room in the device tree for %s: left out", name);
    }

    return del_prop(fdt, offset, name);
}

static int set_keys(void *fdt, int offset, const struct boot_info *info)
{
    int rc;

    /* The key the image is signed with */
    rc = fdt_setprop_u32(fdt, offset, "pb,slc-active-key", info->active_key);

    if (rc != 0)
        return prop_result("pb,slc-active-key", rc);

    /* The list is appended below: no key left is no property, as it was */
    rc = del_prop(fdt, offset, "pb,slc-available-keys");

    for (size_t i = 0; rc == PB_OK && i < info->no_of_available_keys; i++) {
        rc = prop_result(
            "pb,slc-available-keys",
            fdt_appendprop_u32(fdt, offset, "pb,slc-available-keys", info->available_keys[i]));
    }

    return rc;
}

static int
set_initrd(void *fdt, int offset, struct bpak_header *hdr, const struct boot_dtb_chosen *chosen)
{
    struct bpak_part_header *ph;
    uint64_t start = chosen->ramdisk_addr;
    uint64_t end;
    int rc;

    if (bpak_get_part(hdr, chosen->ramdisk_id, &ph) != BPAK_OK) {
        LOG_ERR("Could not read ramdisk metadata");
        return -PB_ERR_BAD_META;
    }

    end = start + bpak_part_size(ph);

    /* One cell each where both fit, the bytes every release wrote. Two cells
     * each when one doesn't: it used to be truncated (boot.md 5.2) */
    if (start <= UINT32_MAX && end <= UINT32_MAX) {
        rc = fdt_setprop_u32(fdt, offset, "linux,initrd-start", (uint32_t)start);

        if (rc == 0)
            rc = fdt_setprop_u32(fdt, offset, "linux,initrd-end", (uint32_t)end);
    } else {
        rc = fdt_setprop_u64(fdt, offset, "linux,initrd-start", start);

        if (rc == 0)
            rc = fdt_setprop_u64(fdt, offset, "linux,initrd-end", end);
    }

    if (rc != 0) {
        LOG_ERR("fdt error: ramdisk (%i)", rc);
        return -PB_ERR;
    }

    LOG_DBG("Ramdisk %" PRIx64 " -> %" PRIx64, start, end);
    return PB_OK;
}

/* The longest of the modes' names: room for pb,boot-mode is asked for with it */
#define MODE_NAME_LONGEST "speculative"

/* The strings of pb,boot-mode (boot.md 5.6). They are never renamed */
static const char *mode_name(enum boot_policy_mode mode)
{
    switch (mode) {
    case BOOT_POLICY_MODE_NORMAL:
        return "normal";
    case BOOT_POLICY_MODE_FALLBACK:
        return "fallback";
    case BOOT_POLICY_MODE_SPECULATIVE:
        return "speculative";
    }

    return NULL;
}

int boot_dtb_set_chosen(void *fdt,
                        struct bpak_header *hdr,
                        const struct boot_info *info,
                        const struct boot_dtb_chosen *chosen)
{
    const char *mode = mode_name(info->boot_mode);
    char uu_str[37];
    int offset;
    int rc;

    if (mode == NULL) {
        LOG_ERR("A boot mode without a name (%i)", (int)info->boot_mode);
        return -PB_ERR_PARAM;
    }

    offset = chosen_node(fdt);

    if (offset < 0)
        return -PB_ERR;

    /* The board's hook first, with the whole tree (boot.md 5.5) */
    if (chosen->patch_cb) {
        rc = chosen->patch_cb(fdt, offset);

        if (rc != PB_OK) {
            LOG_ERR("The board's device tree hook failed (%i)", rc);
            return rc;
        }

        /* What it did to the tree may have moved /chosen, or removed it */
        offset = chosen_node(fdt);

        if (offset < 0)
            return -PB_ERR;
    }

    /* Then pb's properties, so that pb's values win. The ones every release
     * wrote come first, with the bytes they had (boot.md 7) */
    uuid_unparse(info->device_uuid, uu_str);
    rc = set_string(fdt, offset, "pb,device-uuid", uu_str);

    if (rc != PB_OK)
        return rc;

    /* A board without an SLC (no fuses) gets no pb,slc from pb */
    if (info->slc != BOOT_INFO_NO_SLC) {
        rc = prop_result("pb,slc", fdt_setprop_u32(fdt, offset, "pb,slc", (uint32_t)info->slc));

        if (rc != PB_OK)
            return rc;
    }

    rc = set_keys(fdt, offset, info);

    if (rc != PB_OK)
        return rc;

    if (chosen->ramdisk_id) {
        rc = set_initrd(fdt, offset, hdr, chosen);

        if (rc != PB_OK)
            return rc;
    }

    /* "A", "B" or "?" from a policy that has systems (A/B), else not written */
    rc = set_string(fdt, offset, "pb,active-system", info->system_name);

    if (rc != PB_OK)
        return rc;

    /*
     * The new ones, each left out when the tree has no room for it: the
     * mode first, then the partition, then the version. Room for the mode
     * is asked for with the longest name, and its value is set last, so
     * that which of them a next stage finds doesn't depend on how this boot
     * was chosen: a tree that has "normal" and not "fallback" would lose
     * the mode, or the version after it, in the boot that fell back.
     */
    rc = set_new_string(fdt, offset, "pb,boot-mode", MODE_NAME_LONGEST);

    if (rc != PB_OK)
        return rc;

    /* No partition: 'boot bpak' named none */
    if (uuid_is_null(info->boot_partition_uuid)) {
        rc = set_new_string(fdt, offset, "pb,boot-partition", NULL);
    } else {
        uuid_unparse(info->boot_partition_uuid, uu_str);
        rc = set_new_string(fdt, offset, "pb,boot-partition", uu_str);
    }

    if (rc != PB_OK)
        return rc;

    rc = set_new_string(fdt, offset, "pb,version", info->version);

    if (rc != PB_OK)
        return rc;

    /* The mode itself, where it got its room: it is never longer */
    if (fdt_getprop(fdt, offset, "pb,boot-mode", NULL) != NULL)
        rc = prop_result("pb,boot-mode", fdt_setprop_string(fdt, offset, "pb,boot-mode", mode));

    return rc;
}
