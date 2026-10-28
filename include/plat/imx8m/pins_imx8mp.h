/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M Plus IOMUXC pads (RM 8.2.4). The pads reset to their GPIO function
 * (ALT5).
 */

#ifndef INCLUDE_PLAT_IMX8M_PINS_IMX8MP_H
#define INCLUDE_PLAT_IMX8M_PINS_IMX8MP_H

#define MUX_ALT(x)           (x & 0xf)

#define IOMUXC_MUX_UART2_RXD (0x30330228)
#define IOMUXC_MUX_UART2_TXD (0x3033022C)
#define IOMUXC_PAD_UART2_RXD (0x30330488)
#define IOMUXC_PAD_UART2_TXD (0x3033048C)

/* uSDHC3 (eMMC on the EVK) on the NAND pads, ALT2 */
#define IOMUXC_MUX_NAND_WE_B    (0x30330124) /* USDHC3_CLK */
#define IOMUXC_MUX_NAND_WP_B    (0x30330128) /* USDHC3_CMD */
#define IOMUXC_MUX_NAND_DATA04  (0x30330108) /* USDHC3_DATA0 */
#define IOMUXC_MUX_NAND_DATA05  (0x3033010C) /* USDHC3_DATA1 */
#define IOMUXC_MUX_NAND_DATA06  (0x30330110) /* USDHC3_DATA2 */
#define IOMUXC_MUX_NAND_DATA07  (0x30330114) /* USDHC3_DATA3 */
#define IOMUXC_MUX_NAND_RE_B    (0x3033011C) /* USDHC3_DATA4 */
#define IOMUXC_MUX_NAND_CE2_B   (0x303300EC) /* USDHC3_DATA5 */
#define IOMUXC_MUX_NAND_CE3_B   (0x303300F0) /* USDHC3_DATA6 */
#define IOMUXC_MUX_NAND_CLE     (0x303300F4) /* USDHC3_DATA7 */
#define IOMUXC_MUX_NAND_CE1_B   (0x303300E8) /* USDHC3_STROBE */
#define IOMUXC_MUX_NAND_READY_B (0x30330120) /* USDHC3_RESET_B */

#define IOMUXC_PAD_NAND_WE_B    (0x30330384)
#define IOMUXC_PAD_NAND_WP_B    (0x30330388)
#define IOMUXC_PAD_NAND_DATA04  (0x30330368)
#define IOMUXC_PAD_NAND_DATA05  (0x3033036C)
#define IOMUXC_PAD_NAND_DATA06  (0x30330370)
#define IOMUXC_PAD_NAND_DATA07  (0x30330374)
#define IOMUXC_PAD_NAND_RE_B    (0x3033037C)
#define IOMUXC_PAD_NAND_CE2_B   (0x3033034C)
#define IOMUXC_PAD_NAND_CE3_B   (0x30330350)
#define IOMUXC_PAD_NAND_CLE     (0x30330354)
#define IOMUXC_PAD_NAND_CE1_B   (0x30330348)
#define IOMUXC_PAD_NAND_READY_B (0x30330380)

/* uSDHC3 input daisy chains, 1 selects the NAND pad (0, the reset value, an ENET pad) */
#define IOMUXC_USDHC3_CARD_CLK_IN_SELECT_INPUT (0x30330604)
#define IOMUXC_USDHC3_CMD_IN_SELECT_INPUT      (0x3033060C)
#define IOMUXC_USDHC3_DAT_IN_SELECT_INPUT(n)   (0x30330610 + 4 * (n)) /* DAT0..DAT7 */
#define IOMUXC_USDHC3_STROBE_SELECT_INPUT      (0x30330630)

/* SW_PAD_CTL fields (RM 8.2.4) */
#define PAD_CTL_PE             BIT(8) /* Pull enable */
#define PAD_CTL_HYS            BIT(7) /* Schmitt input */
#define PAD_CTL_PUE            BIT(6) /* Pull up (0: down) */
#define PAD_CTL_ODE            BIT(5)
#define PAD_CTL_FSEL_FAST      BIT(4) /* Fast slew rate */
#define PAD_CTL_DSE_X1         (0 << 1)
#define PAD_CTL_DSE_X2         (2 << 1)
#define PAD_CTL_DSE_X4         (1 << 1)
#define PAD_CTL_DSE_X6         (3 << 1)

#endif
