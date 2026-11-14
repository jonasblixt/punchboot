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

#include <pb/mmu.h>
#include <pb/pb.h>
#include <pb/utils_def.h>
#include <xlat_tables.h>

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

#define MAP_RW_XN (MT_RW | MT_MEMORY | MT_EXECUTE_NEVER)

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

int pb_mmu_init(const struct mmap_region *plat_mmap)
{
    struct pb_section pb[PB_SECTIONS];
    size_t n_pb = pb_sections(pb);
    size_t n_regions = 0;

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
