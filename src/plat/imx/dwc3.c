#include <pb.h>
#include <io.h>
#include <usb.h>
#include <string.h>
#include <plat/imx/dwc3.h>
#include <plat.h>
#include <tinyprintf.h>

#define DWC3_EV_BUFFER_SIZE 128
#define DWC3_DEF_TIMEOUT_ms 10
#define DWC3_NO_TRBS 128

static volatile __a4k __no_bss uint32_t _ev_buffer[DWC3_EV_BUFFER_SIZE];
static volatile uint32_t _ev_index = 0;
static volatile __a4k __no_bss struct usb_setup_packet setup_pkt;
static volatile __a4k __no_bss struct dwc3_trb trbs[DWC3_NO_TRBS];
static __no_bss struct dwc3_trb *act_trb[8];
__no_bss uint8_t cmd_in_bfr[64];

static uint32_t dwc3_command(struct dwc3_device *dev,
                         uint8_t ep,
                         uint32_t cmd,
                         uint32_t p0,
                         uint32_t p1,
                         uint32_t p2)
{

    if (ep > 7)
        return PB_ERR;


    uint32_t param0_addr = DWC3_DEPCMDPAR0_0 + 0x10*ep;
    uint32_t param1_addr = DWC3_DEPCMDPAR1_0 + 0x10*ep;
    uint32_t param2_addr = DWC3_DEPCMDPAR2_0 + 0x10*ep;

    pb_write32(p0, dev->base + param0_addr);
    pb_write32(p1, dev->base + param1_addr);
    pb_write32(p2, dev->base + param2_addr);

    pb_write32(DWC3_DEPCMD_ACT | cmd ,
            dev->base + DWC3_DEPCMD_0 + 0x10*ep);
    

    volatile uint32_t timeout = plat_get_us_tick();

    while (pb_read32(dev->base + DWC3_DEPCMD_0 + 0x10*ep) & DWC3_DEPCMD_ACT)
    {
        if ((plat_get_us_tick() - timeout) > (DWC3_DEF_TIMEOUT_ms*1000))
        {
            uint32_t ev_status = pb_read32(dev->base + DWC3_DEPCMD_0 + 0x10*ep);
            LOG_ERR("CMD %2.2X, Timeout, status = 0x%1X",cmd, (ev_status >> 12) & 0xF);
            return PB_TIMEOUT;
        }
    }
    


    return PB_OK;
}

static uint32_t dwc3_config_ep(struct dwc3_device *dev,
                               uint8_t ep,
                               uint32_t sz,
                               uint32_t type)
{
    uint32_t err;

    uint8_t ep2 = ep & 0xFE;
    uint32_t in = ep&1;

    err = dwc3_command(dev, ep, DWC3_DEPCMD_SETEPCONF, 
                    (sz << 3) | (1 << 22) | (type << 1),
                    (ep << 25) | (7 << 8),
                    0);

    if (err != PB_OK)
        return err;

    err = dwc3_command(dev, ep, DWC3_DEPCMD_SETTRANRE, 1,0,0);

    if (err != PB_OK)
        return err;

    
    return PB_OK;
}

static struct dwc3_trb * dwc3_get_next_trb(void)
{
    struct dwc3_trb *trb = &trbs[0];

    for (uint32_t n = 0; n < DWC3_NO_TRBS; n++)
    {
        if ((trb->control & 1) == 0)
            return trb;
        trb++;
    }

    return NULL;
}

uint32_t dwc3_transfer(struct dwc3_device *dev, 
            uint8_t ep, uint8_t *bfr, uint32_t sz)
{
    struct dwc3_trb *trb = dwc3_get_next_trb();
    uint32_t xfr_sz = sz;

    if (trb == NULL)
    {
        LOG_ERR("Could not get free TRB");
        return PB_ERR;
    }

    if ( (sz < 64) && !(ep&1))
        xfr_sz = 64;

    act_trb[ep] = trb;

    trb->bptrh = 0;
    trb->bptrl = ptr_to_u32(bfr);
    trb->ssz = xfr_sz;
    trb->control = (1 << 11) | (1 << 1) | 1;

    switch(ep)
    {
        case USB_EP0_OUT:
            trb->control |= (1 << 10);
            trb->control |= (2 << 4);
        break;
        case USB_EP0_IN:
            trb->control |= (1 << 10);
            trb->control |= (1 << 4);
        break;
        default:
            trb->control |= (1 << 4);
    }

    dwc3_command(dev, ep, DWC3_DEPCMD_STARTRANS,0,ptr_to_u32(trb),0);

    LOG_DBG("trx EP%u %s, %8.8X, sz %ubytes",(ep>>1), (ep&1?"IN":"OUT"),
                    bfr, sz);
    return PB_OK;
}


static void dwc3_reset(struct dwc3_device *dev)
{
    uint32_t reg;

    /* Read DCFG and mask out Device address*/
    reg = pb_read32(dev->base + DWC3_DCFG);
    reg &= ~0x3F8;
    pb_write32(reg, dev->base + DWC3_DCFG);


    dwc3_transfer(dev, USB_EP0_OUT, &setup_pkt, 
                        sizeof(struct usb_setup_packet));
}

uint32_t dwc3_init(struct dwc3_device *dev)
{
    uint32_t err;

    dev->version = (pb_read32(dev->base + DWC3_CAPLENGTH) >> 16) & 0xFFFF;
    dev->caps = pb_read32(dev->base + DWC3_CAPLENGTH) & 0xFF;
    memset(_ev_buffer, 0, DWC3_EV_BUFFER_SIZE*4);
    memset(trbs, 0, sizeof(struct dwc3_trb) * DWC3_NO_TRBS);

    for (uint32_t n = 0; n < 8; n++)
        act_trb[n] = NULL;

    LOG_INFO("HCI ver: 0x%4.4X, caps: 0x%2.2X",dev->version, dev->caps);

    /* Put core in reset */
    pb_setbit32(1<<11, dev->base + DWC3_GCTL);
    /* Reset USB3 PHY */
    pb_setbit32(1 << 31, dev->base + DWC3_GUSB3PIPECTL);
    /* Reset USB2 PHY */
    pb_setbit32(1 << 31, dev->base + DWC3_GUSB2PHYCFG);
    
    plat_delay_ms(100);
    
    /* Release resets */
    pb_clrbit32(1 << 31, dev->base + DWC3_GUSB3PIPECTL);
    pb_clrbit32(1 << 31, dev->base + DWC3_GUSB2PHYCFG);

    plat_delay_ms(100);

    pb_clrbit32(1<<11, dev->base + DWC3_GCTL);

    pb_clrbit32(1<<6, dev->base + DWC3_GUSB2PHYCFG);

    /* Reset usb controller */
    pb_setbit32(1<<30, dev->base + DWC3_DCTL);

    while (pb_read32(dev->base + DWC3_DCTL) & (1<<30))
        asm("nop");

    pb_clrbit32(DWC3_GCTL_SCALEDOWN_MASK, dev->base + DWC3_GCTL);
    pb_clrbit32(1<<17,dev->base + DWC3_GCTL);
    pb_write32(ptr_to_u32(_ev_buffer), dev->base + DWC3_GEVNTADRLO);

    pb_write32(0x82, dev->base + DWC3_GTXFIFOSIZ_0);
    pb_write32(0x305, dev->base + DWC3_GRXFIFOSIZ_0);

    /* Configure event buffer */
    pb_write32(0 , dev->base + DWC3_GEVNTADRHI);
    pb_write32(DWC3_EV_BUFFER_SIZE*4, dev->base + DWC3_GEVNTSIZ);
    pb_write32(0, dev->base + DWC3_GEVNTCOUNT);
    
    pb_setbit32( (2 << 12) | 1, dev->base + DWC3_GCTL);
    pb_setbit32( (1 << 11), dev->base + DWC3_DCFG);

    pb_write32(0xF |(1 << 9),dev->base + DWC3_DEVTEN);

    err = dwc3_command(dev, 0, DWC3_DEPCMD_STARTNEWC, 0,0,0);

    if (err != PB_OK)
        return err;

    /* Perform EP0 out configuration */
    err = dwc3_config_ep(dev, 0, 64, 0);

    if (err != PB_OK)
        return err;

    err = dwc3_config_ep(dev, 1, 64, 0);

    if (err != PB_OK)
        return err;


    dwc3_reset(dev);

    pb_write32(3, dev->base + DWC3_DALEPENA);
    pb_setbit32(1<<31, dev->base + DWC3_DCTL);

    LOG_INFO("Done");
    return PB_OK;
}




void dwc3_set_addr(struct dwc3_device *dev, uint32_t addr)
{
    volatile uint32_t reg = pb_read32(dev->base + DWC3_DCFG);

    reg = (addr << 3) | (64 << 17) | (1 << 11);
    pb_write32(reg, dev->base + DWC3_DCFG);
}

void dwc3_wait_for_ep_completion(struct dwc3_device *dev, uint32_t ep)
{
    volatile struct dwc3_trb *trb = act_trb[ep];

    uint32_t t = plat_get_us_tick();

    while(trb->control & 1)
    {
        if ((plat_get_us_tick()-t) > 1000000)
        {
            LOG_ERR("Timeout EP%u %s ssz: 0x%8.8X 0x%8.8X",
                ep>>1, ep&1?"IN":"OUT",trb->ssz, trb->control);
            break;
        }
    }
}


void dwc3_set_configuration(struct usb_device *dev)
{
    struct dwc3_device *pdev = (struct dwc3_device *) dev->platform_data;
    LOG_INFO("Set config");
    
    dwc3_command(pdev, 0, DWC3_DEPCMD_STARTNEWC|(2<<16), 0,0,0);
    dwc3_config_ep(pdev, USB_EP1_OUT, 512, 2);
    dwc3_config_ep(pdev, USB_EP2_OUT, 64, 3);
    dwc3_config_ep(pdev, USB_EP3_IN, 512, 3);

    pb_setbit32((1 << USB_EP2_OUT) | 
                (1 << USB_EP1_OUT) | 
                (1 << USB_EP3_IN), pdev->base + DWC3_DALEPENA);
    
    dwc3_transfer(pdev, USB_EP2_OUT, (uint8_t *) cmd_in_bfr, 
                        64);

}

static bool dwc3_trb_hwo(struct dwc3_trb *trb)
{
    if (trb == NULL)
        return false;

    if (trb->control & 1)
        return false;

    return true;
}

void dwc3_task(struct usb_device *dev)
{
    struct dwc3_device *pdev = 
            (struct dwc3_device *) dev->platform_data;

    volatile uint32_t evcnt = 
            (pb_read32(pdev->base + DWC3_GEVNTCOUNT) & 0xFFFF);
    volatile uint32_t ev;


    uint32_t sts = pb_read32(pdev->base + DWC3_GSTS);

    if (evcnt > 4)
    {
        ev = _ev_buffer[_ev_index];

        /* Device Specific Events DEVT*/
        if (ev & 1)
        {
            uint8_t ev_dev = (ev >> 1) & 0x7F;
            uint8_t ev_type = (ev >> 8) & 0xF;
            uint16_t ev_info = (ev >> 16) & 0x1FF;

            switch (ev_type)
            {
                case 3: /* USB/Link state change */
                {
                    LOG_INFO("Link state change 0x%8.8X",ev);

                }
                break;
                case 2: /* Connection done */
                {
                    pb_write32(1 << 17, pdev->base + DWC3_GUSB3PIPECTL);
                    LOG_INFO("Connection done 0x%8.8X",ev);
                }
                break;
                case 1: /* USB Reset*/
                {
                    LOG_INFO("USB Reset %u 0x%8.8X", _ev_index, ev);
                    //dwc3_reset(pdev);
                }
                break;
                default:
                    LOG_ERR("Unknown event %2.2X", ev_type);
            }
        } else  { /* Device Endpoint-n events*/
            uint32_t ep = (ev >> 1) & 0x1F;
            uint32_t ev_param = (ev >> 16) & 0xFFFF;
            uint32_t ev_status = (ev >> 12) & 0xF;
            uint32_t ev_cc = (ev >> 6) & 0xF;
            //LOG_INFO("EV EP%u %s, param: %4.4X, sts: %1X, cc: %1X",
            //        ep>>1,ep&1?"IN":"OUT", ev_param, ev_status, ev_cc);
            
        }


        _ev_index = (_ev_index + 1) % DWC3_EV_BUFFER_SIZE;
        pb_write32(4, pdev->base + DWC3_GEVNTCOUNT);
    }

    
    if (dwc3_trb_hwo(act_trb[USB_EP0_OUT]))
    {
        dev->on_setup_pkt(dev, &setup_pkt);
        dwc3_transfer(pdev, USB_EP0_OUT, &setup_pkt, 
                        sizeof(struct usb_setup_packet));
    }

    if (dwc3_trb_hwo(act_trb[USB_EP2_OUT]))
    {
        struct usb_pb_command *cmd = (struct usb_pb_command *) cmd_in_bfr;
        dev->on_command(dev, cmd);
        dwc3_transfer(pdev, USB_EP2_OUT, (uint8_t *) cmd_in_bfr, 64);
    }
}
