#ifndef INCLUDE_PLAT_IMX8X_MM_H
#define INCLUDE_PLAT_IMX8X_MM_H

#define IMX_REG_BASE               0x50000000
#define IMX_REG_SIZE               0x10000000
#define IMX_GPIO_BASE              0x5D080000
#define IMX_CAAM_JR2_BASE          0x31430000
#define IMX_CI_UDC_BASE            0x5b0d0000
#define IMX_USBDCD_BASE            0x5b100800
#define IMX_GPT_BASE               0x5D140000
#define SC_IPC_BASE                0x5d1b0000

/* Both SoCs (QXP and DXL RM, chapter 2 memory maps) */
#define IMX8X_LPUART0_BASE         0x5A060000
#define IMX8X_USDHC1_BASE          0x5B010000 /* The RM's USDHC1, the SCFW's SC_R_SDHC_0 */

/* i.MX8QXP */
#define IMX8QXP_USB3_NON_CORE_BASE 0x5B110000
#define IMX8QXP_USB3_CORE_BASE     0x5B120000
#define IMX8QXP_USB3_PHY_BASE      0x5B160000

/* i.MX8DXL */
#define IMX8DXL_USB_OTG2_BASE      0x5B0E0000
#define IMX8DXL_USB2_PHY2_BASE     0x5B110000

#endif
