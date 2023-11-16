#ifndef DRIVER_MEMC_IMX_FLEXSPI_PRIVATE_H
#define DRIVER_MEMC_IMX_FLEXSPI_PRIVATE_H


#define FLEXSPI_MCR0            0x0000
#define FLEXSPI_MCR1            0x0004
#define FLEXSPI_MCR2            0x0008
#define FLEXSPI_AHBCR           0x000C

#define FLEXSPI_INTR            (0x0014)
#define FLEXSPI_INTR_AHBCMDERR_MASK              (0x10U)
#define FLEXSPI_INTR_IPCMDERR_MASK               (0x8U)
#define FLEXSPI_INTR_AHBCMDGE_MASK               (0x4U)
#define FLEXSPI_INTR_IPCMDGE_MASK                (0x2U)
#define FLEXSPI_INT_IPTXWEEN                     (0x40U)
#define FLEXSPI_INT_IPRXWAEN                     (0x20U)
#define FLEXSPI_INT_SEQTIMEOUTEN                 (0x800U)
#define FLEXSPI_INT_IPCMDERREN                   (0x8U)
#define FLEXSPI_INT_IPCMDGEEN                    (0x2U)

#define FLEXSPI_AHBRXBUFCR0(n)  (0x0020 + (n*4))
#define FLEXSPI_FLSHCR0(n)      (0x0060 + (n*4))

#define FLEXSPI_IPCR0           (0x00A0)

/* IPCR1 */
#define FLEXSPI_IPCR1           (0x00A4)
#define FLEXSPI_IPCR1_IDATSZ_MASK                (0xFFFFU)
#define FLEXSPI_IPCR1_IDATSZ_SHIFT               (0U)
#define FLEXSPI_IPCR1_IDATSZ(x)                  (((uint32_t)(((uint32_t)(x)) << FLEXSPI_IPCR1_IDATSZ_SHIFT)) & FLEXSPI_IPCR1_IDATSZ_MASK)
#define FLEXSPI_IPCR1_ISEQID_MASK                (0xF0000U)
#define FLEXSPI_IPCR1_ISEQID_SHIFT               (16U)
#define FLEXSPI_IPCR1_ISEQID(x)                  (((uint32_t)(((uint32_t)(x)) << FLEXSPI_IPCR1_ISEQID_SHIFT)) & FLEXSPI_IPCR1_ISEQID_MASK)
#define FLEXSPI_IPCR1_ISEQNUM_MASK               (0x7000000U)
#define FLEXSPI_IPCR1_ISEQNUM_SHIFT              (24U)
#define FLEXSPI_IPCR1_ISEQNUM(x)                 (((uint32_t)(((uint32_t)(x)) << FLEXSPI_IPCR1_ISEQNUM_SHIFT)) & FLEXSPI_IPCR1_ISEQNUM_MASK)
#define FLEXSPI_IPCR1_IPAREN_MASK                (0x80000000U)
#define FLEXSPI_IPCR1_IPAREN_SHIFT               (31U)

/* IPCMD */
#define FLEXSPI_IPCMD           (0x00B0)
#define FLEXSPI_IPCMD_TRG_MASK                   (0x1U)

/* IPTXFCR */
#define FLEXSPI_IPTXFCR         (0x00BC)
#define FLEXSPI_IPTXFCR_CLRIPTXF_MASK            (0x1U)
#define FLEXSPI_IPTXFCR_TXDMAEN(x)               (((uint32_t)(((uint32_t)(x)) << FLEXSPI_IPTXFCR_TXDMAEN_SHIFT)) & FLEXSPI_IPTXFCR_TXDMAEN_MASK)
#define FLEXSPI_IPTXFCR_TXWMRK_MASK              (0x3CU)
#define FLEXSPI_IPTXFCR_TXWMRK_SHIFT             (2U)
#define FLEXSPI_IPTXFCR_TXWMRK(n)       (((n)/ 8 ) - 1)

/* IPRXFCR */
#define FLEXSPI_IPRXFCR         0x00B8
#define FLEXSPI_IPRXFCR_CLRIPRXF_MASK            (0x1U)
#define FLEXSPI_IPRXFCR_RXWMRK_MASK     (0xf << 2)
#define FLEXSPI_IPRXFCR_RXWMRK(n)       (((n)/ 8 ) - 1)
#define FLEXSPI_IPRXFCR_RXWMRK_SHIFT             (2U)

/* STS0 */
#define FLEXSPI_STS0            0x00E0

/* IPRXFSTS */
#define FLEXSPI_IPRXFSTS        (0x00F0)
#define FLEXSPI_IPRXFSTS_FILL_MASK               (0xFFU)
#define FLEXSPI_IPRXFSTS_FILL_SHIFT              (0U)

/* IPTXFSTS */
#define FLEXSPI_IPTXFSTS        (0x00F4)
#define FLEXSPI_IPTXFSTS_FILL_MASK               (0xFFU)
#define FLEXSPI_IPTXFSTS_FILL_SHIFT              (0U)

/* RFDR */
#define FLEXSPI_RFDR0           (0x0100)

/* TFDR */
#define FLEXSPI_TFDR0           (0x0180)

/* MCR0 register bits */
#define FLEXSPI_MCR0_SWRESET    BIT(0)
#define FLEXSPI_MCR0_MDIS       BIT(1)

/* MCR1 register bits */

/* MCR2 register bits */
#define FLEXSPI_MCR2_RESUMEWAIT(n)      ((n) << 24)
#define FLEXSPI_MCR2_RESUMEWAIT_MASK    (FLEXSPI_MCR2_RESUMEWAIT(0xff))
#define FLEXSPI_MCR2_SCKBDIFFOPT        BIT(19)
#define FLEXSPI_MCR2_SAMEDEVICEEN       BIT(15)
#define FLEXSPI_MCR2_CLRAHBBUFOPT       BIT(11)

/* AHBCR register bits */
#define FLEXSPI_AHBCR_APAREN            BIT(0)
#define FLEXSPI_AHBCR_CACHABLEEN        BIT(3)
#define FLEXSPI_AHBCR_BUFFERABLEEN      BIT(4)
#define FLEXSPI_AHBCR_PREFETCHEN        BIT(5)
#define FLEXSPI_AHBCR_READADDROPT       BIT(6)

/* AHBRXBUFCR0 register bits */
#define FLEXSPI_AHBRXBUFCR0_BUFSZ(sz)   (((sz) / 8) << 0)


/* STS0 register bits */
#define FLEXSPI_STS0_SEQIDLE            BIT(0)
#define FLEXSPI_STS0_ARBIDLE            BIT(1)

/* Flash Control Register 2 */
#define FLEXSPI_FLSHA1CR2               (0x0080)
#define FLEXSPI_FLSHA2CR2               (0x0084)
#define FLEXSPI_FLSHB1CR2               (0x0088)
#define FLEXSPI_FLSHB2CR2               (0x008C)

#define FLEXSPI_FLSHCR2_CLRINSTRPTR_MASK         (0x80000000U)


#endif
