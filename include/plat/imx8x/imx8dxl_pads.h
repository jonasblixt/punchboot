/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017-2022 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * Header file used to configure SoC pad list.
 */

#ifndef SC_PADS_H
#define SC_PADS_H

/* Includes */

/* Defines */

/*!
 * @name Pad Definitions
 */
/** @{ */
#define SC_P_PCIE_CTRL0_PERST_B            0U /* HSIO.PCIE0.PERST_B, LSIO.GPIO4.IO00, LSIO.GPIO7.IO00 */
#define SC_P_PCIE_CTRL0_CLKREQ_B           1U /* HSIO.PCIE0.CLKREQ_B, LSIO.GPIO4.IO01, LSIO.GPIO7.IO01 */
#define SC_P_PCIE_CTRL0_WAKE_B             2U /* HSIO.PCIE0.WAKE_B, LSIO.GPIO4.IO02, LSIO.GPIO7.IO02 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_PCIESEP 3U /*  */
#define SC_P_USB_SS3_TC0 \
    4U /* ADMA.I2C1.SCL, CONN.USB_OTG1.PWR, CONN.USB_OTG2.PWR, LSIO.GPIO4.IO03, LSIO.GPIO7.IO03 */
#define SC_P_USB_SS3_TC1 5U /* ADMA.I2C1.SCL, CONN.USB_OTG2.PWR, LSIO.GPIO4.IO04, LSIO.GPIO7.IO04 \
                             */
#define SC_P_USB_SS3_TC2 \
    6U /* ADMA.I2C1.SDA, CONN.USB_OTG1.OC, CONN.USB_OTG2.OC, LSIO.GPIO4.IO05, LSIO.GPIO7.IO05 */
#define SC_P_USB_SS3_TC3                   7U /* ADMA.I2C1.SDA, CONN.USB_OTG2.OC, LSIO.GPIO4.IO06, LSIO.GPIO7.IO06 \
                                               */
#define SC_P_COMP_CTL_GPIO_3V3_USB3IO      8U /*  */
#define SC_P_EMMC0_CLK                     9U /* CONN.EMMC0.CLK, CONN.NAND.READY_B, LSIO.GPIO4.IO07 */
#define SC_P_EMMC0_CMD                     10U /* CONN.EMMC0.CMD, CONN.NAND.DQS, LSIO.GPIO4.IO08 */
#define SC_P_EMMC0_DATA0                   11U /* CONN.EMMC0.DATA0, CONN.NAND.DATA00, LSIO.GPIO4.IO09 */
#define SC_P_EMMC0_DATA1                   12U /* CONN.EMMC0.DATA1, CONN.NAND.DATA01, LSIO.GPIO4.IO10 */
#define SC_P_EMMC0_DATA2                   13U /* CONN.EMMC0.DATA2, CONN.NAND.DATA02, LSIO.GPIO4.IO11 */
#define SC_P_EMMC0_DATA3                   14U /* CONN.EMMC0.DATA3, CONN.NAND.DATA03, LSIO.GPIO4.IO12 */
#define SC_P_EMMC0_DATA4                   15U /* CONN.EMMC0.DATA4, CONN.NAND.DATA04, LSIO.GPIO4.IO13 */
#define SC_P_EMMC0_DATA5                   16U /* CONN.EMMC0.DATA5, CONN.NAND.DATA05, LSIO.GPIO4.IO14 */
#define SC_P_EMMC0_DATA6                   17U /* CONN.EMMC0.DATA6, CONN.NAND.DATA06, LSIO.GPIO4.IO15 */
#define SC_P_EMMC0_DATA7                   18U /* CONN.EMMC0.DATA7, CONN.NAND.DATA07, LSIO.GPIO4.IO16 */
#define SC_P_EMMC0_STROBE                  19U /* CONN.EMMC0.STROBE, CONN.NAND.CLE, LSIO.GPIO4.IO17 */
#define SC_P_EMMC0_RESET_B                 20U /* CONN.EMMC0.RESET_B, CONN.NAND.WP_B, LSIO.GPIO4.IO18 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_SD1FIX0 21U /*  */
#define SC_P_USDHC1_RESET_B                                                                     \
    22U /* CONN.USDHC1.RESET_B, CONN.NAND.RE_N, ADMA.SPI2.SCK, CONN.NAND.WE_B, LSIO.GPIO4.IO19, \
           LSIO.GPIO7.IO08 */
#define SC_P_USDHC1_VSELECT                                                                     \
    23U /* CONN.USDHC1.VSELECT, CONN.NAND.RE_P, ADMA.SPI2.SDO, CONN.NAND.RE_B, LSIO.GPIO4.IO20, \
           LSIO.GPIO7.IO09 */
#define SC_P_CTL_NAND_RE_P_N 24U /*  */
#define SC_P_USDHC1_WP                                                                     \
    25U /* CONN.USDHC1.WP, CONN.NAND.DQS_N, ADMA.SPI2.SDI, CONN.NAND.ALE, LSIO.GPIO4.IO21, \
           LSIO.GPIO7.IO10 */
#define SC_P_USDHC1_CD_B                                                                     \
    26U /* CONN.USDHC1.CD_B, CONN.NAND.DQS_P, ADMA.SPI2.CS0, CONN.NAND.DQS, LSIO.GPIO4.IO22, \
           LSIO.GPIO7.IO11 */
#define SC_P_CTL_NAND_DQS_P_N              27U /*  */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_VSELSEP 28U /*  */
#define SC_P_ENET0_RGMII_TXC                                                                     \
    29U /* CONN.ENET0.RGMII_TXC, CONN.ENET0.RCLK50M_OUT, CONN.ENET0.RCLK50M_IN, CONN.NAND.CE1_B, \
           LSIO.GPIO4.IO29, CONN.USDHC2.CLK */
#define SC_P_ENET0_RGMII_TX_CTL \
    30U /* CONN.ENET0.RGMII_TX_CTL, CONN.USDHC1.RESET_B, LSIO.GPIO4.IO30, CONN.USDHC2.CMD */
#define SC_P_ENET0_RGMII_TXD0 \
    31U /* CONN.ENET0.RGMII_TXD0, CONN.USDHC1.VSELECT, LSIO.GPIO4.IO31, CONN.USDHC2.DATA0 */
#define SC_P_ENET0_RGMII_TXD1 \
    32U /* CONN.ENET0.RGMII_TXD1, CONN.USDHC1.WP, LSIO.GPIO5.IO00, CONN.USDHC2.DATA1 */
#define SC_P_ENET0_RGMII_TXD2                                                         \
    33U /* CONN.ENET0.RGMII_TXD2, CONN.NAND.CE0_B, CONN.USDHC1.CD_B, LSIO.GPIO5.IO01, \
           CONN.USDHC2.DATA2 */
#define SC_P_ENET0_RGMII_TXD3 \
    34U /* CONN.ENET0.RGMII_TXD3, CONN.NAND.RE_B, LSIO.GPIO5.IO02, CONN.USDHC2.DATA3 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_ENET_ENETB0 35U /*  */
#define SC_P_ENET0_RGMII_RXC \
    36U /* CONN.ENET0.RGMII_RXC, CONN.NAND.WE_B, CONN.USDHC1.CLK, LSIO.GPIO5.IO03 */
#define SC_P_ENET0_RGMII_RX_CTL 37U /* CONN.ENET0.RGMII_RX_CTL, CONN.USDHC1.CMD, LSIO.GPIO5.IO04 \
                                     */
#define SC_P_ENET0_RGMII_RXD0   38U /* CONN.ENET0.RGMII_RXD0, CONN.USDHC1.DATA0, LSIO.GPIO5.IO05 */
#define SC_P_ENET0_RGMII_RXD1   39U /* CONN.ENET0.RGMII_RXD1, CONN.USDHC1.DATA1, LSIO.GPIO5.IO06 */
#define SC_P_ENET0_RGMII_RXD2 \
    40U /* CONN.ENET0.RGMII_RXD2, CONN.ENET0.RMII_RX_ER, CONN.USDHC1.DATA2, LSIO.GPIO5.IO07 */
#define SC_P_ENET0_RGMII_RXD3 \
    41U /* CONN.ENET0.RGMII_RXD3, CONN.NAND.ALE, CONN.USDHC1.DATA3, LSIO.GPIO5.IO08 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_ENET_ENETB1 42U /*  */
#define SC_P_ENET0_REFCLK_125M_25M                                                          \
    43U /* CONN.ENET0.REFCLK_125M_25M, CONN.ENET0.PPS, CONN.EQOS.PPS_IN, CONN.EQOS.PPS_OUT, \
           LSIO.GPIO5.IO09 */
#define SC_P_ENET0_MDIO \
    44U /* CONN.ENET0.MDIO, ADMA.I2C3.SDA, CONN.EQOS.MDIO, LSIO.GPIO5.IO10, LSIO.GPIO7.IO16 */
#define SC_P_ENET0_MDC \
    45U /* CONN.ENET0.MDC, ADMA.I2C3.SCL, CONN.EQOS.MDC, LSIO.GPIO5.IO11, LSIO.GPIO7.IO17 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOCT 46U /*  */
#define SC_P_ENET1_RGMII_TXC                                                            \
    47U /* LSIO.GPIO0.IO00, CONN.EQOS.RCLK50M_OUT, ADMA.LCDIF.D00, CONN.EQOS.RGMII_TXC, \
           CONN.EQOS.RCLK50M_IN */
#define SC_P_ENET1_RGMII_TXD2   48U /* , ADMA.LCDIF.D01, CONN.EQOS.RGMII_TXD2, LSIO.GPIO0.IO01 */
#define SC_P_ENET1_RGMII_TX_CTL 49U /* , ADMA.LCDIF.D02, CONN.EQOS.RGMII_TX_CTL, LSIO.GPIO0.IO02 \
                                     */
#define SC_P_ENET1_RGMII_TXD3   50U /* , ADMA.LCDIF.D03, CONN.EQOS.RGMII_TXD3, LSIO.GPIO0.IO03 */
#define SC_P_ENET1_RGMII_RXC    51U /* , ADMA.LCDIF.D04, CONN.EQOS.RGMII_RXC, LSIO.GPIO0.IO04 */
#define SC_P_ENET1_RGMII_RXD3   52U /* , ADMA.LCDIF.D05, CONN.EQOS.RGMII_RXD3, LSIO.GPIO0.IO05 */
#define SC_P_ENET1_RGMII_RXD2 \
    53U /* , ADMA.LCDIF.D06, CONN.EQOS.RGMII_RXD2, LSIO.GPIO0.IO06, LSIO.GPIO6.IO00 */
#define SC_P_ENET1_RGMII_RXD1 \
    54U /* , ADMA.LCDIF.D07, CONN.EQOS.RGMII_RXD1, LSIO.GPIO0.IO07, LSIO.GPIO6.IO01 */
#define SC_P_ENET1_RGMII_TXD0 \
    55U /* , ADMA.LCDIF.D08, CONN.EQOS.RGMII_TXD0, LSIO.GPIO0.IO08, LSIO.GPIO6.IO02 */
#define SC_P_ENET1_RGMII_TXD1 \
    56U /* , ADMA.LCDIF.D09, CONN.EQOS.RGMII_TXD1, LSIO.GPIO0.IO09, LSIO.GPIO6.IO03 */
#define SC_P_ENET1_RGMII_RXD0                                                                 \
    57U /* ADMA.SPDIF0.RX, ADMA.MQS.R, ADMA.LCDIF.D10, CONN.EQOS.RGMII_RXD0, LSIO.GPIO0.IO10, \
           LSIO.GPIO6.IO04 */
#define SC_P_ENET1_RGMII_RX_CTL                                                                 \
    58U /* ADMA.SPDIF0.TX, ADMA.MQS.L, ADMA.LCDIF.D11, CONN.EQOS.RGMII_RX_CTL, LSIO.GPIO0.IO11, \
           LSIO.GPIO6.IO05 */
#define SC_P_ENET1_REFCLK_125M_25M                                                          \
    59U /* ADMA.SPDIF0.EXT_CLK, ADMA.LCDIF.D12, CONN.EQOS.REFCLK_125M_25M, LSIO.GPIO0.IO12, \
           LSIO.GPIO6.IO06 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHB 60U /*  */
#define SC_P_SPI3_SCK                      61U /* ADMA.SPI3.SCK, ADMA.LCDIF.D13, LSIO.GPIO0.IO13, ADMA.LCDIF.D00 */
#define SC_P_SPI3_SDO                      62U /* ADMA.SPI3.SDO, ADMA.LCDIF.D14, LSIO.GPIO0.IO14, ADMA.LCDIF.D01 */
#define SC_P_SPI3_SDI                      63U /* ADMA.SPI3.SDI, ADMA.LCDIF.D15, LSIO.GPIO0.IO15, ADMA.LCDIF.D02 */
#define SC_P_SPI3_CS0 \
    64U /* ADMA.SPI3.CS0, ADMA.ACM.MCLK_OUT1, ADMA.LCDIF.HSYNC, LSIO.GPIO0.IO16, ADMA.LCDIF.CS */
#define SC_P_SPI3_CS1                                                                     \
    65U /* ADMA.SPI3.CS1, ADMA.I2C3.SCL, ADMA.LCDIF.RESET, ADMA.SPI2.CS0, ADMA.LCDIF.D16, \
           ADMA.LCDIF.RD_E */
#define SC_P_MCLK_IN1                                                                      \
    66U /* ADMA.ACM.MCLK_IN1, ADMA.I2C3.SDA, ADMA.LCDIF.EN, ADMA.SPI2.SCK, ADMA.LCDIF.D17, \
           ADMA.LCDIF.D03 */
#define SC_P_MCLK_IN0 \
    67U /* ADMA.ACM.MCLK_IN0, ADMA.LCDIF.VSYNC, ADMA.SPI2.SDI, LSIO.GPIO0.IO19, ADMA.LCDIF.RS */
#define SC_P_MCLK_OUT0                                                                           \
    68U /* ADMA.ACM.MCLK_OUT0, ADMA.LCDIF.CLK, ADMA.SPI2.SDO, LSIO.GPIO0.IO20, ADMA.LCDIF.WR_RWN \
         */
#define SC_P_UART1_TX \
    69U /* ADMA.UART1.TX, LSIO.PWM0.OUT, LSIO.GPT0.CAPTURE, LSIO.GPIO0.IO21, ADMA.LCDIF.D04 */
#define SC_P_UART1_RX                                                                       \
    70U /* ADMA.UART1.RX, LSIO.PWM1.OUT, LSIO.GPT0.COMPARE, LSIO.GPT1.CLK, LSIO.GPIO0.IO22, \
           ADMA.LCDIF.D05 */
#define SC_P_UART1_RTS_B                                                                      \
    71U /* ADMA.UART1.RTS_B, LSIO.PWM2.OUT, ADMA.LCDIF.D16, LSIO.GPT1.CAPTURE, LSIO.GPT0.CLK, \
           ADMA.LCDIF.D06 */
#define SC_P_UART1_CTS_B                                                                        \
    72U /* ADMA.UART1.CTS_B, LSIO.PWM3.OUT, ADMA.LCDIF.D17, LSIO.GPT1.COMPARE, LSIO.GPIO0.IO24, \
           ADMA.LCDIF.D07 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHK 73U /*  */
#define SC_P_SPI0_SCK                                                                   \
    74U /* ADMA.SPI0.SCK, ADMA.SAI0.TXC, M40.I2C0.SCL, M40.GPIO0.IO00, LSIO.GPIO1.IO04, \
           ADMA.LCDIF.D08 */
#define SC_P_SPI0_SDI                                                                   \
    75U /* ADMA.SPI0.SDI, ADMA.SAI0.TXD, M40.TPM0.CH0, M40.GPIO0.IO02, LSIO.GPIO1.IO05, \
           ADMA.LCDIF.D09 */
#define SC_P_SPI0_SDO                                                                    \
    76U /* ADMA.SPI0.SDO, ADMA.SAI0.TXFS, M40.I2C0.SDA, M40.GPIO0.IO01, LSIO.GPIO1.IO06, \
           ADMA.LCDIF.D10 */
#define SC_P_SPI0_CS1                                                                       \
    77U /* ADMA.SPI0.CS1, ADMA.SAI0.RXC, ADMA.SAI1.TXD, ADMA.LCD_PWM0.OUT, LSIO.GPIO1.IO07, \
           ADMA.LCDIF.D11 */
#define SC_P_SPI0_CS0                                                                   \
    78U /* ADMA.SPI0.CS0, ADMA.SAI0.RXD, M40.TPM0.CH1, M40.GPIO0.IO03, LSIO.GPIO1.IO08, \
           ADMA.LCDIF.D12 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHT 79U /*  */
#define SC_P_ADC_IN1                                                                   \
    80U /* ADMA.ADC.IN1, M40.I2C0.SDA, M40.GPIO0.IO01, ADMA.I2C0.SDA, LSIO.GPIO1.IO09, \
           ADMA.LCDIF.D13 */
#define SC_P_ADC_IN0                                                                   \
    81U /* ADMA.ADC.IN0, M40.I2C0.SCL, M40.GPIO0.IO00, ADMA.I2C0.SCL, LSIO.GPIO1.IO10, \
           ADMA.LCDIF.D14 */
#define SC_P_ADC_IN3                                                                        \
    82U /* ADMA.ADC.IN3, M40.UART0.TX, M40.GPIO0.IO03, ADMA.ACM.MCLK_OUT0, LSIO.GPIO1.IO11, \
           ADMA.LCDIF.D15 */
#define SC_P_ADC_IN2                                                                       \
    83U /* ADMA.ADC.IN2, M40.UART0.RX, M40.GPIO0.IO02, ADMA.ACM.MCLK_IN0, LSIO.GPIO1.IO12, \
           ADMA.LCDIF.D16 */
#define SC_P_ADC_IN5                                                                        \
    84U /* ADMA.ADC.IN5, M40.TPM0.CH1, M40.GPIO0.IO05, ADMA.LCDIF.LCDBUSY, LSIO.GPIO1.IO13, \
           ADMA.LCDIF.D17 */
#define SC_P_ADC_IN4 \
    85U /* ADMA.ADC.IN4, M40.TPM0.CH0, M40.GPIO0.IO04, ADMA.LCDIF.LCDRESET, LSIO.GPIO1.IO14 */
#define SC_P_FLEXCAN0_RX                                                                      \
    86U /* ADMA.FLEXCAN0.RX, ADMA.SAI2.RXC, ADMA.UART0.RTS_B, ADMA.SAI1.TXC, LSIO.GPIO1.IO15, \
           LSIO.GPIO6.IO08 */
#define SC_P_FLEXCAN0_TX                                                                       \
    87U /* ADMA.FLEXCAN0.TX, ADMA.SAI2.RXD, ADMA.UART0.CTS_B, ADMA.SAI1.TXFS, LSIO.GPIO1.IO16, \
           LSIO.GPIO6.IO09 */
#define SC_P_FLEXCAN1_RX                                                                   \
    88U /* ADMA.FLEXCAN1.RX, ADMA.SAI2.RXFS, ADMA.FTM.CH2, ADMA.SAI1.TXD, LSIO.GPIO1.IO17, \
           LSIO.GPIO6.IO10 */
#define SC_P_FLEXCAN1_TX                                                                       \
    89U /* ADMA.FLEXCAN1.TX, ADMA.SAI3.RXC, ADMA.DMA0.REQ_IN0, ADMA.SAI1.RXD, LSIO.GPIO1.IO18, \
           LSIO.GPIO6.IO11 */
#define SC_P_FLEXCAN2_RX                                                                    \
    90U /* ADMA.FLEXCAN2.RX, ADMA.SAI3.RXD, ADMA.UART3.RX, ADMA.SAI1.RXFS, LSIO.GPIO1.IO19, \
           LSIO.GPIO6.IO12 */
#define SC_P_FLEXCAN2_TX                                                                    \
    91U /* ADMA.FLEXCAN2.TX, ADMA.SAI3.RXFS, ADMA.UART3.TX, ADMA.SAI1.RXC, LSIO.GPIO1.IO20, \
           LSIO.GPIO6.IO13 */
#define SC_P_UART0_RX                                                                  \
    92U /* ADMA.UART0.RX, ADMA.MQS.R, ADMA.FLEXCAN0.RX, SCU.UART0.RX, LSIO.GPIO1.IO21, \
           LSIO.GPIO6.IO14 */
#define SC_P_UART0_TX                                                                  \
    93U /* ADMA.UART0.TX, ADMA.MQS.L, ADMA.FLEXCAN0.TX, SCU.UART0.TX, LSIO.GPIO1.IO22, \
           LSIO.GPIO6.IO15 */
#define SC_P_UART2_TX \
    94U /* ADMA.UART2.TX, ADMA.FTM.CH1, ADMA.FLEXCAN1.TX, LSIO.GPIO1.IO23, LSIO.GPIO6.IO16 */
#define SC_P_UART2_RX \
    95U /* ADMA.UART2.RX, ADMA.FTM.CH0, ADMA.FLEXCAN1.RX, LSIO.GPIO1.IO24, LSIO.GPIO6.IO17 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIOLH 96U /*  */
#define SC_P_JTAG_TRST_B                  97U /* SCU.JTAG.TRST_B, SCU.WDOG0.WDOG_OUT */
#define SC_P_PMIC_I2C_SCL                 98U /* SCU.PMIC_I2C.SCL, SCU.GPIO0.IOXX_PMIC_A35_ON, LSIO.GPIO2.IO01 */
#define SC_P_PMIC_I2C_SDA                 99U /* SCU.PMIC_I2C.SDA, SCU.GPIO0.IOXX_PMIC_GPU_ON, LSIO.GPIO2.IO02 */
#define SC_P_PMIC_INT_B                   100U /* SCU.DSC.PMIC_INT_B */
#define SC_P_SCU_GPIO0_00 \
    101U /* SCU.GPIO0.IO00, SCU.UART0.RX, M40.UART0.RX, ADMA.UART3.RX, LSIO.GPIO2.IO03 */
#define SC_P_SCU_GPIO0_01 \
    102U /* SCU.GPIO0.IO01, SCU.UART0.TX, M40.UART0.TX, ADMA.UART3.TX, SCU.WDOG0.WDOG_OUT */
#define SC_P_SCU_PMIC_STANDBY              103U /* SCU.DSC.PMIC_STANDBY */
#define SC_P_SCU_BOOT_MODE1                104U /* SCU.DSC.BOOT_MODE1 */
#define SC_P_SCU_BOOT_MODE0                105U /* SCU.DSC.BOOT_MODE0 */
#define SC_P_SCU_BOOT_MODE2                106U /* SCU.DSC.BOOT_MODE2, SCU.DSC.RTC_CLOCK_OUTPUT_32K */
#define SC_P_SNVS_TAMPER_OUT1              107U /* , LSIO.GPIO2.IO05_IN, LSIO.GPIO6.IO19_IN */
#define SC_P_SNVS_TAMPER_OUT2              108U /* , LSIO.GPIO2.IO06_IN, LSIO.GPIO6.IO20_IN */
#define SC_P_SNVS_TAMPER_OUT3              109U /* , ADMA.SAI2.RXC, LSIO.GPIO2.IO07_IN, LSIO.GPIO6.IO21_IN */
#define SC_P_SNVS_TAMPER_OUT4              110U /* , ADMA.SAI2.RXD, LSIO.GPIO2.IO08_IN, LSIO.GPIO6.IO22_IN */
#define SC_P_SNVS_TAMPER_IN0               111U /* , ADMA.SAI2.RXFS, LSIO.GPIO2.IO09_IN, LSIO.GPIO6.IO23_IN */
#define SC_P_SNVS_TAMPER_IN1               112U /* , ADMA.SAI3.RXC, LSIO.GPIO2.IO10_IN, LSIO.GPIO6.IO24_IN */
#define SC_P_SNVS_TAMPER_IN2               113U /* , ADMA.SAI3.RXD, LSIO.GPIO2.IO11_IN, LSIO.GPIO6.IO25_IN */
#define SC_P_SNVS_TAMPER_IN3               114U /* , ADMA.SAI3.RXFS, LSIO.GPIO2.IO12_IN, LSIO.GPIO6.IO26_IN */
#define SC_P_SPI1_SCK                      115U /* , ADMA.I2C2.SDA, ADMA.SPI1.SCK, LSIO.GPIO3.IO00 */
#define SC_P_SPI1_SDO                      116U /* , ADMA.I2C2.SCL, ADMA.SPI1.SDO, LSIO.GPIO3.IO01 */
#define SC_P_SPI1_SDI                      117U /* , ADMA.I2C3.SCL, ADMA.SPI1.SDI, LSIO.GPIO3.IO02 */
#define SC_P_SPI1_CS0                      118U /* , ADMA.I2C3.SDA, ADMA.SPI1.CS0, LSIO.GPIO3.IO03 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_GPIORHD 119U /*  */
#define SC_P_QSPI0A_DATA1                  120U /* LSIO.QSPI0A.DATA1, LSIO.GPIO3.IO10 */
#define SC_P_QSPI0A_DATA0                  121U /* LSIO.QSPI0A.DATA0, LSIO.GPIO3.IO09 */
#define SC_P_QSPI0A_DATA3                  122U /* LSIO.QSPI0A.DATA3, LSIO.GPIO3.IO12 */
#define SC_P_QSPI0A_DATA2                  123U /* LSIO.QSPI0A.DATA2, LSIO.GPIO3.IO11 */
#define SC_P_QSPI0A_SS0_B                  124U /* LSIO.QSPI0A.SS0_B, LSIO.GPIO3.IO14 */
#define SC_P_QSPI0A_DQS                    125U /* LSIO.QSPI0A.DQS, LSIO.GPIO3.IO13 */
#define SC_P_QSPI0A_SCLK                   126U /* LSIO.QSPI0A.SCLK, LSIO.GPIO3.IO16 */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_QSPI0A  127U /*  */
#define SC_P_QSPI0B_SCLK                   128U /* LSIO.QSPI0B.SCLK, LSIO.GPIO3.IO17 */
#define SC_P_QSPI0B_DQS                    129U /* LSIO.QSPI0B.DQS, LSIO.GPIO3.IO22 */
#define SC_P_QSPI0B_DATA1                  130U /* LSIO.QSPI0B.DATA1, LSIO.GPIO3.IO19 */
#define SC_P_QSPI0B_DATA0                  131U /* LSIO.QSPI0B.DATA0, LSIO.GPIO3.IO18 */
#define SC_P_QSPI0B_DATA3                  132U /* LSIO.QSPI0B.DATA3, LSIO.GPIO3.IO21 */
#define SC_P_QSPI0B_DATA2                  133U /* LSIO.QSPI0B.DATA2, LSIO.GPIO3.IO20 */
#define SC_P_QSPI0B_SS0_B                  134U /* LSIO.QSPI0B.SS0_B, LSIO.GPIO3.IO23, LSIO.QSPI0A.SS1_B */
#define SC_P_COMP_CTL_GPIO_1V8_3V3_QSPI0B  135U /*  */
/** @} */

#endif /* SC_PADS_H */
