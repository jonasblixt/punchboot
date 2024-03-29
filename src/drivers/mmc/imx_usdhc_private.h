#ifndef PLAT_IMX_USDHC_PRIVATE_H
#define PLAT_IMX_USDHC_PRIVATE_H

#define USDHC_MAKE_CMD(c, f)        (((c & 0xff) << 24) | ((f & 0xff) << 16))

/* USDHC controller register */
#define USDHC_DS_ADDR               0x0000
#define USDHC_BLK_ATT               0x0004
#define USDHC_CMD_ARG               0x0008
#define USDHC_CMD_XFR_TYP           0x000C
#define USDHC_CMD_RSP0              0x0010
#define USDHC_CMD_RSP1              0x0014
#define USDHC_CMD_RSP2              0x0018
#define USDHC_CMD_RSP3              0x001C
#define USDHC_DATA_BUFF_ACC_PORT    0x0020
#define USDHC_PRES_STATE            0x0024
#define USDHC_PROT_CTRL             0x0028
#define USDHC_SYSCTRL               0x002c
#define USDHC_INT_STATUS            0x0030
#define USDHC_INT_STATUS_EN         0x0034
#define USDHC_INT_SIGNAL_EN         0x0038
#define USDHC_AUTOCMD12_ERR_STATUS  0x003C
#define USDHC_HOST_CTRL_CAP         0x0040
#define USDHC_WTMK_LVL              0x0044
#define USDHC_MIX_CTRL              0x0048
#define USDHC_FORCE_EVENT           0x0050
#define USDHC_ADMA_ERR_STATUS       0x0054
#define USDHC_ADMA_SYS_ADDR         0x0058
#define USDHC_DLL_CTRL              0x0060
#define USDHC_DLL_STATUS            0x0064
#define USDHC_CLK_TUNE_CTRL_STATUS  0x0068
#define USDHC_VEND_SPEC             0x00C0
#define USDHC_MMC_BOOT              0x00C4
#define USDHC_VEND_SPEC2            0x00C8
#define USDHC_TUNING_CTRL           0x00CC

/* SYSCTRL Register bits */
#define USDHC_SYSCTRL_RSTD          BIT(26)
#define USDHC_SYSCTRL_RSTC          BIT(25)
#define USDHC_SYSCTRL_RSTA          BIT(24)
#define USDHC_SYSCTRL_INITA         BIT(27)
#define USDHC_SYSCTRL_RSTT          BIT(28)
#define USDHC_SYSCTRL_CLOCK_MASK    0x0000fff0
#define USDHC_SYSCTRL_TIMEOUT_MASK  0x000f0000
#define USDHC_SYSCTRL_TIMEOUT(x)    ((0xf & (x)) << 16)

/* MIX CTRL register */
#define USDHC_MIX_CTRL_FBCLK_SEL    (1 << 25)
#define USDHC_MIX_CTRL_AUTO_TUNE_EN (1 << 24)
#define USDHC_MIX_CTRL_SMPCLK_SEL   (1 << 23)
#define USDHC_MIX_CTRL_EXE_TUNE     (1 << 22)

/* Watermark register */
#define WMKLV_RD_MASK               0xff
#define WMKLV_WR_MASK               0x00ff0000
#define WMKLV_MASK                  (WMKLV_RD_MASK | WMKLV_WR_MASK)

#define USDHC_SMPCLK_SEL            (1 << 23)
#define USDHC_EXE_TUNE              (1 << 22)

#define VENDSPEC_RSRV1              BIT(29)
#define VENDSPEC_CARD_CLKEN         BIT(14)
#define VENDSPEC_PER_CLKEN          BIT(13)
#define VENDSPEC_AHB_CLKEN          BIT(12)
#define VENDSPEC_IPG_CLKEN          BIT(11)
#define VENDSPEC_AC12_CHKBUSY       BIT(3)
#define VENDSPEC_EXTDMA             BIT(0)
#define VENDSPEC_INIT                                                                 \
    (VENDSPEC_RSRV1 | VENDSPEC_CARD_CLKEN | VENDSPEC_PER_CLKEN | VENDSPEC_AHB_CLKEN | \
     VENDSPEC_IPG_CLKEN | VENDSPEC_AC12_CHKBUSY | VENDSPEC_EXTDMA)

/* Interrupt status register */
#define INTSTAT_DMAE   BIT(28)
#define INTSTAT_DEBE   BIT(22)
#define INTSTAT_DCE    BIT(21)
#define INTSTAT_DTOE   BIT(20)
#define INTSTAT_CIE    BIT(19)
#define INTSTAT_CEBE   BIT(18)
#define INTSTAT_CCE    BIT(17)
#define INTSTAT_DINT   BIT(3)
#define INTSTAT_BGE    BIT(2)
#define INTSTAT_TC     BIT(1)
#define INTSTAT_CC     BIT(0)
#define CMD_ERR        (INTSTAT_CIE | INTSTAT_CEBE | INTSTAT_CCE)
#define DATA_ERR       (INTSTAT_DMAE | INTSTAT_DEBE | INTSTAT_DCE | INTSTAT_DTOE)
#define DATA_COMPLETE  (INTSTAT_DINT | INTSTAT_TC)

/* Interrupt status enable register */
#define INTSTATEN_DEBE BIT(22)
#define INTSTATEN_DCE  BIT(21)
#define INTSTATEN_DTOE BIT(20)
#define INTSTATEN_CIE  BIT(19)
#define INTSTATEN_CEBE BIT(18)
#define INTSTATEN_CCE  BIT(17)
#define INTSTATEN_CTOE BIT(16)
#define INTSTATEN_CINT BIT(8)
#define INTSTATEN_BRR  BIT(5)
#define INTSTATEN_BWR  BIT(4)
#define INTSTATEN_DINT BIT(3)
#define INTSTATEN_TC   BIT(1)
#define INTSTATEN_CC   BIT(0)
#define EMMC_INTSTATEN_BITS                                                             \
    (INTSTATEN_CC | INTSTATEN_TC | INTSTATEN_DINT | INTSTATEN_BWR | INTSTATEN_BRR |     \
     INTSTATEN_CINT | INTSTATEN_CTOE | INTSTATEN_CCE | INTSTATEN_CEBE | INTSTATEN_CIE | \
     INTSTATEN_DTOE | INTSTATEN_DCE | INTSTATEN_DEBE)
#define INTSIGEN                0x038

#define XFERTYPE                0x00c
#define XFERTYPE_CMD(x)         (((x) & 0x3f) << 24)
#define XFERTYPE_CMDTYP_ABORT   (3 << 22)
#define XFERTYPE_DPSEL          BIT(21)
#define XFERTYPE_CICEN          BIT(20)
#define XFERTYPE_CCCEN          BIT(19)
#define XFERTYPE_RSPTYP_136     BIT(16)
#define XFERTYPE_RSPTYP_48      BIT(17)
#define XFERTYPE_RSPTYP_48_BUSY (BIT(16) | BIT(17))

#define MIXCTRL_MSBSEL          BIT(5)
#define MIXCTRL_DTDSEL          BIT(4)
#define MIXCTRL_DDREN           BIT(3)
#define MIXCTRL_AC12EN          BIT(2)
#define MIXCTRL_BCEN            BIT(1)
#define MIXCTRL_DMAEN           BIT(0)
#define MIXCTRL_DATMASK         0x7f

#define PSTATE                  0x024
#define PSTATE_DAT0             BIT(24)
#define PSTATE_DLA              BIT(2)
#define PSTATE_CDIHB            BIT(1)
#define PSTATE_CIHB             BIT(0)
#define PROCTL_INIT             0x00000020
#define PROCTL_DTW_4            0x00000002
#define PROCTL_DTW_8            0x00000004

/* Prot ctl register bits */
#define PROTCTRL_LE             BIT(5)
#define PROTCTRL_WIDTH_4        BIT(1)
#define PROTCTRL_WIDTH_8        BIT(2)
#define PROTCTRL_WIDTH_MASK     0x6

#define USDHC_INT_RESPONSE      0x00000001
#define USDHC_INT_DATA_END      0x00000002
#define USDHC_INT_DMA_END       0x00000008
#define USDHC_INT_SPACE_AVAIL   0x00000010
#define USDHC_INT_DATA_AVAIL    0x00000020
#define USDHC_INT_CARD_INSERT   0x00000040
#define USDHC_INT_CARD_REMOVE   0x00000080
#define USDHC_INT_CARD_INT      0x00000100
#define USDHC_INT_ERROR         0x00008000
#define USDHC_INT_TIMEOUT       0x00010000
#define USDHC_INT_CRC           0x00020000
#define USDHC_INT_END_BIT       0x00040000
#define USDHC_INT_INDEX         0x00080000
#define USDHC_INT_DATA_TIMEOUT  0x00100000
#define USDHC_INT_DATA_CRC      0x00200000
#define USDHC_INT_DATA_END_BIT  0x00400000
#define USDHC_INT_BUS_POWER     0x00800000
#define USDHC_INT_ACMD12ERR     0x01000000
#define USDHC_INT_DMA_ERROR     0x10000000

struct usdhc_adma2_desc {
    uint16_t cmd;
    uint16_t len;
    uint32_t addr;
} __attribute__((packed));

#define ADMA2_TRAN_VALID         0x21
#define ADMA2_NOP_END_VALID      0x3
#define ADMA2_END                0x2
#define ADMA2_MAX_BYTES_PER_DESC 65024
#endif
