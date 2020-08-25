
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/arch.h>
#include <pb/io.h>
#include <pb/usb.h>
#include <plat/imx/dwc3.h>
#include <pb/plat.h>

#define DWC3_EV_BUFFER_SIZE 128
#define DWC3_DEF_TIMEOUT_ms 10
#define DWC3_NO_TRBS 128
#define DWC3_PHY_CTRL0            0xF0040
#define DWC3_PHY_CTRL0_REF_SSP_EN    BIT(2)

#define DWC3_PHY_CTRL1            0xF0044
#define DWC3_PHY_CTRL1_RESET        BIT(0)
#define DWC3_PHY_CTRL1_COMMONONN        BIT(1)
#define DWC3_PHY_CTRL1_ATERESET        BIT(3)
#define DWC3_PHY_CTRL1_VDATSRCENB0    BIT(19)
#define DWC3_PHY_CTRL1_VDATDETENB0    BIT(20)

#define DWC3_PHY_CTRL2            0xF0048
#define DWC3_PHY_CTRL2_TXENABLEN0    BIT(8)


static volatile __a4k __no_bss uint32_t _ev_buffer[DWC3_EV_BUFFER_SIZE];
static uint32_t _ev_index;
static __a4k __no_bss struct usb_setup_packet setup_pkt;
static __a4k __no_bss struct dwc3_trb trbs[DWC3_NO_TRBS];
static __no_bss struct dwc3_trb *act_trb[8];
static __iomem base;
static uint32_t version;
static uint32_t caps;
static struct pb_usb_interface iface;

static int dwc3_command(uint8_t ep,
                         uint32_t cmd,
                         uint32_t p0,
                         uint32_t p1,
                         uint32_t p2)
{
    if (ep > 7)
        return PB_ERR;
/*
    LOG_DBG("cmd %u, ep %u, p0 %x, p1 %x, p2 %x",
                cmd, ep, p0, p1, p2);
*/


    uint32_t param0_addr = (DWC3_DEPCMDPAR0_0 + 0x10*ep);
    uint32_t param1_addr = (DWC3_DEPCMDPAR1_0 + 0x10*ep);
    uint32_t param2_addr = (DWC3_DEPCMDPAR2_0 + 0x10*ep);

    pb_write32(p0, (base + param0_addr));
    pb_write32(p1, (base + param1_addr));
    pb_write32(p2, (base + param2_addr));

    pb_write32((DWC3_DEPCMD_ACT | cmd),
            (base + DWC3_DEPCMD_0 + (0x10*ep)));

    volatile uint32_t timeout = arch_get_us_tick();
    volatile uint32_t status;

    do
    {
        status = pb_read32((base + DWC3_DEPCMD_0 + (0x10*ep)));

        if ((arch_get_us_tick() - timeout) > (DWC3_DEF_TIMEOUT_ms*1000))
        {
            uint32_t ev_status = pb_read32(base + DWC3_DEPCMD_0 + 0x10*ep);
            LOG_ERR("CMD %x, Timeout, status = 0x%x", cmd,
                                        (ev_status >> 12) & 0xF);
            return PB_TIMEOUT;
        }
    } while (status & DWC3_DEPCMD_ACT);

    return PB_OK;
}

static int dwc3_config_ep(uint8_t ep,
                           uint32_t sz,
                           uint32_t type)
{
    int err;

    err = dwc3_command(ep, DWC3_DEPCMD_SETEPCONF,
                    ((sz << 3) | (1 << 22) | (type << 1)),
                    ((ep << 25) | (7 << 8)),
                    0);

    if (err != PB_OK)
        return err;

    err = dwc3_command(ep, DWC3_DEPCMD_SETTRANRE, 1, 0, 0);

    if (err != PB_OK)
        return err;


    return PB_OK;
}

static struct dwc3_trb * dwc3_get_next_trb(void)
{
    struct dwc3_trb *trb = (struct dwc3_trb *) &trbs[0];

    for (uint32_t n = 0; n < DWC3_NO_TRBS; n++)
    {
        arch_invalidate_cache_range((uintptr_t) trb, sizeof(*trb));

        if ((trb->control & 1) == 0)
            return trb;
        trb++;
    }

    return NULL;
}

static int dwc3_transfer_no_wait(int ep, void *bfr, size_t sz)
{
    struct dwc3_trb *trb = dwc3_get_next_trb();
    size_t xfr_sz = sz;

    pb_delay_us(200);

    if (ep & 1)
    {
        arch_clean_cache_range((uintptr_t) bfr, sz);
    }

    if (trb == NULL)
    {
        LOG_ERR("Could not get free TRB");
        return PB_ERR;
    }

    act_trb[ep] = trb;

    trb->bptrh = 0;
    trb->bptrl = (uintptr_t) bfr;
    trb->ssz = xfr_sz;
    trb->control = ((1 << 11) | (1 << 1) |1);

    switch (ep)
    {
        case USB_EP0_OUT:
            trb->control |= (2 << 4);
            trb->control |= (1 << 10);
        break;
        case USB_EP0_IN:
            trb->control |= (1 << 4);
            trb->control |= (1 << 10);
        break;
        default:
            trb->control |= (1 << 4);
    }

    LOG_INFO("trx EP%u %s, %p, sz %zu bytes", (ep>>1), (ep&1?"IN":"OUT"),
                    bfr, sz);

    arch_clean_cache_range((uintptr_t) trb, sizeof(*trb));

    return dwc3_command(ep, DWC3_DEPCMD_STARTRANS, 0, (uintptr_t) trb, 0);
}

static int dwc3_transfer(int ep, void *bfr, size_t sz)
{
    int rc;

    rc = dwc3_transfer_no_wait(ep, bfr, sz);

    if (rc != PB_OK)
        return rc;

    volatile struct dwc3_trb *trb = act_trb[ep];

    while ((trb->control & 1) == 1)
    {
        arch_invalidate_cache_range((uintptr_t) trb, sizeof(*trb));
        plat_wdog_kick();

        if ((ep == USB_EP2_OUT) ||
            (ep == USB_EP1_IN))
        {
            dwc3_process();
        }
    }

    return PB_OK;
}


static void dwc3_reset(void)
{
    /* Read DCFG and mask out Device address*/
    pb_clrbit32(0x3F8, base + DWC3_DCFG);

    dwc3_transfer_no_wait(USB_EP0_OUT, (uint8_t *) &setup_pkt,
                        sizeof(struct usb_setup_packet));
}

int dwc3_init(__iomem base_addr)
{
    int err;
    volatile uint32_t reg = 0;
    base = base_addr;
    _ev_index = 0;


    pb_clrbit32((DWC3_PHY_CTRL1_VDATSRCENB0 | DWC3_PHY_CTRL1_VDATDETENB0 |
            DWC3_PHY_CTRL1_COMMONONN), base + DWC3_PHY_CTRL1);

    pb_setbit32((DWC3_PHY_CTRL1_RESET | DWC3_PHY_CTRL1_ATERESET),
                    base + DWC3_PHY_CTRL1);

    pb_setbit32(DWC3_PHY_CTRL0_REF_SSP_EN, base + DWC3_PHY_CTRL0);

    pb_setbit32(DWC3_PHY_CTRL2_TXENABLEN0, base + DWC3_PHY_CTRL2);

    pb_clrbit32((DWC3_PHY_CTRL1_RESET | DWC3_PHY_CTRL1_ATERESET),
                    base + DWC3_PHY_CTRL1);

    version = (pb_read32(base + DWC3_CAPLENGTH) >> 16) & 0xFFFF;
    caps = pb_read32(base + DWC3_CAPLENGTH) & 0xFF;

    memset((void *)_ev_buffer, 0, sizeof(_ev_buffer));
    memset((void *)trbs, 0, sizeof(trbs));

    for (uint32_t n = 0; n < 8; n++)
        act_trb[n] = NULL;

    LOG_INFO("HCI ver: 0x%x, caps: 0x%x", version, caps);

    /* Put core in reset */
    pb_setbit32(1<<11, base + DWC3_GCTL);
    /* Reset USB3 PHY */
    pb_setbit32(1 << 31, base + DWC3_GUSB3PIPECTL);
    /* Reset USB2 PHY */
    pb_setbit32(1 << 31, base + DWC3_GUSB2PHYCFG);

    pb_delay_ms(100);

    /* Release resets */
    pb_clrbit32(1 << 31, base + DWC3_GUSB3PIPECTL);
    pb_clrbit32(1 << 31, base + DWC3_GUSB2PHYCFG);

    pb_delay_ms(100); /* TODO: Is this really needed? */

    pb_clrbit32(1<<11, base + DWC3_GCTL);
    pb_clrbit32(1<<6, base + DWC3_GUSB2PHYCFG);

    /* Reset usb controller */
    pb_setbit32(1<<30, base + DWC3_DCTL);

    do
    {
        reg = pb_read32(base + DWC3_DCTL);
    } while (reg & (1 << 30));

    arch_clean_cache_range((uintptr_t) _ev_buffer, sizeof(_ev_buffer));
    pb_write32((uintptr_t) _ev_buffer, base + DWC3_GEVNTADRLO);

    pb_clrbit32(DWC3_GCTL_SCALEDOWN_MASK, base + DWC3_GCTL);
    pb_clrbit32(1<<17, base + DWC3_GCTL);


    pb_write32(0x82, base + DWC3_GTXFIFOSIZ_0);
    pb_write32(0x305, base + DWC3_GRXFIFOSIZ_0);
    /* Configure event buffer */
    pb_write32(0 , base + DWC3_GEVNTADRHI);
    pb_write32(DWC3_EV_BUFFER_SIZE*4, base + DWC3_GEVNTSIZ);
    pb_write32(0, base + DWC3_GEVNTCOUNT);
    pb_setbit32(((2 << 12) | 1), base + DWC3_GCTL);
    pb_setbit32((1 << 11), base + DWC3_DCFG);

    pb_write32((0xF |(1 << 9)), base + DWC3_DEVTEN);

    err = dwc3_command(0, DWC3_DEPCMD_STARTNEWC, 0, 0, 0);

    if (err != PB_OK)
        return err;

    /* Perform EP0 out configuration */
    err = dwc3_config_ep(0, 64, 0);

    if (err != PB_OK)
        return err;

    err = dwc3_config_ep(1, 64, 0);

    if (err != PB_OK)
        return err;

    dwc3_reset();

    pb_write32(3, base + DWC3_DALEPENA);
    pb_setbit32(1<<31, base + DWC3_DCTL);


    iface.read = dwc3_transfer;
    iface.write = dwc3_transfer;
    iface.set_address = dwc3_set_address;
    iface.set_configuration = dwc3_set_configuration;
    iface.enumerated = false;

    LOG_INFO("Done");
    return PB_OK;
}



int dwc3_set_address(uint32_t addr)
{
    volatile uint32_t reg = pb_read32(base + DWC3_DCFG);

    reg = (addr << 3) | (64 << 17) | (1 << 11);
    pb_write32(reg, base + DWC3_DCFG);
    return PB_OK;
}

int dwc3_set_configuration(void)
{
    dwc3_command(0, DWC3_DEPCMD_STARTNEWC|(2<<16), 0, 0, 0);
    dwc3_config_ep(USB_EP1_IN, 512, 2);
    dwc3_config_ep(USB_EP2_OUT, 512, 2);

    pb_setbit32((1 << USB_EP2_OUT) |
                (1 << USB_EP1_IN), base + DWC3_DALEPENA);

    return PB_OK;
}

static bool dwc3_trb_hwo(struct dwc3_trb *trb)
{
    if (trb == NULL)
        return false;

    arch_invalidate_cache_range((uintptr_t) trb, sizeof(*trb));

    if ((trb->control & 1) == 1)
        return false;

    return true;
}

int dwc3_process(void)
{
    uint32_t evcnt = pb_read32(base + DWC3_GEVNTCOUNT);
    volatile uint32_t ev;

    if (evcnt >= 4)
    {
        arch_invalidate_cache_range((uintptr_t) _ev_buffer, sizeof(_ev_buffer));
        ev = _ev_buffer[_ev_index];

        /* Device Specific Events DEVT*/
        if (ev & 1)
        {
            uint8_t ev_type = ((ev >> 8) & 0xF);

            switch (ev_type)
            {
                case 3: /* USB/Link state change */
                {
                    LOG_INFO("Link state change 0x%x", ev);
                }
                break;
                case 2: /* Connection done */
                {
                    pb_write32(1 << 17, base + DWC3_GUSB3PIPECTL);
                    LOG_INFO("Connection done 0x%x", ev);
                }
                break;
                case 1: /* USB Reset*/
                {
                    LOG_INFO("USB Reset %u 0x%x", _ev_index, ev);
                    // dwc3_reset(pdev);
                }
                break;
                default:
                    LOG_ERR("Unknown event %x", ev_type);
            }
        }
        else
        { /* Device Endpoint-n events*/
          /*
            uint32_t ep = (ev >> 1) & 0x1F;
            uint32_t ev_param = (ev >> 16) & 0xFFFF;
            uint32_t ev_status = (ev >> 12) & 0xF;
            uint32_t ev_cc = (ev >> 6) & 0xF;
            LOG_DBG("EV EP%u %s, param: %x, sts: %x, cc: %x",
                    ep>>1,ep&1?"IN":"OUT", ev_param, ev_status, ev_cc);
           */
        }


        _ev_index = (((_ev_index + 1) % DWC3_EV_BUFFER_SIZE));
        /*LOG_DBG("ev_index %u", _ev_index);*/
        pb_write32(4, base + DWC3_GEVNTCOUNT);
    }

    if (dwc3_trb_hwo(act_trb[USB_EP0_OUT]))
    {
        arch_invalidate_cache_range((uintptr_t) &setup_pkt, sizeof(setup_pkt));
        pb_delay_ms(1);

        usb_process_setup_pkt(&iface, &setup_pkt);
        dwc3_transfer_no_wait(USB_EP0_OUT, (uint8_t *)&setup_pkt,
                        sizeof(struct usb_setup_packet));
    }

    return PB_OK;
}

int dwc3_read(void *buf, size_t size)
{
    int rc;
    rc = dwc3_transfer(USB_EP2_OUT, buf, size);
    arch_invalidate_cache_range((uintptr_t) buf, size);
    return rc;
}

int dwc3_write(void *buf, size_t size)
{
    arch_clean_cache_range((uintptr_t) buf, size);
    return dwc3_transfer(USB_EP1_IN, buf, size);
}

bool dwc3_ready(void)
{
    return iface.enumerated;
}

