#include <boot/boot.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/uart/imx_lpuart.h>
#include <drivers/wdog/imx_wdog.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/imxrt/imxrt.h>
#include <string.h>
#include <uuid.h>

static struct imxrt_platform plat;

int plat_init(void)
{
    int rc;

    /**
     * Clock
     *  PLL3 MUX (CCSR[pll3_sw_clk_se]) either 480MHz or CCM_PLL3_BYP
     *
     * default config:
     *  PLL3_SW_CLK -> /6 -> CG(CSCDR1[UART_CLK_SEL]) -> div (CSCDR1[UART_CLK_PODF])
     *    480MHz    -> 80 MHz -> Derive from pll3_80m -> Div by 1 == 80MHz
     */

    // Clock config
    mmio_write_32(IMXRT_CCM_BASE + 0x24, 0x06490b03); /* CCM CSCDR1 */

    // Ungate a bunch of clocks
    mmio_write_32(IMXRT_CCM_BASE + 0x7c, 0xffffffff);

    // Hard-coded to lpuart1 for now
    // PAD mux: lpuart1 RX = alt2
    mmio_write_32(IMXRT_IOMUXC_BASE + 0xf0, 2);
    // PAD mux: lpuart1 TX = alt2
    mmio_write_32(IMXRT_IOMUXC_BASE + 0xec, 2);

    imx_lpuart_init(IMXRT_LPUART1_BASE, MHz(20), 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(IMXRT_LPUART1_BASE, &ops);
    LOG_DBG("i.MX RT Hello!");

    imx_gpt_init(IMXRT_GPT1_BASE, MHz(72));

#if CONFIG_ENABLE_WATCHDOG
    imx_wdog_init(IMXRT_WDOG1_BASE, CONFIG_WATCHDOG_TIMEOUT);
#endif

    imx_ocotp_init(IMXRT_OCOTP_BASE, 8);

    rc = mbedtls_pb_init();

    return rc;
}

void plat_reset(void)
{
    imx_wdog_reset_now();
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

    imx_ocotp_read(0, 1, &plat_unique.uid[0]);
    imx_ocotp_read(0, 2, &plat_unique.uid[1]);
    memcpy(output, plat_unique.uid_bytes, sizeof(plat_unique.uid_bytes));

    return PB_OK;
}

void plat_wdog_kick(void)
{
    imx_wdog_kick();
}

int plat_board_init(void)
{
    return board_init(&plat);
}

int plat_boot_reason(void)
{
    uint16_t wrsr = mmio_read_16(IMXRT_WDOG1_BASE + 0x4);
    return wrsr;
}

const char *plat_boot_reason_str(void)
{
    int reason = plat_boot_reason();
    switch (reason) {
    case (1 << 4):
        return "POR";
    case (1 << 1):
        return "WDOG";
    case (1 << 0):
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
