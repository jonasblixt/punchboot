/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/usb/imx_salvo_phy.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/pb.h>

/**
 * Register definitions and initial values copied from u-boot, however
 * they originate from a cadence manual and NXP originally. Hopefully this is OK
 * license wise.
 */

/* USB3 PHY register definition */
#define PHY_PMA_CMN_CTRL1                     (0xC800 * 4)
#define TB_ADDR_CMN_DIAG_HSCLK_SEL            (0x01e0 * 4)
#define TB_ADDR_CMN_PLL0_VCOCAL_INIT_TMR      (0x0084 * 4)
#define TB_ADDR_CMN_PLL0_VCOCAL_ITER_TMR      (0x0088 * 4)
#define TB_ADDR_CMN_PLL0_INTDIV               (0x0094 * 4)
#define TB_ADDR_CMN_PLL0_FRACDIV              (0x0095 * 4)
#define TB_ADDR_CMN_PLL0_HIGH_THR             (0x0096 * 4)
#define TB_ADDR_CMN_PLL0_SS_CTRL1             (0x0098 * 4)
#define TB_ADDR_CMN_PLL0_SS_CTRL2             (0x0099 * 4)
#define TB_ADDR_CMN_PLL0_DSM_DIAG             (0x0097 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_OVRD            (0x01c2 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_FBH_OVRD        (0x01c0 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_FBL_OVRD        (0x01c1 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_V2I_TUNE        (0x01C5 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_CP_TUNE         (0x01C6 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_LF_PROG         (0x01C7 * 4)
#define TB_ADDR_CMN_DIAG_PLL0_TEST_MODE       (0x01c4 * 4)
#define TB_ADDR_CMN_PSM_CLK_CTRL              (0x0061 * 4)
#define TB_ADDR_XCVR_DIAG_RX_LANE_CAL_RST_TMR (0x40ea * 4)
#define TB_ADDR_XCVR_PSM_RCTRL                (0x4001 * 4)
#define TB_ADDR_TX_PSC_A0                     (0x4100 * 4)
#define TB_ADDR_TX_PSC_A1                     (0x4101 * 4)
#define TB_ADDR_TX_PSC_A2                     (0x4102 * 4)
#define TB_ADDR_TX_PSC_A3                     (0x4103 * 4)
#define TB_ADDR_TX_DIAG_ECTRL_OVRD            (0x41f5 * 4)
#define TB_ADDR_TX_PSC_CAL                    (0x4106 * 4)
#define TB_ADDR_TX_PSC_RDY                    (0x4107 * 4)
#define TB_ADDR_RX_PSC_A0                     (0x8000 * 4)
#define TB_ADDR_RX_PSC_A1                     (0x8001 * 4)
#define TB_ADDR_RX_PSC_A2                     (0x8002 * 4)
#define TB_ADDR_RX_PSC_A3                     (0x8003 * 4)
#define TB_ADDR_RX_PSC_CAL                    (0x8006 * 4)
#define TB_ADDR_RX_PSC_RDY                    (0x8007 * 4)
#define TB_ADDR_TX_TXCC_MGNLS_MULT_000        (0x4058 * 4)
#define TB_ADDR_TX_DIAG_BGREF_PREDRV_DELAY    (0x41e7 * 4)
#define TB_ADDR_RX_SLC_CU_ITER_TMR            (0x80e3 * 4)
#define TB_ADDR_RX_SIGDET_HL_FILT_TMR         (0x8090 * 4)
#define TB_ADDR_RX_SAMP_DAC_CTRL              (0x8058 * 4)
#define TB_ADDR_RX_DIAG_SIGDET_TUNE           (0x81dc * 4)
#define TB_ADDR_RX_DIAG_LFPSDET_TUNE2         (0x81df * 4)
#define TB_ADDR_RX_DIAG_BS_TM                 (0x81f5 * 4)
#define TB_ADDR_RX_DIAG_DFE_CTRL1             (0x81d3 * 4)
#define TB_ADDR_RX_DIAG_ILL_IQE_TRIM4         (0x81c7 * 4)
#define TB_ADDR_RX_DIAG_ILL_E_TRIM0           (0x81c2 * 4)
#define TB_ADDR_RX_DIAG_ILL_IQ_TRIM0          (0x81c1 * 4)
#define TB_ADDR_RX_DIAG_ILL_IQE_TRIM6         (0x81c9 * 4)
#define TB_ADDR_RX_DIAG_RXFE_TM3              (0x81f8 * 4)
#define TB_ADDR_RX_DIAG_RXFE_TM4              (0x81f9 * 4)
#define TB_ADDR_RX_DIAG_LFPSDET_TUNE          (0x81dd * 4)
#define TB_ADDR_RX_DIAG_DFE_CTRL3             (0x81d5 * 4)
#define TB_ADDR_RX_DIAG_SC2C_DELAY            (0x81e1 * 4)
#define TB_ADDR_RX_REE_VGA_GAIN_NODFE         (0x81bf * 4)
#define TB_ADDR_XCVR_PSM_CAL_TMR              (0x4002 * 4)
#define TB_ADDR_XCVR_PSM_A0BYP_TMR            (0x4004 * 4)
#define TB_ADDR_XCVR_PSM_A0IN_TMR             (0x4003 * 4)
#define TB_ADDR_XCVR_PSM_A1IN_TMR             (0x4005 * 4)
#define TB_ADDR_XCVR_PSM_A2IN_TMR             (0x4006 * 4)
#define TB_ADDR_XCVR_PSM_A3IN_TMR             (0x4007 * 4)
#define TB_ADDR_XCVR_PSM_A4IN_TMR             (0x4008 * 4)
#define TB_ADDR_XCVR_PSM_A5IN_TMR             (0x4009 * 4)
#define TB_ADDR_XCVR_PSM_A0OUT_TMR            (0x400a * 4)
#define TB_ADDR_XCVR_PSM_A1OUT_TMR            (0x400b * 4)
#define TB_ADDR_XCVR_PSM_A2OUT_TMR            (0x400c * 4)
#define TB_ADDR_XCVR_PSM_A3OUT_TMR            (0x400d * 4)
#define TB_ADDR_XCVR_PSM_A4OUT_TMR            (0x400e * 4)
#define TB_ADDR_XCVR_PSM_A5OUT_TMR            (0x400f * 4)
#define TB_ADDR_TX_RCVDET_EN_TMR              (0x4122 * 4)
#define TB_ADDR_TX_RCVDET_ST_TMR              (0x4123 * 4)
#define TB_ADDR_XCVR_DIAG_LANE_FCM_EN_MGN_TMR (0x40f2 * 4)
#define TB_ADDR_TX_RCVDETSC_CTRL              (0x4124 * 4)

/* USB2 PHY register definition */
#define UTMI_REG15                            (0x38000 + 0xaf * 4)
#define UTMI_AFE_RX_REG5                      (0x38000 + 0x12 * 4)
#define UTMI_AFE_BC_REG4                      (0x38000 + 0x29 * 4)

/* TB_ADDR_TX_RCVDETSC_CTRL */
#define RXDET_IN_P3_32KHZ                     BIT(0)
/*
 * UTMI_REG15
 *
 * Gate how many us for the txvalid signal until analog
 * HS/FS transmitters have powered up
 */
#define TXVALID_GATE_THRESHOLD_HS_MASK        (BIT(4) | BIT(5))
/* 0us, txvalid is ready just after HS/FS transmitters have powered up */
#define TXVALID_GATE_THRESHOLD_HS_0US         (BIT(4) | BIT(5))

#define SET_B_SESSION_VALID                   (BIT(6) | BIT(5))
#define CLR_B_SESSION_VALID                   (BIT(6))

struct cdns_reg_pairs {
    uint32_t val;
    uint32_t reg_offset;
};

static const struct cdns_reg_pairs cdns_nxp_sequence_pair[] = {
    { 0x0830, PHY_PMA_CMN_CTRL1 },
    { 0x0010, TB_ADDR_CMN_DIAG_HSCLK_SEL },
    { 0x00f0, TB_ADDR_CMN_PLL0_VCOCAL_INIT_TMR },
    { 0x0018, TB_ADDR_CMN_PLL0_VCOCAL_ITER_TMR },
    { 0x00d0, TB_ADDR_CMN_PLL0_INTDIV },
    { 0x4aaa, TB_ADDR_CMN_PLL0_FRACDIV },
    { 0x0034, TB_ADDR_CMN_PLL0_HIGH_THR },
    { 0x01ee, TB_ADDR_CMN_PLL0_SS_CTRL1 },
    { 0x7f03, TB_ADDR_CMN_PLL0_SS_CTRL2 },
    { 0x0020, TB_ADDR_CMN_PLL0_DSM_DIAG },
    { 0x0000, TB_ADDR_CMN_DIAG_PLL0_OVRD },
    { 0x0000, TB_ADDR_CMN_DIAG_PLL0_FBH_OVRD },
    { 0x0000, TB_ADDR_CMN_DIAG_PLL0_FBL_OVRD },
    { 0x0007, TB_ADDR_CMN_DIAG_PLL0_V2I_TUNE },
    { 0x0027, TB_ADDR_CMN_DIAG_PLL0_CP_TUNE },
    { 0x0008, TB_ADDR_CMN_DIAG_PLL0_LF_PROG },
    { 0x0022, TB_ADDR_CMN_DIAG_PLL0_TEST_MODE },
    { 0x000a, TB_ADDR_CMN_PSM_CLK_CTRL },
    { 0x0139, TB_ADDR_XCVR_DIAG_RX_LANE_CAL_RST_TMR },
    { 0xbefc, TB_ADDR_XCVR_PSM_RCTRL },

    { 0x7799, TB_ADDR_TX_PSC_A0 },
    { 0x7798, TB_ADDR_TX_PSC_A1 },
    { 0x509b, TB_ADDR_TX_PSC_A2 },
    { 0x0003, TB_ADDR_TX_DIAG_ECTRL_OVRD },
    { 0x509b, TB_ADDR_TX_PSC_A3 },
    { 0x2090, TB_ADDR_TX_PSC_CAL },
    { 0x2090, TB_ADDR_TX_PSC_RDY },

    { 0xA6FD, TB_ADDR_RX_PSC_A0 },
    { 0xA6FD, TB_ADDR_RX_PSC_A1 },
    { 0xA410, TB_ADDR_RX_PSC_A2 },
    { 0x2410, TB_ADDR_RX_PSC_A3 },

    { 0x23FF, TB_ADDR_RX_PSC_CAL },
    { 0x2010, TB_ADDR_RX_PSC_RDY },

    { 0x0020, TB_ADDR_TX_TXCC_MGNLS_MULT_000 },
    { 0x00ff, TB_ADDR_TX_DIAG_BGREF_PREDRV_DELAY },
    { 0x0002, TB_ADDR_RX_SLC_CU_ITER_TMR },
    { 0x0013, TB_ADDR_RX_SIGDET_HL_FILT_TMR },
    { 0x0000, TB_ADDR_RX_SAMP_DAC_CTRL },
    { 0x1004, TB_ADDR_RX_DIAG_SIGDET_TUNE },
    { 0x4041, TB_ADDR_RX_DIAG_LFPSDET_TUNE2 },
    { 0x0480, TB_ADDR_RX_DIAG_BS_TM },
    { 0x8006, TB_ADDR_RX_DIAG_DFE_CTRL1 },
    { 0x003f, TB_ADDR_RX_DIAG_ILL_IQE_TRIM4 },
    { 0x543f, TB_ADDR_RX_DIAG_ILL_E_TRIM0 },
    { 0x543f, TB_ADDR_RX_DIAG_ILL_IQ_TRIM0 },
    { 0x0000, TB_ADDR_RX_DIAG_ILL_IQE_TRIM6 },
    { 0x8000, TB_ADDR_RX_DIAG_RXFE_TM3 },
    { 0x0003, TB_ADDR_RX_DIAG_RXFE_TM4 },
    { 0x2408, TB_ADDR_RX_DIAG_LFPSDET_TUNE },
    { 0x05ca, TB_ADDR_RX_DIAG_DFE_CTRL3 },
    { 0x0258, TB_ADDR_RX_DIAG_SC2C_DELAY },
    { 0x1fff, TB_ADDR_RX_REE_VGA_GAIN_NODFE },

    { 0x02c6, TB_ADDR_XCVR_PSM_CAL_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A0BYP_TMR },
    { 0x02c6, TB_ADDR_XCVR_PSM_A0IN_TMR },
    { 0x0010, TB_ADDR_XCVR_PSM_A1IN_TMR },
    { 0x0010, TB_ADDR_XCVR_PSM_A2IN_TMR },
    { 0x0010, TB_ADDR_XCVR_PSM_A3IN_TMR },
    { 0x0010, TB_ADDR_XCVR_PSM_A4IN_TMR },
    { 0x0010, TB_ADDR_XCVR_PSM_A5IN_TMR },

    { 0x0002, TB_ADDR_XCVR_PSM_A0OUT_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A1OUT_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A2OUT_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A3OUT_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A4OUT_TMR },
    { 0x0002, TB_ADDR_XCVR_PSM_A5OUT_TMR },
    /* Change rx detect parameter */
    { 0x0960, TB_ADDR_TX_RCVDET_EN_TMR },
    { 0x01e0, TB_ADDR_TX_RCVDET_ST_TMR },
    { 0x0090, TB_ADDR_XCVR_DIAG_LANE_FCM_EN_MGN_TMR },
};

int imx_salvo_phy_init(uintptr_t base)
{
    LOG_INFO("Init salvo PHY @ 0x%" PRIxPTR, base);

    for (unsigned int i = 0; i < ARRAY_SIZE(cdns_nxp_sequence_pair); i++) {
        mmio_write_32(base + cdns_nxp_sequence_pair[i].reg_offset, cdns_nxp_sequence_pair[i].val);
    }

    mmio_clrsetbits_32(base + TB_ADDR_TX_RCVDETSC_CTRL, 0, RXDET_IN_P3_32KHZ);
    mmio_clrsetbits_32(
        base + UTMI_REG15, TXVALID_GATE_THRESHOLD_HS_MASK, TXVALID_GATE_THRESHOLD_HS_0US);
    mmio_write_32(base + UTMI_AFE_RX_REG5, 0x05);

    pb_delay_ms(1);
    return PB_OK;
}
