/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The memory map (platform.md 5.2): the four layers, built once by
 * pb_mmu_init() in the platform's plat_init(), before the MMU is on.
 */

#include <pb/board.h>
#include <pb/mmu.h>
#include <pb/pb.h>
#include <pb/utils_def.h>
#include <xlat_tables.h>

/* pb, from its first section to the end of the RAM log (platform.md 5.1) */
IMPORT_SYM(uintptr_t, _pb_start, pb_start);
IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _ro_data_region_start, ro_data_start);
IMPORT_SYM(uintptr_t, _ro_data_region_end, ro_data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
/* The rest of pb: .bss and the RAM log */
IMPORT_SYM(uintptr_t, _zero_region_start, pb_rw_start);
IMPORT_SYM(uintptr_t, _pb_end, pb_rw_end);
IMPORT_SYM(uintptr_t, _buffers_start, buffers_start);
IMPORT_SYM(uintptr_t, _buffers_end, buffers_end);

#define MAP_RW_XN       (MT_RW | MT_MEMORY | MT_EXECUTE_NEVER)
/* Not a kind of the board's regions */
#define MT_UNKNOWN_ATTR (~0U)

_Static_assert(PB_MAP_PAGE_SIZE == PAGE_SIZE, "pb/mmu.h's page is xlat_tables' granule");

/* Layer 4: pb itself, from the link script's symbols (platform.md 5.1) */
struct pb_section {
    uintptr_t start;
    uintptr_t end;
    unsigned int attr;
};

static size_t pb_sections(struct pb_section *s)
{
    size_t n = 0;

    s[n++] = (struct pb_section){ code_start, code_end, MT_RO | MT_MEMORY | MT_EXECUTE };
    s[n++] = (struct pb_section){ data_start, data_end, MAP_RW_XN };
    s[n++] =
        (struct pb_section){ ro_data_start, ro_data_end, MT_RO | MT_MEMORY | MT_EXECUTE_NEVER };
    s[n++] = (struct pb_section){ stack_start, stack_end, MAP_RW_XN };
    /* The rest of pb and the buffers, each a region of its own: one span
     * from pb to the buffers mapped what is between them as memory, which
     * is the address space and its devices when they aren't in the same
     * memory (F-158). The DRAM between them is layer 1's */
    s[n++] = (struct pb_section){ pb_rw_start, pb_rw_end, MAP_RW_XN };
    s[n++] = (struct pb_section){ buffers_start, buffers_end, MAP_RW_XN };

    return n;
}

#define PB_SECTIONS 6

static bool plat_region_end(const mmap_region_t *r)
{
    /* mmap_add()'s end: an entry with a size and attributes of 0 */
    return r->size == 0 && r->attr == 0;
}

/* What a board region may not overlap: pb, its buffers, the board's DRAM, a
 * region of the platform's, an earlier region of the board's. Inclusive
 * ends in 64 bits, which no 32 bit range wraps */
static bool overlaps(uint64_t base, uint64_t last, uint64_t o_base, uint64_t o_size)
{
    if (o_size == 0)
        return false;

    return base <= o_base + o_size - 1 && o_base <= last;
}

static unsigned int board_region_attr(enum pb_map_kind kind)
{
    switch (kind) {
    case PB_MAP_KIND_DEVICE:
        return MT_DEVICE | MT_RW | MT_EXECUTE_NEVER;
    case PB_MAP_KIND_MEMORY_RO:
        return MT_MEMORY | MT_RO | MT_EXECUTE_NEVER;
    case PB_MAP_KIND_MEMORY_RW:
        return MAP_RW_XN;
    default:
        return MT_UNKNOWN_ATTR;
    }
}

/* Layer 3's rules (platform.md 5.2, board.md 2.8), before anything is
 * mapped. n_out: the number of regions */
static int check_board_mmap(const struct pb_mmap_region *board_mmap,
                            const mmap_region_t *plat_mmap,
                            size_t *n_out)
{
    size_t n = 0;

    for (const struct pb_mmap_region *r = board_mmap; r != NULL && r->kind != PB_MAP_KIND_END;
         r++, n++) {
        const uint64_t base = r->base;
        const uint64_t last = base + r->size - 1;
        const char *what = NULL;

        if (board_region_attr(r->kind) == MT_UNKNOWN_ATTR)
            what = "is of no kind";
        else if (r->size == 0 || ((r->base | r->size) & (PB_MAP_PAGE_SIZE - 1)) != 0)
            what = "isn't whole pages";
        else if (last < base || last > (uint64_t)PLAT_VIRT_ADDR_SPACE_SIZE - 1)
            what = "is outside the address space";
        else if (overlaps(base, last, pb_start, pb_rw_end - pb_start))
            what = "overlaps pb";
        else if (overlaps(base, last, buffers_start, buffers_end - buffers_start))
            what = "overlaps pb's buffers";
        else if (overlaps(base, last, BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE))
            what = "overlaps the board's DRAM";

        for (const mmap_region_t *p = plat_mmap; what == NULL && p != NULL && !plat_region_end(p);
             p++) {
            if (overlaps(base, last, p->base_va, p->size))
                what = "overlaps a region of the platform's";
        }

        for (const struct pb_mmap_region *e = board_mmap; what == NULL && e != r; e++) {
            if (overlaps(base, last, e->base, e->size))
                what = "overlaps another of the board's";
        }

        if (what != NULL) {
            LOG_ERR("Board mmap %zu (0x%" PRIxPTR ", 0x%zx) %s", n, r->base, r->size, what);
            return -PB_ERR_PARAM;
        }
    }

    *n_out = n;
    return PB_OK;
}

int pb_mmu_init(const struct mmap_region *plat_mmap)
{
    struct pb_section pb[PB_SECTIONS];
    size_t n_pb = pb_sections(pb);
    size_t n_board = 0;
    size_t n_regions;
    int rc;

    /* Layer 3's rules first: a region that breaks one is refused, and
     * nothing is mapped */
    rc = check_board_mmap(board.mmap, plat_mmap, &n_board);

    if (rc != PB_OK)
        return rc;

    n_regions = n_board;

    /* Count what xlat_tables keeps: it skips an empty region. One more
     * than it has room for would overwrite the end of its list */
    if (BOARD_RAM_END > BOARD_RAM_BASE)
        n_regions++;

    for (const mmap_region_t *r = plat_mmap; r != NULL && !plat_region_end(r); r++) {
        if (r->size != 0)
            n_regions++;
    }

    for (size_t i = 0; i < n_pb; i++) {
        if (pb[i].end > pb[i].start)
            n_regions++;
    }

    if (n_regions > MAX_MMAP_REGIONS) {
        LOG_ERR("The memory map has %zu regions, MAX_MMAP_REGIONS is %u",
                n_regions,
                (unsigned int)MAX_MMAP_REGIONS);
        return -PB_ERR_MEM;
    }

    reset_xlat_tables();

    /* Layer 1: the board's DRAM, where images are loaded. What of pb is in
     * it is a region inside it, and the innermost region wins */
    if (BOARD_RAM_END > BOARD_RAM_BASE) {
        mmap_add_region(BOARD_RAM_BASE, BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE, MAP_RW_XN);
    }

    /* Layer 2: the platform's */
    if (plat_mmap != NULL)
        mmap_add(plat_mmap);

    /* Layer 3: the board's, outside everything of layers 1, 2 and 4, so
     * that none changes what pb or the platform mapped */
    for (size_t i = 0; i < n_board; i++) {
        const struct pb_mmap_region *r = &board.mmap[i];

        mmap_add_region(r->base, r->base, r->size, board_region_attr(r->kind));
    }

    /* Layer 4: pb itself */
    for (size_t i = 0; i < n_pb; i++)
        mmap_add_region(pb[i].start, pb[i].start, pb[i].end - pb[i].start, pb[i].attr);

    init_xlat_tables();

#ifdef CONFIG_LIB_XLAT_TBLS_ARMV8
    enable_mmu_el3(0);
#else
    enable_mmu_svc_mon(0);
#endif

    return PB_OK;
}
