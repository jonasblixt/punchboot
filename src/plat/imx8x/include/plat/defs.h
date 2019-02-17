#ifndef __IMX8M_DEFS_H__
#define __IMX8M_DEFS_H__


#include <pb.h>
#include <io.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx/gpt.h>
#include <plat/imx/usdhc.h>
#include <plat/imx/ehci.h>
#include <plat/imx/lpuart.h>
#include <plat/imx/caam.h>
#include <plat/imx8qxp_pads.h>
#include <plat/iomux.h>

#define PB_BOOTPART_OFFSET 0

struct pb_platform_setup
{
    sc_ipc_t ipc_handle;
    struct gp_timer tmr0;
    struct usdhc_device usdhc0;
    struct lpuart_device uart0;
    struct fsl_caam_jr caam;
    struct ehci_device usb;
};



#define ESDHC_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
						 (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ESDHC_CLK_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                             (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
                             (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
						     (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                             (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define UART_PAD_CTRL	(PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
			(SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
			(SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			(SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | \
			(SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))



#define GPIO_PAD_CTRL   (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#endif
