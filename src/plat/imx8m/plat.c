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
#include <pb/pb.h>
#include <pb/io.h>
#include <uuid/uuid.h>
#include <pb/plat.h>
#include <pb/board.h>
#include <pb/fuse.h>
#include <xlat_tables.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx8m/clock.h>
#include <plat/imx8m/plat.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/caam.h>
#include <plat/imx/ehci.h>
#include <plat/imx/dwc3.h>
#include <plat/imx/hab.h>
#include <plat/imx/ocotp.h>
#include <plat/imx/wdog.h>
#include <plat/umctl2.h>

static struct imx8m_private private;
extern struct fuse fuses[];
extern const uint32_t rom_key_map[];

static struct pb_result_slc_key_status key_status;
static struct fuse fuse_uid0 =
        IMX8M_FUSE_BANK_WORD(0, 1, "UID0");

static struct fuse fuse_uid1 =
        IMX8M_FUSE_BANK_WORD(0, 2, "UID1");

static struct fuse rom_key_revoke_fuse =
        IMX8M_FUSE_BANK_WORD(9, 3, "Revoke");

static const char platform_namespace_uuid[] =
    "\x32\x92\xd7\xd2\x28\x25\x41\x00\x90\xc3\x96\x8f\x29\x60\xc9\xf2";


extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _ro_data_region_start, _ro_data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end,
            _big_buffer_start, _big_buffer_end, end;


static const mmap_region_t imx_mmap[] =
{
    /* UMCTL2 state */
    MAP_REGION_FLAT(0x40000000, (128 * 1024), MT_MEMORY | MT_RW),
    /* DDRC */
    MAP_REGION_FLAT(0x3c000000, (22 * 1024 * 1024), MT_DEVICE | MT_RW),
    /* Boot ROM API*/
    MAP_REGION_FLAT(0x00000000, (128 * 1024), MT_MEMORY | MT_RO | MT_EXECUTE),
    /* Periph (AIPS) */
    MAP_REGION_FLAT(0x30000000, (16 * 1024 * 1024), MT_DEVICE | MT_RW),
    /* OCRAM */
    MAP_REGION_FLAT(0x00900000, (128 * 1024), MT_MEMORY | MT_RW),
    /* GVP */
    MAP_REGION_FLAT(0x32000000, 0x20000, MT_DEVICE | MT_RW),
    /* USB */
    MAP_REGION_FLAT(0x38100000, (2 * 1024 * 1024), MT_DEVICE | MT_RW),
    {0}
};

int plat_boot_reason(void)
{
    return -PB_ERR_NOT_IMPLEMENTED;
}

int plat_get_uuid(char *out)
{
    plat_fuse_read(&fuse_uid0);
    plat_fuse_read(&fuse_uid1);

    uint32_t uid[2];
    uid[0] = fuse_uid0.value;
    uid[1] = fuse_uid1.value;

    LOG_INFO("%08x %08x", fuse_uid0.value, fuse_uid1.value);

    return uuid_gen_uuid3(platform_namespace_uuid,
                          (const char *) uid, 8, out);
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

static int imx8m_clock_cfg(uint32_t clk_id, uint32_t flags)
{
    if (clk_id > 133)
        return -PB_ERR;

    pb_write32(flags, (0x30388004 + 0x80*clk_id));

    return PB_OK;
}

/*
#if LOGLEVEL >= 3
static int imx8m_clock_print(uint32_t clk_id)
{
    uint32_t reg;
    uint32_t addr = (0x30388000 + 0x80*clk_id);

    if (clk_id > 133)
        return -PB_ERR;

    reg = pb_read32(addr);

    uint32_t mux = (reg >> 24) & 0x7;
    uint32_t enabled = ((reg & (1 << 28)) > 0);
    uint32_t pre_podf = (reg >> 16) & 7;
    uint32_t post_podf = reg & 0x1f;

    LOG_INFO("CLK %u, 0x%08x = 0x%08x", clk_id, addr, reg);
    LOG_INFO("  MUX %u", mux);
    LOG_INFO("  En: %u", enabled);
    LOG_INFO("  Prepodf: %u", pre_podf);
    LOG_INFO("  Postpodf: %u", post_podf);

    return PB_OK;
}

static int imx8m_cg_print(uint32_t cg_id)
{
    uint32_t reg;
    uint32_t addr = 0x30384000 + 0x10*cg_id;
    reg = pb_read32(addr);
    LOG_INFO("CG %u 0x%08x = 0x%08x", cg_id, addr, reg);
    return PB_OK;
}
#endif
*/


int plat_early_init(void)
{

    /* Read SOC variant and version */

     private.soc_ver_var = pb_read32(CCM_ANALOG_DIGPROG);

    /* Ungate GPIO blocks */

    pb_write32(3, 0x30384004 + 0x10*11);
    pb_write32(3, 0x30384004 + 0x10*12);
    pb_write32(3, 0x30384004 + 0x10*13);
    pb_write32(3, 0x30384004 + 0x10*14);
    pb_write32(3, 0x30384004 + 0x10*15);


    pb_write32(3, 0x30384004 + 0x10*27);
    pb_write32(3, 0x30384004 + 0x10*28);
    pb_write32(3, 0x30384004 + 0x10*29);
    pb_write32(3, 0x30384004 + 0x10*30);
    pb_write32(3, 0x30384004 + 0x10*31);

    /* PLL1 div10 */
    imx8m_clock_cfg(GPT1_CLK_ROOT | (5 << 24), CLK_ROOT_ON);

    gp_timer_init();

    /* Enable and ungate WDOG clocks */
    pb_write32((1 << 28), 0x30388004 + 0x80*114);
    pb_write32(3, 0x30384004 + 0x10*83);
    pb_write32(3, 0x30384004 + 0x10*84);
    pb_write32(3, 0x30384004 + 0x10*85);


    /* Configure main clocks */
    imx8m_clock_cfg(ARM_A53_CLK_ROOT, CLK_ROOT_ON);

    /* Configure PLL's */
    /* bypass the clock */
    pb_write32(pb_read32(ARM_PLL_CFG0) | FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);
    /* Set CPU core clock to 1 GHz */
    pb_write32(FRAC_PLL_INT_DIV_CTL_VAL(49), ARM_PLL_CFG1);

    pb_write32((FRAC_PLL_CLKE_MASK | FRAC_PLL_REFCLK_SEL_OSC_25M |
               FRAC_PLL_LOCK_SEL_MASK | FRAC_PLL_NEWDIV_VAL_MASK |
               FRAC_PLL_REFCLK_DIV_VAL(4) |
               FRAC_PLL_OUTPUT_DIV_VAL(0) | FRAC_PLL_BYPASS_MASK),
               ARM_PLL_CFG0);

    /* unbypass the clock */
    pb_clrbit32(FRAC_PLL_BYPASS_MASK, ARM_PLL_CFG0);

    while (((pb_read32(ARM_PLL_CFG0) & FRAC_PLL_LOCK_MASK) !=
            (uint32_t)FRAC_PLL_LOCK_MASK))
        __asm__("nop");

    pb_clrbit32(FRAC_PLL_NEWDIV_VAL_MASK, ARM_PLL_CFG0);

    pb_setbit32(SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
        SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
        SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
        SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
        SSCG_PLL_DIV20_CLKE_MASK, SYS_PLL1_CFG0);

    pb_setbit32(SSCG_PLL_CLKE_MASK | SSCG_PLL_DIV2_CLKE_MASK |
        SSCG_PLL_DIV3_CLKE_MASK | SSCG_PLL_DIV4_CLKE_MASK |
        SSCG_PLL_DIV5_CLKE_MASK | SSCG_PLL_DIV6_CLKE_MASK |
        SSCG_PLL_DIV8_CLKE_MASK | SSCG_PLL_DIV10_CLKE_MASK |
        SSCG_PLL_DIV20_CLKE_MASK, SYS_PLL2_CFG0);

    /* Configure USB clock */
    imx8m_clock_cfg((USB_BUS_CLK_ROOT | (1 << 24)), CLK_ROOT_ON);
    imx8m_clock_cfg(USB_CORE_REF_CLK_ROOT, CLK_ROOT_ON);
    imx8m_clock_cfg((USB_PHY_REF_CLK_ROOT | (1 << 24)), CLK_ROOT_ON);

    pb_write32(3, 0x30384004 + 0x10*77);
    pb_write32(3, 0x30384004 + 0x10*78);
    pb_write32(3, 0x30384004 + 0x10*79);
    pb_write32(3, 0x30384004 + 0x10*80);

    plat_console_init();



    umctl2_init();

    LOG_DBG("LPDDR4 training complete");

    pb_write32((1<<2), 0x303A00F8);

    pb_write32(0x03030303, 0x30384004 + 0x10*48);
    pb_write32(0x03030303, 0x30384004 + 0x10*81);

    /* Configure MMU */

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


    uintptr_t bb_start = (uintptr_t) &_big_buffer_start;
    size_t bb_size = ((uintptr_t) &_big_buffer_end) -
                      ((uintptr_t) &_big_buffer_start);

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

    enable_mmu_el3(0);

    /* MMU Config end */


    ocotp_init(CONFIG_IMX_OCOTP_BASE,
               CONFIG_IMX_OCOTP_WORDS_PER_BANK);

    LOG_DBG("Board early");
    return board_early_init(&private);
}


int imx_usdhc_plat_init(struct usdhc_device *dev)
{
    /* USDHC1 reset */
    /* Configure as GPIO 2 10*/
    pb_write32(5, 0x303300C8);
    pb_write32((1 << 10), 0x30210004);

    pb_setbit32(1<<10, 0x30210000);

    /* USDHC1 mux */
    pb_write32(0, 0x303300A0);
    pb_write32(0, 0x303300A4);

    pb_write32(0, 0x303300A8);
    pb_write32(0, 0x303300AC);
    pb_write32(0, 0x303300B0);
    pb_write32(0, 0x303300B4);
    pb_write32(0, 0x303300B8);
    pb_write32(0, 0x303300BC);
    pb_write32(0, 0x303300C0);
    pb_write32(0, 0x303300C4);
    //  pb_write32 (0, 0x303300C8);
    pb_write32(0, 0x303300CC);

    /* Setup USDHC1 pins */
#define USDHC1_PAD_CONF ((1 << 7) | (1 << 6) | (2 << 3) | 6)
    pb_write32(USDHC1_PAD_CONF, 0x30330308);
    pb_write32(USDHC1_PAD_CONF, 0x3033030C);
    pb_write32(USDHC1_PAD_CONF, 0x30330310);
    pb_write32(USDHC1_PAD_CONF, 0x30330314);
    pb_write32(USDHC1_PAD_CONF, 0x30330318);
    pb_write32(USDHC1_PAD_CONF, 0x3033031C);
    pb_write32(USDHC1_PAD_CONF, 0x30330320);
    pb_write32(USDHC1_PAD_CONF, 0x30330324);
    pb_write32(USDHC1_PAD_CONF, 0x30330328);
    pb_write32(USDHC1_PAD_CONF, 0x3033032C);

    pb_write32(USDHC1_PAD_CONF, 0x30330334);
    pb_clrbit32(1<<10, 0x30210000);
    return PB_OK;
}

int plat_transport_init(void)
{
    int err;

    err = dwc3_init(CONFIG_DWC3_BASE);

    if (err != PB_OK)
    {
        LOG_ERR("Could not initalize dwc3");
        return err;
    }

    return PB_OK;
}


/* Transport API */

int plat_transport_process(void)
{
    return dwc3_process();
}

int plat_transport_write(void *buf, size_t size)
{
    return dwc3_write(buf, size);
}

int plat_transport_read(void *buf, size_t size)
{
    return dwc3_read(buf, size);
}

bool plat_transport_ready(void)
{
    return dwc3_ready();
}

int imx_ehci_set_address(uint32_t addr)
{
    return dwc3_set_address(addr);
}

/* UART Interface */

int plat_console_init(void)
{
    /* Enable UART1 clock */
    pb_write32((1 << 28), 0x30388004 + 94*0x80);
    /* Ungate UART1 clock */
    pb_write32(3, 0x30384004 + 0x10*73);

    /* UART1 pad mux */
    pb_write32(0, 0x30330234);
    pb_write32(0, 0x30330238);

    /* UART1 PAD settings */
    pb_write32(7, 0x3033049C);
    pb_write32(7, 0x303304A0);

    return imx_uart_init(CONFIG_IMX_UART_BASE,
                  CONFIG_IMX_UART_BAUDRATE);
}

int plat_console_putchar(char c)
{
    imx_uart_putc(c);
    return PB_OK;
}
/* FUSE Interface */
int plat_fuse_read(struct fuse *f)
{
    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank*0x40 + f->word*0x10 + 0x400;
    }

    if (!f->shadow)
        f->shadow = IMX8M_FUSE_SHADOW_BASE + f->addr;

    f->value = pb_read32(f->shadow);

    return PB_OK;
}

int plat_fuse_write(struct fuse *f)
{
    char s[64];

    plat_fuse_to_string(f, s, 64);

    if ((f->status & FUSE_VALID) != FUSE_VALID)
    {
        LOG_ERR("Could not write fuse %s\n", s);
        return PB_ERR;
    }

    LOG_INFO("Writing: %s\n\r", s);

    return ocotp_write(f->bank, f->word, f->value);
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return snprintf(s, n,
            "   FUSE<%u,%u> 0x%x %s = 0x%x\n",
                f->bank, f->word, f->addr,
                f->description, f->value);
}

int plat_crypto_init(void)
{
    return imx_caam_init();
}

int plat_hash_init(struct pb_hash_context *ctx, enum pb_hash_algs alg)
{
    return caam_hash_init(ctx, alg);
}

int plat_hash_update(struct pb_hash_context *ctx, void *buf, size_t size)
{
    return caam_hash_update(ctx, buf, size);
}

int plat_hash_finalize(struct pb_hash_context *ctx, void *buf, size_t size)
{
    return caam_hash_finalize(ctx, buf, size);
}

int plat_pk_verify(void *signature, size_t size, struct pb_hash_context *hash,
                        struct bpak_key *key)
{
    return caam_pk_verify(hash, key, signature, size);
}

int plat_status(void *response_bfr,
                    size_t *response_size)
{
    return board_status(&private, response_bfr, response_size);
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

#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION
    err = board_slc_set_configuration(&private);

    if (err != PB_OK) {
        LOG_ERR("board_slc_set_configuration failed");
        return err;
    }
#endif

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

    return PB_OK;
}

int plat_slc_set_configuration_lock(void)
{
#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION_LOCK
    int rc = board_slc_set_configuration_lock(&private);

    if (rc != PB_OK) {
        LOG_ERR("board_slc_set_configuration_lock failed");
        return rc;
    }
#endif

    /* Not supported yet on IMX8M */
    return -PB_ERR;
}

int plat_slc_set_end_of_life(void)
{
    return -PB_ERR;
}

int plat_slc_read(enum pb_slc *slc)
{
    int err;
    (*slc) = PB_SLC_NOT_CONFIGURED;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }

        if (f->value)
        {
            (*slc) = PB_SLC_CONFIGURATION;
            break;
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
    return -PB_ERR;
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

bool plat_force_command_mode(void)
{
    return board_force_command_mode(&private);
}

int plat_patch_bootargs(void *fdt, int offset, bool verbose_boot)
{
    return board_patch_bootargs(&private, fdt, offset, verbose_boot);
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

#ifdef CONFIG_CALL_EARLY_PLAT_BOOT
int plat_early_boot(void)
{
    return board_early_boot(&private);
}

int plat_late_boot(bool *abort_boot, bool manual)
{
    return board_late_boot(&private, abort_boot, manual);
}
#endif
