#ifndef INCLUDE_PLAT_IMX8X_USDHC_H_
#define INCLUDE_PLAT_IMX8X_USDHC_H_

#include <pb/storage.h>
#include <plat/imx/usdhc.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>

int imx8x_usdhc_setup(struct pb_storage_driver *drv,
                      sc_ipc_t ipc);

#endif  // INCLUDE_PLAT_IMX8X_USDHC_H_
