#include <pb/pb.h>
#include <pb/io.h>
#include <pb/transport.h>
#include <plat/imx/ehci.h>
#include <plat/imx8x/ehci.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>

struct imx8x_ehci_private
{
    sc_ipc_t ipc;
};

#define IMX8X_EHCI_PRIV(drv) \
    ((struct imx8x_ehci_private *)(drv->platform->private))

static int imx8x_ehci_init(struct pb_transport_driver *drv)
{
    struct imx8x_ehci_private *priv = IMX8X_EHCI_PRIV(drv);

    LOG_DBG("init");

    sc_pm_set_resource_power_mode(priv->ipc, SC_R_USB_0, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(priv->ipc, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);

    pb_clrbit32((1 << 31) | (1 << 30), 0x5B100030);

    /* Enable USB PLL */
    pb_write32(0x00E03040, 0x5B100000+0xa0);

    /* Power up USB */
    pb_write32(0x00, 0x5B100000);

    return PB_OK;
}

static int imx8x_ehci_free(struct pb_transport_driver *drv)
{
    return PB_OK;
}

static int imx8x_ehci_set_address(struct pb_transport_driver *drv,
                                    uint32_t addr)
{
    struct imx_ehci_device *dev = (struct imx_ehci_device *) drv->private;

    pb_write32((addr << 25) | (1 <<24), dev->base+EHCI_DEVICEADDR);

    return PB_OK;
}

int imx8x_ehci_setup(struct pb_transport_driver *drv, sc_ipc_t ipc)
{
    struct imx_ehci_device *dev = (struct imx_ehci_device *) drv->private;
    struct imx8x_ehci_private *priv = IMX8X_EHCI_PRIV(drv);

    if (sizeof(*priv) > drv->platform->size)
        return -PB_ERR_MEM;

    drv->init = imx_ehci_usb_init;
    drv->free = imx_ehci_usb_free;
    drv->platform->init = imx8x_ehci_init;
    drv->platform->free = imx8x_ehci_free;
    priv->ipc = ipc;
    dev->iface.set_address = imx8x_ehci_set_address;

    return PB_OK;
}
