/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <pb/mmio.h>
#include <pb/plat.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/wdog/imx_wdog.h>
#include <drivers/uart/imx_uart.h>
#include <plat/imx6ul/imx6ul.h>
#include <xlat_tables.h>
#include <uuid.h>
#include <platform_defs.h>
#include <board_defs.h>

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

static struct imx6ul_platform plat;
static int boot_reason;

/*
static struct fuse lock_fuse =
        IMX6UL_FUSE_BANK_WORD(0, 6, "lockfuse");

static struct fuse rom_key_revoke_fuse =
        IMX6UL_FUSE_BANK_WORD(5, 7, "Revoke");
*/

static const mmap_region_t imx_mmap[] =
{
    /* Boot ROM API*/
    MAP_REGION_FLAT(0x00000000, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Needed for HAB*/
    MAP_REGION_FLAT(0x00900000, (256 * 1024), MT_MEMORY | MT_RW),
    /* AIPS-1 */
    MAP_REGION_FLAT(0x02000000, (1024 * 1024), MT_DEVICE | MT_RW),
    /* AIPS-2 */
    MAP_REGION_FLAT(0x02100000, (1024 * 1024), MT_DEVICE | MT_RW),
    {0}
};

int plat_boot_reason(void)
{
    return -PB_ERR_NOT_IMPLEMENTED;
}

const char *plat_boot_reason_str(void)
{
    return "";
}

/* SLC API */
#ifdef __NOPE
int plat_slc_init(void)
{
    int rc;
    bool sec_boot_active = false;

    rc = hab_secureboot_active(&sec_boot_active);

    if (rc != PB_OK) {
        LOG_ERR("Could not read secure boot status");
        return rc;
    }

    if (sec_boot_active) {
        LOG_INFO("Secure boot active");
    } else {
        LOG_INFO("Secure boot disabled");
    }

    if (hab_has_no_errors() == PB_OK) {
        LOG_INFO("No HAB errors found");
    } else {
        LOG_ERR("HAB is reporting errors");
    }
    return PB_OK;
}

int plat_slc_set_configuration(void)
{
    int err;

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
        if ((f->value & f->default_value) != f->default_value) {
            f->value = f->default_value;
            err = plat_fuse_write(f);

            if (err != PB_OK)
                return err;
        } else {
            LOG_DBG("Fuse %s already programmed", f->description);
        }
    }

    return PB_OK;
}

int plat_slc_set_configuration_lock(void)
{
    int err;
    enum pb_slc slc;


    err = plat_slc_read(&slc);

    if (err != PB_OK)
        return err;

    if (slc == PB_SLC_CONFIGURATION_LOCKED) {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    } else if (slc != PB_SLC_CONFIGURATION) {
        LOG_ERR("SLC is not in configuration, aborting (%u)", slc);
        return PB_ERR;
    }

    LOG_INFO("About to change security state to locked");

    lock_fuse.value = 0x02;

    err = plat_fuse_write(&lock_fuse);

    if (err != PB_OK)
        return err;

    return PB_OK;
}

int plat_slc_read(enum pb_slc *slc)
{
    int err;
    *slc = PB_SLC_NOT_CONFIGURED;

    // Read fuses
    foreach_fuse(f, (struct fuse *) fuses) {
        err = plat_fuse_read(f);

        if (f->value) {
            (*slc) = PB_SLC_CONFIGURATION;
            break;
        }

        if (err != PB_OK) {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    bool sec_boot_active = false;
    err = hab_secureboot_active(&sec_boot_active);

    if (err != PB_OK)
        return err;

    if (sec_boot_active) {
        (*slc) = PB_SLC_CONFIGURATION_LOCKED;
    }

    return PB_OK;
}

int plat_slc_key_active(uint32_t id, bool *active)
{
    int rc;
    unsigned int rom_index = 0;
    bool found_key = false;

    *active = false;

    for (int i = 0; i < 16; i++) {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id) {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key) {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    uint32_t revoke_value = (1 << rom_index);

    if ((rom_key_revoke_fuse.value & revoke_value) == revoke_value)
        (*active) = false;
    else
        (*active) = true;

    return PB_OK;
}

int plat_slc_revoke_key(uint32_t id)
{
    int rc;
    unsigned int rom_index = 0;
    bool found_key = false;
    LOG_INFO("Revoking key 0x%x", id);


    for (int i = 0; i < 16; i++) {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id) {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key) {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK) {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_DBG("Revoke fuse = 0x%x", rom_key_revoke_fuse.value);

    uint32_t revoke_value = (1 << rom_index);

    if ((rom_key_revoke_fuse.value & revoke_value) == revoke_value) {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_DBG("About to write 0x%x", revoke_value);

    rom_key_revoke_fuse.value |= revoke_value;

    return plat_fuse_write(&rom_key_revoke_fuse);
}

#endif

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    imx_ocotp_read(0, 1, &plat_unique.uid[0]);
    imx_ocotp_read(0, 2, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));
    return PB_OK;
}


void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
}

/* UART Interface */

static int plat_console_init(void)
{
    /* Configure UART */
    mmio_write_32(0x020E0094, 0);
    mmio_write_32(0x020E0098, 0);
    mmio_write_32(0x020E0320, UART_PAD_CTRL);
    mmio_write_32(0x020E0324, UART_PAD_CTRL);

    imx_uart_init(0x021E8000, MHz(80), 115200);
    return PB_OK;
}


static int plat_mmu_init(void)
{
    /* Configure MMU */
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

    /* Add the rest of the RAM */
    mmap_add_region(rw_nox_end, rw_nox_end,
                    BOARD_RAM_END - rw_nox_end,
                    MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    mmap_add(imx_mmap);

    init_xlat_tables();

    enable_mmu_svc_mon(0);

    return PB_OK;
}

int plat_init(void)
{
    plat_console_init();
    /* Unmask wdog in SRC control reg */
    mmio_write_32(0x020D8000, 0);
    imx_wdog_init(0x020BC000, CONFIG_WATCHDOG_TIMEOUT);
    imx_gpt_init(0x02098000, MHz(24));

    /**
     * TODO: Some imx6ul can run at 696 MHz and some others at 528 MHz
     *   implement handeling of that.
     *
     */

    /*** Configure ARM Clock ***/
    /* Select step clock, so we can change arm PLL */
    mmio_clrsetbits_32(0x020C400C, 0, 1 << 2);


    /* Power down */
    mmio_write_32(0x020C8000, (1<<12));

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    mmio_write_32(0x020C8000, (1<<13) | 88);


    /* Wait for PLL to lock */
    while (!(mmio_read_32(0x020C8000) & (1<<31)))
        {};

    /* Select re-connect ARM PLL */
    mmio_clrsetbits_32(0x020C400C, 1 << 2, 0);

    /*** End of ARM Clock config ***/

    /* Ungate all clocks */
    mmio_write_32(0x020C4000 + 0x68, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x6C, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x70, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x74, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x78, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x7C, 0xFFFFFFFF); /* Ungate usdhc clk*/
    mmio_write_32(0x020C4000 + 0x80, 0xFFFFFFFF); /* Ungate usdhc clk*/


    uint32_t csu = 0x21c0000;
    /* Allow everything */
    for (int i = 0; i < 40; i ++) {
        *((uint32_t *)csu + i) = 0xffffffff;
    }

    imx_ocotp_init(0x021BC000, 8);
    imx_wdog_kick();
    plat_mmu_init();

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init(&plat);
}
