#ifndef PLAT_IMX8X_CAAM_H_
#define PLAT_IMX8X_CAAM_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/crypto.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>

int imx8x_caam_setup(struct pb_crypto_driver *drv, sc_ipc_t ipc);

#endif  // PLAT_IMX8X_CAAM_H_
