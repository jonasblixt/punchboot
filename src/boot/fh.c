/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The generic handover: Arm's Firmware Handoff (boot.md 5.4, boot/fh.h). A
 * transfer list in the board's region, with the image's device tree or pb's
 * structure in it, and the specification's registers. What the next stage
 * reads is boot/pb_fh.h.
 *
 * The list is the specification's, version 2, with checksums on: the base
 * isn't 0 and is 8-byte aligned, an entry starts at a multiple of 8,
 * used_size and total_size are multiples of 8, and the checksum makes the
 * used bytes sum to 0, as the reference library computes it.
 */

#include <arch/arch.h>
#include <boot/dtb.h>
#include <boot/fh.h>
#include <boot/pb_fh.h>
#include <bpak/id.h>
#include <inttypes.h>
#include <libfdt.h>
#include <pb/pb.h>
#include <string.h>

#if !defined(CONFIG_BOOT_FH_REGION_BASE) || !defined(CONFIG_BOOT_FH_REGION_SIZE)
#error "CONFIG_BOOT_FH needs the transfer list's region: CONFIG_BOOT_FH_REGION_BASE and _SIZE"
#endif

/* The board's region, in 64 bits: nothing wraps where it is checked */
#define FH_BASE        ((uint64_t)CONFIG_BOOT_FH_REGION_BASE)
#define FH_SIZE        ((uint64_t)CONFIG_BOOT_FH_REGION_SIZE)

#define FH_ALIGN_UP(x) (((x) + (PB_FH_TL_GRANULE - 1)) & ~(uint64_t)(PB_FH_TL_GRANULE - 1))

/* The list has one entry: its header follows the list's, then its data */
#define FH_DATA_OFFSET (sizeof(struct pb_fh_tl_header) + sizeof(struct pb_fh_tl_entry))
/* The least: a list with pb's structure. A device tree's is checked when
 * the tree is there */
#define FH_MIN_SIZE    (FH_DATA_OFFSET + FH_ALIGN_UP(sizeof(struct pb_fh_handover)))
/* What the region has for the entry's data */
#define FH_ROOM        ((size_t)(FH_SIZE - FH_DATA_OFFSET))

/* The specification's rules for the list's memory (boot.md 5.4), and
 * placement's for a range (boot.md 4.3), on the board's two values */
_Static_assert(FH_BASE != 0 && FH_BASE % PB_FH_TL_GRANULE == 0,
               "CONFIG_BOOT_FH_REGION_BASE is 0 or not 8-byte aligned");
_Static_assert(FH_SIZE % PB_FH_TL_GRANULE == 0,
               "CONFIG_BOOT_FH_REGION_SIZE is not a multiple of 8");
_Static_assert(FH_SIZE >= FH_MIN_SIZE,
               "CONFIG_BOOT_FH_REGION_SIZE has no room for struct pb_fh_handover");
_Static_assert(FH_SIZE <= UINT32_MAX && FH_BASE <= UINTPTR_MAX &&
                   FH_SIZE - 1 <= UINTPTR_MAX - FH_BASE,
               "The Firmware Handoff region wraps or is above what the arch can address");

/* pb's structure has room for this build's version (boot.md 5.4). That the
 * keystore's keys fit it is checked where the keystore is made (Makefile) */
_Static_assert(sizeof(PB_VERSION) <= sizeof(((struct pb_fh_handover *)0)->version),
               "PB_VERSION has no room in struct pb_fh_handover");
_Static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "The transfer list is little-endian");

/* The least of the entry's part that the hash covers. ARMv7-M: the jump reads
 * the initial stack pointer and the reset vector from it, the vector table's
 * first two words (D-028). ARMv7-A and ARMv8-A: the instruction at the entry
 * is half of that. One rule for all: a part with its padding is empty or 512
 * bytes and more */
#define ENTRY_MIN_SIZE 8

static const struct boot_handover_fh_config *cfg;

int boot_handover_fh_init(const struct boot_handover_fh_config *cfg_in)
{
    if (cfg_in == NULL || cfg_in->entry_bpak_id == 0)
        return -PB_ERR_PARAM;

    cfg = cfg_in;
    return PB_OK;
}

static int entry_address(struct bpak_header *hdr, uintptr_t *entry)
{
    struct bpak_part_header *part;
    struct bpak_meta_header *mh;

    if (bpak_get_part(hdr, cfg->entry_bpak_id, &part) != BPAK_OK) {
        LOG_ERR("Boot failed to find jump address");
        return -PB_ERR_BAD_META;
    }

    /* What the jump reads and runs there is in bytes the payload's hash
     * covers (security.md 4.1). bpak_part_size() is what was hashed at the
     * load address, the padding too; an empty part passes the hash with
     * nothing there (F-292) */
    if (bpak_part_size(part) < ENTRY_MIN_SIZE) {
        LOG_ERR("The entry's part %x is empty", part->id);
        return -PB_ERR_BAD_META;
    }

    /* The address the part was hashed at, loaded or in place */
    if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, part->id, &mh) != BPAK_OK) {
        LOG_ERR("No pb-load-addr for part %x", part->id);
        return -PB_ERR_BAD_META;
    }

    *entry = (uintptr_t)*bpak_get_meta_ptr(hdr, mh, uint64_t);
    return PB_OK;
}

/*
 * The region is the handover's, and no part of the image is in it. A part
 * that was loaded can't be: the board's image data regions leave the region
 * out (boot.md 4.3). One that is booted in place is where its partition is
 * mapped, which the board says. Before the list is written, so that what
 * was hashed is what runs.
 */
static int parts_off_region(struct bpak_header *hdr)
{
    const uint64_t region_last = FH_BASE + (FH_SIZE - 1);
    struct bpak_meta_header *mh;

    bpak_foreach_part(hdr, p) {
        if (!p->id)
            break;

        if (bpak_get_meta(hdr, BPAK_ID_PB_LOAD_ADDR, p->id, &mh) != BPAK_OK)
            continue;

        uint64_t addr = *bpak_get_meta_ptr(hdr, mh, uint64_t);
        uint64_t size = bpak_part_size(p);

        if (size == 0)
            continue;

        uint64_t last = addr + (size - 1);

        if (last < addr || (addr <= region_last && FH_BASE <= last)) {
            LOG_ERR("Part 0x%x is in the Firmware Handoff region", p->id);
            return -PB_ERR_MEM;
        }
    }

    return PB_OK;
}

/* A string of pb's information in a field of pb's structure, which was
 * zeroed: NUL terminated. NULL: empty */
static int set_string(char *field, size_t field_size, const char *value, const char *name)
{
    size_t length;

    if (value == NULL)
        return PB_OK;

    length = strlen(value);

    if (length >= field_size) {
        LOG_ERR("%s doesn't fit pb's structure", name);
        return -PB_ERR_MEM;
    }

    memcpy(field, value, length);
    return PB_OK;
}

/* pb's structure: pb's information, each field as boot.md 5.6 has it */
static int add_handover(const struct boot_info *info, void *data, size_t *data_size)
{
    struct pb_fh_handover *h = data;
    int rc;

    memset(h, 0, sizeof(*h));

    switch (info->boot_mode) {
    case BOOT_POLICY_MODE_NORMAL:
        h->boot_mode = PB_FH_BOOT_MODE_NORMAL;
        break;
    case BOOT_POLICY_MODE_FALLBACK:
        h->boot_mode = PB_FH_BOOT_MODE_FALLBACK;
        break;
    case BOOT_POLICY_MODE_SPECULATIVE:
        h->boot_mode = PB_FH_BOOT_MODE_SPECULATIVE;
        break;
    default:
        LOG_ERR("A boot mode without a value (%i)", (int)info->boot_mode);
        return -PB_ERR_PARAM;
    }

    /* The build has no more keys than the structure has room for (Makefile);
     * a build that wasn't checked doesn't boot with them */
    if (info->no_of_available_keys > ARRAY_SIZE(h->available_keys)) {
        LOG_ERR("%zu keys don't fit pb's structure", info->no_of_available_keys);
        return -PB_ERR_MEM;
    }

    memcpy(h->device_uuid, info->device_uuid, sizeof(h->device_uuid));
    /* All zero: none, 'boot bpak' named no partition */
    memcpy(h->boot_partition_uuid, info->boot_partition_uuid, sizeof(h->boot_partition_uuid));

    /* "A", "B" or "?" from a policy that has systems (A/B), else empty */
    rc = set_string(h->system_name, sizeof(h->system_name), info->system_name, "The system's name");

    if (rc != PB_OK)
        return rc;

    h->slc = info->slc;
    h->active_key = info->active_key;
    h->no_of_available_keys = (uint32_t)info->no_of_available_keys;

    for (size_t i = 0; i < info->no_of_available_keys; i++)
        h->available_keys[i] = info->available_keys[i];

    h->status = info->status;

    rc = set_string(h->version, sizeof(h->version), info->version, "pb's version");

    if (rc != PB_OK)
        return rc;

    *data_size = sizeof(*h);
    return PB_OK;
}

/* The tree in the entry's data is one, and no larger than the region's room */
static int dt_fits(const void *fdt, size_t *size)
{
    if (fdt_check_header(fdt) < 0 || fdt_totalsize(fdt) < sizeof(struct fdt_header) ||
        fdt_totalsize(fdt) > FH_ROOM) {
        LOG_ERR("The device tree doesn't fit the Firmware Handoff region any more");
        return -PB_ERR_MEM;
    }

    *size = fdt_totalsize(fdt);
    return PB_OK;
}

/*
 * The board's hook, and the tree as it left it. The hook has the whole tree
 * and may have made it larger. pb's properties are written with the size
 * the tree says it has: in a tree that is larger than the region's room
 * they would be written past the region. So such a tree is refused here,
 * after the hook and before pb writes anything to it.
 */
static int dt_hook(void *fdt, int offset)
{
    size_t size;

    if (cfg->dtb_patch_cb) {
        int rc = cfg->dtb_patch_cb(fdt, offset);

        if (rc != PB_OK)
            return rc;
    }

    return dt_fits(fdt, &size);
}

/*
 * The image's device tree: opened into the entry's data with its headroom,
 * then the board's hook and pb's /chosen on that copy (boot.md 5.5).
 */
static int add_dt(struct bpak_header *hdr, const struct boot_info *info, void *data, size_t *size)
{
    const struct boot_dtb_chosen chosen = {
        .patch_cb = dt_hook,
    };
    int rc;

    rc = boot_dtb_open_into(hdr, cfg->dtb_bpak_id, CONFIG_BOOT_DTB_HEADROOM, data, FH_ROOM);

    if (rc != PB_OK)
        return rc;

    rc = boot_dtb_set_chosen(data, hdr, info, &chosen);

    if (rc != PB_OK)
        return rc;

    /* The tree is the entry's data, all of it */
    return dt_fits(data, size);
}

static uint8_t byte_sum(const uint8_t *bytes, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; i++)
        sum += bytes[i];

    return sum;
}

int boot_handover_fh_prepare(struct bpak_header *hdr,
                             const struct boot_info *info,
                             struct boot_handover_entry *next)
{
    struct pb_fh_tl_header *tl = (struct pb_fh_tl_header *)(uintptr_t)FH_BASE;
    struct pb_fh_tl_entry *te = (struct pb_fh_tl_entry *)(tl + 1);
    void *data = te + 1;
    uintptr_t entry;
    uintptr_t dt = 0;
    uint32_t tag;
    size_t data_size = 0;
    size_t used;
    int rc;

    if (cfg == NULL)
        return -PB_ERR_STATE;

    rc = parts_off_region(hdr);

    if (rc != PB_OK)
        return rc;

    /* What an earlier candidate left there is no list while this one's is
     * written, and none when it fails */
    memset(tl, 0, sizeof(*tl));

    rc = entry_address(hdr, &entry);

    if (rc != PB_OK)
        return rc;

    if (cfg->dtb_bpak_id) {
        tag = PB_FH_TAG_FDT;
        dt = (uintptr_t)data;
        rc = add_dt(hdr, info, data, &data_size);
    } else {
        tag = PB_FH_TAG_HANDOVER;
        rc = add_handover(info, data, &data_size);
    }

    if (rc != PB_OK)
        return rc;

    /* The used part ends at a multiple of 8, with the padding after the
     * entry's data, which the checksum covers too */
    used = (size_t)FH_ALIGN_UP(FH_DATA_OFFSET + data_size);
    memset((uint8_t *)data + data_size, 0, used - (FH_DATA_OFFSET + data_size));

    te->tag_and_hdr_size = tag | ((uint32_t)sizeof(*te) << 24);
    te->data_size = (uint32_t)data_size;

    tl->signature = PB_FH_TL_SIGNATURE;
    tl->version = PB_FH_TL_VERSION;
    tl->hdr_size = sizeof(*tl);
    tl->alignment = PB_FH_TL_ALIGNMENT;
    tl->used_size = (uint32_t)used;
    tl->total_size = (uint32_t)FH_SIZE;
    tl->flags = PB_FH_TL_FLAG_HAS_CHECKSUM;
    /* The used bytes sum to 0 modulo 256, the checksum among them */
    tl->checksum = (uint8_t)(0u - byte_sum((const uint8_t *)tl, used));

    /* The next stage reads it with the cache off */
    arch_clean_cache_range((uintptr_t)tl, used);

    next->entry = entry;
    next->dt = dt;

    /* The specification's registers (boot.md 5.4). ARMv7-M has AArch32's */
#if defined(CONFIG_ARCH_ARMV7) || defined(CONFIG_ARCH_ARMV7M)
    next->arg[0] = 0;
    next->arg[1] = PB_FH_REG_R1;
    next->arg[2] = dt;
    next->arg[3] = (uintptr_t)tl;
#else
    next->arg[0] = dt;
    next->arg[1] = (uintptr_t)PB_FH_REG_X1;
    next->arg[2] = 0;
    next->arg[3] = (uintptr_t)tl;
#endif

    LOG_INFO("Boot entry: 0x%" PRIxPTR, next->entry);
    LOG_DBG("Transfer list at 0x%" PRIxPTR ", %zu bytes", (uintptr_t)tl, used);

    return PB_OK;
}

static int handover_init(const void *config)
{
    return boot_handover_fh_init(config);
}

const struct boot_handover_ops boot_handover_fh_ops = {
    .init = handover_init,
    .prepare = boot_handover_fh_prepare,
};
