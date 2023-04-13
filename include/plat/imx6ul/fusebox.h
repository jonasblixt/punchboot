#ifndef INCLUDE_PLAT_IMX6UL_FUSEBOX_H
#define INCLUDE_PLAT_IMX6UL_FUSEBOX_H

#include <stdint.h>

struct imx6ul_srk_fuses {
    uint32_t srk[8];
};

int imx6ul_fuse_write_srk(const struct imx6ul_srk_fuses *fuses);

#endif  // INCLUDE_PLAT_IMX6UL_FUSEBOX_H
