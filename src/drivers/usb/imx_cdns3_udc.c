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

#include <arch/arch.h>
#include <drivers/usb/cdns3_udc_core.h>
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_salvo_phy.h>
#include <inttypes.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

#define USB3_CORE_CTRL11 (0x00)
#define USB3_CORE_CTRL12 (0x04)
#define USB3_CORE_INT    (0x08)
#define USB3_CORE_STATUS (0x0c)

/* USB3_CORE_CTRL11 */
#define SW_RESET_MASK    (0x3f << 26)
#define PWR_SW_RESET     BIT(31)
#define APB_SW_RESET     BIT(30)
#define AXI_SW_RESET     BIT(29)
#define RW_SW_RESET      BIT(28)
#define PHY_SW_RESET     BIT(27)
#define PHYAHB_SW_RESET  BIT(26)
#define ALL_SW_RESET \
    (PWR_SW_RESET | APB_SW_RESET | AXI_SW_RESET | RW_SW_RESET | PHY_SW_RESET | PHYAHB_SW_RESET)
#define OC_DISABLE          BIT(9)
#define MDCTRL_CLK_SEL      BIT(7)
#define MODE_STRAP_MASK     (0x7)
#define DEV_MODE            BIT(2)
#define HOST_MODE           BIT(1)
#define OTG_MODE            BIT(0)

/* USB3_INT_REG */
#define CLK_125_REQ         BIT(29)
#define LPM_CLK_REQ         BIT(28)
#define DEVU3_WAEKUP_EN     BIT(14)
#define OTG_WAKEUP_EN       BIT(12)
#define DEV_INT_EN          (3 << 8) /* DEV INT b9:8 */
#define HOST_INT1_EN        BIT(0) /* HOST INT b7:0 */

/* USB3_CORE_STATUS */
#define MDCTRL_CLK_STATUS   BIT(15)
#define DEV_POWER_ON_READY  BIT(13)
#define HOST_POWER_ON_READY BIT(12)

static const struct imx_cdns3_udc_config *cfg;
/* The operation init took the block out of reset, and set the core up on it:
 * what stop has to undo */
static bool started;
static bool core_started;

/* The core's power-on-ready after its resets are released (drivers.md 3).
 * Neither the i.MX 8QXP RM here nor Linux gives a time: 100 ms, as Linux
 * 6.1's cdns3-imx.c bounds its waits on this register (MDCTRL_CLK_STATUS,
 * readl_poll_timeout(..., 10, 100000)) */
#define POWER_ON_READY_TIMEOUT_us 100000

static int imx_cdns3_udc_start(void)
{
    int rc;
    struct pb_timeout to;
    // NOTE: U-boot configures SSPHY_STATUS, but that reg is not documented
    // in imx8x datasheet...

    started = true;
    core_started = false;

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

    pb_timeout_init_us(&to, POWER_ON_READY_TIMEOUT_us);

    while (!(mmio_read_32(cfg->non_core_base + USB3_CORE_STATUS) & DEV_POWER_ON_READY)) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Power-on-ready timeout");
            return -PB_ERR_TIMEOUT;
        }
    }

    LOG_DBG("Okay, let's go");

    rc = cdns3_udc_core_init();
    core_started = (rc == PB_OK);

    return rc;
}

/*
 * drivers.md 2.3, usb.md 4.2: the core leaves the bus and stops its DMA, then
 * the block is held in reset, as init found it after its first write. Its
 * power and clocks stay on: they are the platform's (plat_cm_init()), and the
 * next stage's glue resets the block itself before it uses it (Linux 6.1's
 * cdns3-imx.c, cdns_imx_noncore_init(), sets and clears ALL_SW_RESET in
 * USB3_CORE_CTRL1). Held in reset nothing of pb's runs in it, whatever the
 * core's registers did. The core's registers aren't read again until init
 * takes it out of reset: so a second stop does nothing
 */
static int imx_cdns3_udc_stop(void)
{
    int rc = PB_OK;

    if (!started)
        return PB_OK;

    /* Its DMA stopped before the AXI side is reset */
    if (core_started)
        rc = cdns3_udc_core_stop();

    mmio_clrsetbits_32(cfg->non_core_base + USB3_CORE_CTRL11, 0, ALL_SW_RESET);
    started = false;
    core_started = false;

    return rc;
}

int imx_cdns3_udc_init(const struct imx_cdns3_udc_config *cfg_)
{
    cfg = cfg_;

    cdns3_udc_core_set_base(cfg->base);

    static const struct usbd_hal_ops ops = {
        .init = imx_cdns3_udc_start,
        .stop = imx_cdns3_udc_stop,
        .xfer_start = cdns3_udc_core_xfer_start,
        .xfer_complete = cdns3_udc_core_xfer_complete,
        .xfer_cancel = cdns3_udc_core_xfer_cancel,
        .poll_setup_pkt = cdns3_udc_core_poll_setup_pkt,
        .set_configuration = cdns3_udc_set_configuration,
        .set_address = cdns3_udc_core_set_address,
        .ep0_xfer_zlp = cdns3_udc_core_xfer_zlp,
    };

    return usbd_init_hal_ops(&ops);
}
