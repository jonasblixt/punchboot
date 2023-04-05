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
// TODO: qemu/plat.h contains TEST_FUSE_BANK...
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

extern const struct fuse fuses[];
extern const uint32_t rom_key_map[];

static struct fuse rom_key_revoke_fuse =
        TEST_FUSE_BANK_WORD(8, "Revoke");

static struct fuse security_fuse =
        TEST_FUSE_BANK_WORD(9, "Security fuse");

static struct pb_result_slc_key_status key_status;

static const uint8_t device_unique_id[] =
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

int plat_fuse_read(struct fuse *f)
{
    int val;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;

    val = test_fuse_read(f->bank);

    if (val < 0)
        LOG_ERR("Could not read fuse");

    f->value = val;

    return PB_OK;
}

int plat_fuse_write(struct fuse *f)
{
    int err;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;
    LOG_DBG("Writing fuse %s", f->description);

    err = test_fuse_write(f->bank, f->value);

    if (err != PB_OK)
        LOG_ERR("Could not write fuse");
    return err;
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;

    return snprintf(s, n, "FUSE <%u> %s = 0x%08x\n", f->bank,
                f->description, f->value);
}

/* QEMU SLC Interface */
int plat_slc_set_configuration(void)
{
    int err;

#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION
    err = board_slc_set_configuration(&private);

    if (err != PB_OK) {
        LOG_ERR("board_slc_set_configuration failed");
        return err;
    }
#endif

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses) {
        err = plat_fuse_read(f);

        LOG_DBG("Fuse %s: 0x%08x", f->description, f->value);
        if (err != PB_OK) {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    /* Perform the actual fuse programming */

    LOG_INFO("Writing fuses");

    foreach_fuse(f, fuses) {
        f->value = f->default_value;
        err = plat_fuse_write(f);

        if (err != PB_OK)
            return err;
    }

    return PB_OK;
}

int plat_slc_set_configuration_lock(void)
{
    int rc;

#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION_LOCK
    rc = board_slc_set_configuration_lock(&private);

    if (rc != PB_OK) {
        LOG_ERR("board_slc_set_configuration failed");
        return rc;
    }
#endif

    rc = plat_fuse_read(&security_fuse);

    if (rc != PB_OK)
        return rc;

    if (security_fuse.value & (1 << 0)) {
        LOG_ERR("Already locked");
        return -PB_ERR;
    }

    security_fuse.value |= (1 << 0);

    rc = plat_fuse_write(&security_fuse);

    return rc;
}

int plat_slc_set_end_of_life(void)
{

    int rc;

    rc = plat_fuse_read(&security_fuse);

    if (rc != PB_OK)
        return rc;

    security_fuse.value |= (1 << 1);

    rc = plat_fuse_write(&security_fuse);

    return rc;
}

int plat_slc_read(enum pb_slc *slc)
{

    int rc;

    rc = plat_fuse_read(&security_fuse);

    if (rc != PB_OK)
        return rc;

    if (security_fuse.value & (1 << 0)) {
        LOG_INFO("SLC: Configuration locked");
        *slc = PB_SLC_CONFIGURATION_LOCKED;
    } else if (security_fuse.value & (1 << 1)) {
        LOG_INFO("SLC: End of life");
        *slc = PB_SLC_EOL;
    } else {
        LOG_INFO("SLC: Configuration");
        *slc = PB_SLC_CONFIGURATION;
    }

    return PB_OK;
}

int plat_slc_key_active(uint32_t id, bool *active)
{
    int rc;
    *active = false;
    rc = plat_slc_get_key_status(NULL);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < 16; i++) {
        if (!key_status.active[i])
            continue;
        if (key_status.active[i] == id) {
            *active = true;
            break;
        }
    }

    return PB_OK;
}

int plat_slc_revoke_key(uint32_t id)
{
    int rc;
    LOG_INFO("Revoking key 0x%x", id);

    rc = plat_slc_get_key_status(NULL);

    if (rc != PB_OK)
        return rc;

    rc = plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < 16; i++) {
        if (!rom_key_map[i])
            break;
        if (rom_key_map[i] != id)
            continue;

        if (!(rom_key_revoke_fuse.value & (1 << i))) {
            rom_key_revoke_fuse.value |= (1 << i);
            return plat_fuse_write(&rom_key_revoke_fuse);
        }
    }
    return PB_OK;
}

int plat_slc_get_key_status(struct pb_result_slc_key_status **status)
{
    int rc;

    if (status)
        (*status) = &key_status;

    rc = plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < 16; i++) {
        if (!rom_key_map[i])
            break;

        if (rom_key_revoke_fuse.value & (1 << i)) {
            key_status.active[i] = 0;
            key_status.revoked[i] = rom_key_map[i];
        } else {
            key_status.revoked[i] = 0;
            key_status.active[i] = rom_key_map[i];
        }
    }

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
    memset(&key_status, 0, sizeof(key_status));

    static const struct console_ops ops = {
        .putc = qemu_uart_putc,
    };

    console_init(0x09000000, &ops);

    mmu_init();

#ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
    LOG_DBG("Initializing GCOV");
    gcov_init();
    LOG_DBG("Done");
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
