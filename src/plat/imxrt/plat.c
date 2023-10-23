#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <boot/boot.h>
#include <drivers/crypto/mbedtls.h>
#include <drivers/fuse/imx_ocotp.h>
#include <drivers/timer/imx_gpt.h>
#include <drivers/wdog/imx_wdog.h>
#include <drivers/uart/imx_lpuart.h>
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
    mmio_write_32(0x400fc024, 0x06490b03); /* CCM CSCDR1 */

    // Ungate a bunch of clocks
    mmio_write_32(0x400FC07C, 0xffffffff);

    // Hard-coded to lpuart1 for now
    // PAD mux: lpuart1 RX = alt2
    mmio_write_32(0x401F8000 + 0xf0, 2);
    // PAD mux: lpuart1 TX = alt2
    mmio_write_32(0x401F8000 + 0xec, 2);

    imx_lpuart_init(0x40184000, MHz(20), 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(0x40184000, &ops);
    LOG_DBG("i.MX RT Hello!");

    imx_gpt_init(0x401EC000, MHz(72));
    imx_wdog_init(0x400B8000, CONFIG_WATCHDOG_TIMEOUT);

    imx_ocotp_init(0x401F4000, 8);

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
    return 0;
}

const char *plat_boot_reason_str(void)
{
    return "?";
}

uint32_t plat_get_us_tick(void)
{
    return imx_gpt_get_tick();
}
