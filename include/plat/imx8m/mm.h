#ifndef INCLUDE_PLAT_IMX8M_MM_H
#define INCLUDE_PLAT_IMX8M_MM_H

#define IMX8M_GPT1_BASE            (0x302D0000)
#define IMX8M_WDOG1_BASE           (0x30280000)
#define IMX8M_UART1_BASE           (0x30860000)
#define IMX8M_UART2_BASE           (0x30890000)
#define IMX8M_UART3_BASE           (0x30880000)
#define IMX8M_USB1_BASE            (0x38100000)
#define IMX8M_USB1_PHY_BASE        (0x381F0040)
#define IMX8M_CAAM_JR0_BASE        (0x30901000)
#define IMX8M_USDHC1_BASE          (0x30B40000)
#define IMX8M_USDHC2_BASE          (0x30B50000)
#define IMX8M_USDHC3_BASE          (0x30B60000)
#define IMX8M_OCOTP_BASE           (0x30350000)

#define IMX8M_SRC_BASE             (0x30390000)
#define IMX8M_SRC_SRSR             (0x3039005c)
#define SRC_SRSR_TEMP_RST_B        BIT(9)
#define SRC_SRSR_WDOG4_RST_B       BIT(8)
#define SRC_SRSR_WDOG3_RST_B       BIT(7)
#define SRC_SRSR_JTAG_SW_RST       BIT(6)
#define SRC_SRSR_JTAG_RST_B        BIT(5)
#define SRC_SRSR_WDOG1_RST_B       BIT(4)
#define SRC_SRSR_IPP_USER_RST_B    BIT(3)
#define SRC_SRSR_CSU_RST_B         BIT(2)

/* DDR controller and PHY resets, a set bit holds the reset */
#define SRC_DDRC_RCR_ADDR          0x30391000
#define SRC_DDRC2_RCR_ADDR         0x30391004
#define SRC_DDRC_RCR_DOM_EN        BIT_32(31)
#define SRC_DDRC_RCR_DOMAIN_ALL    GENMASK_32(27, 24)
#define SRC_DDRC_RCR_SYSTEM_RST    BIT_32(4) /* src_system_rst_b, i.MX8M Plus */
#define SRC_DDRC_RCR_PHY_PWROKIN_N BIT_32(3)
#define SRC_DDRC_RCR_PHY_RESET     BIT_32(2)
#define SRC_DDRC_RCR_CORE_RESET_N  BIT_32(1)
#define SRC_DDRC_RCR_PRESET_N      BIT_32(0)
#define SRC_DDRC_RCR_ALL_RESETS                                                        \
    (SRC_DDRC_RCR_PHY_PWROKIN_N | SRC_DDRC_RCR_PHY_RESET | SRC_DDRC_RCR_CORE_RESET_N | \
     SRC_DDRC_RCR_PRESET_N)

/* Power gating controller: power up the DDR1 domain */
#define GPC_PGC_CPU_0_1_MAPPING    0x303A00EC
#define GPC_PU_PGC_SW_PUP_REQ      0x303A00F8
#define GPC_PU_PGC_SW_PUP_REQ_DDR1 BIT_32(5)

#define DDRC_DDR_SS_GPR0           0x3d000000
#define DDRC_DDR_SS_GPR0_LPDDR4    0x01 /* DDR subsystem in LPDDR4 mode */
#define DDR_CSD1_BASE_ADDR         0x40000000

#define CCM_ANALOG_DIGPROG         0x3036006C

#define IMX_CSU_BASE               (0x303e0000)

#endif
