/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "uart.h"
#include <bpak/bpak.h>
#include <pb/console.h>
#include <pb/plat.h>
#include <stdio.h>
#include <string.h>
#include <uuid.h>
#include <xlat_tables.h>

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

static const uint8_t device_unique_id[8] = "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";
const char *platform_ns_uuid = "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5";

static const mmap_region_t qemu_mmap[] = {
    MAP_REGION_FLAT(0x00000000, (1024 * 1024 * 1024), MT_DEVICE | MT_RW),
    { 0 }
};

int plat_boot_reason(void)
{
    return 0;
}

const char *plat_boot_reason_str(void)
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

    mmap_add_region(code_start, code_start, code_end - code_start, MT_RO | MT_MEMORY | MT_EXECUTE);

    mmap_add_region(
        data_start, data_start, data_end - data_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(ro_data_start,
                    ro_data_start,
                    ro_data_end - ro_data_start,
                    MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(
        stack_start, stack_start, stack_end - stack_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add_region(rw_nox_start,
                    rw_nox_start,
                    rw_nox_end - rw_nox_start,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    /* Add the rest of the RAM */
    mmap_add_region(
        rw_nox_end, rw_nox_end, BOARD_RAM_END - rw_nox_end, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(
        data_start, data_start, data_end - data_start, MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add(qemu_mmap);

    init_xlat_tables();
    LOG_DBG("About to enable MMU");

    enable_mmu_el1(0);
    LOG_DBG("MMU Enabled");
}

int plat_init(void)
{
    static const struct console_ops ops = {
        .putc = qemu_uart_putc,
    };

    // TODO: Magic const's
    console_init(0x09000000, &ops);

    mmu_init();

    return PB_OK;
}

int plat_board_init(void)
{
    return 0;
    // return board_init();
}

uint32_t plat_get_us_tick(void)
{
    return 0;
}

void plat_reset(void)
{
}
