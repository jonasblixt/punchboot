#ifndef PLAT_IMX8X_LPUART_H_
#define PLAT_IMX8X_LPUART_H_

#include <stdint.h>
#include <stddef.h>
#include <pb/console.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>

int imx8x_lpuart_setup(struct pb_console_driver *drv, sc_ipc_t ipc);

#endif  // PLAT_IMX8X_LPUART_H_
