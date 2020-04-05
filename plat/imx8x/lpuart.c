#include <plat/imx8x/lpuart.h>
#include <plat/imx/lpuart.h>

struct imx8x_lpuart_private
{
    sc_ipc_t ipc;
};

#define IMX8X_LPUART_PRIV(drv) \
    ((struct imx8x_lpuart_private *)(drv->platform->private))

static int imx8x_lpuart_init(struct pb_console_driver *drv)
{
    struct imx8x_lpuart_private *priv = IMX8X_LPUART_PRIV(drv);
    sc_pm_clock_rate_t rate;

    /* Power up UART0 */
    sc_pm_set_resource_power_mode(priv->ipc, SC_R_UART_0, SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    rate = 80000000;
    sc_pm_set_clock_rate(priv->ipc, SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(priv->ipc, SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(priv->ipc, SC_P_UART0_RX, UART_PAD_CTRL);
    sc_pad_set(priv->ipc, SC_P_UART0_TX, UART_PAD_CTRL);

    return PB_OK;
}

static int imx8x_lpuart_free(struct pb_console_driver *drv)
{
    return PB_OK;
}

int imx8x_lpuart_setup(struct pb_console_driver *drv, sc_ipc_t ipc)
{
    struct imx8x_lpuart_private *priv = IMX8X_LPUART_PRIV(drv);

    if (sizeof(*priv) > drv->platform->size)
        return -PB_ERR_MEM;

    drv->init = imx_lpuart_init;
    drv->free = imx_lpuart_free;
    drv->platform->init = imx8x_lpuart_init;
    drv->platform->free = imx8x_lpuart_free;
    priv->ipc = ipc;

    return PB_OK;
}
