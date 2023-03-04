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
#include <pb/io.h>
#include <xlat_tables.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/wdog.h>
#include <plat/imx/hab.h>
#include <plat/imx/ehci.h>
#include <pb/fuse.h>
#include <plat/imx6ul/plat.h>
#include <plat/defs.h>
#include <uuid.h>

extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _ro_data_region_start, _ro_data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end,
            _no_init_start, _no_init_end, end;

extern const struct fuse fuses[];
extern const uint32_t rom_key_map[];

static struct fuse lock_fuse =
        IMX6UL_FUSE_BANK_WORD(0, 6, "lockfuse");

static struct fuse fuse_uid0 =
        IMX6UL_FUSE_BANK_WORD(0, 1, "UID0");

static struct fuse fuse_uid1 =
        IMX6UL_FUSE_BANK_WORD(0, 2, "UID1");

static struct fuse rom_key_revoke_fuse =
        IMX6UL_FUSE_BANK_WORD(5, 7, "Revoke");

#define IMX6UL_FUSE_SHADOW_BASE 0x021BC000

static struct imx6ul_private private;
static struct pb_result_slc_key_status key_status;

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

bool plat_force_command_mode(void)
{
    return board_force_command_mode(&private);
}

int plat_patch_bootargs(void *fdt, int offset, bool verbose_boot)
{
    const struct pb_boot_config *boot_config = board_boot_config();
    if (boot_config->dtb_patch_cb) {
        return boot_config->dtb_patch_cb(&private, fdt, offset, verbose_boot);
    }

    return PB_OK;
}

int plat_command(uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    return board_command(&private, command, bfr, size,
                            response_bfr, response_size);
}

/* SLC API */

int plat_slc_init(void)
{
    int rc;
    bool sec_boot_active = false;

    rc = hab_secureboot_active(&sec_boot_active);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read secure boot status");
        return rc;
    }

    if (sec_boot_active)
    {
        LOG_INFO("Secure boot active");
    } else {
        LOG_INFO("Secure boot disabled");
    }

    if (hab_has_no_errors() == PB_OK)
    {
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
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        LOG_DBG("Fuse %s: 0x%08x", f->description, f->value);
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    /* Perform the actual fuse programming */

    LOG_INFO("Writing fuses");

    foreach_fuse(f, fuses)
    {
        if ((f->value & f->default_value) != f->default_value)
        {
            f->value = f->default_value;
            err = plat_fuse_write(f);

            if (err != PB_OK)
                return err;
        }
        else
        {
            LOG_DBG("Fuse %s already programmed", f->description);
        }
    }

#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION
    return board_slc_set_configuration(&private);
#else
    return PB_OK;
#endif
}

int plat_slc_set_configuration_lock(void)
{
    int err;
    enum pb_slc slc;


#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION_LOCK
    err = board_slc_set_configuration_lock(&private);

    if (err != PB_OK) {
        LOG_ERR("board_slc_set_configuration failed");
        return err;
    }
#endif


    err = plat_slc_read(&slc);

    if (err != PB_OK)
        return err;

    if (slc == PB_SLC_CONFIGURATION_LOCKED)
    {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    }
    else if (slc != PB_SLC_CONFIGURATION)
    {
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

int plat_slc_set_end_of_life(void)
{
    return -PB_ERR;
}

int plat_slc_read(enum pb_slc *slc)
{
    int err;
    *slc = PB_SLC_NOT_CONFIGURED;

    // Read fuses
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        if (f->value)
        {
            (*slc) = PB_SLC_CONFIGURATION;
            break;
        }

        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    bool sec_boot_active = false;
    err = hab_secureboot_active(&sec_boot_active);

    if (err != PB_OK)
        return err;

    if (sec_boot_active)
    {
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

    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id)
        {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key)
    {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
    {
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


    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_map[i] == id)
        {
            rom_index = i;
            found_key = true;
        }
    }

    if (!found_key)
    {
        LOG_ERR("Could not find key");
        return -PB_ERR;
    }

    rc =  plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }

    LOG_DBG("Revoke fuse = 0x%x", rom_key_revoke_fuse.value);

    uint32_t revoke_value = (1 << rom_index);

    if ((rom_key_revoke_fuse.value & revoke_value) == revoke_value)
    {
        LOG_INFO("Key already revoked");
        return PB_OK;
    }

    LOG_DBG("About to write 0x%x", revoke_value);

    rom_key_revoke_fuse.value |= revoke_value;

    return plat_fuse_write(&rom_key_revoke_fuse);
}

int plat_slc_get_key_status(struct pb_result_slc_key_status **status)
{
    int rc;

    memset(&key_status, 0, sizeof(key_status));

    if (status)
        (*status) = &key_status;

    rc = plat_fuse_read(&rom_key_revoke_fuse);

    if (rc != PB_OK)
        return rc;

    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_revoke_fuse.value & (1 << i))
        {
            key_status.active[i] = 0;
            key_status.revoked[i] = rom_key_map[i];
        }
        else
        {
            key_status.revoked[i] = 0;
            key_status.active[i] = rom_key_map[i];
        }
    }

    return PB_OK;
}

/*
void plat_preboot_cleanup(void)
{
    pb_setbit32(1<<1, plat.usb0.base + EHCI_CMD);
}
*/

static const char platform_namespace_uuid[] =
    "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

static uint8_t out_tmp[16];

int plat_get_uuid(char *out)
{
    int err;
    plat_fuse_read(&fuse_uid0);
    plat_fuse_read(&fuse_uid1);

    uint32_t uid[2];

    uid[0] = fuse_uid0.value;
    uid[1] = fuse_uid1.value;

    LOG_INFO("%08x %08x", fuse_uid0.value, fuse_uid1.value);

    err = uuid_gen_uuid3(platform_namespace_uuid,
                          (const char *) uid, 8, (char *)out_tmp);
    memcpy(out, out_tmp, 16);

    return err;
}

void plat_reset(void)
{
    imx_wdog_reset_now();
}

uint32_t plat_get_us_tick(void)
{
    return gp_timer_get_tick();
}

void plat_wdog_init(void)
{
    imx_wdog_init(CONFIG_IMX_WATCHDOG_BASE, CONFIG_WATCHDOG_TIMEOUT);
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
}

int plat_fuse_read(struct fuse *f)
{
    if (!(f->status & FUSE_VALID))
        return -PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank*0x80 + f->word*0x10 + 0x400;

        if (f->bank >= 6)
            f->addr += 0x100;
    }

    if (!f->shadow)
        f->shadow = IMX6UL_FUSE_SHADOW_BASE + f->addr;

    f->value = pb_read32(f->shadow);

    return PB_OK;
}

int plat_fuse_write(struct fuse *f)
{
    char s[64];

    plat_fuse_to_string(f, s, 64);

    if ((f->status & FUSE_VALID) != FUSE_VALID)
    {
        LOG_ERR("Could not write fuse %s", s);
        return -PB_ERR;
    }

    LOG_INFO("Writing: %s", s);

    return ocotp_write(f->bank, f->word, f->value);
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return snprintf(s, n,
            "   FUSE<%u,%u> 0x%04x %s = 0x%08x",
                f->bank, f->word, f->addr,
                f->description, f->value);
}


/* UART Interface */

int plat_console_init(void)
{
    /* Configure UART */
    pb_write32(0, 0x020E0094);
    pb_write32(0, 0x020E0098);
    pb_write32(UART_PAD_CTRL, 0x020E0320);
    pb_write32(UART_PAD_CTRL, 0x020E0324);

    imx_uart_init(CONFIG_IMX_UART_BASE,
                  CONFIG_IMX_UART_BAUDRATE);
    return PB_OK;
}

int plat_console_putchar(char c)
{
    imx_uart_putc(c);
    return PB_OK;
}

int plat_early_init(void)
{
    uint32_t reg;

    /* Unmask wdog in SRC control reg */
    pb_write32(0, 0x020D8000);
    plat_wdog_init();
    gp_timer_init();

    /**
     * TODO: Some imx6ul can run at 696 MHz and some others at 528 MHz
     *   implement handeling of that.
     *
     */

    /*** Configure ARM Clock ***/
    reg = pb_read32(0x020C400C);
    /* Select step clock, so we can change arm PLL */
    pb_write32(reg | (1<<2), 0x020C400C);


    /* Power down */
    pb_write32((1<<12) , 0x020C8000);

    /* Configure divider and enable */
    /* f_CPU = 24 MHz * 88 / 4 = 528 MHz */
    pb_write32((1<<13) | 88, 0x020C8000);


    /* Wait for PLL to lock */
    while (!(pb_read32(0x020C8000) & (1<<31)))
        __asm__("nop");

    /* Select re-connect ARM PLL */
    pb_write32(reg & ~(1<<2), 0x020C400C);

    /*** End of ARM Clock config ***/



    /* Ungate all clocks */
    pb_write32(0xFFFFFFFF, 0x020C4000+0x68); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x6C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x70); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x74); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x78); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x7C); /* Ungate usdhc clk*/
    pb_write32(0xFFFFFFFF, 0x020C4000+0x80); /* Ungate usdhc clk*/


    uint32_t csu = 0x21c0000;
    /* Allow everything */
    for (int i = 0; i < 40; i ++)
    {
        *((uint32_t *)csu + i) = 0xffffffff;
    }

    ocotp_init(CONFIG_IMX_OCOTP_BASE,
               CONFIG_IMX_OCOTP_WORDS_PER_BANK);


    imx_wdog_kick();

    return board_early_init(&private);
}

int plat_mmu_init(void)
{
    uintptr_t ro_start = (uintptr_t) &_ro_data_region_start;
    size_t ro_size = ((uintptr_t) &_ro_data_region_end) -
                      ((uintptr_t) &_ro_data_region_start);

    uintptr_t code_start = (uintptr_t) &_code_start;
    size_t code_size = ((uintptr_t) &_code_end) -
                      ((uintptr_t) &_code_start);

    uintptr_t stack_start = (uintptr_t) &_stack_start;
    size_t stack_size = ((uintptr_t) &_stack_end) -
                      ((uintptr_t) &_stack_start);

    uintptr_t rw_start = (uintptr_t) &_data_region_start;

    size_t rw_size = ((uintptr_t) &_data_region_end) -
                      ((uintptr_t) &_data_region_start);

    uintptr_t bss_start = (uintptr_t) &_zero_region_start;
    size_t bss_size = ((uintptr_t) &_zero_region_end) -
                      ((uintptr_t) &_zero_region_start);

    uintptr_t bb_start = (uintptr_t) &_no_init_start;
    size_t bb_size = ((uintptr_t) &_no_init_end) -
                      ((uintptr_t) &_no_init_start);


    plat_console_init();

    reset_xlat_tables();

    mmap_add_region(code_start, code_start, code_size,
                            MT_RO | MT_MEMORY | MT_EXECUTE);
    mmap_add_region(stack_start, stack_start, stack_size,
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(ro_start, ro_start, ro_size,
                            MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(rw_start, rw_start, rw_size,
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(bss_start, bss_start, bss_size,
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(bb_start, bb_start, bb_size,
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Add ram */

    mmap_add_region(bb_start + bb_size, bb_start + bb_size,
                            (1024*1024*1024),
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add(imx_mmap);

    init_xlat_tables();
    enable_mmu_svc_mon(0);

    return PB_OK;
}

/* Transport API */

int plat_transport_process(void)
{
    return imx_ehci_usb_process();
}

int plat_transport_write(void *buf, size_t size)
{
    return imx_ehci_usb_write(buf, size);
}

int plat_transport_read(void *buf, size_t size)
{
    return imx_ehci_usb_read(buf, size);
}

bool plat_transport_ready(void)
{
    return imx_ehci_usb_ready();
}

int imx_ehci_set_address(uint32_t addr)
{
    pb_write32((addr << 25) | (1 <<24), IMX_EHCI_BASE + EHCI_DEVICEADDR);
    return PB_OK;
}

int plat_transport_init(void)
{
    uint32_t reg;

    /* Enable USB PLL */
    reg = pb_read32(0x020C8000+0x10);
    reg |= (1<<6);
    pb_write32(reg, 0x020C8000+0x10);

    /* Power up USB */
    pb_write32((1 << 31) | (1 << 30), 0x020C9038);
    pb_write32(0xFFFFFFFF, 0x020C9008);

    return imx_ehci_usb_init();
}

int imx_usdhc_plat_init(struct usdhc_device *dev)
{
    /* Configure pinmux for usdhc1 */
    pb_write32(0, 0x020E0000+0x1C0); /* CLK MUX */
    pb_write32(0, 0x020E0000+0x1BC); /* CMD MUX */
    pb_write32(0, 0x020E0000+0x1C4); /* DATA0 MUX */
    pb_write32(0, 0x020E0000+0x1C8); /* DATA1 MUX */
    pb_write32(0, 0x020E0000+0x1CC); /* DATA2 MUX */
    pb_write32(0, 0x020E0000+0x1D0); /* DATA3 MUX */
    pb_write32(1, 0x020E0000+0x1A8); /* DATA4 MUX */
    pb_write32(1, 0x020E0000+0x1AC); /* DATA5 MUX */
    pb_write32(1, 0x020E0000+0x1B0); /* DATA6 MUX */
    pb_write32(1, 0x020E0000+0x1B4); /* DATA7 MUX */
    pb_write32(1, 0x020E0000+0x1A4); /* RESET MUX */

    return PB_OK;
}

int plat_crypto_init(void)
{
    return imx_caam_init();
}

int plat_hash_init(enum pb_hash_algs alg)
{
    return caam_hash_init(alg);
}

int plat_hash_update(uint8_t *buf, size_t size)
{
    return caam_hash_update(buf, size);
}

int plat_hash_output(uint8_t *buf, size_t size)
{
    return caam_hash_output(buf, size);
}

int plat_pk_verify(uint8_t *signature, size_t signature_len,
                   uint8_t *hash, enum pb_hash_algs alg,
                   struct bpak_key *key)
{
    return caam_pk_verify(signature, signature_len, hash, alg, key);
}

int plat_status(void *response_bfr,
                    size_t *response_size)
{
    return board_status(&private, response_bfr, response_size);
}

int plat_early_boot(void)
{
    const struct pb_boot_config *boot_config = board_boot_config();
    if (boot_config->early_boot_cb)
        return boot_config->early_boot_cb(&private);
    else
        return PB_OK;
}

int plat_late_boot(uuid_t boot_part_uu, enum pb_boot_mode boot_mode)
{
    const struct pb_boot_config *boot_config = board_boot_config();
    if (boot_config->late_boot_cb)
        return boot_config->late_boot_cb(&private, boot_part_uu, boot_mode);
    else
        return PB_OK;
}
