/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The memory map (platform.md 5.2): pb_mmu_init() over the fake xlat_tables
 * (fakes/xlat.c), which notes the regions in the order they are added. The
 * board's DRAM is BOARD_RAM_BASE to BOARD_RAM_END, and MAX_MMAP_REGIONS is
 * small, both from CMakeLists.txt; the address space is 48 bits, which holds
 * the test's own memory, where pb's sections are (fakes/xlat.c). The board's
 * mmap is fakes/board.c's, which the tests fill; built with
 * FAKE_BOARD_NO_MMAP it is NULL.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>
#include <pb/mmu.h>
#include <pb/pb.h>
#include <xlat_tables.h>

#include "fakes/xlat.h"

#define RW_XN     (MT_RW | MT_MEMORY | MT_EXECUTE_NEVER)
#define DEVICE_RW (MT_DEVICE | MT_RW)

/* A platform's list, as the i.MX ports have theirs: a ROM, an on-chip RAM,
 * a peripheral bus, and a SoC region inside the DRAM */
static const mmap_region_t plat_mmap[] = {
    MAP_REGION_FLAT(0x00000000, 0x20000, MT_MEMORY | MT_RO | MT_EXECUTE),
    MAP_REGION_FLAT(0x00900000, 0x40000, MT_MEMORY | MT_RW),
    MAP_REGION_FLAT(0x02000000, 0x100000, DEVICE_RW),
    MAP_REGION_FLAT(BOARD_RAM_BASE, 0x20000, RW_XN),
    { 0 },
};

#define PLAT_REGIONS (membersof(plat_mmap) - 1)

/* The error lines logged: a refusal names what it refuses on the console */
static unsigned int errors_logged;

void log_prefix(const char *level, const char *func)
{
    (void)func;

    if (level[0] == 'E')
        errors_logged++;
}

static int setup(void **state)
{
    (void)state;
    errors_logged = 0;
    memset(fake_board_mmap, 0, sizeof(fake_board_mmap));
    fake_mmu_enables = 0;
    fake_mmu_enabled_by = NULL;
    reset_xlat_tables();
    return 0;
}

/* Region n of the map as it was added */
static void assert_region(size_t n, uintptr_t base, size_t size, unsigned int attr)
{
    assert_true(n < fake_mmap_len);
    assert_int_equal(fake_mmap[n].base, base);
    assert_int_equal(fake_mmap[n].size, size);
    assert_int_equal(fake_mmap[n].attr, attr);
}

/* Layer 4, pb itself, from position n on */
static void assert_pb_regions(size_t n)
{
    struct fake_mmap_region pb[8];
    size_t n_pb = fake_mmap_pb_regions(pb);

    assert_int_equal(fake_mmap_len, n + n_pb);

    for (size_t i = 0; i < n_pb; i++)
        assert_region(n + i, pb[i].base, pb[i].size, pb[i].attr);
}

/* 5.2: the four layers in order, layer 1 the board's DRAM, RW and execute
 * never, layer 2 the platform's list as it is, layer 4 pb itself: .text
 * RX, .rodata RO execute never, the rest RW execute never, the rest of pb
 * and the buffers as two regions. Then the arch's enable, once */
static void test_layers(void **state)
{
    (void)state;

    assert_int_equal(pb_mmu_init(plat_mmap), PB_OK);
    assert_int_equal(errors_logged, 0);

    assert_region(0, BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE, RW_XN);

    for (size_t i = 0; i < PLAT_REGIONS; i++)
        assert_region(1 + i, plat_mmap[i].base_va, plat_mmap[i].size, plat_mmap[i].attr);

    assert_pb_regions(1 + PLAT_REGIONS);
    fake_mmu_check_pb_map(BOARD_RAM_BASE, BOARD_RAM_END);

    assert_int_equal(fake_mmu_enables, 1);
#ifdef CONFIG_LIB_XLAT_TBLS_ARMV8
    assert_string_equal(fake_mmu_enabled_by, "el3");
#else
    assert_string_equal(fake_mmu_enabled_by, "svc_mon");
#endif
}

/* No platform list: layers 1 and 4 */
static void test_no_plat_mmap(void **state)
{
    (void)state;

    assert_int_equal(pb_mmu_init(NULL), PB_OK);
    assert_region(0, BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE, RW_XN);
    assert_pb_regions(1);
    assert_int_equal(fake_mmu_enables, 1);
}

/* What xlat_tables has room for, MAX_MMAP_REGIONS, is checked first: one
 * more would overwrite the end of its list. Too many is an error, nothing is
 * mapped and the MMU stays off; exactly as many is a map */
static void test_too_many_regions(void **state)
{
    /* The DRAM and pb's six take seven */
    static mmap_region_t many[MAX_MMAP_REGIONS - 7 + 2];
    (void)state;

    for (size_t i = 0; i < MAX_MMAP_REGIONS - 7 + 1; i++)
        many[i] = (mmap_region_t)MAP_REGION_FLAT(0x10000000 + i * 0x10000, 0x1000, DEVICE_RW);

    assert_int_equal(pb_mmu_init(many), -PB_ERR_MEM);
    assert_int_equal(errors_logged, 1);
    assert_int_equal(fake_mmap_len, 0);
    assert_int_equal(fake_mmu_enables, 0);

    many[MAX_MMAP_REGIONS - 7] = (mmap_region_t){ 0 };
    assert_int_equal(pb_mmu_init(many), PB_OK);
    assert_int_equal(fake_mmap_len, MAX_MMAP_REGIONS);
    assert_int_equal(fake_mmu_enables, 1);
}

#ifndef FAKE_BOARD_NO_MMAP
/* A board region of each kind: device memory RW, normal memory RO and RW,
 * none executable */
static const struct pb_mmap_region board_regions[] = {
    PB_MAP_DEVICE(0x10000000, 0x10000),
    PB_MAP_MEMORY_RO(0x60000000, 0x1000000),
    PB_MAP_MEMORY_RW(0x20000000, 0x2000),
};

#define BOARD_REGIONS membersof(board_regions)

/* platform.md 5.2, board.md 2.8: the board's regions are layer 3, after the
 * platform's and before pb, with their kinds' attributes. The macros give
 * the regions as written */
static void test_board_regions(void **state)
{
    static const unsigned int attr[BOARD_REGIONS] = {
        MT_DEVICE | MT_RW | MT_EXECUTE_NEVER,
        MT_MEMORY | MT_RO | MT_EXECUTE_NEVER,
        MT_MEMORY | MT_RW | MT_EXECUTE_NEVER,
    };
    (void)state;

    assert_int_equal(board_regions[1].base, 0x60000000);
    assert_int_equal(board_regions[1].size, 0x1000000);
    assert_int_equal(board_regions[1].kind, PB_MAP_KIND_MEMORY_RO);

    memcpy(fake_board_mmap, board_regions, sizeof(board_regions));
    assert_int_equal(pb_mmu_init(plat_mmap), PB_OK);
    assert_int_equal(errors_logged, 0);

    assert_region(0, BOARD_RAM_BASE, BOARD_RAM_END - BOARD_RAM_BASE, RW_XN);

    for (size_t i = 0; i < PLAT_REGIONS; i++)
        assert_region(1 + i, plat_mmap[i].base_va, plat_mmap[i].size, plat_mmap[i].attr);

    for (size_t i = 0; i < BOARD_REGIONS; i++)
        assert_region(1 + PLAT_REGIONS + i, board_regions[i].base, board_regions[i].size, attr[i]);

    assert_pb_regions(1 + PLAT_REGIONS + BOARD_REGIONS);
    assert_int_equal(fake_mmu_enables, 1);
}

/* One board region that breaks a rule fails pb_mmu_init(), with a log line,
 * before anything is mapped: the MMU stays off */
static void assert_refused(const struct pb_mmap_region *bad, size_t length)
{
    memset(fake_board_mmap, 0, sizeof(fake_board_mmap));
    memcpy(fake_board_mmap, bad, length * sizeof(*bad));
    errors_logged = 0;
    fake_mmu_enables = 0;
    reset_xlat_tables();

    assert_int_equal(pb_mmu_init(plat_mmap), -PB_ERR_PARAM);
    assert_int_equal(errors_logged, 1);
    assert_int_equal(fake_mmap_len, 0);
    assert_int_equal(fake_mmu_enables, 0);
}

static void assert_one_refused(uintptr_t base, size_t size, enum pb_map_kind kind)
{
    const struct pb_mmap_region bad = { .base = base, .size = size, .kind = kind };

    assert_refused(&bad, 1);
}

/* 5.2: outside what is mapped already. xlat_tables would take a nested
 * region and let it win, so pb_mmu_init() refuses one that overlaps pb, its
 * buffers, the board's DRAM or a region of the platform's, nested or in
 * part */
static void test_board_region_overlaps(void **state)
{
    const uintptr_t pb = (uintptr_t)_pb_start;
    const uintptr_t pb_end = (uintptr_t)_pb_end;
    const uintptr_t buffers = (uintptr_t)_buffers_start;
    const uintptr_t buffers_end = (uintptr_t)_buffers_end;
    (void)state;

    /* pb: its code, its end in part, all of it */
    assert_one_refused(pb, 0x1000, PB_MAP_KIND_MEMORY_RO);
    assert_one_refused(pb_end - 0x1000, 0x2000, PB_MAP_KIND_DEVICE);
    assert_one_refused(pb - 0x1000, pb_end - pb + 0x2000, PB_MAP_KIND_MEMORY_RW);
    /* Its buffers */
    assert_one_refused(buffers, 0x1000, PB_MAP_KIND_MEMORY_RW);
    assert_one_refused(buffers_end - 0x1000, 0x1000, PB_MAP_KIND_DEVICE);
    /* The board's DRAM: inside it, over its end */
    assert_one_refused(BOARD_RAM_BASE + 0x100000, 0x1000, PB_MAP_KIND_DEVICE);
    assert_one_refused(BOARD_RAM_END - 0x1000, 0x2000, PB_MAP_KIND_MEMORY_RW);
    /* A region of the platform's: inside its bus, over its ROM's end, around
     * its OCRAM */
    assert_one_refused(0x02001000, 0x1000, PB_MAP_KIND_DEVICE);
    assert_one_refused(0x0001f000, 0x2000, PB_MAP_KIND_MEMORY_RO);
    assert_one_refused(0x008ff000, 0x42000, PB_MAP_KIND_MEMORY_RW);
}

/* The board's regions don't overlap each other either: the second, nested
 * or in part, is refused */
static void test_board_regions_overlap_each_other(void **state)
{
    const struct pb_mmap_region nested[] = {
        PB_MAP_DEVICE(0x10000000, 0x10000),
        PB_MAP_DEVICE(0x10004000, 0x1000),
    };
    const struct pb_mmap_region in_part[] = {
        PB_MAP_MEMORY_RO(0x60000000, 0x1000000),
        PB_MAP_MEMORY_RW(0x60fff000, 0x2000),
    };
    (void)state;

    assert_refused(nested, membersof(nested));
    assert_refused(in_part, membersof(in_part));
}

/* 5.2: flat, whole pages, of one of the three kinds, inside the address
 * space; a region that isn't is refused, as one that wraps */
static void test_board_region_rules(void **state)
{
    (void)state;

    assert_one_refused(0x10000800, 0x1000, PB_MAP_KIND_DEVICE);
    assert_one_refused(0x10000000, 0x800, PB_MAP_KIND_DEVICE);
    assert_one_refused(0x10000000, 0, PB_MAP_KIND_DEVICE);
    assert_one_refused(0x10000000, 0x1000, (enum pb_map_kind)7);
    assert_one_refused(UINTPTR_MAX & ~(uintptr_t)0xfff, 0x2000, PB_MAP_KIND_DEVICE);
    assert_one_refused((uintptr_t)PLAT_VIRT_ADDR_SPACE_SIZE, 0x1000, PB_MAP_KIND_DEVICE);
    assert_one_refused((uintptr_t)PLAT_VIRT_ADDR_SPACE_SIZE - 0x1000, 0x2000, PB_MAP_KIND_DEVICE);
}

/* A region next to pb, its buffers, the DRAM or a region of the platform's
 * overlaps none of them: it is mapped */
static void test_board_regions_adjacent(void **state)
{
    const struct pb_mmap_region next_to[] = {
        { (uintptr_t)_pb_start - 0x1000, 0x1000, PB_MAP_KIND_MEMORY_RW },
        { (uintptr_t)_pb_end, 0x1000, PB_MAP_KIND_MEMORY_RW },
        { (uintptr_t)_buffers_end, 0x1000, PB_MAP_KIND_DEVICE },
        { BOARD_RAM_BASE - 0x1000, 0x1000, PB_MAP_KIND_DEVICE },
        { 0x02100000, 0x1000, PB_MAP_KIND_DEVICE },
    };
    (void)state;

    memcpy(fake_board_mmap, next_to, sizeof(next_to));
    assert_int_equal(pb_mmu_init(plat_mmap), PB_OK);
    assert_int_equal(errors_logged, 0);
    assert_int_equal(fake_mmap_len, 1 + PLAT_REGIONS + membersof(next_to) + 6);
    assert_int_equal(fake_mmu_enables, 1);
}

/* MAX_MMAP_REGIONS counts the board's regions too (5.2, room) */
static void test_board_regions_room(void **state)
{
    /* The DRAM, the platform's four and pb's six take eleven */
    const size_t room = MAX_MMAP_REGIONS - 1 - PLAT_REGIONS - 6;
    (void)state;

    for (size_t i = 0; i < room + 1; i++) {
        fake_board_mmap[i] = (struct pb_mmap_region){ .base = 0x10000000 + i * 0x10000,
                                                      .size = 0x1000,
                                                      .kind = PB_MAP_KIND_DEVICE };
    }

    assert_int_equal(pb_mmu_init(plat_mmap), -PB_ERR_MEM);
    assert_int_equal(errors_logged, 1);
    assert_int_equal(fake_mmap_len, 0);
    assert_int_equal(fake_mmu_enables, 0);

    fake_board_mmap[room] = (struct pb_mmap_region)PB_MAP_END;
    assert_int_equal(pb_mmu_init(plat_mmap), PB_OK);
    assert_int_equal(fake_mmap_len, MAX_MMAP_REGIONS);
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_layers, setup),
        cmocka_unit_test_setup(test_no_plat_mmap, setup),
        cmocka_unit_test_setup(test_too_many_regions, setup),
#ifndef FAKE_BOARD_NO_MMAP
        cmocka_unit_test_setup(test_board_regions, setup),
        cmocka_unit_test_setup(test_board_region_overlaps, setup),
        cmocka_unit_test_setup(test_board_regions_overlap_each_other, setup),
        cmocka_unit_test_setup(test_board_region_rules, setup),
        cmocka_unit_test_setup(test_board_regions_adjacent, setup),
        cmocka_unit_test_setup(test_board_regions_room, setup),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
