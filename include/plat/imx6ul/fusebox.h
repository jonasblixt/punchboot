#ifndef INCLUDE_PLAT_IMX6UL_FUSEBOX_H
#define INCLUDE_PLAT_IMX6UL_FUSEBOX_H

#include <stdint.h>

#define IMX6UL_FUSE_LOCK_BANK   (0)
#define IMX6UL_FUSE_LOCK_WORD   (6)
#define IMX6UL_FUSE_REVOKE_BANK (5)
#define IMX6UL_FUSE_REVOKE_WORD (7)

struct imx6ul_srk_fuses {
    uint32_t srk[8];
};

int imx6ul_fuse_write_srk(const struct imx6ul_srk_fuses *fuses);
bool imx6ul_is_srk_fused(void);

#endif // INCLUDE_PLAT_IMX6UL_FUSEBOX_H
