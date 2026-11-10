#ifndef INCLUDE_PLAT_IMX8M_MM_H
#define INCLUDE_PLAT_IMX8M_MM_H

#define IMX8M_ROM_BASE             (0x00000000) /* IMX8M_ROM_SIZE: makefile.mk */
#define IMX8M_OCRAM_BASE           (0x00900000) /* IMX8M_OCRAM_SIZE: makefile.mk */
#define IMX8M_AIPS_BASE            (0x30000000) /* The peripherals, AIPS1..4 */
#define IMX8M_AIPS_SIZE            (0x01000000)
#define IMX8M_GPV_BASE             (0x32000000) /* The NoC's global programmer's view */
#define IMX8M_GPV_SIZE             (0x00020000)
#define IMX8MP_HSIO_BLK_CTRL_BASE  (0x32F10000)
#define IMX8MP_HSIO_BLK_CTRL_SIZE  (0x00010000)
#define IMX8M_USB_SIZE             (0x00200000) /* USB1 and USB2 from IMX8M_USB1_BASE */
/* The pointer to the ROM's event log (NXP AN12853 Table 11) */
#define IMX8M_ROM_LOG_POINTER      (0x000009E0)
/* The HAB API's RVT: U-Boot's addresses, the RMs don't have them */
#define IMX8MQ_HAB_RVT_BASE        (0x00000880)
#define IMX8MP_HAB_RVT_BASE        (0x00000900)

#define IMX8M_GPT1_BASE            (0x302D0000)
#define IMX8M_SCTR_BASE            (0x306C0000)
#define IMX8M_WDOG1_BASE           (0x30280000)
#define IMX8M_WDOG2_BASE           (0x30290000)
#define IMX8M_WDOG3_BASE           (0x302A0000)
#define IMX8M_UART1_BASE           (0x30860000)
#define IMX8M_UART2_BASE           (0x30890000)
#define IMX8M_UART3_BASE           (0x30880000)
#define IMX8M_USB1_BASE            (0x38100000)
#define IMX8M_USB1_PHY_BASE        (0x381F0040)
#define IMX8M_CAAM_BASE            (0x30900000)
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
#define GPC_PGC_CPU_0_1_MAPPING     0x303A00EC
#define GPC_PGC_CPU_0_1_MAPPING_ALL 0x0000FFFF /* Every domain to the A53s, as NXP's code */
#define GPC_PU_PGC_SW_PUP_REQ       0x303A00F8
#define GPC_PU_PGC_SW_PUP_REQ_DDR1  BIT_32(5)

#define DDRC_DDR_SS_GPR0            0x3d000000
#define DDRC_DDR_SS_GPR0_LPDDR4     0x01 /* DDR subsystem in LPDDR4 mode */
#define DDR_CSD1_BASE_ADDR          0x40000000

/* The DDR window from DDR_CSD1_BASE_ADDR, where the TZASC's addresses start
 * (8MP RM 4.9.2.1). The RMs' memory maps: 8MQ 0x4000_0000..0x1_3FFF_FFFF,
 * 8MP 0x4000_0000..0x2_3FFF_FFFF */
#define IMX8MQ_DDR_WINDOW_SIZE      (UINT64_C(4) << 30)
#define IMX8MP_DDR_WINDOW_SIZE      (UINT64_C(8) << 30)

/* The TZASC's enable and its ID swap bypass, and their locks (8MP RM 8.2.3.11) */
#define IMX8M_IOMUXC_GPR10          0x30340028
#define GPR10_TZASC_EN              BIT_32(0)
#define GPR10_TZASC_ID_SWAP_BYPASS  BIT_32(1)
#define GPR10_LOCK_TZASC_EN         BIT_32(16)
#define GPR10_LOCK_TZASC_ID_SWAP    BIT_32(17)
#define IMX8M_TZASC_BASE            0x32F80000

#define CCM_ANALOG_DIGPROG          0x3036006C

#define IMX_CSU_BASE                (0x303e0000)

#endif
