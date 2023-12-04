#ifndef INCLUDE_PLAT_IMX8X_FUSEBOX_H
#define INCLUDE_PLAT_IMX8X_FUSEBOX_H

#include <plat/imx8x/imx8x.h>
#include <stdint.h>

#define IMX8X_FUSE_REVOKE   (11)
#define IMX8X_FUSE_BOOT0    (18)
#define IMX8X_FUSE_BOOT1    (19)

#define IMX8X_FUSE_SRK0     (730)
#define IMX8X_FUSE_SRK1     (731)
#define IMX8X_FUSE_SRK2     (732)
#define IMX8X_FUSE_SRK3     (733)
#define IMX8X_FUSE_SRK4     (734)
#define IMX8X_FUSE_SRK5     (735)
#define IMX8X_FUSE_SRK6     (736)
#define IMX8X_FUSE_SRK7     (737)
#define IMX8X_FUSE_SRK8     (738)
#define IMX8X_FUSE_SRK9     (739)
#define IMX8X_FUSE_SRK10    (740)
#define IMX8X_FUSE_SRK11    (741)
#define IMX8X_FUSE_SRK12    (742)
#define IMX8X_FUSE_SRK13    (743)
#define IMX8X_FUSE_SRK14    (744)
#define IMX8X_FUSE_SRK15    (745)

#define IMX8X_FUSE_TZ_KEY0  (704)
#define IMX8X_FUSE_TZ_KEY1  (705)
#define IMX8X_FUSE_TZ_KEY2  (706)
#define IMX8X_FUSE_TZ_KEY3  (707)

#define IMX8X_FUSE_OEM_KEY0 (722)
#define IMX8X_FUSE_OEM_KEY1 (723)
#define IMX8X_FUSE_OEM_KEY2 (724)
#define IMX8X_FUSE_OEM_KEY3 (725)

struct imx8x_srk_fuses {
    uint32_t srk[16];
};

void imx8x_fuse_init(sc_ipc_t ipc_);
int imx8x_fuse_write(uint32_t addr, uint32_t value);
int imx8x_fuse_read(uint32_t addr, uint32_t *result);
int imx8x_fuse_write_srk(const struct imx8x_srk_fuses *fuses);
bool imx8x_is_srk_fused(void);

#endif // INCLUDE_PLAT_IMX8X_FUSEBOX_H
