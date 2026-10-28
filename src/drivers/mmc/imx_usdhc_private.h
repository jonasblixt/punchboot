/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * NXP uSDHC: registers and fields, offsets from the controller base. From
 * the uSDHC chapters of the i.MX6UL RM (56) and the i.MX8M Plus RM
 * (10.3.6); the layout is the same, the differences are noted. Only the
 * registers and fields this driver uses are defined.
 */

#ifndef DRIVERS_MMC_IMX_USDHC_PRIVATE_H
#define DRIVERS_MMC_IMX_USDHC_PRIVATE_H

#include <pb/utils_def.h>
#include <stdint.h>

#define USDHC_FIELD(x, shift, mask)     (((uint32_t)(x) << (shift)) & (mask))
#define USDHC_FIELD_GET(v, shift, mask) (((v) & (mask)) >> (shift))

/* Block attributes */
#define USDHC_BLK_ATT                   0x04
#define BLK_ATT_BLKCNT_SHIFT            16
#define BLK_ATT_BLKCNT_MASK             (0xffffu << BLK_ATT_BLKCNT_SHIFT)
#define BLK_ATT_BLKCNT(x)               USDHC_FIELD(x, BLK_ATT_BLKCNT_SHIFT, BLK_ATT_BLKCNT_MASK)
#define BLK_ATT_BLKCNT_MAX              0xffff
#define BLK_ATT_BLKSIZE_MASK            0x1fffu
#define BLK_ATT_BLKSIZE(x)              USDHC_FIELD(x, 0, BLK_ATT_BLKSIZE_MASK)

#define USDHC_CMD_ARG                   0x08

/* Command transfer type */
#define USDHC_CMD_XFR_TYP               0x0c
#define CMD_XFR_TYP_CMDINX_SHIFT        24
#define CMD_XFR_TYP_CMDINX_MASK         (0x3fu << CMD_XFR_TYP_CMDINX_SHIFT)
#define CMD_XFR_TYP_CMDINX(x)    USDHC_FIELD(x, CMD_XFR_TYP_CMDINX_SHIFT, CMD_XFR_TYP_CMDINX_MASK)
#define CMD_XFR_TYP_CMDTYP_SHIFT 22
#define CMD_XFR_TYP_CMDTYP_MASK  (0x3u << CMD_XFR_TYP_CMDTYP_SHIFT)
#define CMD_XFR_TYP_CMDTYP(x)    USDHC_FIELD(x, CMD_XFR_TYP_CMDTYP_SHIFT, CMD_XFR_TYP_CMDTYP_MASK)
#define CMDTYP_NORMAL            0
#define CMDTYP_ABORT             3 /* CMD12, CMD52 I/O abort */
#define CMD_XFR_TYP_DPSEL        BIT_32(21) /* Data present */
#define CMD_XFR_TYP_CICEN        BIT_32(20) /* Command index check */
#define CMD_XFR_TYP_CCCEN        BIT_32(19) /* Command CRC check */
#define CMD_XFR_TYP_RSPTYP_SHIFT 16
#define CMD_XFR_TYP_RSPTYP_MASK  (0x3u << CMD_XFR_TYP_RSPTYP_SHIFT)
#define CMD_XFR_TYP_RSPTYP(x)    USDHC_FIELD(x, CMD_XFR_TYP_RSPTYP_SHIFT, CMD_XFR_TYP_RSPTYP_MASK)
#define RSPTYP_NONE              0
#define RSPTYP_136               1
#define RSPTYP_48                2
#define RSPTYP_48_BUSY           3

/* Responses: 48 bit R[39:8] in RSP0, R2 R[127:8] in {RSP3[23:0], RSP2, RSP1, RSP0}, the auto
 * CMD12 response in RSP3 */
#define USDHC_CMD_RSP0           0x10
#define USDHC_CMD_RSP1           0x14
#define USDHC_CMD_RSP2           0x18
#define USDHC_CMD_RSP3           0x1c

/* Present state */
#define USDHC_PRES_STATE         0x24
#define PRES_STATE_DAT0          BIT_32(24) /* DAT0 level, low while the card is busy */
#define PRES_STATE_SDSTB         BIT_32(3) /* Must be set before SDCLKFS/DVS change */
#define PRES_STATE_DLA           BIT_32(2)
#define PRES_STATE_CDIHB         BIT_32(1)
#define PRES_STATE_CIHB          BIT_32(0)

/* Protocol control */
#define USDHC_PROT_CTRL          0x28
#define PROT_CTRL_BURST_LEN_EN_INCR BIT_32(27) /* i.MX6UL; reserved, 1 at reset on the i.MX8M */
#define PROT_CTRL_RSVD_23_21        BIT_32(23) /* Bits 23:21 are always written as 100b */
#define PROT_CTRL_DMASEL_SHIFT      8
#define PROT_CTRL_DMASEL_MASK       (0x3u << PROT_CTRL_DMASEL_SHIFT)
#define PROT_CTRL_DMASEL(x)         USDHC_FIELD(x, PROT_CTRL_DMASEL_SHIFT, PROT_CTRL_DMASEL_MASK)
#define DMASEL_ADMA2                2
#define PROT_CTRL_EMODE_SHIFT       4
#define PROT_CTRL_EMODE_MASK        (0x3u << PROT_CTRL_EMODE_SHIFT)
#define PROT_CTRL_EMODE(x)          USDHC_FIELD(x, PROT_CTRL_EMODE_SHIFT, PROT_CTRL_EMODE_MASK)
#define EMODE_LITTLE_ENDIAN         2
#define PROT_CTRL_DTW_SHIFT         1
#define PROT_CTRL_DTW_MASK          (0x3u << PROT_CTRL_DTW_SHIFT)
#define PROT_CTRL_DTW(x)            USDHC_FIELD(x, PROT_CTRL_DTW_SHIFT, PROT_CTRL_DTW_MASK)
#define DTW_1BIT                    0
#define DTW_4BIT                    1
#define DTW_8BIT                    2

/* System control */
#define USDHC_SYS_CTRL              0x2c
#define SYS_CTRL_INITA              BIT_32(27) /* 80 clocks, ignored while CIHB or CDIHB */
#define SYS_CTRL_RSTD               BIT_32(26)
#define SYS_CTRL_RSTC               BIT_32(25)
#define SYS_CTRL_RSTA               BIT_32(24)
#define SYS_CTRL_RSTT               BIT_32(28)
#define SYS_CTRL_DTOCV_SHIFT        16
#define SYS_CTRL_DTOCV_MASK         (0xfu << SYS_CTRL_DTOCV_SHIFT)
#define SYS_CTRL_DTOCV(x)           USDHC_FIELD(x, SYS_CTRL_DTOCV_SHIFT, SYS_CTRL_DTOCV_MASK)
#define DTOCV_MAX                   0xf
/* Prescaler: 0 is /1, 1 << n is /2^(n + 1); with MIX_CTRL.DDR_EN the card clock is halved */
#define SYS_CTRL_SDCLKFS_SHIFT      8
#define SYS_CTRL_SDCLKFS_MASK       (0xffu << SYS_CTRL_SDCLKFS_SHIFT)
#define SYS_CTRL_SDCLKFS(x)         USDHC_FIELD(x, SYS_CTRL_SDCLKFS_SHIFT, SYS_CTRL_SDCLKFS_MASK)
#define SDCLKFS_MAX_DIV             256
/* Divisor: /(n + 1) */
#define SYS_CTRL_DVS_SHIFT          4
#define SYS_CTRL_DVS_MASK           (0xfu << SYS_CTRL_DVS_SHIFT)
#define SYS_CTRL_DVS(x)             USDHC_FIELD(x, SYS_CTRL_DVS_SHIFT, SYS_CTRL_DVS_MASK)
#define DVS_MAX_DIV                 16

/* Interrupt status (write 1 to clear), status enable and signal enable share the layout. A
 * status bit is only set when its status enable bit is. */
#define USDHC_INT_STATUS            0x30
#define USDHC_INT_STATUS_EN         0x34
#define USDHC_INT_SIGNAL_EN         0x38
#define INT_DMAE                    BIT_32(28) /* ADMA error */
#define INT_AC12E                   BIT_32(24) /* Auto CMD12 error */
#define INT_DEBE                    BIT_32(22) /* Data end bit error */
#define INT_DCE                     BIT_32(21) /* Data CRC error */
#define INT_DTOE                    BIT_32(20) /* Data and busy timeout */
#define INT_CIE                     BIT_32(19) /* Command index error */
#define INT_CEBE                    BIT_32(18) /* Command end bit error */
#define INT_CCE                     BIT_32(17) /* Command CRC error */
#define INT_CTOE                    BIT_32(16) /* Command timeout */
#define INT_CINT                    BIT_32(8)
#define INT_BRR                     BIT_32(5)
#define INT_BWR                     BIT_32(4)
#define INT_DINT                    BIT_32(3)
#define INT_TC                      BIT_32(1) /* Transfer complete, or busy ended */
#define INT_CC                      BIT_32(0) /* Command complete */
#define INT_CMD_ERRORS              (INT_CIE | INT_CEBE | INT_CCE | INT_CTOE)
#define INT_DATA_ERRORS             (INT_DMAE | INT_AC12E | INT_DEBE | INT_DCE | INT_DTOE)
#define INT_ALL                     0xffffffffu

/* Watermark levels, in 32 bit words */
#define USDHC_WTMK_LVL              0x44
#define WTMK_LVL_WR_WML_SHIFT       16
#define WTMK_LVL_WR_WML_MASK        (0xffu << WTMK_LVL_WR_WML_SHIFT)
#define WTMK_LVL_WR_WML(x)          USDHC_FIELD(x, WTMK_LVL_WR_WML_SHIFT, WTMK_LVL_WR_WML_MASK)
#define WTMK_LVL_RD_WML_MASK        0xffu
#define WTMK_LVL_RD_WML(x)          USDHC_FIELD(x, 0, WTMK_LVL_RD_WML_MASK)
#define WTMK_LVL_DMA                16 /* For the internal DMA */

/* Mixer control: the transfer mode of the next command */
#define USDHC_MIX_CTRL              0x48
#define MIX_CTRL_RSVD_31            BIT_32(31) /* Always written as 1 */
#define MIX_CTRL_MSBSEL             BIT_32(5) /* Multiple blocks */
#define MIX_CTRL_DTDSEL             BIT_32(4) /* Card to host */
#define MIX_CTRL_DDR_EN             BIT_32(3)
#define MIX_CTRL_AC12EN             BIT_32(2)
#define MIX_CTRL_BCEN               BIT_32(1)
#define MIX_CTRL_DMAEN              BIT_32(0)
#define MIX_CTRL_XFER_MASK \
    (MIX_CTRL_MSBSEL | MIX_CTRL_DTDSEL | MIX_CTRL_AC12EN | MIX_CTRL_BCEN | MIX_CTRL_DMAEN)

/* ADMA system address: the descriptor list, 32 bits, word aligned */
#define USDHC_ADMA_SYS_ADDR             0x58

/* Delay line: SLV_OVERRIDE uses SLV_OVERRIDE_VAL taps, ENABLE is not needed with it */
#define USDHC_DLL_CTRL                  0x60
#define DLL_CTRL_SLV_OVERRIDE_VAL_SHIFT 9
#define DLL_CTRL_SLV_OVERRIDE_VAL_MASK  (0x7fu << DLL_CTRL_SLV_OVERRIDE_VAL_SHIFT)
#define DLL_CTRL_SLV_OVERRIDE_VAL(x) \
    USDHC_FIELD(x, DLL_CTRL_SLV_OVERRIDE_VAL_SHIFT, DLL_CTRL_SLV_OVERRIDE_VAL_MASK)
#define DLL_CTRL_SLV_OVERRIDE        BIT_32(8)

#define USDHC_CLK_TUNE_CTRL_STATUS   0x68

/* Vendor specific */
#define USDHC_VEND_SPEC              0xc0
#define VEND_SPEC_RSVD_29            BIT_32(29) /* Written as 1 */
/* i.MX6UL: card, ipg_perclk, hclk and ipg_clk soft enables. Reserved, 1 at reset on the i.MX8M */
#define VEND_SPEC_SOFT_EN_MASK       (0xfu << 11)
#define VEND_SPEC_FRC_SDCLK_ON       BIT_32(8) /* Cleared while the clock changes */
#define VEND_SPEC_AC12_WR_CHKBUSY_EN BIT_32(3)
#define VEND_SPEC_EXT_DMA_EN         BIT_32(0)

#define USDHC_MMC_BOOT               0xc4

/* ADMA2 descriptor */
struct usdhc_adma2_desc {
    uint16_t attr;
    uint16_t len; /* Bytes */
    uint32_t addr;
} __attribute__((packed));

#define ADMA2_ATTR_VALID     BIT(0)
#define ADMA2_ATTR_END       BIT(1)
#define ADMA2_ATTR_ACT_SHIFT 4
#define ADMA2_ATTR_ACT_TRAN  (2u << ADMA2_ATTR_ACT_SHIFT)
#define ADMA2_MAX_BYTES      0xfe00 /* 127 blocks, a multiple of 4 below 64 KiB */

#endif // DRIVERS_MMC_IMX_USDHC_PRIVATE_H
