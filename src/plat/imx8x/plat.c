
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
#include <pb/plat.h>
#include <pb/board.h>
#include <xlat_tables.h>
#include <uuid.h>
#include <plat/imx8x/plat.h>
#include <plat/imx/lpuart.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/gpt.h>
#include <plat/imx/ehci.h>
#include <plat/imx/caam.h>
#include <plat/sci/sci.h>
#include <plat/sci/sci_ipc.h>
#include <plat/sci/svc/seco/sci_seco_api.h>
#include <plat/sci/svc/pm/sci_pm_api.h>
#include <plat/imx8qx_pads.h>
#include <plat/defs.h>
#include <board/config.h>

#define LPCG_CLOCK_MASK         0x3U
#define LPCG_CLOCK_OFF          0x0U
#define LPCG_CLOCK_ON           0x2U
#define LPCG_CLOCK_AUTO         0x3U
#define LPCG_CLOCK_STOP         0x8U

#define LPCG_ALL_CLOCK_OFF      0x00000000U
#define LPCG_ALL_CLOCK_ON       0x22222222U
#define LPCG_ALL_CLOCK_AUTO     0x33333333U
#define LPCG_ALL_CLOCK_STOP     0x88888888U


extern char _code_start, _code_end,
            _data_region_start, _data_region_end,
            _ro_data_region_start, _ro_data_region_end,
            _zero_region_start, _zero_region_end,
            _stack_start, _stack_end,
            _big_buffer_start, _big_buffer_end, end;

extern struct fuse fuses[];
extern const uint32_t rom_key_map[];

static struct imx8x_private private;

static struct pb_result_slc_key_status key_status;

static enum pb_slc slc_status;

static struct fuse rom_key_revoke_fuse = IMX8X_FUSE_ROW(11, "Revoke");

static const char platform_namespace_uuid[] =
    "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02";

static const mmap_region_t imx_mmap[] = {
    /* Map UART */
    MAP_REGION_FLAT(IMX_LPUART_BASE, (64 * 1024), MT_DEVICE | MT_RW),
    /* Connecivity */
    MAP_REGION_FLAT(0x5b270000, (64*1024), MT_DEVICE | MT_RW), /* LPCG USB 2*/
    MAP_REGION_FLAT(0x5b280000, (64*1024), MT_DEVICE | MT_RW), /* LPCG USB 3*/
    MAP_REGION_FLAT(0x5b200000, (64*1024), MT_DEVICE | MT_RW), /* LPCG USDHC0*/
    MAP_REGION_FLAT(0x5b210000, (64*1024), MT_DEVICE | MT_RW), /* LPCG USDHC0*/
    MAP_REGION_FLAT(0x5b0d0000, (832*1024), MT_DEVICE | MT_RW), /* USB stuff*/
    MAP_REGION_FLAT(0x5b010000, (128*1024), MT_DEVICE | MT_RW), /* USDHC*/

    /* Low speed I/O */
    MAP_REGION_FLAT(0x5d000000, (8*1024*1024), MT_DEVICE | MT_RW),
    /* CAAM block*/
    MAP_REGION_FLAT(0x31430000, (64 * 1024), MT_DEVICE | MT_RW),
    {0}
};

static int boot_reason;

int plat_get_uuid(char *out)
{
    uint32_t uid[2];

    sc_misc_unique_id(private.ipc, &uid[0], &uid[1]);

    return uuid_gen_uuid3(platform_namespace_uuid,
                          (const char *) uid, 8, out);
}

static int get_soc_rev(uint32_t *soc_id, uint32_t *soc_rev)
{
    uint32_t id;
    sc_err_t err;

    if (!soc_id || !soc_rev)
        return -1;

    err = sc_misc_get_control(private.ipc, SC_R_SYSTEM, SC_C_ID, &id);
    if (err != SC_ERR_NONE)
        return err;

    *soc_rev = (id >> 5)  & 0xf;
    *soc_id = id & 0x1f;

    return 0;
}

/* Platform API Calls */

bool plat_force_command_mode(void)
{
    return board_force_command_mode(&private);
}

void plat_reset(void)
{
    sc_pm_reset(private.ipc, SC_PM_RESET_TYPE_BOARD);
}

unsigned int plat_get_us_tick(void)
{
    return gp_timer_get_tick();
}

void plat_wdog_init(void)
{
#ifdef CONFIG_ENABLE_WATCHDOG
    sc_rm_pt_t partition;
    sc_err_t err;

    err = sc_rm_get_partition(private.ipc, &partition);
    if (err)
    {
        LOG_ERR("Could not get partition for setting watchdog: %u", err);
        goto log_err;
    }

    err = sc_timer_set_wdog_action(private.ipc, partition,
            SC_TIMER_WDOG_ACTION_BOARD);
    if (err)
    {
        LOG_ERR("Could not set watchdog action: %u", err);
        goto log_err;
    }

    err = sc_timer_set_wdog_timeout(private.ipc,
                CONFIG_WATCHDOG_TIMEOUT*1000);
    if (err)
    {
        LOG_ERR("Could not set watchdog timeout: %u", err);
        goto log_err;
    }

    /* If the last argument to sc_timer_start_wdog is set to true, the
     * watchdog will be locked and cannot be reconfigured. Currently,
     * it should be possible to reconfigure the watchdog from Linux. */

    err = sc_timer_start_wdog(private.ipc, false);
    if (err)
    {
        LOG_ERR("Could not enable watchdog: %u", err);
        goto log_err;
    }

    LOG_INFO("Watchdog enabled with a %u ms timeout!",
                        CONFIG_WATCHDOG_TIMEOUT*1000);

    return;

log_err:
    LOG_ERR("Watchdog NOT enabled!");
#endif
}

void plat_wdog_kick(void)
{
    sc_timer_ping_wdog(private.ipc);
}

int plat_early_init(void)
{
    int rc = PB_OK;
    sc_pm_reset_reason_t sc_reset_reason; /* sc_pm_reset_reason_t is defined as uint8_t */
    sc_pm_clock_rate_t rate;

    sc_ipc_open(&private.ipc, SC_IPC_BASE);

    if ((rc = sc_pm_reset_reason(private.ipc, &sc_reset_reason)) == 0) {
        boot_reason = (int) sc_reset_reason;
    } else {
        boot_reason = -rc;
    }

    rc = get_soc_rev(&private.soc_id, &private.soc_rev);

    if (rc != 0) {
        private.soc_id = 0;
        private.soc_rev = 0;
    }

    /* Setup GPT0 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_GPT_0, SC_PM_PW_MODE_ON);
    rate = 24000000;
    sc_pm_set_clock_rate(private.ipc, SC_R_GPT_0, 2, &rate);

    rc = sc_pm_clock_enable(private.ipc, SC_R_GPT_0, 2, true, false);

    if (rc != SC_ERR_NONE)
        return -PB_ERR;

    /* Enable usb stuff */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_USB_0, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);

    pb_clrbit32((1 << 31) | (1 << 30), 0x5B100030);

    /* Enable USB PLL */
    pb_write32(0x00E03040, 0x5B100000+0xa0);

    /* Power up USB */
    pb_write32(0x00, 0x5B100000);

    rc = board_early_init(&private);

    if (rc != PB_OK)
        return rc;

    rc = gp_timer_init();

    if (rc != PB_OK)
        return rc;

    return rc;
}

int plat_mmu_init(void)
{
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

    reset_xlat_tables();
    mmap_add_region(code_start, code_start, code_size,
                            MT_RO | MT_MEMORY | MT_EXECUTE);
    mmap_add_region(stack_start, stack_start, stack_size,
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add_region(ro_start, ro_start, ro_size,
                            MT_RO | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Map ATF hole */
    mmap_add_region(0x80000000, 0x80000000,
                            (0x20000),
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);

    /* Add ram */
    mmap_add_region(rw_start, rw_start,
                            (RAM_SIZE),
                            MT_RW | MT_MEMORY | MT_EXECUTE_NEVER);
    mmap_add(imx_mmap);

    init_xlat_tables();

    enable_mmu_el3(0);

    return PB_OK;
}

int plat_boot_reason(void)
{
    return boot_reason;
}

const char * plat_boot_reason_str(void)
{
    switch (boot_reason) {
        case SC_PM_RESET_REASON_POR:
        return "POR";
        case SC_PM_RESET_REASON_JTAG:
        return "JTAG";
        case SC_PM_RESET_REASON_SW:
        return "SW";
        case SC_PM_RESET_REASON_WDOG:
        return "WDOG";
        case SC_PM_RESET_REASON_LOCKUP:
        return "LOCKUP";
        case SC_PM_RESET_REASON_SNVS:
        return "SNVS";
        case SC_PM_RESET_REASON_TEMP:
        return "TEMP";
        case SC_PM_RESET_REASON_MSI:
        return "MSI";
        case SC_PM_RESET_REASON_UECC:
        return "UECC";
        case SC_PM_RESET_REASON_SCFW_WDOG:
        return "SCFW WDOG";
        case SC_PM_RESET_REASON_ROM_WDOG:
        return "ROM WDOG";
        case SC_PM_RESET_REASON_SECO:
        return "SECO";
        case SC_PM_RESET_REASON_SCFW_FAULT:
        return "SCFW Fault";
        default:
        return "Unknown";
    }

}

/* FUSE Interface */
int plat_fuse_read(struct fuse *f)
{
    sc_err_t err;

    if (!(f->status & FUSE_VALID))
        return PB_ERR;

    if (!f->addr)
    {
        f->addr = f->bank;
    }

    err = sc_misc_otp_fuse_read(private.ipc, f->addr,
                                (uint32_t *) &(f->value));

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

int plat_fuse_write(struct fuse *f)
{
    char s[64];
    uint32_t err;

    plat_fuse_to_string(f, s, 64);

    LOG_INFO("Fusing %s", s);

    err = sc_misc_otp_fuse_write(private.ipc, f->addr, f->value);

    return (err == SC_ERR_NONE)?PB_OK:PB_ERR;
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    return snprintf(s, n,
            "   FUSE<%u> %s = 0x%08x",
                f->bank,
                f->description, f->value);
}

/* Console API */

int plat_console_init(void)
{
    sc_pm_clock_rate_t rate = 80000000;

#ifdef CONFIG_CONSOLE_UART0
    /* Power up UART0 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_UART_0, SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    sc_pm_set_clock_rate(private.ipc, SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(private.ipc, SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(private.ipc, SC_P_UART0_RX, UART_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_UART0_TX, UART_PAD_CTRL);
#elif CONFIG_CONSOLE_UART1
    /* Power up UART1 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_UART_1, SC_PM_PW_MODE_ON);

    /* Set UART1 clock root to 80 MHz */
    sc_pm_set_clock_rate(private.ipc, SC_R_UART_1, SC_PM_CLK_PER, &rate);

    /* Enable UART1 clock root */
    sc_pm_clock_enable(private.ipc, SC_R_UART_1, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(private.ipc, SC_P_UART1_RX, UART_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_UART1_TX, UART_PAD_CTRL);
#elif CONFIG_CONSOLE_UART2
    /* Power up UART2 */
    sc_pm_set_resource_power_mode(private.ipc, SC_R_UART_2, SC_PM_PW_MODE_ON);

    /* Set UART2 clock root to 80 MHz */
    sc_pm_set_clock_rate(private.ipc, SC_R_UART_2, SC_PM_CLK_PER, &rate);

    /* Enable UART2 clock root */
    sc_pm_clock_enable(private.ipc, SC_R_UART_2, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(private.ipc, SC_P_UART2_RX, UART_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_UART2_TX, UART_PAD_CTRL);
#else
    #error "No console uart selected"
#endif
    return imx_lpuart_init();
}

int plat_console_putchar(char c)
{
    imx_lpuart_write((char *) &c, 1);
    return PB_OK;
}

/* Crypto API */

int plat_crypto_init(void)
{

    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(private.ipc,
                                SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);
    return imx_caam_init();
}

int plat_hash_init(enum pb_hash_algs alg)
{
    return caam_hash_init(alg);
}

int plat_hash_update(uint8_t *buf, size_t len)
{
    return caam_hash_update(buf, len);
}

int plat_hash_output(uint8_t *buf, size_t len)
{
    return caam_hash_output(buf, len);
}

int plat_pk_verify(uint8_t *signature, size_t signature_len,
                   uint8_t *hash, enum pb_hash_algs alg,
                   struct bpak_key *key)
{
    return caam_pk_verify(signature, signature_len, hash, alg, key);
}

/* SLC API */

int plat_slc_init(void)
{
    return plat_slc_read(&slc_status);
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
    foreach_fuse(f, fuses)
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
    int err;
    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

#ifdef CONFIG_CALL_BOARD_SLC_SET_CONFIGURATION_LOCK
    err = board_slc_set_configuration_lock(&private);

    if (err != PB_OK) {
        LOG_ERR("board_slc_set_configuration failed");
        return err;
    }
#endif

    sc_seco_chip_info(private.ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128)
    {
        LOG_INFO("Configuration already locked");
        return PB_OK;
    }

    LOG_INFO("About to change security state to locked");

    err = sc_seco_forward_lifecycle(private.ipc, 16);

    if (err != SC_ERR_NONE)
        return -PB_ERR;

    return PB_OK;
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
    foreach_fuse(f, fuses)
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

    uint16_t lc;
    uint16_t monotonic;
    uint32_t uid_l;
    uint32_t uid_h;

    sc_seco_chip_info(private.ipc, &lc, &monotonic, &uid_l, &uid_h);

    if (lc == 128)
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
    uint8_t rom_key_mask = (rom_key_revoke_fuse.value >> 8) & 0x0f;

    if ((rom_key_mask & revoke_value) == revoke_value)
        (*active) = false;
    else
        (*active) = true;

    return PB_OK;
}

int plat_slc_revoke_key(uint32_t id)
{
    int rc;
    uint32_t info, fuse_before;
    UNUSED(id);

    LOG_INFO("Revoking keys as specified in image header");

    rc =  plat_fuse_read(&rom_key_revoke_fuse);
    if (rc != PB_OK)
    {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }
    LOG_INFO("Revocation fuse before revocation = %x",
            rom_key_revoke_fuse.value);
    fuse_before = rom_key_revoke_fuse.value;

    /* Commit OEM revocations = 0x10 */
    info = 0x10;

    /* sc_seco_commit returns which resource was revoked in info. In
     * our case, info should be 0x10 for OEM key after the revocation
     * is done. */
    rc = sc_seco_commit(private.ipc, &info);
    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("sc_seco_commit failed: %i", rc);
        return PB_ERR;
    }
    LOG_INFO("Commit reply: %x", info);

    rc =  plat_fuse_read(&rom_key_revoke_fuse);
    if (rc != PB_OK)
    {
        LOG_ERR("Could not read revoke fuse");
        return rc;
    }
    LOG_INFO("Revocation fuse after revocation = %x",
             rom_key_revoke_fuse.value);

    if (fuse_before == rom_key_revoke_fuse.value)
    {
        LOG_ERR("The revocation fuse had the same value before "
                "and after revocation!");
        return PB_ERR;
    }

    LOG_INFO("Revocation fuse changed bits: %x",
             fuse_before ^ rom_key_revoke_fuse.value);

    return PB_OK;
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

    LOG_DBG("ROM revoke mask: %08x", rom_key_revoke_fuse.value);

    uint8_t rom_key_mask = (rom_key_revoke_fuse.value >> 8) & 0x0f;

    for (int i = 0; i < 16; i++)
    {
        if (!rom_key_map[i])
            break;

        if (rom_key_mask & (1 << i))
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

/* Transport API */

int imx_ehci_set_address(uint32_t addr)
{
    pb_write32((addr << 25) | (1 <<24), IMX_EHCI_BASE + EHCI_DEVICEADDR);
    return PB_OK;
}

int plat_transport_init(void)
{
    return imx_ehci_usb_init();
}

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

int plat_patch_bootargs(void *fdt, int offset, bool verbose_boot)
{
    const struct pb_boot_config *boot_config = board_boot_config();
    if (boot_config->dtb_patch_cb) {
        return boot_config->dtb_patch_cb(&private, fdt, offset, verbose_boot);
    }

    return PB_OK;
}

int imx_usdhc_plat_init(struct usdhc_device *dev)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(private.ipc, SC_R_SDHC_0, SC_PM_PW_MODE_ON);


    sc_pm_clock_enable(private.ipc, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(private.ipc, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = 200000000;
    sc_pm_set_clock_rate(private.ipc, SC_R_SDHC_0, 2, &rate);

    if (rate != 200000000)
    {
        LOG_INFO("USDHC rate %u Hz", rate);
    }

    rc = sc_pm_clock_enable(private.ipc, SC_R_SDHC_0, SC_PM_CLK_PER,
                                true, false);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }


    sc_pad_set(private.ipc, SC_P_EMMC0_CLK, ESDHC_CLK_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_CMD, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA0, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA1, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA2, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA3, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA4, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA5, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA6, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_DATA7, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_STROBE, ESDHC_PAD_CTRL);
    sc_pad_set(private.ipc, SC_P_EMMC0_RESET_B, ESDHC_PAD_CTRL);

    return PB_OK;
}

int plat_status(void *response_bfr,
                    size_t *response_size)
{
    return board_status(&private, response_bfr, response_size);
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

