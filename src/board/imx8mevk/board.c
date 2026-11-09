/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/crypto/imx_caam.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/uart/imx_uart.h>
#include <drivers/usb/dwc3_udc.h>
#include <drivers/usb/imx8m_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <pb/board.h>
#include <pb/board_layout.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>
#include <stdbool.h>
#include <stdio.h>

/* DRAM: all set points of the timing, for DDR frequency scaling in Linux */
static const unsigned int imx8mevk_dram_mts[] = { 3200, 400, 100 };

struct dram_rates dram_rates = {
    .mts = imx8mevk_dram_mts,
    .num = ARRAY_SIZE(imx8mevk_dram_mts),
};

static int board_command(uint32_t command,
                         uint8_t *bfr,
                         size_t size,
                         uint8_t *response_bfr,
                         size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xf93ba110) {
        LOG_DBG("Got test command");
        char *response = (char *)response_bfr;
        size_t resp_buf_size = *response_size;

        (*response_size) = snprintf(response, resp_buf_size, "Test command hello 0x%x\n", command);

        response[(*response_size)++] = 0;
    } else {
        *response_size = 0;
    }

    return PB_OK;
}

void board_console_init(struct imx8m_platform *plat_)
{
    /* Enable UART1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(UART1_CLK_ROOT), CLK_ROOT_ON);
    /* Ungate UART1 clock */
    mmio_write_32(CCM_CCGR_SET(CCGR_UART1), CCGR_CLK_ON_MASK);

    /* UART1 pad mux */
    mmio_write_32(IOMUXC_MUX_UART1_RXD, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_UART1_TXD, MUX_ALT(0));

    /* UART1 PAD settings */
    mmio_write_32(IOMUXC_PAD_UART1_RXD, PAD_CTL_DSE_40_OHM);
    mmio_write_32(IOMUXC_PAD_UART1_TXD, PAD_CTL_DSE_40_OHM);

    imx_uart_init(IMX8M_UART1_BASE, MHz(25), 115200);

    static const struct console_ops ops = {
        .putc = imx_uart_putc,
    };

    console_init(IMX8M_UART1_BASE, &ops);
}

static int usdhc_emmc_setup(void)
{
    unsigned int rate;
    /* Enable and ungate USDHC1 clock */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USDHC1_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_USDHC1), CCGR_CLK_ON_MASK);

    /* USDHC1 mux */
    mmio_write_32(IOMUXC_MUX_SD1_CLK, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_CMD, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA0, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA1, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA2, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA3, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA4, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA5, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA6, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_DATA7, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_RESET_B, MUX_ALT(0));
    mmio_write_32(IOMUXC_MUX_SD1_STROBE, MUX_ALT(0));

    /* Setup USDHC1 pins */
#define USDHC1_PAD_CONF (PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_SRE_FAST | PAD_CTL_DSE_45_OHM)
    mmio_write_32(IOMUXC_PAD_SD1_CLK, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_CMD, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA0, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA1, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA2, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA3, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA4, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA5, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA6, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_DATA7, USDHC1_PAD_CONF);
    mmio_write_32(IOMUXC_PAD_SD1_STROBE, USDHC1_PAD_CONF);

    rate = MHz(200);

    static const struct imx_usdhc_config cfg = {
        .base = IMX8M_USDHC1_BASE,
        .delay_tap = 35,
        .mmc_config = {
            .mode = MMC_BUS_MODE_HS200,
            .width = MMC_BUS_WIDTH_8BIT,
            .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
            .boot0_uu = PART_boot0,
            .boot1_uu = PART_boot1,
            .user_uu = PART_user,
            .rpmb_uu = PART_rpmb,
            .flags = 0,
        },
    };

    return imx_usdhc_init(&cfg, rate);
}

static int board_hw_init(void)
{
    int rc;

    rc = imx_caam_init(IMX8M_CAAM_JR0_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    /* Linux can't instantiate the RNG when it runs in the normal world. Without
     * it Linux has no hardware RNG, which is no reason not to boot. */
    rc = imx_caam_rng_init(IMX8M_CAAM_BASE);

    if (rc != PB_OK)
        LOG_ERR("CAAM RNG init failed (%i)", rc);

    return usdhc_emmc_setup();
}

static int board_cm_init(void)
{
    int rc;

    /* Enable and ungate USB clocks */
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_BUS_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_CORE_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CORE_CLK_ROOT_GEN_TAGET_SET(USB_PHY_REF_CLK_ROOT), CLK_ROOT_ON);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_CTRL1), CCGR_CLK_ON_MASK);
    mmio_write_32(CCM_CCGR_SET(CCGR_USB_PHY1), CCGR_CLK_ON_MASK);

    rc = imx8m_usb_phy_init(IMX8M_USB1_PHY_BASE);

    if (rc != PB_OK) {
        LOG_ERR("imx8m usb phy failed (%i)", rc);
        return rc;
    }

    return imx_dwc3_udc_init(IMX8M_USB1_BASE);
}

/* No revocation and no SLC */
static const struct rot_config rot = { 0 };
static const struct slc_config slc = { 0 };

/*
 * Unfinished (F-033): no A/B state and no boot driver, so board_setup()
 * fails after the partitions and pb always enters command mode.
 */
const struct board board = {
    .name = "imx8mevk",
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .gpt_dev_uu = PART_user,
    .gpt_tables = board_layout_gpt,
    .gpt_tables_length = ARRAY_SIZE(board_layout_gpt),
    .cm_init = board_cm_init,
    .cm_transport = &pb_dev_cls_transport,
    .command = board_command,
};
