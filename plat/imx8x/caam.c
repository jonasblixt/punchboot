#include <pb/pb.h>
#include <pb/io.h>
#include <pb/crypto.h>
#include <plat/imx8x/caam.h>
#include <plat/imx/caam.h>

struct imx8x_caam_private
{
    sc_ipc_t ipc;
};

#define IMX8X_CAAM_PRIV(drv) \
    ((struct imx8x_caam_private *)(drv->platform->private))

static int imx8x_caam_init(struct pb_crypto_driver *drv)
{
    struct imx8x_caam_private *priv = IMX8X_CAAM_PRIV(drv);

    LOG_INFO("init");

    sc_pm_set_resource_power_mode(priv->ipc,
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(priv->ipc,
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(priv->ipc,
                                SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(priv->ipc,
                                SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);

    drv->init = imx_caam_init;
    drv->free = imx_caam_free;

    return PB_OK;
}

int imx8x_caam_setup(struct pb_crypto_driver *drv, sc_ipc_t ipc)
{
    struct imx8x_caam_private *priv = IMX8X_CAAM_PRIV(drv);

    priv->ipc = ipc;
    drv->platform->init = imx8x_caam_init;

    return PB_OK;
}
