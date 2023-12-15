/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PLAT_IMXRT_CLOCK_H
#define INCLUDE_PLAT_IMXRT_CLOCK_H

#include <pb/utils_def.h>

#define IMXRT_CCM_CCR                (0x400FC000)
#define CCR_RBC_EN                   BIT(27)
#define CCR_REG_BYPASS_COUNT_SHIFT   (21)
#define CCR_REG_BYPASS_COUNT_MASK    (0x3f << CCR_REG_BYPASS_COUNT_SHIFT)
#define CCR_REG_BYPASS_COUNT(x)      ((x << CCR_REG_BYPASS_COUNT_SHIFT) & CCR_REG_BYPASS_COUNT_MASK)
#define CCR_COSC_EN                  BIT(12)
#define CCR_OSCNT_SHIFT              (0)
#define CCR_OSCNT_MASK               (0x3f << CCR_OSCNT_SHIFT)
#define CCR_OSCNT(x)                 ((x << CCR_OSCNT_SHIFT) & CCR_OSCNT_MASK)

#define IMXRT_CCM_CSR                (0x400FC008)
#define CSR_COSC_READY               BIT(5)
#define CSR_CAMP2_READY              BIT(3)
#define CSR_REF_EN_B                 BIT(0)

#define IMXRT_CCM_CCSR               (0x400FC00C)
#define CCSR_PLL3_SW_CLK_SEL         BIT(0)

#define IMXRT_CCM_CACRR              (0x400FC010)
#define CACRR_ARM_PODF(x)            (x & 7)
#define CACRR_ARM_PODF_MASK          (7)
#define CACRR_ARM_PODF_SHIFT         (0)

#define IMXRT_CCM_CBCDR              (0x400FC014)
#define CBCDR_PERIPH_CLK2_PODF_SHIFT (27)
#define CBCDR_PERIPH_CLK2_PODF_MASK  (7 << CBCDR_PERIPH_CLK2_PODF_SHIFT)
#define CBCDR_PERIPH_CLK2_PODF(x) \
    ((x << CBCDR_PERIPH_CLK2_PODF_SHIFT) & CBCDR_PERIPH_CLK2_PODF_MASK)
#define CBCDR_PERIPH_CLK_SEL           BIT(25)
#define CBCDR_SEMC_PODF_SHIFT          (16)
#define CBCDR_SEMC_PODF_MASK           (7 << CBCDR_SEMC_PODF_SHIFT)
#define CBCDR_SEMC_PODF(x)             ((x << CBCDR_SEMC_PODF_SHIFT) & CBCDR_SEMC_PODF_MASK)
#define CBCDR_AHB_PODF_SHIFT           (10)
#define CBCDR_AHB_PODF_MASK            (7 << CBCDR_AHB_PODF_SHIFT)
#define CBCDR_AHB_PODF(x)              ((x << CBCDR_AHB_PODF_SHIFT) & CBCDR_AHB_PODF_MASK)
#define CBCDR_IPG_PODF_SHIFT           (8)
#define CBCDR_IPG_PODF_MASK            (3 << CBCDR_IPG_PODF_SHIFT)
#define CBCDR_IPG_PODF(x)              ((x << CBCDR_IPG_PODF_SHIFT) & CBCDR_IPG_PODF_MASK)
#define CBCDR_SEMC_ALT_CLK_SEL         BIT(7)
#define CBCDR_SEMC_CLK_SEL             BIT(6)

#define IMXRT_CCM_CBCMR                (0x400FC018)
#define CBCMR_FLEXSPI2_PODF_SHIFT      (29)
#define CBCMR_FLEXSPI2_PODF_MASK       (7 << CBCMR_FLEXSPI2_PODF_SHIFT)
#define CBCMR_FLEXSPI2_PODF(x)         ((x << CBCMR_FLEXSPI2_PODF_SHIFT) & CBCMR_FLEXSPI2_PODF_MASK)
#define CBCMR_LPSPI_PODF_SHIFT         (26)
#define CBCMR_LPSPI_PODF_MASK          (7 << CBCMR_LPSPI_PODF_SHIFT)
#define CBCMR_LPSPI_PODF(x)            ((x << CBCMR_LPSPI_PODF_SHIFT) & CBCMR_LPSPI_PODF_MASK)
#define CBCMR_LCDIF_PODF_SHIFT         (23)
#define CBCMR_LCDIF_PODF_MASK          (7 << CBCMR_LCDIF_PODF_SHIFT)
#define CBCMR_LCDIF_PODF(x)            ((x << CBCMR_LCDIF_PODF_SHIFT) & CBCMR_LCDIF_PODF_MASK)
#define CBCMR_PRE_PERIPH_CLK_SEL_SHIFT (18)
#define CBCMR_PRE_PERIPH_CLK_SEL_MASK  (3 << CBCMR_PRE_PERIPH_CLK_SEL_SHIFT)
#define CBCMR_PRE_PERIPH_CLK_SEL(x) \
    ((x << CBCMR_PRE_PERIPH_CLK_SEL_SHIFT) & CBCMR_PRE_PERIPH_CLK_SEL_MASK)
#define CBCMR_TRACE_CLK_SEL_SHIFT    (14)
#define CBCMR_TRACE_CLK_SEL_MASK     (3 << CBCMR_TRACE_CLK_SEL_SHIFT)
#define CBCMR_TRACE_CLK_SEL(x)       ((x << CBCMR_TRACE_CLK_SEL_SHIFT) & CBCMR_TRACE_CLK_SEL_MASK)
#define CBCMR_PERIPH_CLK2_SEL_SHIFT  (12)
#define CBCMR_PERIPH_CLK2_SEL_MASK   (3 << CBCMR_PERIPH_CLK2_SEL_SHIFT)
#define CBCMR_PERIPH_CLK2_SEL(x)     ((x << CBCMR_PERIPH_CLK2_SEL_SHIFT) & CBCMR_PERIPH_CLK2_SEL_MASK)
#define CBCMR_FLEXSPI2_CLK_SEL_SHIFT (8)
#define CBCMR_FLEXSPI2_CLK_SEL_MASK  (3 << CBCMR_FLEXSPI2_CLK_SEL_SHIFT)
#define CBCMR_FLEXSPI2_CLK_SEL(x) \
    ((x << CBCMR_FLEXSPI2_CLK_SEL_SHIFT) & CBCMR_FLEXSPI2_CLK_SEL_MASK)
#define CBCMR_LPSPI_CLK_SEL_SHIFT    (4)
#define CBCMR_LPSPI_CLK_SEL_MASK     (3 << CBCMR_LPSPI_CLK_SEL_SHIFT)
#define CBCMR_LPSPI_CLK_SEL(x)       ((x << CBCMR_LPSPI_CLK_SEL_SHIFT) & CBCMR_LPSPI_CLK_SEL_MASK)

#define IMXRT_CCM_CSCMR1             (0x400FC01C)
#define CSCMR1_FLEXSPI_CLK_SEL_SHIFT (29)
#define CSCMR1_FLEXSPI_CLK_SEL_MASK  (3 << CSCMR1_FLEXSPI_CLK_SEL_SHIFT)
#define CSCMR1_FLEXSPI_CLK_SEL(x) \
    ((x << CSCMR1_FLEXSPI_CLK_SEL_SHIFT) & CSCMR1_FLEXSPI_CLK_SEL_MASK)
#define CSCMR1_FLEXSPI_PODF_SHIFT    (23)
#define CSCMR1_FLEXSPI_PODF_MASK     (7 << CSCMR1_FLEXSPI_PODF_SHIFT)
#define CSCMR1_FLEXSPI_PODF(x)       ((x << CSCMR1_FLEXSPI_PODF_SHIFT) & CSCMR1_FLEXSPI_PODF_MASK)
#define CSCMR1_USDHC2_CLK_SEL        BIT(17)
#define CSCMR1_USDHC1_CLK_SEL        BIT(16)
#define CSCMR1_PERCLK_CLK_SEL        BIT(6)
#define CSCMR1_SAI3_CLK_SEL_SHIFT    (14)
#define CSCMR1_SAI3_CLK_SEL_MASK     (3 << CSCMR1_SAI3_CLK_SEL_SHIFT)
#define CSCMR1_SAI3_CLK_SEL(x)       ((x & CSCMR1_SAI3_CLK_SEL_MASK) << CSCMR1_SAI3_CLK_SEL_SHIFT)
#define CSCMR1_SAI2_CLK_SEL_SHIFT    (12)
#define CSCMR1_SAI2_CLK_SEL_MASK     (3 << CSCMR1_SAI2_CLK_SEL_SHIFT)
#define CSCMR1_SAI2_CLK_SEL(x)       ((x & CSCMR1_SAI2_CLK_SEL_MASK) << CSCMR1_SAI2_CLK_SEL_SHIFT)
#define CSCMR1_SAI1_CLK_SEL_SHIFT    (10)
#define CSCMR1_SAI1_CLK_SEL_MASK     (3 << CSCMR1_SAI1_CLK_SEL_SHIFT)
#define CSCMR1_SAI1_CLK_SEL(x)       ((x & CSCMR1_SAI1_CLK_SEL_MASK) << CSCMR1_SAI1_CLK_SEL_SHIFT)
#define CSCMR1_PERCLK_PODF_SHIFT     (0)
#define CSCMR1_PERCLK_PODF_MASK      (0x3f << CSCMR1_PERCLK_PODF_SHIFT)
#define CSCMR1_PERCLK_PODF(x)        ((x & CSCMR1_PERCLK_PODF_MASK) << CSCMR1_PERCLK_PODF_SHIFT)

#define IMXRT_CCM_CSCMR2             (0x400FC020)
#define CSCMR2_FLEXIO2_CLK_SEL_SHIFT (19)
#define CSCMR2_FLEXIO2_CLK_SEL_MASK  (3 << CSCMR1_FLEXIO2_CLK_SEL_SHIFT)
#define CSCMR2_FLEXIO2_CLK_SEL(x) \
    ((x & CSCMR1_FLEXIO2_CLK_SEL_MASK) << CSCMR1_FLEXIO2_CLK_SEL_SHIFT)
#define CSCMR2_CAN_CLK_SEL_SHIFT      (8)
#define CSCMR2_CAN_CLK_SEL_MASK       (3 << CSCMR1_CAN_CLK_SEL_SHIFT)
#define CSCMR2_CAN_CLK_SEL(x)         ((x & CSCMR1_CAN_CLK_SEL_MASK) << CSCMR1_CAN_CLK_SEL_SHIFT)
#define CSCMR2_CAN_PODF_SHIFT         (2)
#define CSCMR2_CAN_PODF_MASK          (0x3f << CSCMR1_PODF_SHIFT)
#define CSCMR2_CAN_PODF(x)            ((x & CSCMR1_PODF_MASK) << CSCMR1_PODF_SHIFT)

#define IMXRT_CCM_CSCDR1              (0x400FC024)
#define CSCDR1_TRACE_PODF_SHIFT       (25)
#define CSCDR1_TRACE_PODF_MASK        (3 << CSCDR1_TRACE_PODF_SHIFT)
#define CSCDR1_TRACE_PODF(x)          ((x << CSCDR1_TRACE_PODF_SHIFT) & CSCDR1_TRACE_PODF_MASK)
#define CSCDR1_USDHC2_PODF_SHIFT      (11)
#define CSCDR1_USDHC2_PODF_MASK       (7 << CSCDR1_USDHC2_PODF_SHIFT)
#define CSCDR1_USDHC2_PODF(x)         ((x << CSCDR1_USDHC2_PODF_SHIFT) & CSCDR1_USDHC2_PODF_MASK)
#define CSCDR1_USDHC1_PODF_SHIFT      (11)
#define CSCDR1_USDHC1_PODF_MASK       (7 << CSCDR1_USDHC1_PODF_SHIFT)
#define CSCDR1_USDHC1_PODF(x)         ((x << CSCDR1_USDHC1_PODF_SHIFT) & CSCDR1_USDHC1_PODF_MASK)
#define CSCDR1_UART_CLK_SEL           BIT(6)
#define CSCDR1_UART_PODF_SHIFT        (0)
#define CSCDR1_UART_PODF_MASK         (0x3f)
#define CSCDR1_UART_PODF(x)           (x & CSCDR1_UART_PODF_MASK)

#define IMXRT_CCM_CS1CDR              (0x400FC028)
#define CS1CDR_FLEXIO2_CLK_PODF_SHIFT (25)
#define CS1CDR_FLEXIO2_CLK_PODF_MASK  (0x7 << CS1CDR_FLEXIO2_CLK_PODF_SHIFT)
#define CS1CDR_FLEXIO2_CLK_PODF(x) \
    ((x << CS1CDR_FLEXIO2_CLK_PODF_SHIFT) & CS1CDR_FLEXIO2_CLK_PODF_MASK)
#define CS1CDR_SAI3_CLK_PRED_SHIFT    (22)
#define CS1CDR_SAI3_CLK_PRED_MASK     (0x7 << CS1CDR_SAI3_CLK_PRED_SHIFT)
#define CS1CDR_SAI3_CLK_PRED(x)       ((x << CS1CDR_SAI3_CLK_PRED_SHIFT) & CS1CDR_SAI3_CLK_PRED_MASK)
#define CS1CDR_SAI3_CLK_PODF_SHIFT    (16)
#define CS1CDR_SAI3_CLK_PODF_MASK     (0x3f << CS1CDR_SAI3_CLK_PODF_SHIFT)
#define CS1CDR_SAI3_CLK_PODF(x)       ((x << CS1CDR_SAI3_CLK_PODF_SHIFT) & CS1CDR_SAI3_CLK_PODF_MASK)
#define CS1CDR_FLEXIO2_CLK_PRED_SHIFT (9)
#define CS1CDR_FLEXIO2_CLK_PRED_MASK  (0x7 << CS1CDR_FLEXIO2_CLK_PRED_SHIFT)
#define CS1CDR_FLEXIO2_CLK_PRED(x) \
    ((x << CS1CDR_FLEXIO2_CLK_PRED_SHIFT) & CS1CDR_FLEXIO2_CLK_PRED_MASK)
#define CS1CDR_SAI1_CLK_PRED_SHIFT   (6)
#define CS1CDR_SAI1_CLK_PRED_MASK    (0x7 << CS1CDR_SAI1_CLK_PRED_SHIFT)
#define CS1CDR_SAI1_CLK_PRED(x)      ((x << CS1CDR_SAI1_CLK_PRED_SHIFT) & CS1CDR_SAI1_CLK_PRED_MASK)
#define CS1CDR_SAI1_CLK_PODF_SHIFT   (0)
#define CS1CDR_SAI1_CLK_PODF_MASK    (0x3f << CS1CDR_SAI1_CLK_PODF_SHIFT)
#define CS1CDR_SAI1_CLK_PODF(x)      ((x << CS1CDR_SAI1_CLK_PODF_SHIFT) & CS1CDR_SAI1_CLK_PODF_MASK)

#define IMXRT_CCM_CS2CDR             (0x400FC02C)
#define CS2CDR_SAI2_CLK_PRED_SHIFT   (6)
#define CS2CDR_SAI2_CLK_PRED_MASK    (0x7 << CS2CDR_SAI2_CLK_PRED_SHIFT)
#define CS2CDR_SAI2_CLK_PRED(x)      ((x << CS2CDR_SAI2_CLK_PRED_SHIFT) & CS2CDR_SAI2_CLK_PRED_MASK)
#define CS2CDR_SAI2_CLK_PODF_SHIFT   (0)
#define CS2CDR_SAI2_CLK_PODF_MASK    (0x3f << CS2CDR_SAI2_CLK_PODF_SHIFT)
#define CS2CDR_SAI2_CLK_PODF(x)      ((x << CS2CDR_SAI2_CLK_PODF_SHIFT) & CS2CDR_SAI2_CLK_PODF_MASK)

#define IMXRT_CCM_CDCDR              (0x400FC030)
#define CDCDR_SPDIF0_CLK_PRED_SHIFT  (25)
#define CDCDR_SPDIF0_CLK_PRED_MASK   (0x7 << CDCDR_SPDIF0_CLK_PRED_SHIFT)
#define CDCDR_SPDIF0_CLK_PRED(x)     ((x << CDCDR_SPDIF0_CLK_PRED_SHIFT) & CDCDR_SPDIF0_CLK_PRED_MASK)
#define CDCDR_SPDIF0_CLK_PODF_SHIFT  (22)
#define CDCDR_SPDIF0_CLK_PODF_MASK   (0x7 << CDCDR_SPDIF0_CLK_PODF_SHIFT)
#define CDCDR_SPDIF0_CLK_PODF(x)     ((x << CDCDR_SPDIF0_CLK_PODF_SHIFT) & CDCDR_SPDIF0_CLK_PODF_MASK)
#define CDCDR_SPDIF0_CLK_SEL_SHIFT   (20)
#define CDCDR_SPDIF0_CLK_SEL_MASK    (0x3 << CDCDR_SPDIF0_CLK_SEL_SHIFT)
#define CDCDR_SPDIF0_CLK_SEL(x)      ((x << CDCDR_SPDIF0_CLK_SEL_SHIFT) & CDCDR_SPDIF0_CLK_SEL_MASK)
#define CDCDR_FLEXIO1_CLK_PRED_SHIFT (12)
#define CDCDR_FLEXIO1_CLK_PRED_MASK  (0x7 << CDCDR_FLEXIO1_CLK_PRED_SHIFT)
#define CDCDR_FLEXIO1_CLK_PRED(x) \
    ((x << CDCDR_FLEXIO1_CLK_PRED_SHIFT) & CDCDR_FLEXIO1_CLK_PRED_MASK)
#define CDCDR_FLEXIO1_CLK_PODF_SHIFT (9)
#define CDCDR_FLEXIO1_CLK_PODF_MASK  (0x7 << CDCDR_FLEXIO1_CLK_PODF_SHIFT)
#define CDCDR_FLEXIO1_CLK_PODF(x) \
    ((x << CDCDR_FLEXIO1_CLK_PODF_SHIFT) & CDCDR_FLEXIO1_CLK_PODF_MASK)
#define CDCDR_FLEXIO1_CLK_SEL_SHIFT    (7)
#define CDCDR_FLEXIO1_CLK_SEL_MASK     (0x3 << CDCDR_FLEXIO1_CLK_SEL_SHIFT)
#define CDCDR_FLEXIO1_CLK_SEL(x)       ((x << CDCDR_FLEXIO1_CLK_SEL_SHIFT) & CDCDR_FLEXIO1_CLK_SEL_MASK)

#define IMXRT_CCM_CSCDR2               (0x400FC038)
#define CSCDR2_LPI2C_CLK_PODF_SHIFT    (19)
#define CSCDR2_LPI2C_CLK_PODF_MASK     (0x3f << CSCDR2_LPI2C_CLK_PODF_SHIFT)
#define CSCDR2_LPI2C_CLK_PODF(x)       ((x << CSCDR2_LPI2C_CLK_PODF_SHIFT) & CSCDR2_LPI2C_CLK_PODF_MASK)
#define CSCDR2_LPI2C_CLK_SEL           BIT(18)
#define CSCDR2_LCDIF_PRE_CLK_SEL_SHIFT (15)
#define CSCDR2_LCDIF_PRE_CLK_SEL_MASK  (0x7 << CSCDR2_LCDIF_PRE_CLK_SEL_SHIFT)
#define CSCDR2_LCDIF_PRE_CLK_SEL(x) \
    ((x << CSCDR2_LCDIF_PRE_CLK_SEL_SHIFT) & CSCDR2_LCDIF_PRE_CLK_SEL_MASK)
#define CSCDR2_LCDIF_PRED_SHIFT             (12)
#define CSCDR2_LCDIF_PRED_MASK              (0x7 << CSCDR2_LCDIF_PRED_SHIFT)
#define CSCDR2_LCDIF_PRED(x)                ((x << CSCDR2_LCDIF_PRED_SHIFT) & CSCDR2_LCDIF_PRED_MASK)

#define IMXRT_CCM_CSCDR3                    (0x400FC03C)
#define CSCDR3_CSI_PODF_SHIFT               (11)
#define CSCDR3_CSI_PODF_MASK                (0x7 << CSCDR3_CSI_PODF_SHIFT)
#define CSCDR3_CSI_PODF(x)                  ((x << CSCDR3_CSI_PODF_SHIFT) & CSCDR3_CSI_PODF_MASK)
#define CSCDR3_CSI_SEL_SHIFT                (9)
#define CSCDR3_CSI_SEL_MASK                 (0x7 << CSCDR3_CSI_SEL_SHIFT)
#define CSCDR3_CSI_SEL(x)                   ((x << CSCDR3_CSI_SEL_SHIFT) & CSCDR3_CSI_SEL_MASK)

#define IMXRT_CCM_CDHIPR                    (0x400FC048)
#define CDHIPR_ARM_PODF_BUSY                BIT(16)
#define CDHIPR_PERIPH_CLK_SEL_BUSY          BIT(5)
#define CDHIPR_PERIPH2_CLK_SEL_BUSY         BIT(3)
#define CDHIPR_AHB_PODF_BUSY                BIT(1)
#define CDHIPR_SEMC_PODF_BUSY               BIT(0)

#define IMXRT_CCM_CLPCR                     (0x400FC054)
#define CLPCR_MASK_L2CC_IDLE                BIT(27)
#define CLPCR_MASK_SCU_IDLE                 BIT(26)
#define CLPCR_MASK_CORE0_WFI                BIT(22)
#define CLPCR_BYPASS_LPM_HS0                BIT(21)
#define CLPCR_BYPASS_LPM_HS1                BIT(19)
#define CLPCR_COSC_PWRDOWN                  BIT(11)
#define CLPCR_STBY_COUNT_SHIFT              (9)
#define CLPCR_STBY_COUNT_MASK               (0x3 << CLPCR_STBY_COUNT_SHIFT)
#define CLPCR_STBY_COUNT(x)                 ((x << CLPCR_STBY_COUNT_SHIFT) & CLPCR_STBY_COUNT_MASK)
#define CLPCR_VSTBY                         BIT(8)
#define CLPCR_DIS_REF_OSC                   BIT(7)
#define CLPCR_SBYOS                         BIT(6)
#define CLPCR_ARM_CLK_DIS_ON_LPM            BIT(5)
#define CLPCR_LPM_SHIFT                     (0)
#define CLPCR_LPM_MASK                      (0x3 << CLPCR_LPM_SHIFT)
#define CLPCR_LPM(x)                        ((x << CLPCR_LPM_SHIFT) & CLPCR_LPM_MASK)

#define IMXRT_CCM_CISR                      (0x400FC058)
#define CISR_ARM_PODF_LOADED                BIT(26)
#define CISR_PERIPH_CLK_SEL_LOADED          BIT(22)
#define CISR_AHB_PODF_LOADED                BIT(20)
#define CISR_PERIPH2_CLK_SEL_LOADED         BIT(19)
#define CISR_SEMC_PODF_LOADED               BIT(17)
#define CISR_COSC_READY                     BIT(6)
#define CISR_LRF_PLL                        BIT(0)

#define IMXRT_CCM_CIMR                      (0x400FC05C)
#define CIMR_ARM_PODF_LOADED                BIT(26)
#define CIMR_MASK_PERIPH_CLK_SEL_LOADED     BIT(22)
#define CIMR_MASK_AHB_PODF_LOADED           BIT(20)
#define CIMR_MASK_PERIPH2_CLK_SEL_LOADED    BIT(19)
#define CIMR_MASK_SEMC_PODF_LOADED          BIT(17)
#define CIMR_MASK_COSC_READY                BIT(6)
#define CIMR_MASK_LRF_PLL                   BIT(0)

#define IMXRT_CCM_CCOSR                     (0x400FC060)
#define CCOSR_CLKO2_EN                      BIT(24)
#define CCOSR_CLKO2_DIV_SHIFT               (21)
#define CCOSR_CLKO2_DIV_MASK                (0x7 << CCOSR_CLKO2_DIV_SHIFT)
#define CCOSR_CLKO2_DIV(x)                  ((x << CCOSR_CLKO2_DIV_SHIFT) & CCOSR_CLKO2_DIV_MASK)
#define CCOSR_CLKO2_SEL_SHIFT               (16)
#define CCOSR_CLKO2_SEL_MASK                (0x1f << CCOSR_CLKO2_SEL_SHIFT)
#define CCOSR_CLKO2_SEL(x)                  ((x << CCOSR_CLKO2_SEL_SHIFT) & CCOSR_CLKO2_SEL_MASK)
#define CCOSR_CLK_OUT_SEL                   BIT(8)
#define CCOSR_CLKO1_EN                      BIT(7)
#define CCOSR_CLKO1_DIV_SHIFT               (4)
#define CCOSR_CLKO1_DIV_MASK                (0x7 << CCOSR_CLKO1_DIV_SHIFT)
#define CCOSR_CLKO1_DIV(x)                  ((x << CCOSR_CLKO1_DIV_SHIFT) & CCOSR_CLKO1_DIV_MASK)
#define CCOSR_CLKO1_SEL_SHIFT               (0)
#define CCOSR_CLKO1_SEL_MASK                (0xf << CCOSR_CLKO1_SEL_SHIFT)
#define CCOSR_CLKO1_SEL(x)                  ((x << CCOSR_CLKO1_SEL_SHIFT) & CCOSR_CLKO1_SEL_MASK)

#define IMXRT_CCM_CGPR                      (0x400FC064)
#define CGPR_INT_MEM_CLK_LPM                BIT(17)
#define CGPR_SYS_MEM_DS_CTRL_SHIFT          (14)
#define CGPR_SYS_MEM_DS_CTRL_MASK           (0x3 << CGPR_SYS_MEM_DS_CTRL_SHIFT)
#define CGPR_SYS_MEM_DS_CTRL(x)             ((x << CGPR_SYS_MEM_DS_CTRL_SHIFT) & CGPR_SYS_MEM_DS_CTRL_MASK)
#define CGPR_EFUSE_PROG_SUPPLY_GATE         BIT(4)
#define CGPR_PMIC_DELAY_SCALER              BIT(0)

/* Clock gating */
#define CG0                                 (3 << 0)
#define CG1                                 (3 << 2)
#define CG2                                 (3 << 4)
#define CG3                                 (3 << 6)
#define CG4                                 (3 << 8)
#define CG5                                 (3 << 10)
#define CG6                                 (3 << 12)
#define CG7                                 (3 << 14)
#define CG8                                 (3 << 16)
#define CG9                                 (3 << 18)
#define CG10                                (3 << 20)
#define CG11                                (3 << 22)
#define CG12                                (3 << 24)
#define CG13                                (3 << 26)
#define CG14                                (3 << 28)
#define CG15                                (3 << 30)

#define IMXRT_CCM_CCGR0                     (0x400FC068)
#define CCGR0_GPIO2                         CG15
#define CCGR0_LPUART2                       CG14
#define CCGR0_GPT2_SERIAL                   CG13
#define CCGR0_GPT2_BUS                      CG12
#define CCGR0_TRACE                         CG11
#define CCGR0_CAN2_SERIAL                   CG10
#define CCGR0_CAN2                          CG9
#define CCGR0_CAN1_SERIAL                   CG8
#define CCGR0_CAN1                          CG7
#define CCGR0_LPUART3                       CG6
#define CCGR0_DCP                           CG5
#define CCGR0_SIM                           CG4
#define CCGR0_MQS                           CG2
#define CCGR0_AIPS_TZ2                      CG1
#define CCGR0_APIS_TZ1                      CG0

#define IMXRT_CCM_CCGR1                     (0x400FC06C)
#define CCGR1_GPIO5                         CG15
#define CCGR1_CSU                           CG14
#define CCGR1_GPIO1                         CG13
#define CCGR1_LPUART4                       CG12
#define CCGR1_GPT1_SERIAL                   CG11
#define CCGR1_GPT_BUS                       CG10
#define CCGR1_SEMC_EXSC                     CG9
#define CCGR1_ADC1                          CG8
#define CCGR1_AOI2                          CG7
#define CCGR1_PIT                           CG6
#define CCGR1_ENET                          CG5
#define CCGR1_ADC2                          CG4
#define CCGR1_LPSPI4                        CG3
#define CCGR1_LPSPI3                        CG2
#define CCGR1_LPSPI2                        CG1
#define CCGR1_LPSPI1                        CG0

#define IMXRT_CCM_CCGR2                     (0x400FC070)
#define CCGR2_PXP                           CG15
#define CCGR2_LCD                           CG14
#define CCGR2_GPIO3                         CG13
#define CCGR2_XBAR2                         CG12
#define CCGR2_XBAR1                         CG11
#define CCGR2_IPMUX3                        CG10
#define CCGR2_IPMUX2                        CG9
#define CCGR2_IPMUX1                        CG8
#define CCGR2_XBAR3                         CG7
#define CCGR2_OCOTP                         CG6
#define CCGR2_LPI2C3                        CG5
#define CCGR2_LPI2C2                        CG4
#define CCGR2_LPI2C1                        CG3
#define CCGR2_IOMUX_SNVS                    CG2
#define CCGR2_CSI                           CG1
#define CCGR2_OCRAM_EXSC                    CG0

#define IMXRT_CCM_CCGR3                     (0x400FC074)
#define CCGR3_IOMUXC_SNVS_GPR               CG15
#define CCGR3_OCRAM                         CG14
#define CCGR3_ACMP4                         CG13
#define CCGR3_ACMP3                         CG12
#define CCGR3_ACMP2                         CG11
#define CCGR3_ACMP1                         CG10
#define CCGR3_FLEXRAM                       CG9
#define CCGR3_WDOG1                         CG8
#define CCGR3_EWM                           CG7
#define CCGR3_GPIO4                         CG6
#define CCGR3_LCDIF_PIX                     CG5
#define CCGR3_AOI1                          CG4
#define CCGR3_LPUART6                       CG3
#define CCGR3_SEMC                          CG2
#define CCGR3_LPUART5                       CG1
#define CCGR3_FLEXIO2                       CG0

#define IMXRT_CCM_CCGR4                     (0x400FC078)
#define CCGR4_QDC4                          CG15
#define CCGR4_QDC3                          CG14
#define CCGR4_QDC2                          CG13
#define CCGR4_QDC1                          CG12
#define CCGR4_PWM4                          CG11
#define CCGR4_PWM3                          CG10
#define CCGR4_PWM2                          CG9
#define CCGR4_PWM1                          CG8
#define CCGR4_SIM_EMS                       CG7
#define CCGR4_SIM_M                         CG6
#define CCGR4_TSC_DIG                       CG5
#define CCGR4_SIM_M7                        CG4
#define CCGR4_BEE                           CG3
#define CCGR4_IOMUXC_GPR                    CG2
#define CCGR4_IOMUX                         CG1
#define CCGR4_SIM_M7_REG                    CG0

#define IMXRT_CCM_CCGR5                     (0x400FC07C)
#define CCGR5_SNVS_LP                       CG15
#define CCGR5_SNVS_HP                       CG14
#define CCGR5_LPUART7                       CG13
#define CCGR5_LPUART1                       CG12
#define CCGR5_SAI3                          CG11
#define CCGR5_SAI2                          CG10
#define CCGR5_SAI1                          CG9
#define CCGR5_SIM_MAIN                      CG8
#define CCGR5_SPDIF                         CG7
#define CCGR5_AIPSTZ4                       CG6
#define CCGR5_WDOG2                         CG5
#define CCGR5_KPP                           CG4
#define CCGR5_DMA                           CG3
#define CCGR5_WDOG3                         CG2
#define CCGR5_FLEXIO1                       CG1
#define CCGR5_ROM                           CG0

#define IMXRT_CCM_CCGR6                     (0x400FC080)
#define CCGR6_TIMER3                        CG15
#define CCGR6_TIMER2                        CG14
#define CCGR6_TIMER1                        CG13
#define CCGR6_LPI2C4                        CG12
#define CCGR6_ANADIG                        CG11
#define CCGR6_SIM_PER                       CG10
#define CCGR6_AIPS_TZ3                      CG9
#define CCGR6_TIMER4                        CG8
#define CCGR6_LPUART8                       CG7
#define CCGR6_TRNG                          CG6
#define CCGR6_FLEXSPI                       CG5
#define CCGR6_IPMUX4                        CG4
#define CCGR6_DCDC                          CG3
#define CCGR6_USDHC2                        CG2
#define CCGR6_USDHC1                        CG1
#define CCGR6_USBOH3                        CG0

#define IMXRT_CCM_CCGR7                     (0x400FC084)
#define CCGR7_FLEXIO3                       CG6
#define CCGR7_AIPS_LITE                     CG5
#define CCGR7_CAN3                          CG4
#define CCGR7_AXBS                          CG2
#define CCGR7_FLEXSPI2                      CG1
#define CCGR7_ENET2                         CG0

#define IMXRT_CCM_CMEOR                     (0x400FC088)

#define IMXRT_CCM_ANALOG_PLL_ARM            (0x400D8000)
#define ANALOG_PLL_ARM_LOCK                 BIT(31)
#define ANALOG_PLL_ARM_PLL_SEL              BIT(19)
#define ANALOG_PLL_ARM_BYPASS               BIT(16)
#define ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_ARM_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_ARM_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_ARM_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_ARM_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_ARM_ENABLE           BIT(13)
#define ANALOG_PLL_ARM_POWERDOWN        BIT(12)
#define ANALOG_PLL_ARM_DIV_SELECT_SHIFT (0)
#define ANALOG_PLL_ARM_DIV_SELECT_MASK  (0x7f << ANALOG_PLL_ARM_DIV_SELECT_SHIFT)
#define ANALOG_PLL_ARM_DIV_SELECT(x) \
    ((x << ANALOG_PLL_ARM_DIV_SELECT_SHIFT) & ANALOG_PLL_ARM_DIV_SELECT_MASK)

#define IMXRT_CCM_ANALOG_PLL_USB1           (0x400D8010)
#define IMXRT_CCM_ANALOG_PLL_USB2           (0x400D8020)
#define ANALOG_PLL_USB_LOCK                 BIT(31)
#define ANALOG_PLL_USB_BYPASS               BIT(16)
#define ANALOG_PLL_USB_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_USB_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_USB_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_USB_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_USB_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_USB_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_USB_ENABLE               BIT(13)
#define ANALOG_PLL_USB_POWER                BIT(12)
#define ANALOG_PLL_USB_EN_USB_CLKS          BIT(6)
#define ANALOG_PLL_USB_DIV_SELECT           BIT(1)

#define IMXRT_CCM_ANALOG_PLL_SYS            (0x400D8030)
#define ANALOG_PLL_SYS_LOCK                 BIT(31)
#define ANALOG_PLL_SYS_BYPASS               BIT(16)
#define ANALOG_PLL_SYS_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_SYS_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_SYS_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_SYS_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_SYS_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_SYS_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_SYS_ENABLE        BIT(13)
#define ANALOG_PLL_SYS_POWERDOWN     BIT(12)
#define ANALOG_PLL_SYS_DIV_SELECT    BIT(0)

#define IMXRT_CCM_ANALOG_PLL_SYS_SS  (0x400D8040)
#define ANALOG_PLL_SYS_SS_STOP_SHIFT (16)
#define ANALOG_PLL_SYS_SS_STOP_MASK  (0xffff << ANALOG_PLL_SYS_SS_STOP_SHIFT)
#define ANALOG_PLL_SYS_SS_STOP(x) \
    ((x << ANALOG_PLL_SYS_SS_STOP_SHIFT) & ANALOG_PLL_SYS_SS_STOP_MASK)
#define ANALOG_PLL_SYS_SS_ENABLE               BIT(15)
#define ANALOG_PLL_SYS_SS_STEP_SHIFT           (0)
#define ANALOG_PLL_SYS_SS_STEP_MASK            (0x7fff)
#define ANALOG_PLL_SYS_SS_STEP(x)              (x & 0x7fff)

#define IMXRT_CCM_ANALOG_PLL_SYS_NUM           (0x400D8050)

#define IMXRT_CCM_ANALOG_PLL_SYS_DENOM         (0x400D8060)

#define IMXRT_CCM_ANALOG_PLL_AUDIO             (0x400D8070)
#define ANALOG_PLL_AUDIO_LOCK                  BIT(31)
#define ANALOG_PLL_AUDIO_POST_DIV_SELECT_SHIFT (19)
#define ANALOG_PLL_AUDIO_POST_DIV_SELECT_MASK  (3 << ANALOG_PLL_AUDIO_POST_DIV_SELECT_SHIFT)
#define ANALOG_PLL_AUDIO_POST_DIV_SELECT(x) \
    ((x << ANALOG_PLL_AUDIO_POST_DIV_SELECT_SHIFT) & ANALOG_PLL_AUDIO_POST_DIV_SELECT_MASK)
#define ANALOG_PLL_AUDIO_BYPASS               BIT(16)
#define ANALOG_PLL_AUDIO_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_AUDIO_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_AUDIO_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_AUDIO_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_AUDIO_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_AUDIO_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_AUDIO_ENABLE           BIT(13)
#define ANALOG_PLL_AUDIO_POWERDOWN        BIT(12)
#define ANALOG_PLL_AUDIO_DIV_SELECT_SHIFT (0)
#define ANALOG_PLL_AUDIO_DIV_SELECT_MASK  (0x3f << ANALOG_PLL_AUDIO_DIV_SELECT_SHIFT)
#define ANALOG_PLL_AUDIO_DIV_SELECT(x) \
    ((x << ANALOG_PLL_AUDIO_DIV_SELECT_SHIFT) & ANALOG_PLL_AUDIO_DIV_SELECT_MASK)

#define IMXRT_CCM_ANALOG_PLL_AUDIO_NUM         (0x400D8080)

#define IMXRT_CCM_ANALOG_PLL_AUDIO_DENOM       (0x400D8090)

#define IMXRT_CCM_ANALOG_PLL_VIDEO             (0x400D80A0)
#define ANALOG_PLL_VIDEO_LOCK                  BIT(31)
#define ANALOG_PLL_VIDEO_POST_DIV_SELECT_SHIFT (19)
#define ANALOG_PLL_VIDEO_POST_DIV_SELECT_MASK  (3 << ANALOG_PLL_VIDEO_POST_DIV_SELECT_SHIFT)
#define ANALOG_PLL_VIDEO_POST_DIV_SELECT(x) \
    ((x << ANALOG_PLL_VIDEO_POST_DIV_SELECT_SHIFT) & ANALOG_PLL_VIDEO_POST_DIV_SELECT_MASK)
#define ANALOG_PLL_VIDEO_BYPASS               BIT(16)
#define ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_VIDEO_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_VIDEO_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_VIDEO_ENABLE           BIT(13)
#define ANALOG_PLL_VIDEO_POWERDOWN        BIT(12)
#define ANALOG_PLL_VIDEO_DIV_SELECT_SHIFT (0)
#define ANALOG_PLL_VIDEO_DIV_SELECT_MASK  (0x3f << ANALOG_PLL_VIDEO_DIV_SELECT_SHIFT)
#define ANALOG_PLL_VIDEO_DIV_SELECT(x) \
    ((x << ANALOG_PLL_VIDEO_DIV_SELECT_SHIFT) & ANALOG_PLL_VIDEO_DIV_SELECT_MASK)

#define IMXRT_CCM_ANALOG_PLL_VIDEO_NUM       (0x400D80B0)
#define IMXRT_CCM_ANALOG_PLL_VIDEO_DENOM     (0x400D80C0)

#define IMXRT_CCM_ANALOG_PLL_ENET            (0x400D80E0)
#define ANALOG_PLL_ENET_LOCK                 BIT(31)
#define ANALOG_PLL_ENET_ENET_25M_REF_EN      BIT(21)
#define ANALOG_PLL_ENET_ENET2_REF_EN         BIT(20)
#define ANALOG_PLL_ENET_BYPASS               BIT(16)
#define ANALOG_PLL_ENET_BYPASS_CLK_SRC_SHIFT (14)
#define ANALOG_PLL_ENET_BYPASS_CLK_SRC_MASK  (3 << ANALOG_PLL_ENET_BYPASS_CLK_SRC_SHIFT)
#define ANALOG_PLL_ENET_BYPASS_CLK_SRC(x) \
    ((x << ANALOG_PLL_ENET_BYPASS_CLK_SRC_SHIFT) & ANALOG_PLL_ENET_BYPASS_CLK_SRC_MASK)
#define ANALOG_PLL_ENET_ENABLE                 BIT(13)
#define ANALOG_PLL_ENET_POWERDOWN              BIT(12)
#define ANALOG_PLL_ENET_ENET2_DIV_SELECT_SHIFT (2)
#define ANALOG_PLL_ENET_ENET2_DIV_SELECT_MASK  (3 << ANALOG_PLL_ENET_ENET2_DIV_SELECT_SHIFT)
#define ANALOG_PLL_ENET_ENET2_DIV_SELECT(x) \
    ((x << ANALOG_PLL_ENET_ENET2_DIV_SELECT_SHIFT) & ANALOG_PLL_ENET_ENET2_DIV_SELECT_MASK)
#define ANALOG_PLL_ENET_DIV_SELECT_SHIFT (0)
#define ANALOG_PLL_ENET_DIV_SELECT_MASK  (0x03)
#define ANALOG_PLL_ENET_DIV_SELECT(x)    (x & ANALOG_PLL_ENET_DIV_SELECT_MASK)

#define IMXRT_CCM_ANALOG_PFD_480         (0x400D80F0)
#define IMXRT_CCM_ANALOG_PFD_528         (0x400D8100)
#define ANALOG_PFD_PFD3_CLKGATE          BIT(31)
#define ANALOG_PFD_PFD3_STABLE           BIT(30)
#define ANALOG_PFD_PFD3_FRAC_SHIFT       (24)
#define ANALOG_PFD_PFD3_FRAC_MASK        (0x3f << ANALOG_PFD_PFD3_FRAC_SHIFT)
#define ANALOG_PFD_PFD3_FRAC(x)          ((x << ANALOG_PFD_PFD3_FRAC_SHIFT) & ANALOG_PFD_PFD3_FRAC_MASK)
#define ANALOG_PFD_PFD2_CLKGATE          BIT(23)
#define ANALOG_PFD_PFD2_STABLE           BIT(22)
#define ANALOG_PFD_PFD2_FRAC_SHIFT       (16)
#define ANALOG_PFD_PFD2_FRAC_MASK        (0x3f << ANALOG_PFD_PFD2_FRAC_SHIFT)
#define ANALOG_PFD_PFD2_FRAC(x)          ((x << ANALOG_PFD_PFD2_FRAC_SHIFT) & ANALOG_PFD_PFD2_FRAC_MASK)
#define ANALOG_PFD_PFD1_CLKGATE          BIT(15)
#define ANALOG_PFD_PFD1_STABLE           BIT(14)
#define ANALOG_PFD_PFD1_PFD1_FRAC_SHIFT  (8)
#define ANALOG_PFD_PFD1_PFD1_FRAC_MASK   (0x3f << ANALOG_PFD_PFD1_PFD1_FRAC_SHIFT)
#define ANALOG_PFD_PFD1_PFD1_FRAC(x) \
    ((x << ANALOG_PFD_PFD1_PFD1_FRAC_SHIFT) & ANALOG_PFD_PFD1_PFD1_FRAC_MASK)
#define ANALOG_PFD_PFD0_CLKGATE          BIT(7)
#define ANALOG_PFD_PFD0_STABLE           BIT(6)
#define ANALOG_PFD_PFD0_FRAC_SHIFT       (0)
#define ANALOG_PFD_PFD0_FRAC_MASK        (0x3f)
#define ANALOG_PFD_PFD0_FRAC(x)          (x & 0x3f)

#define IMXRT_CCM_ANALOG_MISC0           (0x400D8150)
#define ANALOG_MISC0_XTAL_24M_PWD        BIT(30)
#define ANALOG_MISC0_RTC_XTAL_SOURCE     BIT(29)
#define ANALOG_MISC0_CLKGATE_DELAY_SHIFT (26)
#define ANALOG_MISC0_CLKGATE_DELAY_MASK  (7 << ANALOG_MISC0_CLKGATE_DELAY_SHIFT)
#define ANALOG_MISC0_CLKGATE_DELAY(x) \
    ((x << ANALOG_MISC0_CLKGATE_DELAY_SHIFT) & ANALOG_MISC0_CLKGATE_DELAY_MASK)
#define ANALOG_MISC0_CLKGATE_CTRL     BIT(25)
#define ANALOG_MISC0_OSC_XTALOK_EN    BIT(16)
#define ANALOG_MISC0_OSC_XTALOK       BIT(15)
#define ANALOG_MISC0_OSC_I_SHIFT      (13)
#define ANALOG_MISC0_OSC_I_MASK       (3 << ANALOG_MISC0_OSC_I_SHIFT)
#define ANALOG_MISC0_OSC_I(x)         ((x << ANALOG_MISC0_OSC_I_SHIFT) & ANALOG_MISC0_OSC_I_MASK)
#define ANALOG_MISC0_DISCON_HIGH_SNVS BIT(12)
#define ANALOG_MISC0_STOP_MODE_SHIFT  (10)
#define ANALOG_MISC0_STOP_MODE_MASK   (3 << ANALOG_MISC0_STOP_MODE_SHIFT)
#define ANALOG_MISC0_STOP_MODE_CONFIG(x) \
    ((x << ANALOG_MISC0_STOP_MODE_SHIFT) & ANALOG_MISC0_STOP_MODE_MASK)
#define ANALOG_MISC0_REFTOP_VBGUP        BIT(7)
#define ANALOG_MISC0_REFTOP_VBGADJ_SHIFT (4)
#define ANALOG_MISC0_REFTOP_VBGADJ_MASK  (7 << ANALOG_MISC0_REFTOP_VBGADJ_SHIFT)
#define ANALOG_MISC0_REFTOP_VBGADJ(x) \
    ((x << ANALOG_MISC0_REFTOP_VBGADJ_SHIFT) & ANALOG_MISC0_REFTOP_VBGADJ_MASK)
#define ANALOG_MISC0_REFTOP_SELFBIASOFF BIT(3)
#define ANALOG_MISC0_REFTOP_PWD         BIT(0)

#define IMXRT_CCM_ANALOG_MISC1          (0x400D8160)
#define IMXRT_CCM_ANALOG_MISC2          (0x400D8170)

#endif
