#include <pb.h>
#include <io.h>
#include <usb.h>
#include <plat/imx/xhci.h>

#define XHCI_EV_BUFFER_SIZE 32
#define XHCI_DEF_TIMEOUT_ms 10

static uint32_t _ev_buffer[XHCI_EV_BUFFER_SIZE];
static uint32_t _ev_index = 0;
static __a16b struct usb_setup_packet setup_pkt;
static __a16b struct xhci_trb setup_trb;

static uint32_t xhci_command(struct xhci_device *dev,
                         uint8_t ep,
                         uint8_t cmd,
                         uint32_t p0,
                         uint32_t p1,
                         uint32_t p2)
{

    if (ep > 7)
        return PB_ERR;

    if (cmd > 0xF)
        return PB_ERR;

    uint32_t param0_addr = XHCI_DEPCMDPAR0_0 + 0x10*ep;
    uint32_t param1_addr = XHCI_DEPCMDPAR1_0 + 0x10*ep;
    uint32_t param2_addr = XHCI_DEPCMDPAR2_0 + 0x10*ep;


    pb_write32(p0, dev->base + param0_addr);
    pb_write32(p1, dev->base + param1_addr);
    pb_write32(p2, dev->base + param2_addr);

    pb_write32(XHCI_DEPCMD_ACT | (cmd & 0xF) ,
            dev->base + XHCI_DEPCMD_0 + 0x10*ep);
    

    volatile uint32_t timeout = plat_get_us_tick();

    while (pb_read32(dev->base + XHCI_DEPCMD_0 + 0x10*ep) & XHCI_DEPCMD_ACT)
    {
        LOG_DBG("%u", plat_get_us_tick());
        if ((plat_get_us_tick() - timeout) > (XHCI_DEF_TIMEOUT_ms))
        {
            uint32_t ev_status = pb_read32(dev->base + XHCI_DEPCMD_0 + 0x10*ep);
            LOG_ERR("Timeout, status = 0x%1X", (ev_status >> 12) & 0xF);
            return PB_TIMEOUT;
        }
    }
    
    return PB_OK;
}

static void xhci_reset(struct xhci_device *dev)
{
    uint32_t reg;

    /* Read DCFG and mask out Device address*/
    reg = pb_read32(dev->base + XHCI_DCFG) & 0x3F8;
    pb_write32(reg, dev->base + XHCI_DCFG);

}

uint32_t xhci_init(struct xhci_device *dev)
{
    uint32_t err;

    dev->version = (pb_read32(dev->base + XHCI_CAPLENGTH) >> 16) & 0xFFFF;
    dev->caps = pb_read32(dev->base + XHCI_CAPLENGTH) & 0xFF;

    LOG_INFO("HCI ver: 0x%4.4X, caps: 0x%2.2X",dev->version, dev->caps);

    
    /* Reset usb controller */
    pb_setbit32(1<<30, dev->base + XHCI_DCTL);

    while (pb_read32(dev->base + XHCI_DCTL) & (1<<30))
        asm("nop");



    pb_write32((uint32_t) _ev_buffer, dev->base + XHCI_GEVNTADRLO);
    pb_write32(0 , dev->base + XHCI_GEVNTADRHI);
    pb_write32(XHCI_EV_BUFFER_SIZE, dev->base + XHCI_GEVNTSIZ);
    pb_write32(0, dev->base + XHCI_GEVNTCOUNT);
    
    pb_write32( (2 << 12), dev->base + XHCI_GCTL);
    pb_write32( (1 << 11), dev->base + XHCI_DCFG);

    pb_write32(0xF,dev->base + XHCI_DEVTEN);

    err = xhci_command(dev, 0, XHCI_DEPCMD_STARTNEWC, 0,0,0);

    if (err != PB_OK)
        return err;

    /* Perform EP0 out configuration */

    err = xhci_command(dev, 0, XHCI_DEPCMD_SETEPCONF, 
                    (0x200 << 3),
                    (1 << 10) | (1 << 8),
                    0);

    if (err != PB_OK)
        return err;

    err = xhci_command(dev, 1, XHCI_DEPCMD_SETEPCONF, 
                    (0x200 << 3),
                    (1 << 25) | (1 << 10) | (1 << 8),
                    0);

    if (err != PB_OK)
        return err;

    err = xhci_command(dev, 0, XHCI_DEPCMD_SETTRANRE, 1,0,0);

    if (err != PB_OK)
        return err;

    err = xhci_command(dev, 1, XHCI_DEPCMD_SETTRANRE, 1,0,0);

    if (err != PB_OK)
        return err;

    LOG_DBG("Preparing setup pkt");
    setup_trb.bptrh = 0;
    setup_trb.bptrl = (uint32_t) &setup_pkt;
    setup_trb.ssz = 8;
    setup_trb.control = (2 << 4) | (1 << 11) | (1 << 1) | 1;
    
    err = xhci_command(dev, 0, XHCI_DEPCMD_STARTRANS,
                        0,
                        (uint32_t) &setup_trb,
                        0);

    if (err != PB_OK)
        return err;

    pb_write32(3, dev->base + XHCI_DALEPENA);
    pb_write32(1<<31, dev->base + XHCI_DCTL);

    LOG_INFO("Done");
    return PB_OK;
}


uint32_t xhci_transfer(struct xhci_device *dev, 
            uint8_t ep, uint8_t *bfr, uint32_t sz)
{
    return PB_ERR;
}

void xhci_task(struct xhci_device *dev)
{
    uint32_t evcnt = pb_read32(dev->base + XHCI_GEVNTCOUNT);
    uint32_t ev;

    if (evcnt)
    {
        ev = _ev_buffer[_ev_index];
        
        /* Device Specific Events DEVT*/
        if (ev & 1)
        {
            uint8_t ev_dev = (ev >> 1) & 0x7F;
            uint8_t ev_type = (ev >> 8) & 0xF;
            LOG_INFO("%2.2X %2.2X", setup_pkt.bRequestType, setup_pkt.bRequest);
            switch (ev_type)
            {
                case 3: /* USB/Link state change */
                {
                    LOG_INFO("Link state change");
                }
                break;
                case 2: /* Connection done */
                {
                    LOG_INFO("Connection done");
                }
                break;
                case 1: /* USB Reset*/
                {
                    LOG_INFO("USB Reset");
                }
                break;
                default:
                    LOG_ERR("Unknown event %2.2X", ev_type);
            }
        } else { /* Device Endpoint-n events*/

            uint8_t ep = (ev >> 1) & 0x1F;
            uint16_t ev_param = (ev >> 16) & 0xFFFF;
            uint8_t ev_status = (ev >> 12) & 0xF;
            uint8_t ev_cc = (ev >> 6) & 0xF;
            LOG_INFO("EV EP%u, param: %4.4X, sts: %1X, cc: %1X",
                    ep, ev_param, ev_status, ev_cc);
        }


        _ev_index = (_ev_index + 4) % XHCI_EV_BUFFER_SIZE;
        pb_write32(4, dev->base + XHCI_GEVNTCOUNT);
    }
}
