#ifndef INCLUDE_PLAT_IMX8X_FUSEBOX_H
#define INCLUDE_PLAT_IMX8X_FUSEBOX_H

#include <pb/utils_def.h>
#include <plat/imx8x/imx8x.h>
#include <stdint.h>

#define IMX8X_FUSE_REVOKE    (11)
#define IMX8X_FUSE_BOOT0     (18)
#define IMX8X_FUSE_BOOT1     (19)

/*
 * BOOT0 (0x920): BT_MODE_FUSES, the ROM's boot mode ([5:0] on the QXP, [2:0]
 * on the DXL), and BT_FUSE_SEL: the ROM takes the boot mode from the fuses
 * when the boot pins are 0 and BT_FUSE_SEL is set (QXP RM Table 5-1, 5-3,
 * 7-2; DXL RM Table 5-1)
 */
#define IMX8X_BOOT0_EMMC0    (0x2) /* eMMC on uSDHC0 */
#define IMX8X_BOOT0_SD1      (0x3) /* SD on uSDHC1 */
#define IMX8X_BOOT0_FUSE_SEL BIT(6)

/* BOOT1 (0x930): the uSDHC boot (RM Table 5-15) */
#define IMX8X_BOOT1_FAST     BIT(0) /* Fast boot */
#define IMX8X_BOOT1_FAST_ACK BIT(1) /* Fast boot acknowledge */
#define IMX8X_BOOT1_MMC_HS   BIT(2) /* [3:2] x1: eMMC high speed */
#define IMX8X_BOOT1_BUS(x)   ((x) << 4) /* [5:4] bus width */
#define IMX8X_BUS_8BIT       (0)
#define IMX8X_BUS_4BIT       (1)
#define IMX8X_BUS_8BIT_DDR   (2)
#define IMX8X_BUS_4BIT_DDR   (3)

#define IMX8X_FUSE_SRK0      (730)
#define IMX8X_FUSE_SRK1      (731)
#define IMX8X_FUSE_SRK2      (732)
#define IMX8X_FUSE_SRK3      (733)
#define IMX8X_FUSE_SRK4      (734)
#define IMX8X_FUSE_SRK5      (735)
#define IMX8X_FUSE_SRK6      (736)
#define IMX8X_FUSE_SRK7      (737)
#define IMX8X_FUSE_SRK8      (738)
#define IMX8X_FUSE_SRK9      (739)
#define IMX8X_FUSE_SRK10     (740)
#define IMX8X_FUSE_SRK11     (741)
#define IMX8X_FUSE_SRK12     (742)
#define IMX8X_FUSE_SRK13     (743)
#define IMX8X_FUSE_SRK14     (744)
#define IMX8X_FUSE_SRK15     (745)

#define IMX8X_FUSE_TZ_KEY0   (704)
#define IMX8X_FUSE_TZ_KEY1   (705)
#define IMX8X_FUSE_TZ_KEY2   (706)
#define IMX8X_FUSE_TZ_KEY3   (707)

#define IMX8X_FUSE_OEM_KEY0  (722)
#define IMX8X_FUSE_OEM_KEY1  (723)
#define IMX8X_FUSE_OEM_KEY2  (724)
#define IMX8X_FUSE_OEM_KEY3  (725)

#define IMX8X_SRK_HASH_WORDS (16)

void imx8x_fuse_init(sc_ipc_t ipc_);
int imx8x_fuse_write(uint32_t addr, uint32_t value);
int imx8x_fuse_read(uint32_t addr, uint32_t *result);
int imx8x_fuse_write_srk(const uint32_t *srk, size_t words);
/* Any of the SRK hash's words fused. PB_OK, or the read's error: the
 * answer is unknown then and *fused is false */
int imx8x_is_srk_fused(bool *fused);

#endif // INCLUDE_PLAT_IMX8X_FUSEBOX_H
