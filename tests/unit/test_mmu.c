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
 * small, both from CMakeLists.txt.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

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

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_layers, setup),
        cmocka_unit_test_setup(test_no_plat_mmap, setup),
        cmocka_unit_test_setup(test_too_many_regions, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
