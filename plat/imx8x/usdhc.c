#include <pb/pb.h>
#include <plat/imx8x/usdhc.h>

struct imx8x_usdhc_private
{
    sc_ipc_t ipc;
};

#define IMX8X_USDHC_PRIV(drv) \
    ((struct imx8x_usdhc_private *)(drv->platform->private))

static int imx8x_usdhc_init(struct pb_storage_driver *drv)
{
    int rc;
    unsigned int rate;
    struct imx8x_usdhc_private *priv = IMX8X_USDHC_PRIV(drv);

    sc_pm_set_resource_power_mode(priv->ipc, SC_R_SDHC_0, SC_PM_PW_MODE_ON);


    sc_pm_clock_enable(priv->ipc, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(priv->ipc, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = 200000000;
    sc_pm_set_clock_rate(priv->ipc, SC_R_SDHC_0, 2, &rate);

    if (rate != 200000000)
    {
        LOG_INFO("USDHC rate %u Hz", rate);
    }

    rc = sc_pm_clock_enable(priv->ipc, SC_R_SDHC_0, SC_PM_CLK_PER,
                                true, false);

    if (rc != SC_ERR_NONE)
    {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }


    sc_pad_set(priv->ipc, SC_P_EMMC0_CLK, ESDHC_CLK_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_CMD, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA0, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA1, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA2, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA3, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA4, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA5, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA6, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_DATA7, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_STROBE, ESDHC_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_EMMC0_RESET_B, ESDHC_PAD_CTRL);

    return imx_usdhc_init(drv);
}

int imx8x_usdhc_setup(struct pb_storage_driver *drv,
                      sc_ipc_t ipc)
{
    struct imx8x_usdhc_private * priv = IMX8X_USDHC_PRIV(drv);

    if (drv->platform->size < sizeof(*priv))
        return -PB_ERR_MEM;

    priv->ipc = ipc;
    drv->platform->init = imx8x_usdhc_init;
    drv->platform->free = NULL;


    return PB_OK;
}
