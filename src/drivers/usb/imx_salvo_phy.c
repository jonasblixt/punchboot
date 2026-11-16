/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X's USB3 PHY, Cadence's Salvo: the values its registers get
 * before the cdns3 controller starts (the MEK's command mode). The PHY has
 * no public manual. The registers and their values are the ones NXP's
 * drivers write for it (U-Boot's and Linux's); the code, its order of
 * groups and its words are pb's (F-205).
 */

#include <drivers/usb/imx_salvo_phy.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>

/* Register n of the PHY is the 32 bit word at n * 4: its value is in the
 * word's low 16 bits. The USB 2.0 part's registers (UTMI) are from 0xe000 */
#define SALVO(n) ((uintptr_t)(n) * 4)

struct salvo_write {
    uint16_t reg;
    uint16_t value;
};

/*
 * The writes, in the order the PHY gets them, one group after the other.
 * Each line names its register by Cadence's name.
 */
static const struct salvo_write salvo_init[] = {
    /* The common block: its clock, and PLL0, which makes the lane's
     * 5 GT/s from the reference clock */
    { 0xc800, 0x0830 }, /* PHY_PMA_CMN_CTRL1 */
    { 0x01e0, 0x0010 }, /* CMN_DIAG_HSCLK_SEL */
    { 0x0084, 0x00f0 }, /* CMN_PLL0_VCOCAL_INIT_TMR */
    { 0x0085, 0x0018 }, /* CMN_PLL0_VCOCAL_ITER_TMR (F-468) */
    { 0x0094, 0x00d0 }, /* CMN_PLL0_INTDIV */
    { 0x0095, 0x4aaa }, /* CMN_PLL0_FRACDIV */
    { 0x0096, 0x0034 }, /* CMN_PLL0_HIGH_THR */
    { 0x0098, 0x01ee }, /* CMN_PLL0_SS_CTRL1 */
    { 0x0099, 0x7f03 }, /* CMN_PLL0_SS_CTRL2 */
    { 0x0097, 0x0020 }, /* CMN_PLL0_DSM_DIAG */
    { 0x01c2, 0x0000 }, /* CMN_DIAG_PLL0_OVRD */
    { 0x01c0, 0x0000 }, /* CMN_DIAG_PLL0_FBH_OVRD */
    { 0x01c1, 0x0000 }, /* CMN_DIAG_PLL0_FBL_OVRD */
    { 0x01c5, 0x0007 }, /* CMN_DIAG_PLL0_V2I_TUNE */
    { 0x01c6, 0x0027 }, /* CMN_DIAG_PLL0_CP_TUNE */
    { 0x01c7, 0x0008 }, /* CMN_DIAG_PLL0_LF_PROG */
    { 0x01c4, 0x0022 }, /* CMN_DIAG_PLL0_TEST_MODE */
    { 0x0061, 0x000a }, /* CMN_PSM_CLK_CTRL */
    { 0x40ea, 0x0139 }, /* XCVR_DIAG_RX_LANE_CAL_RST_TMR */
    { 0x4001, 0xbefc }, /* XCVR_PSM_RCTRL */

    /* The transmitter's power states A0 to A3, calibration and ready */
    { 0x4100, 0x7799 }, /* TX_PSC_A0 */
    { 0x4101, 0x7798 }, /* TX_PSC_A1 */
    { 0x4102, 0x509b }, /* TX_PSC_A2 */
    { 0x41f5, 0x0003 }, /* TX_DIAG_ECTRL_OVRD */
    { 0x4103, 0x509b }, /* TX_PSC_A3 */
    { 0x4106, 0x2090 }, /* TX_PSC_CAL */
    { 0x4107, 0x2090 }, /* TX_PSC_RDY */

    /* The receiver's, the same */
    { 0x8000, 0xa6fd }, /* RX_PSC_A0 */
    { 0x8001, 0xa6fd }, /* RX_PSC_A1 */
    { 0x8002, 0xa410 }, /* RX_PSC_A2 */
    { 0x8003, 0x2410 }, /* RX_PSC_A3 */
    { 0x8006, 0x23ff }, /* RX_PSC_CAL */
    { 0x8007, 0x2010 }, /* RX_PSC_RDY */

    /* The transmitter's margins, the receiver's signal detection and
     * equalisation */
    { 0x4058, 0x0020 }, /* TX_TXCC_MGNLS_MULT_000 */
    { 0x41e7, 0x00ff }, /* TX_DIAG_BGREF_PREDRV_DELAY */
    { 0x80e3, 0x0002 }, /* RX_SLC_CU_ITER_TMR */
    { 0x8090, 0x0013 }, /* RX_SIGDET_HL_FILT_TMR */
    { 0x8058, 0x0000 }, /* RX_SAMP_DAC_CTRL */
    { 0x81dc, 0x1004 }, /* RX_DIAG_SIGDET_TUNE */
    { 0x81df, 0x4041 }, /* RX_DIAG_LFPSDET_TUNE2 */
    { 0x81f5, 0x0480 }, /* RX_DIAG_BS_TM */
    { 0x81d3, 0x8006 }, /* RX_DIAG_DFE_CTRL1 */
    { 0x81c7, 0x003f }, /* RX_DIAG_ILL_IQE_TRIM4 */
    { 0x81c2, 0x543f }, /* RX_DIAG_ILL_E_TRIM0 */
    { 0x81c1, 0x543f }, /* RX_DIAG_ILL_IQ_TRIM0 */
    { 0x81c9, 0x0000 }, /* RX_DIAG_ILL_IQE_TRIM6 */
    { 0x81f8, 0x8000 }, /* RX_DIAG_RXFE_TM3 */
    { 0x81f9, 0x0003 }, /* RX_DIAG_RXFE_TM4 */
    { 0x81dd, 0x2408 }, /* RX_DIAG_LFPSDET_TUNE */
    { 0x81d5, 0x05ca }, /* RX_DIAG_DFE_CTRL3 */
    { 0x81e1, 0x0258 }, /* RX_DIAG_SC2C_DELAY */
    { 0x81bf, 0x1fff }, /* RX_REE_VGA_GAIN_NODFE */

    /* The power state machine's timers: calibration, bypass, and the
     * entry to and the exit from each of the states A0 to A5 */
    { 0x4002, 0x02c6 }, /* XCVR_PSM_CAL_TMR */
    { 0x4004, 0x0002 }, /* XCVR_PSM_A0BYP_TMR */
    { 0x4003, 0x02c6 }, /* XCVR_PSM_A0IN_TMR */
    { 0x4005, 0x0010 }, /* XCVR_PSM_A1IN_TMR */
    { 0x4006, 0x0010 }, /* XCVR_PSM_A2IN_TMR */
    { 0x4007, 0x0010 }, /* XCVR_PSM_A3IN_TMR */
    { 0x4008, 0x0010 }, /* XCVR_PSM_A4IN_TMR */
    { 0x4009, 0x0010 }, /* XCVR_PSM_A5IN_TMR */
    { 0x400a, 0x0002 }, /* XCVR_PSM_A0OUT_TMR */
    { 0x400b, 0x0002 }, /* XCVR_PSM_A1OUT_TMR */
    { 0x400c, 0x0002 }, /* XCVR_PSM_A2OUT_TMR */
    { 0x400d, 0x0002 }, /* XCVR_PSM_A3OUT_TMR */
    { 0x400e, 0x0002 }, /* XCVR_PSM_A4OUT_TMR */
    { 0x400f, 0x0002 }, /* XCVR_PSM_A5OUT_TMR */

    /* The receiver detection's timing on the transmitter */
    { 0x4122, 0x0960 }, /* TX_RCVDET_EN_TMR */
    { 0x4123, 0x01e0 }, /* TX_RCVDET_ST_TMR */
    { 0x40f2, 0x0090 }, /* XCVR_DIAG_LANE_FCM_EN_MGN_TMR */
};

/* Receiver detection in P3 on the 32 kHz clock */
#define SALVO_TX_RCVDETSC_CTRL  0x4124
#define RCVDETSC_P3_32KHZ       BIT(0)

/* USB 2.0: txvalid isn't held back once the HS and FS transmitters are on
 * (field 5:4 to 0b11), and the receiver's analog setting */
#define SALVO_UTMI_REG15        0xe0af
#define UTMI_REG15_TXVALID_GATE (BIT(4) | BIT(5))
#define SALVO_UTMI_AFE_RX_REG5  0xe012

int imx_salvo_phy_init(uintptr_t base)
{
    LOG_INFO("Init salvo PHY @ 0x%" PRIxPTR, base);

    for (size_t i = 0; i < ARRAY_SIZE(salvo_init); i++)
        mmio_write_32(base + SALVO(salvo_init[i].reg), salvo_init[i].value);

    mmio_clrsetbits_32(base + SALVO(SALVO_TX_RCVDETSC_CTRL), 0, RCVDETSC_P3_32KHZ);
    mmio_clrsetbits_32(
        base + SALVO(SALVO_UTMI_REG15), UTMI_REG15_TXVALID_GATE, UTMI_REG15_TXVALID_GATE);
    mmio_write_32(base + SALVO(SALVO_UTMI_AFE_RX_REG5), 0x05);

    /* The PHY settles before the controller is started */
    pb_delay_ms(1);
    return PB_OK;
}
