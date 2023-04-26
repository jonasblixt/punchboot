/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A minimalistic driver for the cadence USB3 OTG controller. This only
 * supports the Device role.
 *
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arch/arch.h>
#include <pb/pb.h>
#include <pb/mmio.h>
#include <pb/delay.h>
#include <drivers/usb/cdns3_udc_core.h>
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_salvo_phy.h>

#define USB3_CORE_CTRL11 (0x00)
#define USB3_CORE_CTRL12 (0x04)
#define USB3_CORE_INT    (0x08)
#define USB3_CORE_STATUS (0x0c)

/* USB3_CORE_CTRL11 */
#define SW_RESET_MASK    (0x3f << 26)
#define PWR_SW_RESET    BIT(31)
#define APB_SW_RESET    BIT(30)
#define AXI_SW_RESET    BIT(29)
#define RW_SW_RESET    BIT(28)
#define PHY_SW_RESET    BIT(27)
#define PHYAHB_SW_RESET    BIT(26)
#define ALL_SW_RESET    (PWR_SW_RESET | APB_SW_RESET | AXI_SW_RESET | \
        RW_SW_RESET | PHY_SW_RESET | PHYAHB_SW_RESET)
#define OC_DISABLE    BIT(9)
#define MDCTRL_CLK_SEL    BIT(7)
#define MODE_STRAP_MASK    (0x7)
#define DEV_MODE    BIT(2)
#define HOST_MODE    BIT(1)
#define OTG_MODE    BIT(0)

/* USB3_INT_REG */
#define CLK_125_REQ    BIT(29)
#define LPM_CLK_REQ    BIT(28)
#define DEVU3_WAEKUP_EN    BIT(14)
#define OTG_WAKEUP_EN    BIT(12)
#define DEV_INT_EN (3 << 8) /* DEV INT b9:8 */
#define HOST_INT1_EN BIT(0) /* HOST INT b7:0 */

/* USB3_CORE_STATUS */
#define MDCTRL_CLK_STATUS    BIT(15)
#define DEV_POWER_ON_READY    BIT(13)
#define HOST_POWER_ON_READY    BIT(12)

static const struct imx_cdns3_udc_config *cfg;

static int imx_cdns3_udc_start(void)
{
    int rc;
    // NOTE: U-boot configures SSPHY_STATUS, but that reg is not documented
    // in imx8x datasheet...

    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_CTRL11, 0, ALL_SW_RESET);
    pb_delay_us(1);
    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_CTRL11, MODE_STRAP_MASK, DEV_MODE);
    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_CTRL11, PHYAHB_SW_RESET, 0);
    pb_delay_ms(1);

    rc = imx_salvo_phy_init(cfg->phy_base);

    if (rc != PB_OK)
        return rc;

    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_INT, 0, DEV_INT_EN);
    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_CTRL11, ALL_SW_RESET, 0);

    LOG_DBG("Waiting for core to come out of reset");

    while (!(mmio_read_32(cfg->non_core_base + USB3_CORE_STATUS) & DEV_POWER_ON_READY))
        ;
    LOG_DBG("Okay, let's go");

    return cdns3_udc_core_init();
}

int imx_cdns3_udc_init(const struct imx_cdns3_udc_config *cfg_)
{
    cfg = cfg_;

    cdns3_udc_core_set_base(cfg->base);

    static const struct usbd_hal_ops ops = {
        .init = imx_cdns3_udc_start,
        .stop = cdns3_udc_core_stop,
        .xfer_start = cdns3_udc_core_xfer_start,
        .xfer_complete = cdns3_udc_core_xfer_complete,
        .xfer_cancel = cdns3_udc_core_xfer_cancel,
        .poll_setup_pkt = cdns3_udc_core_poll_setup_pkt,
        .configure_ep = cdns3_udc_core_configure_ep,
        .set_address = cdns3_udc_core_set_address,
        .ep0_xfer_zlp = cdns3_udc_core_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
