#ifndef INCLUDE_DRIVERS_USB_IMX_CDNS3_UDC_H
#define INCLUDE_DRIVERS_USB_IMX_CDNS3_UDC_H

#include <stdint.h>

struct imx_cdns3_udc_config {
    uintptr_t base;
    uintptr_t non_core_base;
    uintptr_t phy_base;
};

int imx_cdns3_udc_init(const struct imx_cdns3_udc_config *cfg);

#endif
