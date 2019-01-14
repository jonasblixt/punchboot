#ifndef __XHCI_H__
#define __XHCI_H__

#include <pb.h>

#define XHCI_CAPLENGTH   0x0000
#define XHCI_GSTS        0xC118
#define XHCI_DCFG        0xC700
#define XHCI_DCTL        0xC704
#define XHCI_GUSB2PHYCFG 0xC200
#define XHCI_GEVNTADRLO  0xC400
#define XHCI_GEVNTADRHI  0xC404
#define XHCI_GEVNTSIZ    0xC408
#define XHCI_GEVNTCOUNT  0xC40C
#define XHCI_DEVTEN      0xC708
#define XHCI_DEPCMD_0    0xC80C
#define XHCI_DEPCMD_1    0xC81C
#define XHCI_DEPCMD_2    0xC82C
#define XHCI_DEPCMD_3    0xC83C
#define XHCI_DEPCMD_4    0xC84C
#define XHCI_DEPCMD_5    0xC85C
#define XHCI_DEPCMD_6    0xC86C
#define XHCI_DEPCMD_7    0xC87C
#define XHCI_DALEPENA    0xC720
#define XHCI_GCTL        0xC110

/* Device Endpoint command */
#define XHCI_DEPCMD_SETEPCONF  1
#define XHCI_DEPCMD_SETTRANRE  2
#define XHCI_DEPCMD_GETEPSTATE 3
#define XHCI_DEPCMD_SETSTALL   4
#define XHCI_DEPCMD_CLRSTALL   5
#define XHCI_DEPCMD_STARTRANS  6
#define XHCI_DEPCMD_UPDSTRANS  7
#define XHCI_DEPCMD_ENDTRANS   8
#define XHCI_DEPCMD_STARTNEWC  9

#define XHCI_DEPCMD_ACT (1 << 10)

#define XHCI_DEPCMDPAR2_0 0x800
#define XHCI_DEPCMDPAR2_1 0x810
#define XHCI_DEPCMDPAR2_2 0x820
#define XHCI_DEPCMDPAR2_3 0x830
#define XHCI_DEPCMDPAR2_4 0x840
#define XHCI_DEPCMDPAR2_5 0x850
#define XHCI_DEPCMDPAR2_6 0x860
#define XHCI_DEPCMDPAR2_7 0x870

#define XHCI_DEPCMDPAR1_0 0x804
#define XHCI_DEPCMDPAR1_1 0x814
#define XHCI_DEPCMDPAR1_2 0x824
#define XHCI_DEPCMDPAR1_3 0x834
#define XHCI_DEPCMDPAR1_4 0x844
#define XHCI_DEPCMDPAR1_5 0x854
#define XHCI_DEPCMDPAR1_6 0x864
#define XHCI_DEPCMDPAR1_7 0x874

#define XHCI_DEPCMDPAR0_0 0x808
#define XHCI_DEPCMDPAR0_1 0x818
#define XHCI_DEPCMDPAR0_2 0x828
#define XHCI_DEPCMDPAR0_3 0x838
#define XHCI_DEPCMDPAR0_4 0x848
#define XHCI_DEPCMDPAR0_5 0x858
#define XHCI_DEPCMDPAR0_6 0x868
#define XHCI_DEPCMDPAR0_7 0x878


struct xhci_device
{
    __iomem base;
    uint32_t version;
    uint32_t caps;
};

struct xhci_trb
{
    uint32_t bptrl;
    uint32_t bptrh;
    uint32_t ssz;
    uint32_t control;
} __packed;

uint32_t xhci_init(struct xhci_device *dev);
void xhci_task(struct xhci_device *dev);
uint32_t xhci_transfer(struct xhci_device *dev, 
            uint8_t ep, uint8_t *bfr, uint32_t sz);
#endif
