#ifndef INCLUDE_PLAT_IMX6UL_CLOCK_H
#define INCLUDE_PLAT_IMX6UL_CLOCK_H

#include <pb/utils_def.h>

#define IMX6UL_CCM_CCSR                    (0x020C400C)
#define CCM_CCSR_STEP_SEL                  BIT(8)
#define CCM_CCSR_SECONDARY_CLK_SEL         BIT(3)
#define CCM_CCSR_PLL1_SW_CLK_SEL           BIT(2)
#define CCM_CCSR_PLL3_SW_CLK_SEL           BIT(0)

#define IMX6UL_CCM_CACRR                   (0x020C4010)
#define CCM_CACRR_AR_PODF_SHIFT            (0)
#define CCM_CACRR_AR_PODF_MASK             (0x7 << CCM_CACRR_AR_PODF_SHIFT)
#define CCM_CACRR_AR_PODF(x)               ((x << CCM_CACRR_AR_PODF_SHIFT) & CCM_CACRR_AR_PODF_MASK)

#define IMX6UL_CCM_CSCMR1                  (0x020C401C)
#define CCM_CSCMR1_ACLK_EIM_SLOW_SEL_SHIFT (29)
#define CCM_CSCMR1_ACLK_EIM_SLOW_SEL_MASK  (0x3 << CCM_CSCMR1_ACLK_EIM_SLOW_SEL_SHIFT)
#define CCM_CSCMR1_ACLK_EIM_SLOW_SEL(x) \
    ((x << CCM_CSCMR1_ACLK_EIM_SLOW_SEL_SHIFT) & CCM_CSCMR1_ACLK_EIM_SLOW_SEL_MASK)
#define CCM_CSCMR1_QSPI1_PODF_SHIFT         (26)
#define CCM_CSCMR1_QSPI1_PODF_MASK          (0x7 << CCM_CSCMR1_QSPI1_PODF_SHIFT)
#define CCM_CSCMR1_QSPI1_PODF(x)            ((x << CCM_CSCMR1_QSPI1_PODF_SHIFT) & CCM_CSCMR1_QSPI1_PODF_MASK)
#define CCM_CSCMR1_ACLK_EIM_SLOW_PODF_SHIFT (23)
#define CCM_CSCMR1_ACLK_EIM_SLOW_PODF_MASK  (0x7 << CCM_CSCMR1_ACLK_EIM_SLOW_PODF_SHIFT)
#define CCM_CSCMR1_ACLK_EIM_SLOW_PODF(x) \
    ((x << CCM_CSCMR1_ACLK_EIM_SLOW_PODF_SHIFT) & CCM_CSCMR1_ACLK_EIM_SLOW_PODF_MASK)
#define CCM_CSCMR1_GPMI_CLK_SEL       BIT(19)
#define CCM_CSCMR1_BCH_CLK_SEL        BIT(18)
#define CCM_CSCMR1_USDHC2_CLK_SEL     BIT(17)
#define CCM_CSCMR1_USDHC1_CLK_SEL     BIT(16)
#define CCM_CSCMR1_SAI3_CLK_SEL_SHIFT (14)
#define CCM_CSCMR1_SAI3_CLK_SEL_MASK  (0x3 << CCM_CSCMR1_SAI3_CLK_SEL_SHIFT)
#define CCM_CSCMR1_SAI3_CLK_SEL(x) \
    ((x << CCM_CSCMR1_SAI3_CLK_SEL_SHIFT) & CCM_CSCMR1_SAI3_CLK_SEL_MASK)
#define CCM_CSCMR1_SAI2_CLK_SEL_SHIFT (12)
#define CCM_CSCMR1_SAI2_CLK_SEL_MASK  (0x3 << CCM_CSCMR1_SAI2_CLK_SEL_SHIFT)
#define CCM_CSCMR1_SAI2_CLK_SEL(x) \
    ((x << CCM_CSCMR1_SAI2_CLK_SEL_SHIFT) & CCM_CSCMR1_SAI2_CLK_SEL_MASK)
#define CCM_CSCMR1_SAI1_CLK_SEL_SHIFT (10)
#define CCM_CSCMR1_SAI1_CLK_SEL_MASK  (0x3 << CCM_CSCMR1_SAI1_CLK_SEL_SHIFT)
#define CCM_CSCMR1_SAI1_CLK_SEL(x) \
    ((x << CCM_CSCMR1_SAI1_CLK_SEL_SHIFT) & CCM_CSCMR1_SAI1_CLK_SEL_MASK)
#define CCM_CSCMR1_QSPI1_CLK_SEL_SHIFT (7)
#define CCM_CSCMR1_QSPI1_CLK_SEL_MASK  (0x7 << CCM_CSCMR1_QSPI1_CLK_SEL_SHIFT)
#define CCM_CSCMR1_QSPI1_CLK_SEL(x) \
    ((x << CCM_CSCMR1_QSPI1_CLK_SEL_SHIFT) & CCM_CSCMR1_QSPI1_CLK_SEL_MASK)
#define CCM_CSCMR1_PERCLK_CLK_SEL    BIT(6)
#define CCM_CSCMR1_PERCLK_PODF_SHIFT (0)
#define CCM_CSCMR1_PERCLK_PODF_MASK  (0x3f << CCM_CSCMR1_PERCLK_PODF_SHIFT)
#define CCM_CSCMR1_PERCLK_PODF(x) \
    ((x << CCM_CSCMR1_PERCLK_PODF_SHIFT) & CCM_CSCMR1_PERCLK_PODF_MASK)

#define IMX6UL_CCM_CSCDR1            (0x020C4024)
#define CCM_CSCDR1_GPMI_PODF_SHIFT   (22)
#define CCM_CSCDR1_GPMI_PODF_MASK    (0x7 << CCM_CSCDR1_GPMI_PODF_SHIFT)
#define CCM_CSCDR1_GPMI_PODF(x)      ((x << CCM_CSCDR1_GPMI_PODF_SHIFT) & CCM_CSCDR1_GPMI_PODF_MASK)
#define CCM_CSCDR1_BCH_PODF_SHIFT    (19)
#define CCM_CSCDR1_BCH_PODF_MASK     (0x7 << CCM_CSCDR1_BCH_PODF_SHIFT)
#define CCM_CSCDR1_BCH_PODF(x)       ((x << CCM_CSCDR1_BCH_PODF_SHIFT) & CCM_CSCDR1_BCH_PODF_MASK)
#define CCM_CSCDR1_USDHC2_PODF_SHIFT (16)
#define CCM_CSCDR1_USDHC2_PODF_MASK  (0x7 << CCM_CSCDR1_USDHC2_PODF_SHIFT)
#define CCM_CSCDR1_USDHC2_PODF(x) \
    ((x << CCM_CSCDR1_USDHC2_PODF_SHIFT) & CCM_CSCDR1_USDHC2_PODF_MASK)
#define CCM_CSCDR1_USDHC1_PODF_SHIFT (11)
#define CCM_CSCDR1_USDHC1_PODF_MASK  (0x7 << CCM_CSCDR1_USDHC1_PODF_SHIFT)
#define CCM_CSCDR1_USDHC1_PODF(x) \
    ((x << CCM_CSCDR1_USDHC1_PODF_SHIFT) & CCM_CSCDR1_USDHC1_PODF_MASK)
#define CCM_CSCDR1_UART_CLK_SEL        BIT(6)
#define CCM_CSCDR1_UART_CLK_PODF_SHIFT (0)
#define CCM_CSCDR1_UART_CLK_PODF_MASK  (0x3f << CCM_CSCDR1_UART_CLK_PODF_SHIFT)
#define CCM_CSCDR1_UART_CLK_PODF(x) \
    ((x << CCM_CSCDR1_UART_CLK_PODF_SHIFT) & CCM_CSCDR1_UART_CLK_PODF_MASK)

#define IMX6UL_CCM_ANALOG_PLL_ARM               (0x020C8000)
#define CCM_ANALOG_PLL_ARM_LOCK                 BIT(31)
#define CCM_ANALOG_PLL_ARM_PLL_SEL              BIT(19)
#define CCM_ANALOG_PLL_ARM_BYPASS               BIT(16)
#define CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT (14)
#define CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC_MASK  (0x3 << CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT)
#define CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC(x) \
    ((x << CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT) & CCM_ANALOG_PLL_ARM_BYPASS_CLK_SRC_MASK)
#define CCM_ANALOG_PLL_ENABLE               BIT(13)
#define CCM_ANALOG_PLL_POWERDOWN            BIT(12)
#define CCM_ANALOG_PLL_ARM_DIV_SELECT_SHIFT (0)
#define CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK  (0x7f << CCM_ANALOG_PLL_ARM_DIV_SELECT_SHIFT)
#define CCM_ANALOG_PLL_ARM_DIV_SELECT(x) \
    ((x << CCM_ANALOG_PLL_ARM_DIV_SELECT_SHIFT) & CCM_ANALOG_PLL_ARM_DIV_SELECT_MASK)

#define IMX6UL_CCM_ANALOG_PLL_USB1      (0x020C8010)
#define CCM_ANALOG_PLL_USB1_LOCK        BIT(31)
#define CCM_ANALOG_PLL_USB1_BYPASS      BIT(16)
#define CCM_ANALOG_PLL_USB1_ENABLE      BIT(13)
#define CCM_ANALOG_PLL_USB1_POWER       BIT(12)
#define CCM_ANALOG_PLL_USB1_EN_USB_CLKS BIT(6)

/* Clock gating */
#define CG0                             (3 << 0)
#define CG1                             (3 << 2)
#define CG2                             (3 << 4)
#define CG3                             (3 << 6)
#define CG4                             (3 << 8)
#define CG5                             (3 << 10)
#define CG6                             (3 << 12)
#define CG7                             (3 << 14)
#define CG8                             (3 << 16)
#define CG9                             (3 << 18)
#define CG10                            (3 << 20)
#define CG11                            (3 << 22)
#define CG12                            (3 << 24)
#define CG13                            (3 << 26)
#define CG14                            (3 << 28)
#define CG15                            (3 << 30)

#define IMX6UL_CCM_CCGR0                (0x020C4068)
#define CCM_CCGR0_GPIO2                 CG15
#define CCM_CCGR0_UART2                 CG14
#define CCM_CCGR0_GPT2                  CG13
#define CCM_CCGR0_DCIC1                 CG12
#define CCM_CCGR0_CPU_DEBUG             CG11
#define CCM_CCGR0_CAN2_SERIAL           CG10
#define CCM_CCGR0_CAN2                  CG9
#define CCM_CCGR0_CAN1_SERIAL           CG8
#define CCM_CCGR0_CAN1                  CG7
#define CCM_CCGR0_CAAM_IPG              CG6
#define CCM_CCGR0_CAAM_ACLK             CG5
#define CCM_CCGR0_SMEM                  CG4
#define CCM_CCGR0_APBHDMA_HCLK          CG2
#define CCM_CCGR0_AIPS2_TZ2             CG1
#define CCM_CCGR0_AIPS_TZ1              CG0

#define IMX6UL_CCM_CCGR1                (0x020C406C)
#define IMX6UL_CCM_CCGR2                (0x020C4070)

#define IMX6UL_CCM_CCGR3                (0x020C4074)
#define CCM_CCGR3_AXI                   CG14
#define CCM_CCGR3_MMDC_CORE_IPG_CLK_P1  CG13
#define CCM_CCGR3_MMDC_CORE_IPG_CLK_P2  CG12
#define CCM_CCGR3_MMDC_CORE_ACLK        CG10
#define CCM_CCGR3_A7_DIVCLK_PATCH_CLK   CG9
#define CCM_CCGR3_WDOG1                 CG8
#define CCM_CCGR3_QSPI1                 CG7
#define CCM_CCGR3_GPIO4                 CG6
#define CCM_CCGR3_LCDIF_PIX_CLK         CG5
#define CCM_CCGR3_CA7_CCM_DAP           CG4
#define CCM_CCGR3_UART6                 CG3
#define CCM_CCGR3_ENET                  CG2
#define CCM_CCGR3_UART5                 CG1

#define IMX6UL_CCM_CCGR4                (0x020C4078)
#define CCM_CCGR4_NAND_1                CG15
#define CCM_CCGR4_NAND_2                CG14
#define CCM_CCGR4_NAND_3                CG13
#define CCM_CCGR4_NAND_4                CG12
#define CCM_CCGR4_PWM4                  CG11
#define CCM_CCGR4_PWM3                  CG10
#define CCM_CCGR4_PWM2                  CG9
#define CCM_CCGR4_PWM1                  CG8

#define IMX6UL_CCM_CCGR5                (0x020C407C)

#define IMX6UL_CCM_CCGR6                (0x020C4080)
#define CCM_CCGR6_PWM7                  CG15
#define CCM_CCGR6_PWM6                  CG14
#define CCM_CCGR6_PWM5                  CG13
#define CCM_CCGR6_I2C4                  CG12
#define CCM_CCGR6_ANADIG                CG11
#define CCM_CCGR6_WDOG3                 CG10
#define CCM_CCGR6_PWM8                  CG8
#define CCM_CCGR6_UART8                 CG7
#define CCM_CCGR6_EIM_SLOW              CG5
#define CCM_CCGR6_SIM2                  CG4
#define CCM_CCGR6_SIM1                  CG3
#define CCM_CCGR6_USDHC2                CG2
#define CCM_CCGR6_USDHC1                CG1
#define CCM_CCGR6_USBOH3                CG0

#endif
