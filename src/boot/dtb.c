/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Room in the device tree for /chosen and pb's properties, so that a board's
 * device tree doesn't need placeholders or free space for them (F-071).
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
