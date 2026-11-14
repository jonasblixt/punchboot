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
 *
 * The room is after the part, where the image handover leaves the tree
 * (boot_dtb_grow()), or in memory the handover gives, where Firmware Handoff
 * copies it to: its transfer list (boot_dtb_open_into(), boot.md 5.4).
 */

#include <boot/boot.h>
#include <boot/dtb.h>
#include <boot/image_helpers.h>
#include <boot/image_regions.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <libfdt.h>
#include <limits.h>
#include <pb/pb.h>
#include <string.h>

/* [a, a + a_len) and [b, b + b_len) share a byte */
static bool overlaps(uintptr_t a, size_t a_len, uintptr_t b, size_t b_len)
{
    return a < b + b_len && b < a + a_len;
}

/* The device tree's part: its load address and what was hashed there */
static int dtb_part(struct bpak_header *hdr, bpak_id_t dtb_id, uintptr_t *addr, size_t *part_size)
{
    struct bpak_part_header *ph;
    uint64_t load_addr;

    if (bpak_get_part(hdr, dtb_id, &ph) != BPAK_OK) {
        LOG_ERR("Could not read dtb bpak part meta");
        return -PB_ERR_BAD_META;
    }

    if (boot_image_load_addr(hdr, dtb_id, &load_addr) != PB_OK) {
        LOG_ERR("Could not read dtb load addr meta");
        return -PB_ERR_BAD_META;
    }

    *addr = (uintptr_t)load_addr;
    *part_size = bpak_part_size(ph);

    /* The header is read at the load address: it is in bytes the payload's
     * hash covers. bpak_part_size() is what was hashed there, the padding
     * too; an empty part passes the hash with nothing there (security.md
     * 4.2, F-330) */
    if (*part_size < sizeof(struct fdt_header)) {
        LOG_ERR("The device tree's part is smaller than an FDT header");
        return -PB_ERR_BAD_META;
    }

    return PB_OK;
}

/* The size of the device tree in a part of 'part_size' bytes: a device tree
 * that is inside its part */
static int dtb_size(const void *fdt, size_t part_size, size_t *size)
{
    if (fdt_check_header(fdt) < 0) {
        LOG_ERR("Invalid device tree");
        return -PB_ERR;
    }

    *size = fdt_totalsize(fdt);

    /* fdt_check_header() checks the magic and the version only */
    if (*size < sizeof(struct fdt_header)) {
        LOG_ERR("Device tree (%zu bytes) is smaller than its header", *size);
        return -PB_ERR;
    }

    if (*size > part_size) {
        LOG_ERR("Device tree (%zu bytes) is larger than its part (%zu)", *size, part_size);
        return -PB_ERR;
    }

    return PB_OK;
}

int boot_dtb_grow(struct bpak_header *hdr, bpak_id_t dtb_id, size_t headroom, void **fdt)
{
    int rc;
    uint64_t p_load_addr;
    uintptr_t addr;
    size_t part_size;
    size_t size;

    rc = dtb_part(hdr, dtb_id, &addr, &part_size);

    if (rc != PB_OK)
        return rc;

    size_t room = part_size + headroom;

    if (room < part_size || room > INT_MAX || addr + room < addr) {
        LOG_ERR("Device tree headroom too large");
        return -PB_ERR_MEM;
    }

    /* Before the device tree is read: the part was placed without the room
     * it grows by (boot.md 4.3) */
    if (!boot_image_in_regions(addr, room)) {
        LOG_ERR("Device tree with headroom isn't inside an image data region");
        return -PB_ERR_MEM;
    }

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (p->id == dtb_id || boot_image_load_addr(hdr, p->id, &p_load_addr) != PB_OK)
            continue;

        uintptr_t p_addr = (uintptr_t)p_load_addr;

        if (overlaps(addr, room, p_addr, bpak_part_size(p))) {
            LOG_ERR("Device tree with headroom overlaps part 0x%x", p->id);
            return -PB_ERR_MEM;
        }
    }

    *fdt = (void *)addr;
    rc = dtb_size(*fdt, part_size, &size);

    if (rc != PB_OK)
        return rc;

    rc = fdt_open_into(*fdt, *fdt, (int)(size + headroom));

    if (rc < 0) {
        LOG_ERR("Could not grow the device tree (%i)", rc);
        return -PB_ERR;
    }

    LOG_DBG("Device tree at 0x%" PRIxPTR ", %zu -> %zu bytes", addr, size, size + headroom);
    return PB_OK;
}

int boot_dtb_open_into(struct bpak_header *hdr,
                       bpak_id_t dtb_id,
                       size_t headroom,
                       void *dest,
                       size_t dest_size)
{
    int rc;
    uintptr_t addr;
    size_t part_size;
    size_t size;

    rc = dtb_part(hdr, dtb_id, &addr, &part_size);

    if (rc != PB_OK)
        return rc;

    /* What is written is 'dest', never the part: one that lies in it would be
     * changed after it was hashed, and read while it is written */
    if (addr + part_size < addr || (uintptr_t)dest + dest_size < (uintptr_t)dest ||
        overlaps(addr, part_size, (uintptr_t)dest, dest_size)) {
        LOG_ERR("The device tree's part is where its copy goes");
        return -PB_ERR_MEM;
    }

    rc = dtb_size((const void *)addr, part_size, &size);

    if (rc != PB_OK)
        return rc;

    size_t room = size + headroom;

    if (room < size || room > INT_MAX || room > dest_size) {
        LOG_ERR("Device tree (%zu bytes) with headroom doesn't fit %zu bytes", size, dest_size);
        return -PB_ERR_MEM;
    }

    /* The room is all the copy's, also what the tree doesn't fill of it */
    memset(dest, 0, room);
    rc = fdt_open_into((const void *)addr, dest, (int)room);

    if (rc < 0) {
        LOG_ERR("Could not open the device tree (%i)", rc);
        return -PB_ERR;
    }

    LOG_DBG("Device tree at 0x%" PRIxPTR " to 0x%" PRIxPTR ", %zu -> %zu bytes",
            addr,
            (uintptr_t)dest,
            size,
            room);
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

/* The strings of pb,status (boot.md 5.6), one for each bit of pb's status,
 * in the bits' order. They are never renamed, and new ones are appended */
static const struct {
    uint32_t bit;
    const char *name;
} status_names[] = {
    { BOOT_INFO_STATUS_WATCHDOG_NOT_ARMED, "watchdog-not-armed" },
    { BOOT_INFO_STATUS_BOARD_DEGRADED, "board-degraded" },
};

/* pb,status as the list of the strings whose bit 'status' has: none is an
 * empty property. A bit without a string is pb's structure's only */
static int write_status(void *fdt, int offset, uint32_t status)
{
    /* Not NULL: fdt_setprop() copies from it, 0 bytes */
    int rc = fdt_setprop(fdt, offset, "pb,status", "", 0);

    for (size_t i = 0; rc == 0 && i < ARRAY_SIZE(status_names); i++) {
        if (status & status_names[i].bit)
            rc = fdt_appendprop_string(fdt, offset, "pb,status", status_names[i].name);
    }

    return rc;
}

/*
 * pb's status, new with pb's information like the three before it, and left
 * out the same way when the tree has no room. It is always written where it
 * has room, empty when there is nothing to report, so that a next stage can
 * tell "nothing" from a pb that doesn't report.
 *
 * Room is asked for with every string of the list, and the boot's own list
 * is set after it, which is never longer: a tree that has room for the empty
 * list and not for an entry would otherwise lose the property in the very
 * boot that has something to report.
 */
static int set_status(void *fdt, int offset, uint32_t status)
{
    int rc = write_status(fdt, offset, UINT32_MAX);

    if (rc == -FDT_ERR_NOSPACE) {
        LOG_WARN("No room in the device tree for pb,status: left out");
        return del_prop(fdt, offset, "pb,status");
    }

    if (rc == 0)
        rc = write_status(fdt, offset, status);

    return prop_result("pb,status", rc);
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

    /* The board's kernel command line, then its hook, with the whole tree
     * (boot.md 5.5) */
    const char *bootargs = chosen->bootargs;

    if ((boot_get_flags() & BOOT_FLAG_VERBOSE) && chosen->bootargs_verbose)
        bootargs = chosen->bootargs_verbose;

    rc = set_string(fdt, offset, "bootargs", bootargs);

    if (rc != PB_OK)
        return rc;

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
     * mode first, then the partition, then the version, then pb's status.
     * Room for the mode is asked for with the longest name, and its value
     * is set last, so that which of them a next stage finds doesn't depend
     * on how this boot was chosen: a tree that has "normal" and not
     * "fallback" would lose the mode, or the version after it, in the boot
     * that fell back.
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

    rc = set_status(fdt, offset, info->status);

    if (rc != PB_OK)
        return rc;

    /* The mode itself, where it got its room: it is never longer */
    if (fdt_getprop(fdt, offset, "pb,boot-mode", NULL) != NULL)
        rc = prop_result("pb,boot-mode", fdt_setprop_string(fdt, offset, "pb,boot-mode", mode));

    return rc;
}
