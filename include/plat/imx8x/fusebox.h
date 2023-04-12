#ifndef INCLUDE_PLAT_IMX8X_FUSEBOX_H
#define INCLUDE_PLAT_IMX8X_FUSEBOX_H

#include <stdint.h>
#include <plat/imx8x/imx8x.h>

#define IMX8X_FUSE_REVOKE (11)
#define IMX8X_FUSE_BOOT0  (18)
#define IMX8X_FUSE_BOOT1  (19)

struct imx8x_srk_fuses
{
    uint32_t srk[16];
};

void imx8x_fuse_init(sc_ipc_t ipc_);
int imx8x_fuse_write(uint32_t addr, uint32_t value);
int imx8x_fuse_read(uint32_t addr, uint32_t *result);
int imx8x_fuse_write_srk(const struct imx8x_srk_fuses *fuses);
bool imx8x_is_srk_fused(void);

#endif  // INCLUDE_PLAT_IMX8X_FUSEBOX_H
