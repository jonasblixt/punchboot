/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <uuid.h>
#include <pb/fuse.h>
#include <pb/board.h>
#include <xlat_tables.h>
#include <plat/qemu/semihosting.h>
#include <board/config.h>
#include <bpak/bpak.h>
#include <platform_defs.h>
#include <board_defs.h>
#include <pb/console.h>
#include <plat/qemu/qemu.h>
#include <drivers/fuse/test_fuse_bio.h>
#include "uart.h"
#include "gcov.h"

IMPORT_SYM(uintptr_t, _code_start, code_start);
IMPORT_SYM(uintptr_t, _code_end, code_end);
IMPORT_SYM(uintptr_t, _data_region_start, data_start);
IMPORT_SYM(uintptr_t, _data_region_end, data_end);
IMPORT_SYM(uintptr_t, _ro_data_region_start, ro_data_start);
IMPORT_SYM(uintptr_t, _ro_data_region_end, ro_data_end);
IMPORT_SYM(uintptr_t, _stack_start, stack_start);
IMPORT_SYM(uintptr_t, _stack_end, stack_end);
IMPORT_SYM(uintptr_t, _zero_region_start, rw_nox_start);
IMPORT_SYM(uintptr_t, _no_init_end, rw_nox_end);
IMPORT_SYM(uintptr_t, __init_array_start, init_array_start);
IMPORT_SYM(uintptr_t, __init_array_end2, init_array_end);
IMPORT_SYM(uintptr_t, __fini_array_start, fini_array_start);
IMPORT_SYM(uintptr_t, __fini_array_end2, fini_array_end);

static const uint8_t device_unique_id[8] =
    "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";

static const mmap_region_t qemu_mmap[] =
{
    MAP_REGION_FLAT(0x00000000, (1024 * 1024 * 1024), MT_DEVICE | MT_RW),
    {0}
};

int plat_boot_reason(void)
{
    return 0;
}

const char * plat_boot_reason_str(void)
{
    return "";
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    if (sizeof(device_unique_id) > *length)
        return -PB_ERR_BUF_TOO_SMALL;

    memcpy(output, device_unique_id, sizeof(device_unique_id));
    *length = sizeof(device_unique_id);
    return PB_OK;
}

static void mmu_init(void)
{
    reset_xlat_tables();

    mmap_add_region(code_start, code_start,
                    code_end - code_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(data_start, data_start,
                    data_end - data_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(ro_data_start, ro_data_start,
                    ro_data_end - ro_data_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(stack_start, stack_start,
                    stack_end - stack_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(rw_nox_start, rw_nox_start,
                    rw_nox_end - rw_nox_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

#ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
    mmap_add_region(init_array_start, init_array_start,
                    init_array_end - init_array_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(fini_array_start, fini_array_start,
                    fini_array_end - fini_array_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE);
#endif

    /* Add the rest of the RAM */
    mmap_add_region(rw_nox_end, rw_nox_end,
                    BOARD_RAM_END - rw_nox_end,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(data_start, data_start,
                    data_end - data_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add(qemu_mmap);

    init_xlat_tables();
    LOG_DBG("About to enable MMU");
    enable_mmu_svc_mon(0);
    LOG_DBG("MMU Enabled");
}

int plat_init(void)
{
    static const struct console_ops ops = {
        .putc = qemu_uart_putc,
    };

    console_init(0x09000000, &ops);

    mmu_init();

#ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
    gcov_init();
#endif

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init();
}

uint32_t plat_get_us_tick(void)
{
    return 0;
}
