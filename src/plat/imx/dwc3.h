#ifndef __DWC3_H__
#define __DWC3_H__

#include <pb.h>
#include <usb.h>

#define DWC3_CAPLENGTH   0x0000
#define DWC3_GSTS        0xC118
#define DWC3_DCFG        0xC700
#define DWC3_DCTL        0xC704
#define DWC3_DSTS        0xC70C
#define DWC3_GUSB2PHYCFG 0xC200
#define DWC3_GUSB3PIPECTL 0xC2C0
#define DWC3_GEVNTADRLO  0xC400
#define DWC3_GEVNTADRHI  0xC404
#define DWC3_GEVNTSIZ    0xC408
#define DWC3_GEVNTCOUNT  0xC40C
#define DWC3_DEVTEN      0xC708
#define DWC3_DEPCMD_0    0xC80C
#define DWC3_DEPCMD_1    0xC81C
#define DWC3_DEPCMD_2    0xC82C
#define DWC3_DEPCMD_3    0xC83C
#define DWC3_DEPCMD_4    0xC84C
#define DWC3_DEPCMD_5    0xC85C
#define DWC3_DEPCMD_6    0xC86C
#define DWC3_DEPCMD_7    0xC87C
#define DWC3_DALEPENA    0xC720
#define DWC3_GCTL        0xC110
#define DWC3_GTXFIFOSIZ_0 0xC300
#define DWC3_GRXFIFOSIZ_0 0xC380


/* Device Endpoint command */
#define DWC3_DEPCMD_SETEPCONF  1
#define DWC3_DEPCMD_SETTRANRE  2
#define DWC3_DEPCMD_GETEPSTATE 3
#define DWC3_DEPCMD_SETSTALL   4
#define DWC3_DEPCMD_CLRSTALL   5
#define DWC3_DEPCMD_STARTRANS  6
#define DWC3_DEPCMD_UPDSTRANS  7
#define DWC3_DEPCMD_ENDTRANS   8
#define DWC3_DEPCMD_STARTNEWC  9

#define DWC3_DEPCMD_ACT (1 << 10)

#define DWC3_DEPCMDPAR2_0 0xC800
#define DWC3_DEPCMDPAR2_1 0xC810
#define DWC3_DEPCMDPAR2_2 0xC820
#define DWC3_DEPCMDPAR2_3 0xC830
#define DWC3_DEPCMDPAR2_4 0xC840
#define DWC3_DEPCMDPAR2_5 0xC850
#define DWC3_DEPCMDPAR2_6 0xC860
#define DWC3_DEPCMDPAR2_7 0xC870

#define DWC3_DEPCMDPAR1_0 0xC804
#define DWC3_DEPCMDPAR1_1 0xC814
#define DWC3_DEPCMDPAR1_2 0xC824
#define DWC3_DEPCMDPAR1_3 0xC834
#define DWC3_DEPCMDPAR1_4 0xC844
#define DWC3_DEPCMDPAR1_5 0xC854
#define DWC3_DEPCMDPAR1_6 0xC864
#define DWC3_DEPCMDPAR1_7 0xC874

#define DWC3_DEPCMDPAR0_0 0xC808
#define DWC3_DEPCMDPAR0_1 0xC818
#define DWC3_DEPCMDPAR0_2 0xC828
#define DWC3_DEPCMDPAR0_3 0xC838
#define DWC3_DEPCMDPAR0_4 0xC848
#define DWC3_DEPCMDPAR0_5 0xC858
#define DWC3_DEPCMDPAR0_6 0xC868
#define DWC3_DEPCMDPAR0_7 0xC878

#define DWC3_GCTL_CORESOFTRESET			(1 << 11)
#define DWC3_GCTL_SCALEDOWN(n)			((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK		DWC3_GCTL_SCALEDOWN(3)

struct dwc3_device
{
    __iomem base;
    uint32_t version;
    uint32_t caps;
};

struct dwc3_trb
{
    uint32_t bptrl;
    uint32_t bptrh;
    uint32_t ssz;
    uint32_t control;
} __packed;

uint32_t dwc3_init(struct dwc3_device *dev);
void dwc3_task(struct usb_device *dev);
uint32_t dwc3_transfer(struct dwc3_device *dev, 
            uint8_t ep, uint8_t *bfr, uint32_t sz);
void dwc3_set_addr(struct dwc3_device *dev, uint32_t addr);
void dwc3_wait_for_ep_completion(struct dwc3_device *dev, uint32_t ep);
void dwc3_set_configuration(struct usb_device *dev);

#endif
