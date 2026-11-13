#include <boot/boot.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/rom_log/imx_hab_log.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imxrt/imxrt.h>
#include <string.h>
#include <uuid.h>

const char *platform_ns_uuid = "\x48\xf8\xc6\xcb\x8d\x07\x40\x66\xb9\x21\x0c\x1d\x8f\xd0\x54\x71";

static struct imxrt_platform plat;

/* WDOG1's WRSR as plat_early_init() read it */
static uint16_t wrsr;

static uint32_t fuse_read_helper(uint8_t row, uint8_t shift, uint32_t mask, uint32_t default_value)
{
    int rc;
    uint32_t fuse_data;

    rc = imx_ocotp_read(0, row, &fuse_data);

    if (rc != 0) {
        LOG_ERR("Could not read fuse %i (%i)", row, rc);
        return default_value;
    }

    fuse_data >>= shift;
    return (fuse_data & mask);
}

/* The HAB API */
static const struct imx_hab_log_config hab_log_cfg = {
    .rvt = IMXRT_HAB_RVT_BASE,
    .entry_size = 4,
};

static const struct imx_hab_fuses hab_fuses = {
    .sec_config_bank = IMXRT_FUSE_SEC_CONFIG_BANK,
    .sec_config_word = IMXRT_FUSE_SEC_CONFIG_WORD,
    .closed = IMXRT_FUSE_SEC_CONFIG_CLOSED,
    .srk_bank = IMXRT_FUSE_SRK_BANK,
    .srk_word = IMXRT_FUSE_SRK_WORD,
    .revoke_bank = IMXRT_FUSE_REVOKE_BANK,
    .revoke_word = IMXRT_FUSE_REVOKE_WORD,
    .revocable = IMXRT_FUSE_REVOCABLE_SRKS,
};

int plat_early_init(void)
{
    /* The WDOGs' power-down counters, whether the watchdog is used or not */
    mmio_clrsetbits_32(IMXRT_CCM_CCGR3, 0, CCGR3_WDOG1);
    mmio_clrsetbits_32(IMXRT_CCM_CCGR5, 0, CCGR5_WDOG2);
    imx_wdog_disable_powerdown(IMXRT_WDOG1_BASE);
    imx_wdog_disable_powerdown(IMXRT_WDOG2_BASE);

    /* The reset reason. The register is read only */
    wrsr = mmio_read_16(IMXRT_WDOG1_BASE + WDOG_WRSR);

#if CONFIG_ENABLE_WATCHDOG
    imx_wdog_init(IMXRT_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif

    /* Commont UART clock source config:
     *
     *  PLL3_SW_CLK -> /6 -> CG(CSCDR1[UART_CLK_SEL]) -> div (CSCDR1[UART_CLK_PODF])
     *    480MHz    -> 80 MHz -> Derive from pll3_80m -> Div by 4 == 20MHz
     */
    mmio_clrsetbits_32(
        IMXRT_CCM_CSCDR1, CSCDR1_UART_PODF_MASK | CSCDR1_UART_CLK_SEL, CSCDR1_UART_PODF(3));

    /* Configure systick. From the 24 MHz crystal: perclk, the GPT's other
     * source, changes its rate when plat_init() sets the ARM PLL */
    imx_gpt_init_24m(IMXRT_GPT1_BASE);
    log_time_start();
    return PB_OK;
}

int plat_init(void)
{
    int rc;

    (void)imx_hab_log_init(&hab_log_cfg);

    imx_ocotp_init(IMXRT_OCOTP_BASE, IMXRT_FUSE_WORDS_PER_BANK);
    imx_hab_fuses_init(&hab_fuses);
    /* Read speed grade fuse */

    plat.speed_grade = fuse_read_helper(
        IMXRT_FUSE_SPEED_GRADE_WORD, IMXRT_FUSE_SPEED_GRADE_SHIFT, IMXRT_FUSE_SPEED_GRADE_MASK, 1);
    LOG_INFO("Speed grade: %u", plat.speed_grade);

    /* Configure ARM Core clock */
    // Use PLL2 for AHB Root while we reconfigure the ARM PLL.
    mmio_clrsetbits_32(IMXRT_CCM_CBCMR, CBCMR_PRE_PERIPH_CLK_SEL_MASK, 0);

    // We probably don't need to wait for both clock switches, but the
    // manual is unclear.
    while (mmio_read_32(IMXRT_CCM_CDHIPR) & CDHIPR_PERIPH_CLK_SEL_BUSY)
        ;
    while (mmio_read_32(IMXRT_CCM_CDHIPR) & CDHIPR_PERIPH2_CLK_SEL_BUSY)
        ;
    // Update PLL1 Divider, 1200 MHz or 1056 MHz depending on speed grade
    // Speed grate 1 core frequency: 528 MHz
    // Speed grade 2 core frequency: 600 MHz

    if (plat.speed_grade == 1) {
        mmio_write_32(IMXRT_CCM_ANALOG_PLL_ARM,
                      ANALOG_PLL_ARM_ENABLE | ANALOG_PLL_ARM_DIV_SELECT(88));
    } else if (plat.speed_grade == 2) {
        mmio_write_32(IMXRT_CCM_ANALOG_PLL_ARM,
                      ANALOG_PLL_ARM_ENABLE | ANALOG_PLL_ARM_DIV_SELECT(100));
    } else {
        LOG_WARN("Unknown speed grade (%u), setting core speed to 528MHz", plat.speed_grade);
        mmio_write_32(IMXRT_CCM_ANALOG_PLL_ARM,
                      ANALOG_PLL_ARM_ENABLE | ANALOG_PLL_ARM_DIV_SELECT(88));
    }

    // Wait for PLL to lock
    while (!(mmio_read_32(IMXRT_CCM_ANALOG_PLL_ARM) & ANALOG_PLL_ARM_LOCK))
        ;

    // Switch back to PLL1 for AHB Root
    mmio_clrsetbits_32(IMXRT_CCM_CBCMR, CBCMR_PRE_PERIPH_CLK_SEL_MASK, CBCMR_PRE_PERIPH_CLK_SEL(3));

    while (mmio_read_32(IMXRT_CCM_CDHIPR) & CDHIPR_PERIPH_CLK_SEL_BUSY)
        ;
    while (mmio_read_32(IMXRT_CCM_CDHIPR) & CDHIPR_PERIPH2_CLK_SEL_BUSY)
        ;

    /* Compute some of the core clocks */
    uint32_t cbcdr = mmio_read_32(IMXRT_CCM_CBCDR);
    uint32_t cscmr1 = mmio_read_32(IMXRT_CCM_CSCMR1);
    uint32_t arm_pll = mmio_read_32(IMXRT_CCM_ANALOG_PLL_ARM);
    uint32_t cacrr = mmio_read_32(IMXRT_CCM_CACRR);

    uint32_t div_select = (arm_pll & ANALOG_PLL_ARM_DIV_SELECT_MASK) >>
                          ANALOG_PLL_ARM_DIV_SELECT_SHIFT;
    uint32_t arm_podf = ((cacrr & CACRR_ARM_PODF_MASK) >> CACRR_ARM_PODF_SHIFT) + 1;
    uint32_t ahb_podf = ((cbcdr & CBCDR_AHB_PODF_MASK) >> CBCDR_AHB_PODF_SHIFT) + 1;
    uint32_t ipg_podf = ((cbcdr & CBCDR_IPG_PODF_MASK) >> CBCDR_IPG_PODF_SHIFT) + 1;
    uint32_t per_podf = ((cscmr1 & CSCMR1_PERCLK_PODF_MASK) >> CSCMR1_PERCLK_PODF_SHIFT) + 1;
    unsigned int pll1_clk = (24 * div_select / 2) / arm_podf;
    plat.ahb_root_clk_MHz = pll1_clk / ahb_podf;
    plat.ipg_root_clk_MHz = plat.ahb_root_clk_MHz / ipg_podf;
    plat.per_root_clk_MHz = plat.ipg_root_clk_MHz / per_podf;

    LOG_INFO("PLL1 clock = %d MHz", pll1_clk);
    LOG_INFO("AHB root clock = %d MHz", plat.ahb_root_clk_MHz);
    LOG_INFO("IPG root clock = %d MHz", plat.ipg_root_clk_MHz);
    LOG_INFO("PER root clock = %d MHz", plat.per_root_clk_MHz);

    /* Command mode can start without it, so it isn't plat_init()'s error
     * (platform.md 2.2): main() would reset. Without a provider no image
     * verifies, and the boot stops with that. To be recorded in
     * plat_get_status() (F-248) */
    rc = mbedtls_pb_init();

    if (rc != PB_OK)
        LOG_ERR("Mbed TLS init failed (%i)", rc);

    return PB_OK;
}

#ifdef CONFIG_IMXRT_CM_USB_OTG1
/* Command mode's USB (platform.md 3.10): USB OTG1, with USBPHY1 and the
 * clocks of PLL_USB1 */
int plat_cm_init(void)
{
    /* Enable USB PLL */
    // TODO: Here we assume that a lot of other bit's are correctly set...
    mmio_clrsetbits_32(IMXRT_CCM_ANALOG_PLL_USB1, 0, ANALOG_PLL_USB_EN_USB_CLKS);

    mmio_setbits_32(IMXRT_CCM_CCGR6, CCGR6_USBOH3);

    imx_usb2_phy_init(IMXRT_USBPHY1_BASE);

    return imx_ci_udc_init(IMXRT_USB_BASE);
}
#endif

void plat_reset(void)
{
    imx_wdog_reset_now(IMXRT_WDOG1_BASE);
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    union {
        uint32_t uid[2];
        uint8_t uid_bytes[8];
    } plat_unique;

    if (*length < sizeof(plat_unique.uid_bytes))
        return -PB_ERR_BUF_TOO_SMALL;
    *length = sizeof(plat_unique.uid_bytes);

    imx_ocotp_read(0, IMXRT_FUSE_UNIQUE_1, &plat_unique.uid[0]);
    imx_ocotp_read(0, IMXRT_FUSE_UNIQUE_2, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));

    return PB_OK;
}

void plat_wdog_kick(void)
{
    imx_wdog_kick(IMXRT_WDOG1_BASE);
}

/* The priority and what WRSR doesn't have: plat/imxrt/imxrt.h */
int plat_get_boot_reason(struct pb_boot_reason *reason)
{
    reason->raw = wrsr;

    if (wrsr & WDOG_WRSR_TOUT) {
        reason->kind = PB_BOOT_REASON_WDOG;
        reason->name = "WDOG";
    } else if (wrsr & WDOG_WRSR_SFTW) {
        reason->kind = PB_BOOT_REASON_SW;
        reason->name = "SW";
    } else if (wrsr & WDOG_WRSR_POR) {
        reason->kind = PB_BOOT_REASON_POR;
        reason->name = "POR";
    } else {
        reason->kind = PB_BOOT_REASON_UNKNOWN;
        reason->name = "Unknown";
    }

    return PB_OK;
}

int plat_boot_reason(void)
{
    return mmio_read_16(IMXRT_WDOG1_BASE + WDOG_WRSR);
}

const char *plat_boot_reason_str(void)
{
    int reason = plat_boot_reason();
    switch (reason) {
    case WDOG_WRSR_POR:
        return "POR";
    case WDOG_WRSR_TOUT:
        return "WDOG";
    case WDOG_WRSR_SFTW:
        return "SW";
    default:
        break;
    }
    return "?";
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}

int plat_get_boot_source(struct pb_boot_source *src)
{
    (void)src;
    return -PB_ERR_NOT_SUPPORTED;
}

int plat_lockdown(void)
{
    /* Nothing yet: pb's image isn't HAB signed here, no CSF unlocks
     * revocation; the OCOTP's locks are unchecked (F-244) */
    return PB_OK;
}
